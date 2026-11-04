#include "Curtain.h"

void FCurtain::Lower(double WorldNow, double InTimeoutSeconds, double InGraceSeconds)
{
	bDown = true;
	bRaised = false;
	GraceSeconds = InGraceSeconds;
	LoweredWorldSeconds = WorldNow;
	TimeoutSeconds = InTimeoutSeconds;
}

bool FCurtain::Raise(double WorldNow)
{
	if (!bDown)
	{
		return false;
	}
	bDown = false;
	bRaised = true;
	RaisedWorldSeconds = WorldNow;
	return true;
}

bool FCurtain::IsDown(double WorldNow) const
{
	return bDown || (bRaised && WorldNow < RaisedWorldSeconds + GraceSeconds);
}

bool FCurtain::TimedOut(double WorldNow) const
{
	return bDown && (TimeoutSeconds <= 0.0 || WorldNow - LoweredWorldSeconds >= TimeoutSeconds);
}
