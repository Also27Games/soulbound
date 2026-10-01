#include "Decider/Decider.h"

int32 FDummyDecider::ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng)
{
	if (Options.Num() == 0)
	{
		return INDEX_NONE;
	}
	return Options[Rng.RandRange(0, Options.Num() - 1)];
}

int32 FRaceDummyDecider::ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng)
{
	return INDEX_NONE;
}
