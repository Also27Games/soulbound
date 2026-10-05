#include "Simulation/MatchSimulation.h"

#include "Decider/Decider.h"
#include "HAL/PlatformTime.h"
#include "Match/MatchStart.h"
#include "Match/TurnRules.h"

namespace
{
	/** Respuesta de un decisor que todavía no ha llegado: qué destino confirmará y cuándo. */
	struct FPendingAnswer
	{
		bool bPending = false;
		double AtSeconds = 0.0;
		int32 TileId = INDEX_NONE;
	};

	/** Marca de tiempo del registro: minutos y segundos del reloj de partida, "[mm:ss]". */
	FString Stamp(double ClockSeconds)
	{
		const int32 Seconds = FMath::FloorToInt32(ClockSeconds);
		return FString::Printf(TEXT("[%02d:%02d]"), Seconds / 60, Seconds % 60);
	}

	/** Nombre de una elección para la crónica (S-009). */
	const TCHAR* ChoiceName(int32 Kind)
	{
		switch (static_cast<EChoiceKind>(Kind))
		{
		case EChoiceKind::DiceCount: return TEXT("dados");
		case EChoiceKind::Reroll: return TEXT("repetir");
		case EChoiceKind::PlanarJump: return TEXT("salto planar");
		case EChoiceKind::PlayerAction: return TEXT("acción contra un jugador");
		case EChoiceKind::SafeRune: return TEXT("runa a salvo de Koschei");
		case EChoiceKind::ShellPick: return TEXT("esfera del trilero");
		default: return TEXT("?");
		}
	}

	/** "casilla 17 (Fuente de Poder, Dominio 3)": lo que hace falta para leer adónde va alguien. */
	FString DescribeTile(const FBoardState& Board, int32 TileId)
	{
		if (!Board.IsValidTile(TileId))
		{
			return FString::Printf(TEXT("casilla %d"), TileId);
		}
		const FTileState& Tile = Board.Tiles[TileId];
		FString Kind;
		switch (Tile.Type)
		{
		case ETileType::Altar: return FString::Printf(TEXT("casilla %d (Altar)"), TileId);
		case ETileType::Niche: Kind = TEXT("Nicho, "); break;
		case ETileType::PowerSource: Kind = TEXT("Fuente de Poder, "); break;
		case ETileType::PowerNode:
			Kind = Tile.PowerNodeEffect == EPowerNodeEffect::Duel ? TEXT("Nodo de Duelo, ")
				: Tile.PowerNodeEffect == EPowerNodeEffect::Oracle ? TEXT("Nodo del Oráculo, ")
				: TEXT("Nodo Planar, ");
			break;
		default: break;
		}
		return FString::Printf(TEXT("casilla %d (%sDominio %d)"), TileId, *Kind, Tile.DomainId);
	}
}

FMatchSimulation::FMatchSimulation(const FSimulationParams& InParams, TArray<TSharedPtr<IDecider>> InDeciders)
	: Params(InParams)
	, Deciders(MoveTemp(InDeciders))
{
}

