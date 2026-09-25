#include "Simulation/MatchSimulation.h"

#include "Board/MovementRules.h"
#include "Decider/Decider.h"
#include "HAL/PlatformTime.h"

FMatchSimulation::FMatchSimulation(const FSimulationParams& InParams, TArray<TSharedPtr<IDecider>> InDeciders)
	: Params(InParams)
	, Deciders(MoveTemp(InDeciders))
{
}

FSimulationResult FMatchSimulation::Run()
{
	const double WallStart = FPlatformTime::Seconds();

	FSimulationResult Result;
	FMatchState State = FMatchState::MakeSpikeMatch();
	FRandomStream Rng(Params.Seed);
	const FWardenBrain Brain;

	Result.Log.Add(FString::Printf(TEXT("inicio semilla=%d asientos=%d casillas=%d"), Params.Seed, State.Seats.Num(), State.Board.Tiles.Num()));

	double NextEvaluation = Params.EvaluationPeriodSeconds;
	double NextTurn = Params.TurnPeriodSeconds;

	// Bucle por sucesos: el reloj virtual salta al siguiente suceso, nunca avanza por fotogramas.
	while (true)
	{
		const bool bEvaluationFirst = NextEvaluation <= NextTurn;
		const double Next = bEvaluationFirst ? NextEvaluation : NextTurn;
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
			const int32 Seat = State.ActiveSeat;
			FSeatState& SeatState = State.Seats[Seat];
			const int32 Roll = Rng.RandRange(1, 6);
			const FMoverTraits Mover;
			const TArray<int32> Options = Soulbound::Rules::ReachableTiles(State.Board, SeatState.TileId, Roll, Mover);

			int32 Destination = INDEX_NONE;
			if (Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid())
			{
				Destination = Deciders[Seat]->ChooseDestination(State, Seat, Options, Rng);
			}
			if (Destination != INDEX_NONE && Options.Contains(Destination))
			{
				SeatState.TileId = Destination;
			}

			Result.Log.Add(FString::Printf(TEXT("t=%.0f turno=%d asiento=%d tirada=%d opciones=%d destino=%d"),
				State.ClockSeconds, State.TurnCount, Seat, Roll, Options.Num(), Destination));

			++State.TurnCount;
			++Result.Turns;
			State.ActiveSeat = (State.ActiveSeat + 1) % State.Seats.Num();
			NextTurn += Params.TurnPeriodSeconds;
		}
	}

	Result.Log.Add(FString::Printf(TEXT("fin turnos=%d evaluaciones=%d intervenciones=%d"), Result.Turns, Result.Evaluations, Result.Interventions));
	Result.FinalState = State;
	Result.WallSeconds = FPlatformTime::Seconds() - WallStart;
	return Result;
}
