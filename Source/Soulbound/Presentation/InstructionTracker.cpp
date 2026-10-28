#include "InstructionTracker.h"

#include "Match/MatchState.h"

namespace Soulbound::Presentation
{
	TArray<FInstructionCue> FInstructionTracker::Observe(const FMatchState& State, const TArray<int32>& HumanSeats)
	{
		return TArray<FInstructionCue>();
	}

	bool TeachAndRecord(EHintsMode Mode, TArray<FString>& Seen, EInstructionMoment Moment)
	{
		return false;
	}

	bool ShowsRequestHints(EHintsMode Mode, const TArray<FString>& Seen)
	{
		return false;
	}

	bool ParseHintsMode(const FString& Text, EHintsMode& OutMode)
	{
		return false;
	}

	FString HintsModeText(EHintsMode Mode)
	{
		return FString();
	}
}
