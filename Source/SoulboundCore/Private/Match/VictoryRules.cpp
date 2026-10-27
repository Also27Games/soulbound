#include "Match/VictoryRules.h"

namespace Soulbound::Rules
{
	int32 RuneTypeCount(const FSeatState& SeatState)
	{
		return 0;
	}

	bool IsAspirant(const FMatchState& State, int32 Seat)
	{
		return false;
	}

	bool SyncAspirants(FMatchState& State, double NowSeconds)
	{
		return false;
	}

	bool IsMatchEndDue(const FMatchState& State, double NowSeconds)
	{
		return false;
	}

	bool DeclareWardenVictory(FMatchState& State, EMatchEndReason Reason, double NowSeconds)
	{
		return false;
	}

	bool ApplyFinalChallengeResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		return bPassed;
	}
}
