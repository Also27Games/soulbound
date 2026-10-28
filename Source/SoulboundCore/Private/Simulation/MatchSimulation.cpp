#include "Simulation/MatchSimulation.h"

#include "Decider/Decider.h"
#include "HAL/PlatformTime.h"
#include "Match/EffectCatalog.h"
#include "Match/MatchStart.h"
#include "Match/TurnRules.h"
#include "Match/WardenEventRules.h"
#include "Warden/WardenRules.h"

FSimulationPace FSimulationPace::Fast()
{
	return FSimulationPace();
}

FSimulationPace FSimulationPace::Human()
{
	FSimulationPace Pace;
	Pace.DestinationSeconds = 12.0;
	Pace.DestinationJitterSeconds = 4.0;
	Pace.ChoiceSeconds = 6.0;
	Pace.ChoiceJitterSeconds = 2.0;
	Pace.TrialSeconds = 8.0;
	Pace.DuelSeconds = 8.0;
	Pace.OracleSeconds = 3.0;
	Pace.VoteSeconds = 4.0;
	return Pace;
}

namespace
{
	/** Respuesta de un decisor que todavía no ha llegado: qué destino confirmará y cuándo. */
	struct FPendingAnswer
	{
		bool bPending = false;
		double AtSeconds = 0.0;
		int32 TileId = INDEX_NONE;
	};

	/** Qué espera una respuesta con ritmo (S-018, corte B) que no es el destino. */
	enum class EPacedWait : uint8
	{
		Choice,
		Vote,
		Oracle,
		Trial,
		Duel,
	};

	/**
	 * Respuesta con ritmo que todavía no ha llegado (S-018, corte B): a qué espera (tipo y dos datos que la distinguen, y su plazo) y cuándo
	 * llega. Si lo que se espera cambia (un golpe corta el turno, se abre otra elección), la respuesta deja de valer y se pide otra.
	 */
	struct FPacedAnswer
	{
		bool bActive = false;
		EPacedWait Wait = EPacedWait::Choice;
		int32 A = 0;
		int32 B = 0;
		double DeadlineSeconds = 0.0;
		double AtSeconds = 0.0;

