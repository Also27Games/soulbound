#include "GrimoireBook.h"

// S-044: stubs para ver los tests en rojo; la implementación llega después.

namespace Soulbound::Presentation
{
	const TArray<EGrimoireTab>& GrimoireTabs()
	{
		static const TArray<EGrimoireTab> Tabs;
		return Tabs;
	}

	FString GrimoireTabTitle(EGrimoireTab Tab)
	{
		return FString();
	}

	void FGrimoireTabState::Open()
	{
	}

	void FGrimoireTabState::Next()
	{
	}

	void FGrimoireTabState::Previous()
	{
	}

	void FGrimoireTabState::Select(EGrimoireTab Tab)
	{
	}

	FGrimoireYouView GrimoireYouView(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors)
	{
		return FGrimoireYouView();
	}

	TArray<FLegendRow> GrimoireTableRows(const FMatchState& State, int32 HumanSeat, const TArray<bool>& ForgottenShown, const TArray<FLinearColor>& SeatColors,
		const TArray<FLinearColor>& DomainColors)
	{
		return {};
	}

	FString GrimoireHowToPlayText(const FHowToPlayLoadResult& Loaded, const FGrimoireKeyNames& Keys)
	{
		return FString();
	}

	bool FGrimoireCloseWatch::Observe(const TOptional<FDecisionView>& Panel, bool bRingOpen)
	{
		return false;
	}

	bool CanOpenGrimoire(bool bRingOpen)
	{
		return true;
	}

	FString GrimoireButtonLabel(const FString& KeyName)
	{
		return FString();
	}

	bool IsGrimoireButtonShown(bool bPrologue, bool bMatchOver, bool bWardenLineOnScreen)
	{
		return true;
	}
}
