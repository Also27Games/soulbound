#include "Match/VoidRules.h"

// S-013: firmas sin implementar, para ver los tests en rojo.
namespace Soulbound::Rules
{
	const TArray<ESkillDifficulty>& VoidEscapeLadderFor(const FMatchState& State, int32 Seat)
	{
		return State.Params.VoidEscapeLadder;
	}

	bool LeaveVoid(FMatchState& State, int32 Seat, EVoidExit Reason, double NowSeconds)
	{
		return false;
	}

	int32 DeflectionRuneType(const FMatchState& State, int32 Seat)
	{
		return INDEX_NONE;
	}

	bool UseDeflection(FMatchState& State, int32 Seat, double NowSeconds)
	{
		return false;
	}

	bool ApplyVoidEscapeResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		return false;
	}
}
