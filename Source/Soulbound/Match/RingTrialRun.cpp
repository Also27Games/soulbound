#include "RingTrialRun.h"

void FRingTrialRun::Begin(const FRingSequence& InSequence, double InStartSeconds)
{
	Sequence = InSequence;
	StartSeconds = InStartSeconds;
	Resolved.Reset();
}

bool FRingTrialRun::Press(double Now)
{
	Advance(Now);
	const int32 Pulse = CurrentPulse(Now);
	if (Pulse == INDEX_NONE)
	{
		return false; // anuncio, entre pulsos, pulso ya resuelto o prueba terminada (R-MIN-2)
	}
	Resolved.Add(Soulbound::Rules::PulsePrecision(StartSeconds + Sequence.CoincidenceAt(Pulse), Now, Sequence.SweetZoneSeconds));
	return true;
}

void FRingTrialRun::Advance(double Now)
{
	// En orden: un pulso no se cierra antes que el anterior. Uno que vence sin pulsación, 0.
	while (!IsFinished() && Now >= StartSeconds + Sequence.ExpiryAt(Resolved.Num()))
	{
		Resolved.Add(0.0);
	}
}

bool FRingTrialRun::IsFinished() const
{
	return Resolved.Num() >= Sequence.Num();
}

int32 FRingTrialRun::CurrentPulse(double Now) const
{
	if (IsFinished())
	{
		return INDEX_NONE;
	}
	const int32 Pulse = Resolved.Num();
	const double Begins = StartSeconds + Sequence.PulseStarts[Pulse];
	const double Ends = StartSeconds + Sequence.ExpiryAt(Pulse);
	return Now >= Begins && Now <= Ends ? Pulse : INDEX_NONE;
}

double FRingTrialRun::CloseFraction(double Now) const
{
	if (IsFinished() || !(Sequence.CloseSeconds > 0.0))
	{
		return 1.0;
	}
	const double Begins = StartSeconds + Sequence.PulseStarts[Resolved.Num()];
	return FMath::Clamp((Now - Begins) / Sequence.CloseSeconds, 0.0, 1.0);
}
