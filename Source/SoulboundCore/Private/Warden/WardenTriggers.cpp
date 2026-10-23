#include "Warden/WardenTriggers.h"

#include "Match/MatchState.h"

namespace Soulbound::Rules
{
	TArray<FActiveTrigger> ActiveTriggers(const FMatchState& State, double NowSeconds)
	{
		return {};
	}

	double TriggeredEventWeight(const FMatchState& State, int32 EventIndex, EAct Act, int32 Director, const TArray<FActiveTrigger>& Triggers)
	{
		return State.WardenEvents.Events.IsValidIndex(EventIndex) ? State.WardenEvents.Events[EventIndex].WeightIn(Act) : 0.0;
	}

	double TriggeredInterveneProbability(const FMatchState& State, EAct Act, const TArray<FActiveTrigger>& Triggers)
	{
		return State.WardenParams.Acts.IsValidIndex(static_cast<int32>(Act)) ? State.WardenParams.Acts[static_cast<int32>(Act)].InterveneProbability : 0.0;
	}

	bool IsStrikeAgainstActive(const FMatchState& State, int32 EventIndex)
	{
		return false;
	}

	double RandomTargetWeight(const FMatchState& State, int32 Seat, double NowSeconds, const TArray<FActiveTrigger>& Triggers)
	{
		return 1.0;
	}

	TArray<double> SuggestedRivalWeights(const FMatchState& State, int32 EventIndex, const TArray<int32>& Rivals, const TArray<FActiveTrigger>& Triggers)
	{
		TArray<double> Weights;
		Weights.Init(0.0, Rivals.Num());
		return Weights;
	}

	void ApplyTriggersToEvent(FMatchState& State, const TArray<FActiveTrigger>& Triggers)
	{
	}
}
