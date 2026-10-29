#include "WardenLineQueue.h"

#include "Match/MatchState.h"

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
		}
		return Pending[0];
	}

	double OpeningSeconds(const FMatchState& State, double LineSeconds)
	{
		return -1.0;
	}
}