FSimulationResult FMatchSimulation::Run()
{
	const double WallStart = FPlatformTime::Seconds();

	FSimulationResult Result;
	// Tres sorteos separados, como en el juego (decisión del autor del 05/10/2026): el de la partida (inicio y dados), el
	// de los decisores (Seed + 1) y el del cerebro del Warden (Seed + 2, como WardenHostSubsystem). Así la misma semilla da
	// los mismos dados ocupe quien ocupe cada asiento (grabación y reproducción, GDD 08), aunque un trilero retrase los
	// turnos respecto a las evaluaciones del Warden (S-010).
	FRandomStream Rng(Params.Seed);
	FRandomStream DeciderRng(Params.Seed + 1);
	FRandomStream WardenRng(Params.Seed + 2);
	// En Fase 1 los bots rellenan los huecos: siempre juegan cuatro (R-TAB-6, decisión del autor del 25/09/2026).
	FMatchStartResult Start = Soulbound::Rules::StartMatch(Params.Board, Params.MatchParams, Params.Elects, Soulbound::Match::MaxPlayers, Rng);
	if (!Start.IsSuccess())
	{
		Result.Log.Add(FString::Printf(TEXT("error semilla=%d \"%s\""), Params.Seed, *Start.Message));
		Result.WallSeconds = FPlatformTime::Seconds() - WallStart;
		return Result;
	}
	FMatchState State = MoveTemp(Start.State);
	const FWardenBrain Brain;

	// El registro se lee como una crónica: una línea por turno, por destino y por intervención del Warden.
	Result.Log.Add(FString::Printf(TEXT("Partida | semilla %d | orden de turno %s | %d casillas | reloj de turno %.0f s | respuesta del decisor %.0f s"),
		Params.Seed, *FString::JoinBy(State.TurnOrder, TEXT(", "), [](int32 Seat) { return FString::FromInt(Seat); }),
		State.Board.Tiles.Num(), State.Params.TurnClockSeconds, Params.DummyResponseSeconds));

	double NextEvaluation = Params.EvaluationPeriodSeconds;
	double NextTurnEvent = 0.0;
	FPendingAnswer Pending;
	int32 LoggedEvents = 0;

	// Pasa al registro de texto lo que el motor ha dejado en su registro de eventos y la crónica no cuenta ya de otra
	// forma: agotamientos, turnos saltados, Sellos que caen por el tope y la carrera por el vínculo (vínculos, Despertar,
	// hogares, vueltas al hogar, victoria del Warden y fin de carrera) y los movimientos (Moved, S-007). Inicio y tirada
	// los escribe StepTurn, que tiene las opciones; los Sellos que pone o quita el Warden, su propia línea.
	auto LogNewEvents = [&Result, &State, &LoggedEvents]()
	{
		for (; LoggedEvents < State.EventLog.Num(); ++LoggedEvents)
		{
			const FMatchEvent& Event = State.EventLog[LoggedEvents];
			// El liberado por tope va justo antes del SealPlaced que lo provoca (R-TAB-22).
			const bool bDroppedByCap = Event.Type == EMatchEventType::SealReleased && State.EventLog.IsValidIndex(LoggedEvents + 1)
				&& State.EventLog[LoggedEvents + 1].Type == EMatchEventType::SealPlaced;
			if (bDroppedByCap)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    cae el Sello de la %s"), *Stamp(Event.ClockSeconds), *DescribeTile(State.Board, Event.Value)));
			}
			if (Event.Type == EMatchEventType::Moved)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d va a la %s"), *Stamp(Event.ClockSeconds), Event.Seat, *DescribeTile(State.Board, Event.Value)));
			}
			else if (Event.Type == EMatchEventType::TurnClockExpired)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d agota el reloj | insolencia %d"), *Stamp(Event.ClockSeconds), Event.Seat, State.Seats[Event.Seat].Insolence));
			}
			else if (Event.Type == EMatchEventType::TurnSkipped)
			{
				Result.Log.Add(FString::Printf(TEXT("%s asiento %d pierde el turno | le quedan %d"), *Stamp(Event.ClockSeconds), Event.Seat, State.Seats[Event.Seat].LostTurns));
			}
			else if (Event.Type == EMatchEventType::Bonded)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d se vincula al Elegido del Dominio %d"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::BoardAwakening)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d despierta el tablero: los Nodos se revelan"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::WardenVictory)
			{
				Result.Log.Add(FString::Printf(TEXT("%s Victoria del Warden: nadie se ha vinculado"), *Stamp(Event.ClockSeconds)));
			}
			else if (Event.Type == EMatchEventType::BondRaceEnded)
			{
				Result.Log.Add(FString::Printf(TEXT("%s Fin de la carrera | %d Elegidos"), *Stamp(Event.ClockSeconds), Event.Value));
			}
			else if (Event.Type == EMatchEventType::HomeAssigned)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d queda Eco | hogar: Nicho Cerrado del Dominio %d"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::ReturnedHome)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d vuelve a su hogar, la %s"), *Stamp(Event.ClockSeconds), Event.Seat, *DescribeTile(State.Board, Event.Value)));
			}
			else if (Event.Type == EMatchEventType::RuneObtained)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d obtiene la runa %d"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::SkillMinigamePassed || Event.Type == EMatchEventType::SkillMinigameFailed)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d %s el minijuego de la Fuente del Dominio %d"), *Stamp(Event.ClockSeconds), Event.Seat,
					Event.Type == EMatchEventType::SkillMinigamePassed ? TEXT("supera") : TEXT("falla"), Event.Value));
			}
			else if (Event.Type == EMatchEventType::SourceDepleted)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d llega a la Fuente del Dominio %d, agotada: sin runa"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::RuneReturned)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d devuelve la runa %d a su Fuente"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::DiceChosen)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d elige %d dados para tirar"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::Rerolled)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d repite la tirada: %d"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::PlanarJumped)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d salta a la %s"), *Stamp(Event.ClockSeconds), Event.Seat, *DescribeTile(State.Board, Event.Value)));
			}
			else if (Event.Type == EMatchEventType::Expelled)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d manda al asiento %d a su hogar de una expulsión"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat));
			}
			else if (Event.Type == EMatchEventType::ChoiceExpired)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d deja agotar la elección (%s)"), *Stamp(Event.ClockSeconds), Event.Seat, ChoiceName(Event.Value)));
			}
			// S-010: el robo del Eco y el trilero.
			else if (Event.Type == EMatchEventType::ShellGameStarted)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d abre el trilero contra el asiento %d: %d esferas"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::SafeRuneSetAside)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d (Koschei) aparta la runa %d antes del trilero"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::RuneStolen)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d roba la runa %d al asiento %d"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value, Event.OtherSeat));
			}
			else if (Event.Type == EMatchEventType::ShellGameExpired)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d no elige esfera a tiempo: no roba nada"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::StealNoRunes)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d intenta robar al asiento %d, que no tiene runas"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat));
			}
			else if (Event.Type == EMatchEventType::StealCancelled)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d intenta robar al asiento %d (Koschei), que solo tiene una runa: robo anulado"), *Stamp(Event.ClockSeconds),
					Event.Seat, Event.OtherSeat));
			}
		}
	};

	// Contesta al instante la elección pendiente con el decisor del asiento al que se le pide (S-009; desde S-010 puede no
	// tener el turno), como el minijuego: así la duración de un turno no depende de lo que se elija, ni los dados de una
	// semilla de quién ocupe cada asiento. Devuelve false si el motor rechaza la respuesta (no debería: el decisor elige
	// entre las opciones).
	auto AnswerChoice = [&](double Now) -> bool
	{
		const int32 Seat = State.PendingChoice.Seat;
		const FPendingChoice Choice = State.PendingChoice;
		LogNewEvents();
		const bool bHasDecider = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid();
		const int32 Option = bHasDecider ? Deciders[Seat]->ChooseOption(State, Seat, Choice, DeciderRng)
			: Choice.CanDecline() ? INDEX_NONE : Choice.AnswerAt(0);
		const bool bAccepted = Soulbound::Rules::SubmitChoiceIntent(State, Seat, Choice.Kind, Option, Now, Rng) == EIntentResult::Accepted;
		LogNewEvents();
		return bAccepted;
	};

	// Juega el turno en el instante actual hasta que haya que esperar a alguien. Devuelve cuándo volver.
	// Las reglas son del motor: aquí solo se pregunta al decisor, se entrega su intención y se avanza el reloj.
	auto StepTurn = [&]() -> double
	{
		const double Now = State.ClockSeconds;
		while (true)
		{
			// Elecciones de dados, salto, acción y trilero (S-009, S-010): al instante. La repetición, en la tirada, tras la línea
			// del turno. Contestada una, se vuelve a mirar: puede abrir otra en el acto (salto rechazado y acción en el Nodo,
			// robo y runa de Koschei). Hasta S-010 se seguía hacia AdvanceTurn con la nueva abierta y la simulación se paraba.
			if (State.PendingChoice.IsOpen() && State.PendingChoice.Kind != EChoiceKind::Reroll)
			{
				if (!AnswerChoice(Now))
				{
					return TNumericLimits<double>::Max();
				}
				continue;
			}
			if (State.TurnPhase == ETurnPhase::RollAndChoose)
			{
				if (!Pending.bPending)
				{
					const int32 Seat = State.ActiveSeat;
					LogNewEvents();
					Result.Log.Add(FString::Printf(TEXT("%s Turno %d | asiento %d | tira %d%s | %d opciones"),
						*Stamp(Now), State.TurnCount + 1, Seat, State.Roll, State.DiceCount > 1 ? TEXT(" con dos dados") : TEXT(""), State.ReachableOptions.Num()));
					if (State.PendingChoice.Kind == EChoiceKind::Reroll && !AnswerChoice(Now))
					{
						return TNumericLimits<double>::Max();
					}
					Pending.bPending = true;
					Pending.AtSeconds = Now + Params.DummyResponseSeconds;
					Pending.TileId = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid()
						? Deciders[Seat]->ChooseDestination(State, Seat, State.ReachableOptions, DeciderRng)
						: INDEX_NONE;
				}
				// El siguiente suceso de turno es la respuesta del decisor o el límite del reloj, lo que llegue antes.
				const double WakeAt = FMath::Min(Pending.AtSeconds, State.TurnDeadlineSeconds);
				if (Now < WakeAt)
				{
					return WakeAt;
				}
				if (Now >= Pending.AtSeconds && Pending.TileId != INDEX_NONE)
				{
					const EIntentResult Intent = Soulbound::Rules::SubmitDestinationIntent(State, State.ActiveSeat, Pending.TileId, Now);
					if (Intent == EIntentResult::Accepted)
					{
						// El movimiento sale del registro de eventos (Moved), como el resto de sucesos de la crónica.
						LogNewEvents();
					}
				}
				else if (Now >= Pending.AtSeconds && State.ReachableOptions.IsEmpty())
				{
					Result.Log.Add(FString::Printf(TEXT("%s    asiento %d no puede moverse desde la %s"), *Stamp(Now), State.ActiveSeat,
						*DescribeTile(State.Board, State.Seats[State.ActiveSeat].TileId)));
				}
				Pending.bPending = false;
			}
			if (State.TurnPhase == ETurnPhase::SkillTrial)
			{
				// Como con el destino: se pregunta al decisor y se entrega su resultado. Un dummy responde al instante, sin el
				// retardo del destino: así la duración de un turno no depende de dónde caiga la ficha, y los dados de una
				// semilla no cambian según quién ocupe cada asiento (S-003, revisión del autor).
				const int32 Seat = State.ActiveSeat;
				LogNewEvents();
				const bool bPassed = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid()
					&& Deciders[Seat]->PassSkillMinigame(State, Seat, State.PendingTrial, State.PendingDifficulty, DeciderRng);
				if (Soulbound::Rules::SubmitSkillTrialResult(State, Seat, bPassed, Now) != EIntentResult::Accepted)
				{
					return TNumericLimits<double>::Max();
				}
				LogNewEvents();
				// Tras el minijuego puede quedar abierta una elección (S-009, R-INT-1): se contesta al principio del bucle.
				continue;
			}
			// Trilero (S-010): la muestra y la mezcla duran tiempo de partida, en datos; se vuelve al acabar la etapa. Sus
			// elecciones (runa de Koschei, esfera) se contestan al instante al principio del bucle, como las demás.
			if (State.TurnPhase == ETurnPhase::ShellGame && !State.PendingChoice.IsOpen() && Now < State.ShellGame.StageDeadlineSeconds)
			{
				return State.ShellGame.StageDeadlineSeconds;
			}
			if (!Soulbound::Rules::AdvanceTurn(State, Now, Rng) && State.TurnPhase != ETurnPhase::RollAndChoose)
			{
				// La máquina no avanza fuera de una espera: estado sin orden de turno. No se vuelve a llamar.
				return TNumericLimits<double>::Max();
			}
		}
	};

	// Bucle por sucesos: el reloj virtual salta al siguiente suceso, nunca avanza por fotogramas. Con la partida
	// terminada (victoria del Warden, R-CAR-8) no queda nada que simular: ni turnos ni Warden.
	while (!State.IsMatchOver())
	{
		const bool bEvaluationFirst = NextEvaluation <= NextTurnEvent;
		const double Next = bEvaluationFirst ? NextEvaluation : NextTurnEvent;
		if (Next > Params.DurationSeconds)
		{
			break;
		}
		State.ClockSeconds = Next;

		if (bEvaluationFirst)
		{
			const FWardenDecision Decision = Brain.Evaluate(State, WardenRng);
			FWardenBrain::Apply(State, Decision);
			++Result.Evaluations;
			if (Decision.IsIntervention())
			{
				++Result.Interventions;
				const TCHAR* Verb = Decision.Action == EWardenAction::SealTile ? TEXT("sella la ")
					: Decision.Action == EWardenAction::ReleaseTile ? TEXT("libera la ")
					: TEXT("comenta");
				const FString Target = Decision.Action == EWardenAction::Comment ? FString() : DescribeTile(State.Board, Decision.TileId);
				Result.Log.Add(FString::Printf(TEXT("%s Warden %s%s: \"%s\""), *Stamp(State.ClockSeconds), Verb, *Target, *Decision.Line));
			}
			LogNewEvents();
			NextEvaluation += Params.EvaluationPeriodSeconds;
		}
		else
		{
			NextTurnEvent = StepTurn();
			LogNewEvents();
		}
	}

	Result.Turns = State.TurnCount;
	Result.Log.Add(FString::Printf(TEXT("Fin | %d turnos | %d evaluaciones del Warden | %d intervenciones"), Result.Turns, Result.Evaluations, Result.Interventions));
	for (const int32 Seat : State.TurnOrder)
	{
		const FSeatState& SeatState = State.Seats[Seat];
		const FString Role = SeatState.Role == ESeatRole::Elect
			? FString::Printf(TEXT("Elegido del Dominio %d"), SeatState.DomainId)
			: SeatState.DomainId == INDEX_NONE ? FString(TEXT("Eco")) : FString::Printf(TEXT("Eco, hogar en el Dominio %d"), SeatState.DomainId);
		Result.Log.Add(FString::Printf(TEXT("    asiento %d, %s, en la %s | insolencia %d | runas por tipo %s"), Seat, *Role, *DescribeTile(State.Board, SeatState.TileId),
			SeatState.Insolence, *FString::JoinBy(SeatState.Runes, TEXT("/"), [](int32 Count) { return FString::FromInt(Count); })));
	}
	Result.FinalState = State;
	Result.WallSeconds = FPlatformTime::Seconds() - WallStart;
	return Result;
}
