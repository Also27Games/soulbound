#include "Match/BondRaceRules.h"

#include "Math/RandomStream.h"

namespace Soulbound::Rules
{
	bool ResolveNicheLanding(FMatchState& State, int32 Seat, double NowSeconds, FRandomStream& Random)
	{
		return false;
	}

	bool EndBondRace(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		return false;
	}

	int32 HomeTile(const FMatchState& State, int32 Seat)
	{
		return INDEX_NONE;
	}

	FMoverTraits MakeMoverTraits(const FMatchState& State, int32 Seat)
	{
		return FMoverTraits();
	}
}
