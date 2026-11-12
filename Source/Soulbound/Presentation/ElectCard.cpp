#include "ElectCard.h"

#include "Match/MatchState.h"

void Soulbound::Presentation::FElectCardTracker::Observe(const FMatchState& State, int32 HumanSeat, double Now)
{
	// Un registro más corto que lo leído es otra partida: se empieza de cero.
	if (Scanned > State.EventLog.Num())
	{
		Scanned = 0;
		bOpen = false;
		DomainId = INDEX_NONE;
	}
	for (; Scanned < State.EventLog.Num(); ++Scanned)
	{
		const FMatchEvent& Event = State.EventLog[Scanned];
		if (Event.Type == EMatchEventType::Bonded && HumanSeat != INDEX_NONE && Event.Seat == HumanSeat)
		{
			bOpen = true;
			OpenedAt = Now;
			DomainId = Event.Value;
		}
	}
}

void Soulbound::Presentation::FElectCardTracker::SkipExisting(const FMatchState& State)
{
	Scanned = State.EventLog.Num();
}

void Soulbound::Presentation::FElectCardTracker::Dismiss()
{
	bOpen = false;
	DomainId = INDEX_NONE;
}

void Soulbound::Presentation::FElectCardTracker::SetGrimoireOpen(bool bInOpen)
{
	bGrimoireOpen = bInOpen;
	// Abrir el Grimorio cierra la carta (R-VIS-13); cerrarlo no la devuelve.
	if (bInOpen)
	{
		Dismiss();
	}
}

bool Soulbound::Presentation::FElectCardTracker::IsVisible(double Now, double ElectCardSeconds) const
{
	return bOpen && !bGrimoireOpen && Now >= OpenedAt && Now < OpenedAt + ElectCardSeconds;
}
