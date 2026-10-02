#include "Match/RuneRules.h"

// TODO(R-RUN-2): esqueleto de S-008 para que los tests compilen y fallen; la implementación llega en el siguiente commit.
namespace Soulbound::Rules
{
	ERuneLanding ResolvePowerSourceLanding(FMatchState& State, int32 Seat, double NowSeconds)
	{
		return ERuneLanding::None;
	}

	bool ApplySkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		return false;
	}

	bool ReturnRuneToSource(FMatchState& State, int32 Seat, int32 RuneType, int32 SourceTile, double NowSeconds)
	{
		return false;
	}

	bool IsPowerActive(const FMatchState& State, int32 Seat, EPower Power)
	{
		return false;
	}

	TArray<EPower> GetActivePowers(const FMatchState& State, int32 Seat)
	{
		return {};
	}
}
