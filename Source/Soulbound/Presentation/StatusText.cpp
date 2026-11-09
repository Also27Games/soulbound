#include "StatusText.h"

#include "Match/MatchState.h"

// TODO(R-HUD-5): esqueleto para ver los tests en rojo.
namespace Soulbound::Presentation
{
	TArray<FPanelLine> StatusLines(const FMatchState& State, const FStatusContext& Context) { return {}; }
	TArray<FPanelLine> DebugLines(const FMatchState& State, const FStatusContext& Context) { return {}; }
	int32 ScaledPanelFontSize(float ViewportHeight, int32 FontSizeAt1080, int32 MinFontSize) { return 0; }
	float ScaledPanelWidth(float ViewportHeight, float WidthAt1080) { return 0.f; }
	FGrimoireTextsLoadResult ParseGrimoireTexts(const FString& Json) { return {}; }
	FGrimoireTextsLoadResult LoadGrimoireTexts(const FString& FilePath) { return {}; }
	FString AffinityName(const FGrimoireTexts& Texts, EAffinity Affinity) { return FString(); }
	FString AffinityText(const FGrimoireTexts& Texts, EAffinity Affinity) { return FString(); }
	FString PowerText(const FGrimoireTexts& Texts, EPower Power) { return FString(); }
	TArray<FGrimoireRow> GrimoireRows(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors) { return {}; }
	TArray<FString> GrimoirePowerLines(const FGrimoireTexts& Texts) { return {}; }
}
