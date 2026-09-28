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

	const TCHAR* EventName(EMatchEventType Type)
	{
		switch (Type)
		{
		case EMatchEventType::TurnStarted: return TEXT("inicio");
		case EMatchEventType::Rolled: return TEXT("tirada");
		case EMatchEventType::TurnClockExpired: return TEXT("agotado");
		case EMatchEventType::TurnSkipped: return TEXT("saltado");
		case EMatchEventType::TurnEnded: return TEXT("fin");
		}
		return TEXT("?");
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
	FRandomStream Rng(Params.Seed);
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

	Result.Log.Add(FString::Printf(TEXT("inicio semilla=%d asientos=%d casillas=%d orden=%s"), Params.Seed, State.Seats.Num(), State.Board.Tiles.Num(),
		*FString::JoinBy(State.TurnOrder, TEXT(","), [](int32 Seat) { return FString::FromInt(Seat); })));

	double NextEvaluation = Params.EvaluationPeriodSeconds;
	double NextTurnEvent = 0.0;
	FPendingAnswer Pending;
	int32 LoggedEvents = 0;

	// Traduce al registro de texto los eventos que el motor ha añadido desde la última vez.
	auto LogNewEvents = [&Result, &State, &LoggedEvents]()
	{
		for (; LoggedEvents < State.EventLog.Num(); ++LoggedEvents)
		{
			const FMatchEvent& Event = State.EventLog[LoggedEvents];
			Result.Log.Add(FString::Printf(TEXT("t=%.0f %s asiento=%d valor=%d"), Event.ClockSeconds, EventName(Event.Type), Event.Seat, Event.Value));
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
					Pending.bPending = true;
					Pending.AtSeconds = Now + Params.DummyResponseSeconds;
					Pending.TileId = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid()
						? Deciders[Seat]->ChooseDestination(State, Seat, State.ReachableOptions, Rng)
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
						Result.Log.Add(FString::Printf(TEXT("t=%.0f destino asiento=%d casilla=%d"), Now, State.ActiveSeat, Pending.TileId));
					}
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

	// Bucle por sucesos: el reloj virtual salta al siguiente suceso, nunca avanza por fotogramas.
	while (true)
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
				Result.Log.Add(FString::Printf(TEXT("t=%.0f warden accion=%d casilla=%d \"%s\""),
					State.ClockSeconds, static_cast<int32>(Decision.Action), Decision.TileId, *Decision.Line));
			}
			NextEvaluation += Params.EvaluationPeriodSeconds;
		}
		else
		{
			NextTurnEvent = StepTurn();
			LogNewEvents();
		}
	}

	Result.Turns = State.TurnCount;
	Result.Log.Add(FString::Printf(TEXT("fin turnos=%d evaluaciones=%d intervenciones=%d"), Result.Turns, Result.Evaluations, Result.Interventions));
	Result.FinalState = State;
	Result.WallSeconds = FPlatformTime::Seconds() - WallStart;
	return Result;
}
