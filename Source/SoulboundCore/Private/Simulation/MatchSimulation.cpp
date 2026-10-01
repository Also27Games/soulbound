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
	// Sorteo de la partida: inicio, dados y cerebro del Warden. Los decisores tienen el suyo, con semilla Seed + 1,
	// para que la misma semilla dé los mismos dados ocupe quien ocupe cada asiento (grabación y reproducción, GDD 08).
	FRandomStream Rng(Params.Seed);
	FRandomStream DeciderRng(Params.Seed + 1);
	// En Fase 1 los bots rellenan los huecos: siempre juegan cuatro (R-TAB-6, decisión del autor del 25/09/2026).
	FMatchStartResult Start = Soulbound::Rules::StartMatch(Params.Board, Params.MatchParams, Soulbound::Match::MaxPlayers, Rng);
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
		}
	};

	// Juega el turno en el instante actual hasta que haya que esperar a alguien. Devuelve cuándo volver.
	// Las reglas son del motor: aquí solo se pregunta al decisor, se entrega su intención y se avanza el reloj.
	auto StepTurn = [&]() -> double
	{
		const double Now = State.ClockSeconds;
		while (true)
		{
			if (State.TurnPhase == ETurnPhase::RollAndChoose)
			{
				if (!Pending.bPending)
				{
					const int32 Seat = State.ActiveSeat;
					LogNewEvents();
					Result.Log.Add(FString::Printf(TEXT("%s Turno %d | asiento %d | tira %d | %d opciones"),
						*Stamp(Now), State.TurnCount + 1, Seat, State.Roll, State.ReachableOptions.Num()));
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
			const FWardenDecision Decision = Brain.Evaluate(State, Rng);
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
		Result.Log.Add(FString::Printf(TEXT("    asiento %d, %s, en la %s | insolencia %d"), Seat, *Role, *DescribeTile(State.Board, SeatState.TileId), SeatState.Insolence));
	}
	Result.FinalState = State;
	Result.WallSeconds = FPlatformTime::Seconds() - WallStart;
	return Result;
}
