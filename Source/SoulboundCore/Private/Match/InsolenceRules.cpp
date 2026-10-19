#include "Match/InsolenceRules.h"

#include "Math/RandomStream.h"

// TODO(R-INS-2): firmas vacías del ciclo rojo de S-016 (B).

namespace Soulbound::Rules
{
	TArray<EEffect> InsolencePunishmentEffects(EEffectSeverity Severity)
	{
		return {};
	}

	EInsolencePunishment DrawInsolencePunishment(const FWardenParams& Params, int32 LevelBefore, FRandomStream& Random)
	{
		return EInsolencePunishment::Light;
	}

	void PunishInsolence(FMatchState& State, int32 Seat, const FPendingPunishment& Punishment, double NowSeconds)
	{
	}

	void UnleashFury(FMatchState& State, int32 Seat, double NowSeconds)
	{
	}

	void PunishOrDefer(FMatchState& State, int32 Seat, const FPendingPunishment& Punishment, double NowSeconds)
	{
	}

	void DischargePendingPunishments(FMatchState& State, double NowSeconds)
	{
	}

	void DischargeVoidPunishments(FMatchState& State, int32 Seat, double NowSeconds)
	{
	}
}
