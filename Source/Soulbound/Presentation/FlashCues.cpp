#include "FlashCues.h"

#include "Match/MatchState.h"

TArray<Soulbound::Presentation::FFlashCue> Soulbound::Presentation::FlashCuesFromEvents(const FMatchState&, int32, float)
{
	return {};
}

FLinearColor Soulbound::Presentation::FlashColorOf(const FFlashCue&, const TArray<FLinearColor>&, const FLinearColor&, const FLinearColor&)
{
	return FLinearColor::Black;
}