		bool IsFor(EPacedWait InWait, int32 InA, int32 InB, double InDeadline) const
		{
			return bActive && Wait == InWait && A == InA && B == InB && DeadlineSeconds == InDeadline;
		}
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
		// S-014: las de los eventos del Warden; desde S-016 el cerebro real los lanza y los dummies las contestan.
		case EChoiceKind::Dilemma: return TEXT("dilema");
		case EChoiceKind::ChosenRival: return TEXT("rival señalado");
		case EChoiceKind::GiftRune: return TEXT("runa regalada");
		case EChoiceKind::LoseRune: return TEXT("runa a perder");
		default: return TEXT("?");
		}
	}

	/** Nombre de una fase del turno para la crónica (S-016: la fase en que se corta un turno). */
	const TCHAR* PhaseName(int32 Phase)
	{
		switch (static_cast<ETurnPhase>(Phase))
		{
		case ETurnPhase::Start: return TEXT("Start, al empezar");
		case ETurnPhase::PreRoll: return TEXT("PreRoll, antes de tirar");
		case ETurnPhase::RollAndChoose: return TEXT("RollAndChoose, eligiendo destino");
		case ETurnPhase::Moving: return TEXT("Moving, a medio camino");
		case ETurnPhase::MoveAndResolve: return TEXT("MoveAndResolve, tras mover");
		// S-018: el fin de la partida corta también lo que el Warden no corta.
		case ETurnPhase::SkillTrial: return TEXT("SkillTrial, en un minijuego");
		case ETurnPhase::Duel: return TEXT("Duel, en un duelo");
		case ETurnPhase::ShellGame: return TEXT("ShellGame, en un trilero");
		case ETurnPhase::OracleConsult: return TEXT("OracleConsult, ante el Oráculo");
		default: return TEXT("?");
		}
	}

	/** Canal y nombre de un evento del Warden para la crónica (S-016): «golpe "Tropiezo"»; los de prueba, por su identificador. */
	FString DescribeWardenEvent(const FMatchState& State, int32 EventIndex)
	{
		if (!State.WardenEvents.Events.IsValidIndex(EventIndex))
		{
			return FString::Printf(TEXT("el evento %d"), EventIndex);
		}
		const FWardenEventDef& Event = State.WardenEvents.Events[EventIndex];
		const TCHAR* Channel = !Event.bInBag ? TEXT("el evento") : Event.Channel == EWardenChannel::Strike ? TEXT("el golpe") : TEXT("la escena");
		return FString::Printf(TEXT("%s \"%s\""), Channel, *Event.DisplayName());
	}

	/**
	 * Cifras del Warden por acto (informe de S-016), contadas del registro: intervenciones con efecto, turnos empezados y cortados
	 * (por el acto en que empezaron) y el hueco más largo sin golpe ni escena (por el acto en que empieza), desde el inicio y hasta
	 * EndSeconds. El acto de cada instante sale del tiempo y del fin de la carrera, como CurrentAct. Desde S-017 (decisión del
	 * autor del 23/10/2026), las intervenciones son solo lo que decide el cerebro: los eventos que lanza (WardenEventStarted que no
	 * es una sentencia) y los golpes por el Oráculo, al abrir su consulta; las sentencias van aparte, por el origen de su consulta.
	 */
	TArray<FWardenActReport> WardenReport(const FMatchState& State, double EndSeconds)
	{
		TArray<FWardenActReport> Acts;
		Acts.SetNum(Soulbound::Warden::ActCount);
		for (FWardenActReport& Act : Acts)
		{
			Act.TriggerCounts.Init(0, Soulbound::Warden::TriggerCount);
			Act.SentencesByOrigin.Init(0, Soulbound::Match::OracleOriginCount);
		}
		const FWardenParams& Params = State.WardenParams;
		if (Params.Acts.Num() < Soulbound::Warden::ActCount)
		{
			return Acts;
		}
		double RaceEnd = TNumericLimits<double>::Max();
		for (const FMatchEvent& Event : State.EventLog)
		{
			if (Event.Type == EMatchEventType::BondRaceEnded)
			{
				RaceEnd = Event.ClockSeconds;
				break;
			}
		}
		auto ActAt = [&Params, RaceEnd](double Seconds)
		{
			if (Seconds >= Params.Acts[2].StartSeconds)
			{
				return 2;
			}
			return Seconds < Params.Acts[1].StartSeconds && Seconds < RaceEnd ? 0 : 1;
		};
		double LastEffect = 0.0;
		int32 TurnAct = 0;
		auto CountIntervention = [&](double Seconds)
		{
			++Acts[ActAt(Seconds)].EffectInterventions;
			FWardenActReport& Gap = Acts[ActAt(LastEffect)];
			Gap.MaxGapSeconds = FMath::Max(Gap.MaxGapSeconds, Seconds - LastEffect);
			LastEffect = Seconds;
		};
		// El origen de la última consulta abierta, para la sentencia que la sigue; y si el siguiente evento lanzado es esa sentencia.
		int32 OracleOrigin = INDEX_NONE;
		bool bSentenceEventNext = false;
		for (const FMatchEvent& Event : State.EventLog)
		{
			if (Event.Type == EMatchEventType::TurnStarted)
			{
				TurnAct = ActAt(Event.ClockSeconds);
				++Acts[TurnAct].TurnsStarted;
			}
			// S-018: el corte del fin de partida no es una decisión del Warden ni cambia el ritmo de nadie: no cuenta.
			else if (Event.Type == EMatchEventType::TurnInterrupted && Event.Detail != Soulbound::Match::MatchEndCut)
			{
				++Acts[TurnAct].TurnsCut;
			}
			else if (Event.Type == EMatchEventType::WardenEventStarted)
			{
				// La sentencia de una consulta no es una decisión del cerebro: el golpe por el Oráculo ya contó al abrirse.
				if (!bSentenceEventNext)
				{
					CountIntervention(Event.ClockSeconds);
				}
				bSentenceEventNext = false;
			}
			// S-017: consultas, sentencias por origen, consultas que acaban en insolencia y disparadores activos, por el acto en que ocurren.
			else if (Event.Type == EMatchEventType::OracleOpened)
			{
				OracleOrigin = Event.Value;
				if (Event.Value == static_cast<int32>(EOracleOrigin::Strike))
				{
					CountIntervention(Event.ClockSeconds);
				}
			}
			else if (Event.Type == EMatchEventType::OracleSentence)
			{
				FWardenActReport& Act = Acts[ActAt(Event.ClockSeconds)];
				++Act.Sentences;
				if (Act.SentencesByOrigin.IsValidIndex(OracleOrigin))
				{
					++Act.SentencesByOrigin[OracleOrigin];
				}
				bSentenceEventNext = true;
			}
			else if (Event.Type == EMatchEventType::OracleIgnored)
			{
				++Acts[ActAt(Event.ClockSeconds)].IgnoredConsults;
			}
			// S-017 (PR B): votaciones resueltas, desempates y votos al azar de quien no votó.
			else if (Event.Type == EMatchEventType::VoteResolved)
			{
				FWardenActReport& Act = Acts[ActAt(Event.ClockSeconds)];
				++Act.Votes;
				Act.VoteTies += Event.Detail == 1 ? 1 : 0;
			}
			else if (Event.Type == EMatchEventType::VoteCast && Event.Detail == 1)
			{
				++Acts[ActAt(Event.ClockSeconds)].RandomVotes;
			}
			else if (Event.Type == EMatchEventType::WardenTriggered && Acts[0].TriggerCounts.IsValidIndex(Event.Value))
			{
				++Acts[ActAt(Event.ClockSeconds)].TriggerCounts[Event.Value];
			}
		}
		FWardenActReport& Last = Acts[ActAt(LastEffect)];
		Last.MaxGapSeconds = FMath::Max(Last.MaxGapSeconds, EndSeconds - LastEffect);
		return Acts;
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
	// de los decisores (Seed + 1) y el del Warden (Seed + 2). Desde S-016 el del Warden vive en el estado (FWardenState, lo
	// fija StartMatch con la misma semilla más 2), igual en el anfitrión. Así la misma semilla da los mismos dados ocupe
	// quien ocupe cada asiento con el Warden apagado (grabación y reproducción, GDD 08).
	FRandomStream Rng(Params.Seed);
	FRandomStream DeciderRng(Params.Seed + 1);
	// En Fase 1 los bots rellenan los huecos: siempre juegan cuatro (R-TAB-6, decisión del autor del 25/09/2026). S-016: con
	// los eventos y, si juega, el Warden.
	const FWardenParams WardenParams = Params.bWardenEnabled ? Params.WardenParams : FWardenParams();
	FMatchStartResult Start = Soulbound::Rules::StartMatch(Params.Board, Params.MatchParams, Params.Elects, Soulbound::Match::MaxPlayers, Rng,
		Params.WardenEvents, WardenParams, Params.WardenLines);
	if (!Start.IsSuccess())
	{
		Result.Log.Add(FString::Printf(TEXT("error semilla=%d \"%s\""), Params.Seed, *Start.Message));
		Result.WallSeconds = FPlatformTime::Seconds() - WallStart;
		return Result;
	}
	FMatchState State = MoveTemp(Start.State);

	// El registro se lee como una crónica: una línea por turno, por destino y por intervención del Warden.
	Result.Log.Add(FString::Printf(TEXT("Partida | semilla %d | orden de turno %s | %d casillas | reloj de turno %.0f s | respuesta del decisor %.0f s"),
		Params.Seed, *FString::JoinBy(State.TurnOrder, TEXT(", "), [](int32 Seat) { return FString::FromInt(Seat); }),
		State.Board.Tiles.Num(), State.Params.TurnClockSeconds, Params.Pace.DestinationSeconds));

	double NextTurnEvent = 0.0;
	FPendingAnswer Pending;
	// S-018 (corte B): el ritmo. La variación sale de un generador propio con semilla Seed + 4 (Seed + 3 es la del reparto de hogares), y
	// solo si el ritmo la tiene: el rápido no saca nada y sus partidas son las de antes.
	FPacedAnswer Paced;
	FRandomStream PaceRng(Params.Seed + 4);
	// Turno cuya línea de crónica ya se escribió: con la repetición de Apep con ritmo, la línea va antes de esperar su respuesta.
	int32 TurnLineFor = INDEX_NONE;
	int32 LoggedEvents = 0;
	// Casillas en las que ha entrado la ficha en el movimiento en curso (S-020), para la línea del camino.
	TArray<int32> EnteredTiles;
	// Votos de la votación que se está resolviendo (S-017), para su línea: "0→3".
	TArray<FString> CastVotes;

	// Pasa al registro de texto lo que el motor ha dejado en su registro de eventos y la crónica no cuenta ya de otra
	// forma: agotamientos, turnos saltados, Sellos que caen por el tope y la carrera por el vínculo (vínculos, Despertar,
	// hogares, vueltas al hogar, victoria del Warden y fin de carrera) y los movimientos (Moved, S-007). Inicio y tirada
	// los escribe StepTurn, que tiene las opciones. Desde S-016, el Warden real: sus eventos con canal y nombre, sus
	// comentarios, los turnos que corta con la fase y lo que aplican sus efectos. Desde S-020, tras cada movimiento, una línea
	// con su camino (decisión del autor del 20/10/2026), de los TileEntered que lo preceden.
	auto LogNewEvents = [&Result, &State, &LoggedEvents, &EnteredTiles, &CastVotes]()
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
			if (Event.Type == EMatchEventType::TileEntered)
			{
				EnteredTiles.Add(Event.Value);
			}
			else if (Event.Type == EMatchEventType::Moved)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d va a la %s"), *Stamp(Event.ClockSeconds), Event.Seat, *DescribeTile(State.Board, Event.Value)));
				Result.Log.Add(FString::Printf(TEXT("%s    camino del asiento %d: %s"), *Stamp(Event.ClockSeconds), Event.Seat,
					*FString::JoinBy(EnteredTiles, TEXT(", "), [](int32 Tile) { return FString::FromInt(Tile); })));
				EnteredTiles.Reset();
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
				// S-018: con el motivo, nadie vinculado o el tiempo.
				Result.Log.Add(FString::Printf(TEXT("%s Victoria del Warden: %s"), *Stamp(Event.ClockSeconds),
					Event.Value == static_cast<int32>(EMatchEndReason::TimeOut) ? TEXT("se acaba el tiempo") : TEXT("nadie se ha vinculado")));
			}
			// S-018: el aspirante, el desafío final y la victoria de un jugador.
			else if (Event.Type == EMatchEventType::AspirantAnnounced)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d, ASPIRANTE: en el Altar con los seis tipos de runa"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::FinalChallengeStarted)
			{
				Result.Log.Add(FString::Printf(TEXT("%s DESAFÍO FINAL | asiento %d, en vez de su turno"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::FinalChallengeFailed)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d falla el desafío final (al azar): el Warden le arranca la runa del Dominio %d y lo manda a casa"),
					*Stamp(Event.ClockSeconds), Event.Seat, Event.Value));
			}
			else if (Event.Type == EMatchEventType::MatchWon)
			{
				Result.Log.Add(FString::Printf(TEXT("%s VICTORIA del asiento %d: supera el desafío final (al azar)"), *Stamp(Event.ClockSeconds), Event.Seat));
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
				// S-018 (corte B): el ping-pong del Altar se lee aquí.
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d manda al asiento %d a su hogar de una expulsión%s"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat,
					State.Board.IsAltar(Event.Detail) ? TEXT(", desde el Altar") : TEXT("")));
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
			// S-012: el duelo.
			else if (Event.Type == EMatchEventType::DuelChallenged)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d reta al asiento %d a duelo %s"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat,
					Event.Value == 1 ? TEXT("desde un Nodo de Duelo") : Event.Value == 2 ? TEXT("por orden del Warden") : TEXT("con el poder Duelo")));
			}
			// S-016: el Warden real.
			else if (Event.Type == EMatchEventType::WardenEventStarted)
			{
				Result.Log.Add(FString::Printf(TEXT("%s Warden lanza %s | dirige el asiento %d"), *Stamp(Event.ClockSeconds), *DescribeWardenEvent(State, Event.Value), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::WardenSpoke)
			{
				// S-021: la crónica de la simulación escribe la clave y la variante (1 a n), nunca el texto: SoulboundCore no tiene frases fuera de la tabla.
				const FString Key = State.WardenLines.Entries.IsValidIndex(Event.Value) ? State.WardenLines.Entries[Event.Value].Id : FString::Printf(TEXT("entrada %d"), Event.Value);
				Result.Log.Add(FString::Printf(TEXT("%s Warden dice %s (%d)%s"), *Stamp(Event.ClockSeconds), *Key, Event.Detail + 1,
					Event.Seat != INDEX_NONE ? *FString::Printf(TEXT(" al asiento %d"), Event.Seat) : TEXT("")));
			}
			else if (Event.Type == EMatchEventType::MatchEndWarning)
			{
				Result.Log.Add(FString::Printf(TEXT("%s Warden avisa: quedan %d s"), *Stamp(Event.ClockSeconds), Event.Value));
			}
			else if (Event.Type == EMatchEventType::WardenRemark)
			{
				Result.Log.Add(FString::Printf(TEXT("%s Warden comenta"), *Stamp(Event.ClockSeconds)));
			}
			else if (Event.Type == EMatchEventType::TurnInterrupted)
			{
				// S-017: también lo corta el fin de la carrera (Detail 1); S-018, el fin de la partida (Detail 2).
				Result.Log.Add(FString::Printf(TEXT("%s    %s corta el turno del asiento %d en %s"), *Stamp(Event.ClockSeconds),
					Event.Detail == Soulbound::Match::RaceEndCut ? TEXT("el fin de la carrera") : Event.Detail == Soulbound::Match::MatchEndCut ? TEXT("el fin de la partida") : TEXT("el Warden"),
					Event.Seat, PhaseName(Event.Value)));
				// A medio camino (S-020) no hay línea de camino: el movimiento no llegó.
				EnteredTiles.Reset();
			}
			else if (Event.Type == EMatchEventType::SealPlaced)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    el Warden sella la %s"), *Stamp(Event.ClockSeconds), *DescribeTile(State.Board, Event.Value)));
			}
			else if (Event.Type == EMatchEventType::EffectApplied)
			{
				const FString Effect = Soulbound::Rules::EffectCode(static_cast<EEffect>(Event.Value));
				Result.Log.Add(Event.Seat == INDEX_NONE
					? FString::Printf(TEXT("%s    efecto %s sobre la partida"), *Stamp(Event.ClockSeconds), *Effect)
					: FString::Printf(TEXT("%s    efecto %s sobre el asiento %d"), *Stamp(Event.ClockSeconds), *Effect, Event.Seat));
			}
			else if (Event.Type == EMatchEventType::WardenTaunt)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    el Warden se burla: %s sin objetivo válido"), *Stamp(Event.ClockSeconds),
					*Soulbound::Rules::EffectCode(static_cast<EEffect>(Event.Value))));
			}
			else if (Event.Type == EMatchEventType::DilemmaChosen)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d elige la opción %d del dilema%s"), *Stamp(Event.ClockSeconds), Event.Seat, Event.Value + 1,
					Event.Detail == 1 ? TEXT(", sorteada en su contra") : TEXT("")));
			}
			else if (Event.Type == EMatchEventType::WardenIgnored)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d desatiende al Warden (%s) | insolencia %d"), *Stamp(Event.ClockSeconds), Event.Seat, ChoiceName(Event.Value),
					State.Seats[Event.Seat].Insolence));
			}
			else if (Event.Type == EMatchEventType::Banished)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d, al Vacío"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::LeftVoid)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d sale del Vacío"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			// S-016 (B): la insolencia con castigo y la Furia.
			else if (Event.Type == EMatchEventType::InsolencePunished)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    el Warden castiga la insolencia %d del asiento %d"), *Stamp(Event.ClockSeconds), Event.Value, Event.Seat));
			}
			else if (Event.Type == EMatchEventType::FuryUnleashed)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    FURIA del Warden contra el asiento %d"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::FuryRefusal)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    el Warden se niega en el Vacío de la Furia del asiento %d (%s)"), *Stamp(Event.ClockSeconds), Event.Seat,
					Event.Value == 0 ? TEXT("Desvío") : TEXT("Liberación")));
			}
			else if (Event.Type == EMatchEventType::SentHome)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d, a su hogar: la %s"), *Stamp(Event.ClockSeconds), Event.Seat, *DescribeTile(State.Board, Event.Value)));
			}
			else if (Event.Type == EMatchEventType::DuelNoRunes)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d reta al asiento %d, sin runas: no hay duelo"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat));
			}
			else if (Event.Type == EMatchEventType::DuelWon)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d gana el duelo al asiento %d (%s, rendimiento al azar)"), *Stamp(Event.ClockSeconds), Event.Seat, Event.OtherSeat,
					Event.Value == 1 ? TEXT("el retado") : TEXT("el retador")));
			}
			else if (Event.Type == EMatchEventType::DuelLoserSentHome)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d pierde el duelo sin runas y vuelve a su hogar, la %s"), *Stamp(Event.ClockSeconds), Event.OtherSeat,
					*DescribeTile(State.Board, Event.Value)));
			}
			// S-017: el Oráculo, una línea por sentencia (y por lo que la cambia), y los disparadores no se listan: van en el informe.
			else if (Event.Type == EMatchEventType::OracleSentence)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    el Oráculo dicta al asiento %d %s%s"), *Stamp(Event.ClockSeconds), Event.Seat, *DescribeWardenEvent(State, Event.Value),
					Event.Detail == 1 ? TEXT(", tras el rechazo de Tifón") : TEXT("")));
			}
			else if (Event.Type == EMatchEventType::OracleEmpty)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    el orbe no muestra nada al asiento %d"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			else if (Event.Type == EMatchEventType::OracleIgnored)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d no consulta el Oráculo a tiempo | insolencia %d"), *Stamp(Event.ClockSeconds), Event.Seat, State.Seats[Event.Seat].Insolence));
			}
			else if (Event.Type == EMatchEventType::OracleRefused)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    asiento %d (Tifón) rechaza la sentencia"), *Stamp(Event.ClockSeconds), Event.Seat));
			}
			// S-017 (PR B): una línea por votación resuelta, con los votos (el que no votó, con un asterisco: su voto fue al azar).
			else if (Event.Type == EMatchEventType::VoteCast)
			{
				CastVotes.Add(FString::Printf(TEXT("%d→%d%s"), Event.Seat, Event.OtherSeat, Event.Detail == 1 ? TEXT("*") : TEXT("")));
			}
			else if (Event.Type == EMatchEventType::VoteResolved)
			{
				Result.Log.Add(FString::Printf(TEXT("%s    votación: %s | el más votado: asiento %d, con %d votos%s"), *Stamp(Event.ClockSeconds),
					*FString::Join(CastVotes, TEXT(", ")), Event.Seat, Event.Value, Event.Detail == 1 ? TEXT(" (desempate del Warden)") : TEXT("")));
				CastVotes.Reset();
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

	// S-018 (corte B): segundos que tarda una respuesta: los del ritmo, con su variación uniforme, y como mucho la mitad de lo que queda de
	// su plazo (decisión del autor del 27/10/2026): así ningún decisor agota un plazo ni es insolente. Sin plazo, Deadline es el máximo.
	auto PacedDelay = [&PaceRng](double Now, double Seconds, double Jitter, double Deadline) -> double
	{
		double Delay = Jitter > 0.0 ? FMath::Max(0.0, Seconds + PaceRng.FRandRange(-Jitter, Jitter)) : Seconds;
		if (Deadline < TNumericLimits<double>::Max())
		{
			Delay = FMath::Min(Delay, (Deadline - Now) * 0.5);
		}
		return Delay;
	};

	// Instante en que llega la respuesta a lo que se espera (Wait, A, B, Deadline), con el ritmo Seconds ± Jitter; Now si es en el acto. La
	// primera vez que se pregunta por una espera se sortea su hora; las siguientes, la misma, hasta que se usa (Paced.bActive = false).
	auto AnswerAt = [&](double Now, EPacedWait Wait, int32 A, int32 B, double Deadline, double Seconds, double Jitter) -> double
	{
		if (Seconds <= 0.0 && Jitter <= 0.0)
		{
			return Now;
		}
		if (!Paced.IsFor(Wait, A, B, Deadline))
		{
			Paced.bActive = true;
			Paced.Wait = Wait;
			Paced.A = A;
			Paced.B = B;
			Paced.DeadlineSeconds = Deadline;
			Paced.AtSeconds = Now + PacedDelay(Now, Seconds, Jitter, Deadline);
		}
		return Paced.AtSeconds;
	};

	// La hora de la respuesta a la elección pendiente: la distinguen su tipo, su asiento y el turno, y la acota su plazo.
	auto ChoiceAnswerAt = [&](double Now) -> double
	{
		const FPendingChoice& Choice = State.PendingChoice;
		return AnswerAt(Now, EPacedWait::Choice, static_cast<int32>(Choice.Kind) * 16 + Choice.Seat, State.TurnCount, Choice.DeadlineSeconds,
			Params.Pace.ChoiceSeconds, Params.Pace.ChoiceJitterSeconds);
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
			// S-018 (corte B): con ritmo, cada respuesta llega a su hora (AnswerAt); con el rápido, en el acto, como siempre.
			if (State.PendingChoice.IsOpen() && State.PendingChoice.Kind != EChoiceKind::Reroll)
			{
				const double At = ChoiceAnswerAt(Now);
				if (Now < At)
				{
					return At;
				}
				Paced.bActive = false;
				if (!AnswerChoice(Now))
				{
					return TNumericLimits<double>::Max();
				}
				continue;
			}
			// S-017 (R-VOT-5): una votación abierta, todos los que tienen a quién votar votan en el acto con su decisor, en orden de
			// asiento; el último voto la resuelve.
			if (State.Vote.IsOpen())
			{
				// Con ritmo, todos votan a la vez, a su hora, si alguno tiene a quién votar.
				bool bCanVote = false;
				for (int32 Seat = 0; Seat < State.Seats.Num() && !bCanVote; ++Seat)
				{
					bCanVote = State.Vote.Votes[Seat] == INDEX_NONE && !Soulbound::Rules::VoteOptions(State, Seat).IsEmpty() && Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid();
				}
				if (bCanVote)
				{
					const double At = AnswerAt(Now, EPacedWait::Vote, 0, 0, State.Vote.DeadlineSeconds, Params.Pace.VoteSeconds, 0.0);
					if (Now < At)
					{
						return At;
					}
					Paced.bActive = false;
				}
				LogNewEvents();
				bool bVoted = false;
				for (int32 Seat = 0; Seat < State.Seats.Num() && State.Vote.IsOpen(); ++Seat)
				{
					const TArray<int32> Options = Soulbound::Rules::VoteOptions(State, Seat);
					if (State.Vote.Votes[Seat] != INDEX_NONE || Options.IsEmpty() || !Deciders.IsValidIndex(Seat) || !Deciders[Seat].IsValid())
					{
						continue;
					}
					if (Soulbound::Rules::SubmitVote(State, Seat, Deciders[Seat]->ChooseVote(State, Seat, Options, DeciderRng), Now) != EIntentResult::Accepted)
					{
						return TNumericLimits<double>::Max();
					}
					bVoted = true;
				}
				LogNewEvents();
				if (bVoted)
				{
					continue;
				}
			}
			// S-017 (R-VOT-5, R-ORA-3): una consulta del Oráculo, dentro del turno o entre turnos, se consulta en el acto con el decisor
			// del consultante (los dummies nunca rechazan): así nunca se agota.
			if (State.Oracle.IsWaiting())
			{
				const int32 Seat = State.Oracle.Consultant;
				const double At = AnswerAt(Now, EPacedWait::Oracle, Seat, static_cast<int32>(State.Oracle.Origin), State.Oracle.DeadlineSeconds, Params.Pace.OracleSeconds, 0.0);
				if (Now < At)
				{
					return At;
				}
				Paced.bActive = false;
				LogNewEvents();
				const bool bRefuse = State.Oracle.bRefusable && Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid()
					&& Deciders[Seat]->RefusesOracleSentence(State, Seat, DeciderRng);
				const EIntentResult Intent = bRefuse ? Soulbound::Rules::SubmitOracleRefuse(State, Seat, Now) : Soulbound::Rules::SubmitOracleConsult(State, Seat, Now);
				if (Intent != EIntentResult::Accepted)
				{
					return TNumericLimits<double>::Max();
				}
				LogNewEvents();
				continue;
			}
			if (State.TurnPhase == ETurnPhase::RollAndChoose)
			{
				if (!Pending.bPending)
				{
					const int32 Seat = State.ActiveSeat;
					if (TurnLineFor != State.TurnCount)
					{
						LogNewEvents();
						Result.Log.Add(FString::Printf(TEXT("%s Turno %d | asiento %d | tira %d%s | %d opciones"),
							*Stamp(Now), State.TurnCount + 1, Seat, State.Roll, State.DiceCount > 1 ? TEXT(" con dos dados") : TEXT(""), State.ReachableOptions.Num()));
						TurnLineFor = State.TurnCount;
					}
					if (State.PendingChoice.Kind == EChoiceKind::Reroll)
					{
						const double At = ChoiceAnswerAt(Now);
						if (Now < At)
						{
							return At;
						}
						Paced.bActive = false;
						if (!AnswerChoice(Now))
						{
							return TNumericLimits<double>::Max();
						}
					}
					// S-018 (corte B): el destino, con su variación y como mucho a la mitad de lo que queda del reloj de turno.
					Pending.bPending = true;
					Pending.AtSeconds = Now + PacedDelay(Now, Params.Pace.DestinationSeconds, Params.Pace.DestinationJitterSeconds, State.TurnDeadlineSeconds);
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
			// S-020: la simulación entrega el camino entero en el acto, como los dummies del anfitrión; la salvaguarda no llega a
			// correr y la duración de un turno no cambia.
			if (State.TurnPhase == ETurnPhase::Moving)
			{
				if (Soulbound::Rules::SubmitMoveProgress(State, State.ActiveSeat, State.Movement.NextTile(), Now) != EIntentResult::Accepted)
				{
					return TNumericLimits<double>::Max();
				}
				continue;
			}
			if (State.TurnPhase == ETurnPhase::SkillTrial)
			{
				// Como con el destino: se pregunta al decisor y se entrega su resultado. Un dummy responde al instante, sin el
				// retardo del destino: así la duración de un turno no depende de dónde caiga la ficha, y los dados de una
				// semilla no cambian según quién ocupe cada asiento (S-003, revisión del autor).
				const int32 Seat = State.ActiveSeat;
				const double At = AnswerAt(Now, EPacedWait::Trial, static_cast<int32>(State.PendingTrial) * 16 + Seat, State.TurnCount, TNumericLimits<double>::Max(),
					Params.Pace.TrialSeconds, 0.0);
				if (Now < At)
				{
					return At;
				}
				Paced.bActive = false;
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
			if (State.TurnPhase == ETurnPhase::Duel)
			{
				// Duelo (S-012): como el minijuego, cada participante rinde al instante con su decisor, primero el retador y
				// después el retado; con los dos, el motor lo resuelve y puede abrir el trilero del botín. Con ritmo, los dos a la vez, a su hora.
				const double At = AnswerAt(Now, EPacedWait::Duel, State.Duel.Challenger * 16 + State.Duel.Challenged, State.TurnCount, TNumericLimits<double>::Max(),
					Params.Pace.DuelSeconds, 0.0);
				if (Now < At)
				{
					return At;
				}
				Paced.bActive = false;
				LogNewEvents();
				for (const int32 Seat : {State.Duel.Challenger, State.Duel.Challenged})
				{
					if (State.Duel.IsActive() && !State.Duel.HasPerformed(Seat))
					{
						const double Performance = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid() ? Deciders[Seat]->DuelPerformance(State, Seat, DeciderRng) : 0.0;
						if (Soulbound::Rules::SubmitDuelPerformance(State, Seat, Performance, Now) != EIntentResult::Accepted)
						{
							return TNumericLimits<double>::Max();
						}
					}
				}
				LogNewEvents();
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
	// terminada (victoria del Warden, R-CAR-8; desde S-018 también la de un jugador y el fin por tiempo) no queda nada que simular:
	// ni turnos ni Warden. Con Warden, la partida termina en matchEndSeconds (R-VIC-4): la máquina de turnos lo aplica al llamarla.
	const double MatchEndAt = State.WardenParams.HasWarden() ? State.WardenParams.MatchEndSeconds : TNumericLimits<double>::Max();
	while (!State.IsMatchOver())
	{
		// S-016 (R-WAR-1): el Warden evalúa en el instante que devolvió en la evaluación anterior, con el mismo contrato que el
		// anfitrión; una aplazada (R-WAR-7), en cuanto termina lo que la aplazó, que siempre acaba con un suceso del turno.
		double WardenAt = TNumericLimits<double>::Max();
		if (State.WardenParams.HasWarden())
		{
			WardenAt = !State.Warden.bDeferred ? State.Warden.NextEvaluationSeconds
				: Soulbound::Rules::IsWardenDeferred(State) ? TNumericLimits<double>::Max() : State.ClockSeconds;
		}
		const bool bEvaluationFirst = WardenAt <= NextTurnEvent;
		const double Next = bEvaluationFirst ? WardenAt : NextTurnEvent;
		// S-018 (R-VIC-4): el fin por tiempo llega antes que cualquier suceso a su hora o después, también una evaluación; corta lo que
		// haya en marcha y la partida termina. Solo AdvanceTurn mira el reloj.
		if (Next >= MatchEndAt && MatchEndAt <= Params.DurationSeconds)
		{
			State.ClockSeconds = FMath::Max(State.ClockSeconds, MatchEndAt);
			Soulbound::Rules::AdvanceTurn(State, State.ClockSeconds, Rng);
			LogNewEvents();
			break;
		}
		if (Next > Params.DurationSeconds)
		{
			break;
		}
		State.ClockSeconds = FMath::Max(State.ClockSeconds, Next);

		if (bEvaluationFirst)
		{
			const int32 TurnsBefore = State.TurnCount;
			const FWardenDecision Decision = Soulbound::Rules::EvaluateWarden(State, State.ClockSeconds, Rng);
			LogNewEvents();
			// Un turno cortado deja sin objeto el destino que el decisor tenía preparado; y lo que el Warden lanza (el evento,
			// sus elecciones, el turno siguiente) se atiende ya.
			if (State.TurnCount != TurnsBefore)
			{
				Pending.bPending = false;
			}
			if (Decision.IsIntervention())
			{
				NextTurnEvent = FMath::Min(NextTurnEvent, State.ClockSeconds);
			}
		}
		else
		{
			NextTurnEvent = StepTurn();
			LogNewEvents();
		}
	}

	Result.Turns = State.TurnCount;
	Result.Evaluations = State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::WardenEvaluated; }).Num();
	Result.WardenActs = WardenReport(State, State.IsMatchOver() ? State.ClockSeconds : Params.DurationSeconds);
	for (const FWardenActReport& Act : Result.WardenActs)
	{
		Result.Interventions += Act.EffectInterventions;
	}
	Result.Furies = State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::FuryUnleashed; }).Num();
	Result.RaceEndCuts = State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::TurnInterrupted && Event.Detail == 1; }).Num();
	// S-018: el resultado, uno de los tres finales o ninguno (sin Warden, la partida no termina por tiempo).
	const FString Outcome = State.Outcome == EMatchOutcome::PlayerVictory ? FString::Printf(TEXT("gana el asiento %d"), State.WinnerSeat)
		: State.Outcome == EMatchOutcome::WardenVictory ? FString(State.EndReason == EMatchEndReason::TimeOut ? TEXT("gana el Warden por tiempo") : TEXT("gana el Warden: nadie vinculado"))
		: FString(TEXT("sin resultado"));
	Result.Log.Add(FString::Printf(TEXT("Fin | %s | %d turnos | %d evaluaciones del Warden | %d intervenciones"), *Outcome, Result.Turns, Result.Evaluations, Result.Interventions));
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
