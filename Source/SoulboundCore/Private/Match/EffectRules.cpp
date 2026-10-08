#include "Match/EffectRules.h"

// S-013: firmas sin implementar, para ver los tests en rojo.
namespace Soulbound::Rules
{
	bool IsSeatInPlay(const FMatchState& State, int32 Seat)
	{
		return false;
	}

	EEffectResult CanBanish(const FMatchState& State, int32 Seat, bool bFury)
	{
		return EEffectResult::InvalidSeat;
	}

	EEffectResult Banish(FMatchState& State, int32 Seat, bool bFury, double NowSeconds)
	{
		return EEffectResult::InvalidSeat;
	}

	EEffectResult CanReleaseVoid(const FMatchState& State)
	{
		return EEffectResult::InvalidSeat;
	}

	EEffectResult ReleaseVoid(FMatchState& State, double NowSeconds)
	{
		return EEffectResult::InvalidSeat;
	}

	EEffectResult CanSendHome(const FMatchState& State, int32 Seat)
	{
		return EEffectResult::InvalidSeat;
	}

	EEffectResult SendHome(FMatchState& State, int32 Seat, double NowSeconds)
	{
		return EEffectResult::InvalidSeat;
	}
}
