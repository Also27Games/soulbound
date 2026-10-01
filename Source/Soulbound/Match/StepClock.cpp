#include "StepClock.h"

double FStepClock::MatchSeconds(double WorldNow) const
{
	return bHeld ? FrozenMatchSeconds : WorldNow - OriginWorldSeconds;
}

bool FStepClock::ShouldHold(bool bStepMode, ETurnPhase TurnPhase, EMatchPhase MatchPhase) const
{
	return bStepMode && !bHeld && !bStepGranted && TurnPhase == ETurnPhase::BetweenTurns && MatchPhase != EMatchPhase::MatchOver;
}

void FStepClock::Hold(double WorldNow)
{
	if (bHeld)
	{
		return;
	}
	FrozenMatchSeconds = WorldNow - OriginWorldSeconds;
	bHeld = true;
}

bool FStepClock::Step(double WorldNow)
{
	if (!bHeld)
	{
		return false;
	}
	// Desplazar el origen: el reloj sigue desde el segundo congelado, la espera no cuenta.
	OriginWorldSeconds = WorldNow - FrozenMatchSeconds;
	bHeld = false;
	bStepGranted = true;
	return true;
}

void FStepClock::NoteTurnPhase(ETurnPhase TurnPhase)
{
	if (TurnPhase != ETurnPhase::BetweenTurns)
	{
		bStepGranted = false;
	}
}

void FStepClock::Disable(double WorldNow)
{
	if (bHeld)
	{
		OriginWorldSeconds = WorldNow - FrozenMatchSeconds;
		bHeld = false;
	}
	bStepGranted = false;
}
