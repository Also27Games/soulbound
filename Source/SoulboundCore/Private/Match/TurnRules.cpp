#include "Match/TurnRules.h"

#include "Math/RandomStream.h"

namespace Soulbound::Rules
{
	int32 RollDice(FRandomStream& Random, int32 DiceCount)
	{
		return 0;
	}

	bool AdvanceTurn(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		return false;
	}

	EIntentResult SubmitDestinationIntent(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds)
	{
		return EIntentResult::WrongPhase;
	}

	bool GrantExtraTurns(FMatchState& State, int32 Seat, int32 Count)
	{
		return false;
	}

	bool GrantLostTurns(FMatchState& State, int32 Seat, int32 Count)
	{
		return false;
	}

	bool RaiseInsolence(FMatchState& State, int32 Seat)
	{
		return false;
	}
}
