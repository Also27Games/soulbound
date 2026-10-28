#include "Warden/WardenRemarks.h"

#include "Match/MatchEvents.h"
#include "Match/MatchState.h"
#include "Match/VictoryRules.h"
#include "Warden/WardenTriggers.h"
#include "Warden/WardenVoice.h"

namespace
{
	/** Los asientos en el orden de turno; sin orden (una partida a medias en un test), por número. */
	TArray<int32> SeatsInOrder(const FMatchState& State)
	{
		if (!State.TurnOrder.IsEmpty())
		{
			return State.TurnOrder;
		}
		TArray<int32> Seats;
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			Seats.Add(Seat);
		}
		return Seats;
	}

	int32 RuneTotal(const FSeatState& Seat)
	{
		int32 Total = 0;
		for (const int32 Count : Seat.Runes)
		{
			Total += Count;
		}
		return Total;
	}
}

namespace Soulbound::Rules
{
	TArray<FRemarkCandidate> ApplicableRemarks(const FMatchState& State, double NowSeconds)
	{
		TArray<FRemarkCandidate> Candidates;
		if (!State.WardenParams.HasWarden() || State.Seats.IsEmpty())
		{
			return Candidates;
		}
		const FWardenParams& Params = State.WardenParams;
		const TArray<int32> Order = SeatsInOrder(State);

		// MockLast (siempre): el de menos runas, con los duplicados; si empatan, el último del orden de turno de los empatados.
		int32 Fewest = MAX_int32;
		for (const int32 Seat : Order)
		{
			Fewest = FMath::Min(Fewest, RuneTotal(State.Seats[Seat]));
		}
		int32 Last = INDEX_NONE;
		for (const int32 Seat : Order)
		{
			Last = RuneTotal(State.Seats[Seat]) == Fewest ? Seat : Last;
		}
		Candidates.Add({EWardenRemarkKind::MockLast, Last});

		// FiveRunes: cinco tipos distintos (seis es un aspirante); el primero del orden si son varios.
		for (const int32 Seat : Order)
		{
			if (RuneTypeCount(State.Seats[Seat]) == Soulbound::Board::DomainCount - 1)
			{
				Candidates.Add({EWardenRemarkKind::FiveRunes, Seat});
				break;
			}
		}

		// Hurry: el activo, esperando su intención (el destino o una elección suya), más allá de la fracción de su reloj de turno.
		const double Clock = State.Params.TurnClockSeconds;
		if (State.TurnPhase != ETurnPhase::BetweenTurns && State.Seats.IsValidIndex(State.ActiveSeat) && Clock > 0.0)
		{
			const bool bWaiting = State.TurnPhase == ETurnPhase::RollAndChoose || (State.PendingChoice.IsOpen() && State.PendingChoice.Seat == State.ActiveSeat);
			const double Elapsed = NowSeconds - (State.TurnDeadlineSeconds - Clock);
			if (bWaiting && Elapsed > Params.HurryFraction * Clock)
			{
				Candidates.Add({EWardenRemarkKind::Hurry, State.ActiveSeat});
			}
		}

		// Hoarder: quien causa el disparador Acaparador; el primero del orden si son varios.
		const TArray<FActiveTrigger> Triggers = ActiveTriggers(State, NowSeconds);
		for (const int32 Seat : Order)
		{
			if (Triggers.ContainsByPredicate([Seat](const FActiveTrigger& Trigger) { return Trigger.Trigger == EWardenTrigger::Hoarder && Trigger.Seat == Seat; }))
			{
				Candidates.Add({EWardenRemarkKind::Hoarder, Seat});
				break;
			}
		}

		// CutLaugh: el cortado por el TurnInterrupted más reciente, si cae en la ventana. El registro va en orden de reloj: al pasar la ventana se para.
		for (int32 Index = State.EventLog.Num() - 1; Index >= 0; --Index)
		{
			const FMatchEvent& Event = State.EventLog[Index];
			if (Event.ClockSeconds < NowSeconds - Params.CutLaughWindowSeconds)
			{
				break;
			}
			if (Event.Type == EMatchEventType::TurnInterrupted && State.Seats.IsValidIndex(Event.Seat))
			{
				Candidates.Add({EWardenRemarkKind::CutLaugh, Event.Seat});
				break;
			}
		}
		return Candidates;
	}

	void SpeakRemark(FMatchState& State, EWardenRemarkKind Kind, int32 Seat, double NowSeconds)
	{
		LogEvent(State, EMatchEventType::WardenRemark, NowSeconds, Seat, static_cast<int32>(Kind));
		SpeakWardenLine(State, WardenRemarkKey(Kind), NowSeconds, Seat);
	}

	FRemarkCandidate SpeakEvaluatedRemark(FMatchState& State, double NowSeconds)
	{
		const TArray<FRemarkCandidate> Candidates = ApplicableRemarks(State, NowSeconds);
		if (Candidates.IsEmpty())
		{
			return FRemarkCandidate();
		}
		// El mismo peso para cada tipo, con el sorteo de la voz: no gasta el del Warden ni el de los dados (R-WAR-9).
		FRandomStream Draw = VoiceDraw(State);
		const FRemarkCandidate Chosen = Candidates[Draw.RandRange(0, Candidates.Num() - 1)];
		SpeakRemark(State, Chosen.Kind, Chosen.Seat, NowSeconds);
		return Chosen;
	}
}
