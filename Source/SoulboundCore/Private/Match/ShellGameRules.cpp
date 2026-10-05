#include "Match/ShellGameRules.h"

#include "Match/MatchEvents.h"

using Soulbound::Rules::LogEvent;

namespace Soulbound::Rules
{
	int32 ShellGameSeed(int32 SeedBase, int32 GameIndex)
	{
		return 0;
	}

	TArray<FIntPoint> ShellGameSwaps(int32 Seed, int32 OrbCount, int32 SwapCount)
	{
		return {};
	}

	TArray<int32> ShellGameFinalPositions(int32 Seed, int32 OrbCount, int32 SwapCount)
	{
		return {};
	}

	int32 ShellRuneAtPosition(const FMatchState& State, int32 Position)
	{
		return INDEX_NONE;
	}

	EStealStart BeginSteal(FMatchState& State, int32 Thief, int32 Victim, double NowSeconds)
	{
		return EStealStart::NoRunes;
	}

	TArray<int32> SafeRuneOptions(const FMatchState& State)
	{
		return {};
	}

	void SetAsideSafeRune(FMatchState& State, int32 RuneType, double NowSeconds)
	{
	}

	void StartShellReveal(FMatchState& State, double NowSeconds)
	{
	}

	void StartShellShuffle(FMatchState& State, double NowSeconds)
	{
	}

	void StartShellPick(FMatchState& State)
	{
	}

	bool ResolveShellPick(FMatchState& State, int32 Position, double NowSeconds)
	{
		return false;
	}

	void ExpireShellPick(FMatchState& State, double NowSeconds)
	{
	}
}
