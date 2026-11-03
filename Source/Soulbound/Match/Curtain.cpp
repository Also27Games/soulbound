#include "Curtain.h"

void FCurtain::Lower(double WorldNow, double InTimeoutSeconds)
{
	bDown = true;
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
	return true;
}

bool FCurtain::TimedOut(double WorldNow) const
{
	return bDown && (TimeoutSeconds <= 0.0 || WorldNow - LoweredWorldSeconds >= TimeoutSeconds);
}
