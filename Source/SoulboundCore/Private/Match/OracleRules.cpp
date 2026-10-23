#include "Match/OracleRules.h"

#include "Math/RandomStream.h"

namespace Soulbound::Rules
{
	TArray<int32> OracleSentenceCandidates(const FMatchState& State, int32 Consultant, double NowSeconds)
	{
		return {};
	}

	TArray<TPair<int32, double>> OracleSentenceProbabilities(const FMatchState& State, int32 Consultant, double NowSeconds, const TArray<FActiveTrigger>& Triggers)
	{
		return {};
	}

	int32 DrawOracleSentence(const FMatchState& State, int32 Consultant, double NowSeconds, const TArray<FActiveTrigger>& Triggers, FRandomStream& Random)
	{
		return INDEX_NONE;
	}
}
