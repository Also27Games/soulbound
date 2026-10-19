#include "Warden/WardenBrain.h"

#include "Match/MatchState.h"
#include "Math/RandomStream.h"

// TODO(R-WAR-1): firmas vacías del ciclo rojo de S-016.

FWardenDecision FWardenBrain::Evaluate(const FMatchState& State, double NowSeconds, FRandomStream& Random) const
{
	return FWardenDecision();
}

namespace Soulbound::Rules
{
	EAct CurrentAct(const FMatchState& State, double NowSeconds)
	{
		return EAct::ActOne;
	}

	bool IsInWardenMargin(const FMatchState& State, double NowSeconds)
	{
		return false;
	}

	bool IsWardenDeferred(const FMatchState& State)
	{
		return false;
	}

	TArray<int32> EventCandidates(const FMatchState& State, EWardenChannel Channel, double NowSeconds)
	{
		return {};
	}
}
