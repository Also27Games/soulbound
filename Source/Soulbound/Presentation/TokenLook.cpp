#include "TokenLook.h"

#include "Match/MatchState.h"

FLinearColor Soulbound::Presentation::TokenColor(ESeatRole, int32, const FLinearColor&, const TArray<FLinearColor>&)
{
	return FLinearColor::Black;
}

TArray<int32> Soulbound::Presentation::RingedSeats(const FMatchState&)
{
	return {-7};
}

float Soulbound::Presentation::RingPulse(double, float, float)
{
	return -1.f;
}
