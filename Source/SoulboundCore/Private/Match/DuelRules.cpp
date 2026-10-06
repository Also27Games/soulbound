#include "Match/DuelRules.h"

// Esqueleto de S-012: firmas sin implementar, para ver los tests en rojo.

namespace Soulbound::Rules
{
	bool ChallengesFromDuelNode(const FMatchState& State, int32 Seat)
	{
		return false;
	}

	TArray<int32> DuelTargets(const FMatchState& State, int32 Seat)
	{
		return {};
	}

	int32 DuelWinner(const FMatchState& State, int32 Challenger, int32 Challenged, double ChallengerPerformance, double ChallengedPerformance)
	{
		return INDEX_NONE;
	}

	EDuelStart BeginDuel(FMatchState& State, int32 Challenger, int32 Challenged, double NowSeconds)
	{
		return EDuelStart::NoRunes;
	}

	bool RecordDuelPerformance(FMatchState& State, int32 Seat, double Performance)
	{
		return false;
	}

	FDuelResult ResolveDuel(FMatchState& State, double NowSeconds)
	{
		return FDuelResult();
	}
}
