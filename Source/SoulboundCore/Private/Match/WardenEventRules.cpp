#include "Match/WardenEventRules.h"

namespace Soulbound::Rules
{
	// TODO(R-COM-1): sin implementar (S-014, tests en rojo).
	TArray<int32> LeaderSeats(const FMatchState& State) { return {}; }

	int32 EventDirector(const FMatchState& State) { return INDEX_NONE; }

	EEffectResult CanApplyEffect(const FMatchState& State, EEffect Effect, int32 Seat, int32 Director) { return EEffectResult::InvalidSeat; }

	EEffectResult ApplyEffect(FMatchState& State, const FEffectApplication& Application, int32 Seat, int32 Director, int32 Pointer, int32 Rune,
		double NowSeconds, FRandomStream& Random)
	{
		return EEffectResult::InvalidSeat;
	}

	double DilemmaClockSeconds(const FMatchParams& Params, int32 OptionCount) { return 0.0; }

	EWardenEventStart CanApplyEvent(const FMatchState& State, const FString& EventId) { return EWardenEventStart::UnknownEvent; }

	EWardenEventStart ApplyEvent(FMatchState& State, const FString& EventId, double NowSeconds, FRandomStream& Random) { return EWardenEventStart::UnknownEvent; }

	bool AdvanceWardenEvent(FMatchState& State, double NowSeconds) { return false; }

	void AnswerWardenEventChoice(FMatchState& State, EChoiceKind Kind, int32 Option, double NowSeconds) {}
}
