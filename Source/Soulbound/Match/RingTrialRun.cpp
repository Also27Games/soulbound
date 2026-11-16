#include "RingTrialRun.h"

void FRingTrialRun::Begin(const FRingSequence& InSequence, double InStartSeconds)
{
	Sequence = InSequence;
	StartSeconds = InStartSeconds;
	Resolved.Reset();
	bWaiting = false;
	StartCause = ERingStartCause::None;
	OpenSeconds = InStartSeconds;
	StartedAtSeconds = InStartSeconds;
	MaxWaitSeconds = 0.0;
	BreatherSeconds = 0.0;
}

void FRingTrialRun::Open(const FRingSequence& InSequence, double InOpenSeconds, double InMaxWaitSeconds, double InBreatherSeconds)
{
	// R-MIN-19: la secuencia ya viene sorteada (misma semilla, mismos pulsos); solo se aplaza su inicio hasta que el jugador esté listo.
	Sequence = InSequence;
	Resolved.Reset();
	OpenSeconds = InOpenSeconds;
	MaxWaitSeconds = FMath::Max(InMaxWaitSeconds, 0.0);
	BreatherSeconds = FMath::Max(InBreatherSeconds, 0.0);
	StartedAtSeconds = InOpenSeconds;
	StartSeconds = InOpenSeconds;
	bWaiting = true;
	StartCause = ERingStartCause::None;
}

bool FRingTrialRun::Start(double Now)
{
	if (!bWaiting)
	{
		return false;
	}
	// Si la cuenta ya venció, ganó ella: empieza sola, en su instante, aunque la pulsación llegue después.
	if (StartIfDue(Now))
	{
		return false;
	}
	bWaiting = false;
	StartCause = ERingStartCause::Button;
	StartedAtSeconds = FMath::Max(Now, OpenSeconds);
	StartSeconds = StartedAtSeconds + BreatherSeconds;
	return true;
}

bool FRingTrialRun::StartIfDue(double Now)
{
	if (!bWaiting || Now < StartDueSeconds())
	{
		return false;
	}
	// Fechada en el vencimiento, no en Now: un tick tardío no desplaza la prueba.
	bWaiting = false;
	StartCause = ERingStartCause::Countdown;
	StartedAtSeconds = StartDueSeconds();
	StartSeconds = StartedAtSeconds + BreatherSeconds;
	return true;
}

double FRingTrialRun::CountdownSeconds(double Now) const
{
	return bWaiting ? FMath::Max(StartDueSeconds() - Now, 0.0) : 0.0;
}

double FRingTrialRun::WaitedSeconds() const
{
	return bWaiting || StartCause == ERingStartCause::None ? 0.0 : StartedAtSeconds - OpenSeconds;
}

bool FRingTrialRun::Press(double Now)
{
	Advance(Now);
	const FRingPressQuery Query = QueryPress(Now);
	if (Query.Kind != ERingPressKind::Counts)
	{
		return false; // espera, anuncio, entre pulsos, pulso ya resuelto o prueba terminada (R-MIN-2, R-MIN-19)
	}
	Resolved.Add(Soulbound::Rules::PulsePrecision(StartSeconds + Sequence.CoincidenceAt(Query.Pulse), Now, Sequence.SweetZoneSeconds));
	return true;
}

FRingPressQuery FRingTrialRun::QueryPress(double Now) const
{
	FRingPressQuery Query;
	// R-MIN-19: mientras espera, ninguna pulsación cuenta ni penaliza.
	if (bWaiting)
	{
		Query.Kind = ERingPressKind::Waiting;
		return Query;
	}
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
		// Antes del primer pulso es el anuncio (desde S-042, el respiro tras empezar); antes de cualquier otro, el hueco entre dos.
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
	// R-MIN-19: mientras espera, ningún pulso corre ni vence. Empezar es de Start o StartIfDue, no de aquí.
	if (bWaiting)
	{
		return;
	}
	// En orden: un pulso no se cierra antes que el anterior. Uno que vence sin pulsación, 0.
	while (!IsFinished() && Now >= StartSeconds + Sequence.ExpiryAt(Resolved.Num()))
	{
		Resolved.Add(0.0);
	}
}

bool FRingTrialRun::IsFinished() const
{
	return !bWaiting && Resolved.Num() >= Sequence.Num();
}

int32 FRingTrialRun::CurrentPulse(double Now) const
{
	if (bWaiting || IsFinished())
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
	// QueryPress ya salta los pulsos vencidos y distingue espera, anuncio, hueco entre pulsos y fin; solo cuenta el pulso en curso.
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
	// R-MIN-19: en la espera el aro está quieto en su radio mayor.
	if (bWaiting)
	{
		return 0.0;
	}
	if (IsFinished() || !(Sequence.CloseSeconds > 0.0))
	{
		return 1.0;
	}
	const double Begins = StartSeconds + Sequence.PulseStarts[Resolved.Num()];
	return FMath::Clamp((Now - Begins) / Sequence.CloseSeconds, 0.0, 1.0);
}
