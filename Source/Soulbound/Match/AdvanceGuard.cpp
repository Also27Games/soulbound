#include "AdvanceGuard.h"

namespace Soulbound::Match
{
	EAfterAnswer AfterAnswer(EIntentResult Result)
	{
		return Result == EIntentResult::Accepted ? EAfterAnswer::Repeat : EAfterAnswer::Continue;
	}
}

bool FAdvanceGuard::Allow()
{
	if (Laps < MAX_int32)
	{
		++Laps;
	}
	return Laps <= Limit;
}

void FAdvanceGuard::Reset()
{
	Laps = 0;
}

bool FAdvanceTripNotice::NoteTrip(double WorldNow, int32& OutTrips)
{
	if (PendingTrips < MAX_int32)
	{
		++PendingTrips;
	}
	if (bNoticed && WorldNow - LastNoticeWorldSeconds < MinIntervalSeconds)
	{
		OutTrips = 0;
		return false;
	}
	OutTrips = PendingTrips;
	PendingTrips = 0;
	bNoticed = true;
	LastNoticeWorldSeconds = WorldNow;
	return true;
}
