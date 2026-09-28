#include "Board/SealRules.h"

#include "Match/MatchState.h"

namespace Soulbound::Rules
{
	ESealResult CanSealTile(const FMatchState& State, int32 TileId)
	{
		return ESealResult::InvalidTile;
	}

	ESealResult SealTile(FMatchState& State, int32 TileId, double NowSeconds)
	{
		return ESealResult::InvalidTile;
	}

	bool ReleaseSeal(FMatchState& State, int32 TileId, double NowSeconds)
	{
		return false;
	}
}
