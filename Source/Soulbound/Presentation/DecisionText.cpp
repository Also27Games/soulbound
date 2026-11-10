#include "DecisionText.h"

// TODO(R-DEC-1): esqueleto para ver los tests en rojo; la implementación va en el siguiente commit.
namespace Soulbound::Presentation
{
	TOptional<FDecisionView> DecisionView(const FMatchState& State, int32 HumanSeat, bool bWardenSpeaking)
	{
		return {};
	}

	bool IsDecisionPanelShown(const TOptional<FDecisionView>& View, bool bRingOpen)
	{
		return false;
	}

	const FDecisionOption* FindDecisionOption(const FDecisionView& View, const FString& Key)
	{
		return nullptr;
	}

	double AdvanceDecisionClock(FDecisionClock& Clock, const TOptional<FDecisionView>& View)
	{
		return 0.0;
	}
}
