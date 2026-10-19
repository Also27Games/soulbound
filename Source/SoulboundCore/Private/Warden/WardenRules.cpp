#include "Warden/WardenRules.h"

#include "Match/MatchState.h"

// TODO(R-WAR-1): firmas vacías del ciclo rojo de S-016.

namespace Soulbound::Rules
{
	FRandomStream WardenDraw(FMatchState& State)
	{
		return FRandomStream(State.Warden.Seed);
	}

	bool IsWardenEvaluationDue(const FMatchState& State, double NowSeconds)
	{
		return false;
	}

	FWardenDecision EvaluateWarden(FMatchState& State, double NowSeconds, FRandomStream& MatchRandom)
	{
		return FWardenDecision();
	}

	void ApplyWardenDecision(FMatchState& State, const FWardenDecision& Decision, double NowSeconds, FRandomStream& MatchRandom)
	{
	}

	EWardenEventStart LaunchWardenEvent(FMatchState& State, int32 EventIndex, bool bInterruptedTurn, double NowSeconds)
	{
		return EWardenEventStart::UnknownEvent;
	}
}
