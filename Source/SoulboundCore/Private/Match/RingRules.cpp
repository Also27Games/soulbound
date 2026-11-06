#include "Match/RingRules.h"

namespace Soulbound::Rules
{
	double PulsePrecision(double CoincidenceSeconds, TOptional<double> PressSeconds, double SweetZoneSeconds)
	{
		return 0.0;
	}

	TOptional<FRingTrialParams> RingParamsForTrial(const FRingParams& Ring, ESkillTrial Trial, ESkillDifficulty Difficulty)
	{
		return {};
	}

	FRingTrialParams RingParamsForDuel(const FRingParams& Ring)
	{
		return {};
	}

	FRingSequence MakeRingSequence(const FRingParams& Ring, const FRingTrialParams& Trial, int32 Seed)
	{
		return {};
	}

	bool RingTrialPassed(const FRingTrialParams& Trial, TConstArrayView<double> Precisions)
	{
		return false;
	}

	double RingDuelPerformance(TConstArrayView<double> Precisions)
	{
		return 0.0;
	}
}
