#include "WardenLineQueue.h"

#include "Match/MatchState.h"
#include "Warden/WardenLines.h"

namespace Soulbound::Presentation
{
	void FWardenLineQueue::Push(const FString& Text, double GameClock)
	{
		// Una frase dicha más tarde que las que esperan las sustituye ("hasta la siguiente línea"); en el mismo instante, hace cola.
		if (!Pending.IsEmpty() && GameClock > LastGameClock)
		{
			Pending.Reset();
			ShownSince = -1.0;
		}
		LastGameClock = GameClock;
		Pending.Add(Text);
	}

	FString FWardenLineQueue::Current(double NowReal, double LineSeconds)
	{
		if (Pending.IsEmpty())
		{
			return FString();
		}
		if (ShownSince < 0.0)
		{
			ShownSince = NowReal;
			++Shown;
		}
		// Pasa a la siguiente cada vez que la actual cumple su tiempo; sin siguiente, la cola se vacía.
		while (NowReal - ShownSince >= LineSeconds)
		{
			if (Pending.Num() == 1)
			{
				Pending.Reset();
				ShownSince = -1.0;
				return FString();
			}
			Pending.RemoveAt(0);
			ShownSince += LineSeconds;
			++Shown;
		}
		return Pending[0];
	}

	double OpeningSeconds(const FMatchState& State, double LineSeconds)
	{
		// La apertura es lo que StartMatch dice en el instante cero (inicio, corrupciones y orden); nada más se dice en él.
		int32 Lines = 0;
		for (const FMatchEvent& Event : State.EventLog)
		{
			if (Event.Type == EMatchEventType::WardenSpoke && Event.ClockSeconds == 0.0)
			{
				++Lines;
			}
		}
		return Lines * LineSeconds;
	}

	namespace
	{
		/**
		 * Si la frase de clave Key detiene la mesa (S-023, decisión del autor del 29/10/2026). La lista es de lo que no detiene: una clave
		 * nueva detiene por defecto, y que no lo haga se decide aquí a propósito.
		 */
		bool LineHoldsTheTable(const FString& Key)
		{
			// Los comentarios son el Warden sin materializarse (R-COM-1); las instrucciones son de cada máquina y en red el anfitrión no
			// para la mesa por lo que ve un cliente.
			if (Key.StartsWith(TEXT("comentario:")) || Key.StartsWith(TEXT("instruccion:")))
			{
				return false;
			}
			// Los avisos del final no paran el reloj que anuncian; con la victoria, el fin por tiempo y nadie vinculado la partida ya terminó.
			return Key != Soulbound::Warden::LineLastMinute && Key != Soulbound::Warden::LineTenSeconds && Key != Soulbound::Warden::LineVictory
				&& Key != Soulbound::Warden::LineTimeOut && Key != Soulbound::Warden::LineNobodyBonded;
		}
	}

	double WardenWaitSeconds(const FMatchState& State, int32 FromIndex, double LineSeconds)
	{
		double Seconds = 0.0;
		for (int32 Index = FMath::Max(FromIndex, 0); Index < State.EventLog.Num(); ++Index)
		{
			const FMatchEvent& Event = State.EventLog[Index];
			// Las del instante cero son la apertura: las cuenta el prólogo (OpeningSeconds), no la espera.
			if (Event.Type != EMatchEventType::WardenSpoke || Event.ClockSeconds == 0.0 || !State.WardenLines.Entries.IsValidIndex(Event.Value))
			{
				continue;
			}
			if (LineHoldsTheTable(State.WardenLines.Entries[Event.Value].Id))
			{
				Seconds += LineSeconds;
			}
		}
		return Seconds;
	}
}
