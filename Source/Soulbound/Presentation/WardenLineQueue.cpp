#include "WardenLineQueue.h"

#include "Match/MatchState.h"
#include "Warden/WardenLines.h"

// S-041: cuerpos vacíos para ver los tests en rojo antes de implementar.
namespace Soulbound::Presentation
{
	void FWardenLineQueue::Push(const FWardenLineItem& Item, double NowReal)
	{
	}

	FWardenLineItem FWardenLineQueue::Current(double NowReal)
	{
		return FWardenLineItem();
	}

	void FWardenLineQueue::Advance(double NowReal)
	{
	}

	double WardenLineDuration(const FString& Text, const FString& CutNote, double MinSeconds, double SecondsPerChar)
	{
		return 0.0;
	}

	bool LineHoldsTheTable(const FString& Key)
	{
		return false;
	}

	TArray<FWardenLineItem> WardenLineItems(const FMatchState& State, int32 FromIndex, int32 HumanSeat, double MinSeconds, double SecondsPerChar)
	{
		return {};
	}

	double OpeningSeconds(const FMatchState& State, int32 HumanSeat, double MinSeconds, double SecondsPerChar)
	{
		return 0.0;
	}

	double WardenWaitSeconds(const FMatchState& State, int32 FromIndex, int32 HumanSeat, double MinSeconds, double SecondsPerChar)
	{
		return 0.0;
	}

	float CutDimOpacity(double NowReal, double CutStartReal, float MaxOpacity, double Seconds)
	{
		return 0.f;
	}
}
