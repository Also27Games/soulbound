#include "RingTrialRun.h"

void FRingTrialRun::Begin(const FRingSequence& InSequence, double InStartSeconds)
{
}

bool FRingTrialRun::Press(double Now)
{
	return false;
}

void FRingTrialRun::Advance(double Now)
{
}

bool FRingTrialRun::IsFinished() const
{
	return false;
}

int32 FRingTrialRun::CurrentPulse(double Now) const
{
	return INDEX_NONE;
}

double FRingTrialRun::CloseFraction(double Now) const
{
	return -1.0;
}
