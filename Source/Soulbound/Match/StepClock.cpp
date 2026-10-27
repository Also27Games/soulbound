#include "StepClock.h"

double FStepClock::MatchSeconds(double WorldNow) const
{
	return bHeld ? FrozenMatchSeconds : WorldNow - OriginWorldSeconds;
}

bool FStepClock::ShouldHold(bool bStepMode, ETurnPhase TurnPhase, EMatchPhase MatchPhase, bool bWardenEventInProgress) const
{
	return bStepMode && !bHeld && !bStepGranted && TurnPhase == ETurnPhase::BetweenTurns && MatchPhase != EMatchPhase::MatchOver && !bWardenEventInProgress;
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

void FStepClock::Resume(double WorldNow)
{
	// Como al soltar un paso, pero sin concederlo: el turno siguiente volverá a esperar.
	if (bHeld)
	{
		OriginWorldSeconds = WorldNow - FrozenMatchSeconds;
		bHeld = false;
	}
	bStepGranted = false;
}

void FStepClock::Disable(double WorldNow)
{
	Resume(WorldNow);
}

void FStepClock::Skip(double DeltaSeconds)
{
	if (DeltaSeconds <= 0.0)
	{
		return;
	}
	OriginWorldSeconds -= DeltaSeconds;
	if (bHeld)
	{
		FrozenMatchSeconds += DeltaSeconds;
	}
}
