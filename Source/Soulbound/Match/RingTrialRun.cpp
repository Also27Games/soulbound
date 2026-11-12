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
	const FRingPressQuery Query = QueryPress(Now);
	if (Query.Kind != ERingPressKind::Counts)
	{
		return false; // anuncio, entre pulsos, pulso ya resuelto o prueba terminada (R-MIN-2)
	}
	Resolved.Add(Soulbound::Rules::PulsePrecision(StartSeconds + Sequence.CoincidenceAt(Query.Pulse), Now, Sequence.SweetZoneSeconds));
	return true;
}

FRingPressQuery FRingTrialRun::QueryPress(double Now) const
{
	FRingPressQuery Query;
	// Los pulsos vencidos sin pulsar se saltan como lo hace Advance, pero sin resolverlos: la consulta no muta.
	int32 Pulse = Resolved.Num();
	while (Pulse < Sequence.Num() && Now >= StartSeconds + Sequence.ExpiryAt(Pulse))
	{
		++Pulse;
	}
	if (Pulse >= Sequence.Num())
	{
		Query.Kind = ERingPressKind::Finished;
		return Query;
	}
	if (Now < StartSeconds + Sequence.PulseStarts[Pulse])
	{
		// Antes del primer pulso es el anuncio; antes de cualquier otro, el hueco entre dos.
		Query.Kind = Pulse == 0 ? ERingPressKind::Announcement : ERingPressKind::BetweenPulses;
		return Query;
	}
	Query.Kind = ERingPressKind::Counts;
	Query.Pulse = Pulse;
	Query.OffsetSeconds = Now - (StartSeconds + Sequence.CoincidenceAt(Pulse));
	return Query;
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

bool FRingTrialRun::IsInSweetZone(double Now) const
{
	// QueryPress ya salta los pulsos vencidos y distingue anuncio, hueco entre pulsos y fin; solo cuenta el pulso en curso.
	const FRingPressQuery Query = QueryPress(Now);
	if (Query.Kind != ERingPressKind::Counts)
	{
		return false;
	}
	// Media zona antes y después de la coincidencia; el final no cuenta, como en Advance (vence en cuanto Now lo alcanza).
	const double HalfZone = Sequence.SweetZoneSeconds * 0.5;
	return Query.OffsetSeconds >= -HalfZone && Query.OffsetSeconds < HalfZone;
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
