#include "StepClock.h"

double FStepClock::MatchSeconds(double WorldNow) const
{
	return 0.0;
}

bool FStepClock::ShouldHold(bool bStepMode, ETurnPhase TurnPhase, EMatchPhase MatchPhase) const
{
	return false;
}

void FStepClock::Hold(double WorldNow)
{
}

bool FStepClock::Step(double WorldNow)
{
	return false;
}

void FStepClock::NoteTurnPhase(ETurnPhase TurnPhase)
{
}

void FStepClock::Disable(double WorldNow)
{
}
