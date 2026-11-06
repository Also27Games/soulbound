#include "Match/RingRules.h"

#include "Math/RandomStream.h"

namespace
{
	/** Los parámetros de la dificultad Difficulty en el bloque del anillo; una desconocida, los de normal (la validación no la deja pasar). */
	const FRingDifficultyParams& ForDifficulty(const FRingParams& Ring, ESkillDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case ESkillDifficulty::Easy: return Ring.Easy;
		case ESkillDifficulty::Hard: return Ring.Hard;
		default: return Ring.Normal;
		}
	}

	/** Un uso con la dificultad Difficulty, Pulses pulsos y el criterio Criterion: cierre, zona y umbral de la dificultad. */
	FRingTrialParams MakeTrial(const FRingParams& Ring, ESkillDifficulty Difficulty, int32 Pulses, ERingCriterion Criterion)
	{
		const FRingDifficultyParams& Tier = ForDifficulty(Ring, Difficulty);
		FRingTrialParams Trial;
		Trial.Difficulty = Difficulty;
		Trial.CloseSeconds = Tier.CloseSeconds;
		Trial.SweetZone = Tier.SweetZone;
		Trial.Pulses = Pulses;
		Trial.PassThreshold = Tier.PassThreshold;
		Trial.Criterion = Criterion;
		return Trial;
	}

	/** Media de Values; sin valores, 0. */
	double Mean(TConstArrayView<double> Values)
	{
		if (Values.Num() == 0)
		{
			return 0.0;
		}
		double Sum = 0.0;
		for (const double Value : Values)
		{
			Sum += Value;
		}
		return Sum / Values.Num();
	}
}

namespace Soulbound::Rules
{
	double PulsePrecision(double CoincidenceSeconds, TOptional<double> PressSeconds, double SweetZoneSeconds)
	{
		const double HalfZone = SweetZoneSeconds * 0.5;
		if (!PressSeconds.IsSet() || !(HalfZone > 0.0))
		{
			return 0.0;
		}
		const double Distance = FMath::Abs(PressSeconds.GetValue() - CoincidenceSeconds);
		return FMath::Max(0.0, 1.0 - Distance / HalfZone);
	}

	TOptional<FRingTrialParams> RingParamsForTrial(const FRingParams& Ring, ESkillTrial Trial, ESkillDifficulty Difficulty)
	{
		switch (Trial)
		{
		case ESkillTrial::ForgottenSource:
			return MakeTrial(Ring, Difficulty, Ring.ForgottenSourcePulses, ERingCriterion::MeanAtLeastThreshold);
		case ESkillTrial::WardenTrial:
			return MakeTrial(Ring, Difficulty, Ring.WardenTrialPulses, ERingCriterion::MeanAtLeastThreshold);
		case ESkillTrial::VoidEscape:
			return MakeTrial(Ring, Difficulty, Ring.VoidEscapePulses, ERingCriterion::MeanAtLeastThreshold);
		case ESkillTrial::FinalChallenge:
		{
			// R-MIN-12: el cierre de su dificultad, su zona propia y sin fallos.
			FRingTrialParams Final = MakeTrial(Ring, Difficulty, Ring.FinalChallengePulses, ERingCriterion::NoMiss);
			Final.SweetZone = Ring.FinalChallengeSweetZone;
			return Final;
		}
		default:
			return {};
		}
	}

	FRingTrialParams RingParamsForDuel(const FRingParams& Ring)
	{
		return MakeTrial(Ring, Ring.DuelDifficulty, Ring.DuelPulses, ERingCriterion::Performance);
	}

	FRingSequence MakeRingSequence(const FRingParams& Ring, const FRingTrialParams& Trial, int32 Seed)
	{
		FRingSequence Sequence;
		Sequence.CloseSeconds = Trial.CloseSeconds;
		Sequence.SweetZoneSeconds = Trial.SweetZoneSeconds();
		FRandomStream Random(Seed);
		double Start = 0.0;
		for (int32 Pulse = 0; Pulse < Trial.Pulses; ++Pulse)
		{
			if (Pulse > 0)
			{
				// El hueco se cuenta desde que vence el pulso anterior (decisión del autor del 06/11/2026): así nunca se solapan.
				Start = Sequence.ExpiryAt(Pulse - 1) + Random.FRandRange(Ring.PulseGapMinSeconds, Ring.PulseGapMaxSeconds);
			}
			Sequence.PulseStarts.Add(Start);
		}
		return Sequence;
	}

	bool RingTrialPassed(const FRingTrialParams& Trial, TConstArrayView<double> Precisions)
	{
		if (Precisions.Num() == 0)
		{
			return false;
		}
		switch (Trial.Criterion)
		{
		case ERingCriterion::MeanAtLeastThreshold:
			return Mean(Precisions) >= Trial.PassThreshold - Soulbound::Ring::MeanTolerance;
		case ERingCriterion::NoMiss:
			for (const double Precision : Precisions)
			{
				if (!(Precision > 0.0))
				{
					return false;
				}
			}
			return true;
		default:
			return false;
		}
	}

	double RingDuelPerformance(TConstArrayView<double> Precisions)
	{
		return FMath::Clamp(Mean(Precisions), 0.0, 1.0);
	}
}
