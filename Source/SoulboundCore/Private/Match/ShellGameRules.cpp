#include "Match/ShellGameRules.h"

#include "Match/EffectRules.h"
#include "Match/MatchEvents.h"
#include "Match/RuneRules.h"
#include "Warden/WardenRemarks.h"
#include "Math/RandomStream.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** Runas del Brazal del asiento, todos los tipos. */
	int32 TotalRunes(const FSeatState& Seat)
	{
		int32 Total = 0;
		for (const int32 Count : Seat.Runes)
		{
			Total += Count;
		}
		return Total;
	}

	/** El trilero termina: el estado vuelve a "sin trilero". El contador de trileros de la partida se queda. */
	void EndShellGame(FMatchState& State)
	{
		State.ShellGame = FShellGameState();
	}
}

namespace Soulbound::Rules
{
	int32 ShellGameSeed(int32 SeedBase, int32 GameIndex)
	{
		// Una combinación fija de los dos números: misma base y mismo trilero, misma semilla. Positiva, como las del sorteo.
		return static_cast<int32>(HashCombineFast(static_cast<uint32>(SeedBase), static_cast<uint32>(GameIndex)) & 0x7fffffffu);
	}

	TArray<FIntPoint> ShellGameSwaps(int32 Seed, int32 OrbCount, int32 SwapCount)
	{
		TArray<FIntPoint> Swaps;
		if (OrbCount < 2)
		{
			return Swaps;
		}
		// R-ROB-6: un generador propio con la semilla de la mezcla, nunca el sorteo de la partida. Cada intercambio son dos
		// posiciones distintas al azar: la segunda se saca entre las que quedan, así nunca se repite la primera.
		FRandomStream Mix(Seed);
		for (int32 Swap = 0; Swap < SwapCount; ++Swap)
		{
			const int32 First = Mix.RandRange(0, OrbCount - 1);
			int32 Second = Mix.RandRange(0, OrbCount - 2);
			Second += Second >= First ? 1 : 0;
			Swaps.Add(FIntPoint(FMath::Min(First, Second), FMath::Max(First, Second)));
		}
		return Swaps;
	}

	TArray<int32> ShellGameFinalPositions(int32 Seed, int32 OrbCount, int32 SwapCount)
	{
		// Qué esfera hay en cada posición, intercambio a intercambio; al final se invierte: dónde acaba cada esfera.
		TArray<int32> OrbAt;
		for (int32 Orb = 0; Orb < OrbCount; ++Orb)
		{
			OrbAt.Add(Orb);
		}
		for (const FIntPoint& Swap : ShellGameSwaps(Seed, OrbCount, SwapCount))
		{
			OrbAt.Swap(Swap.X, Swap.Y);
		}
		TArray<int32> Positions;
		Positions.SetNum(OrbCount);
		for (int32 Position = 0; Position < OrbCount; ++Position)
		{
			Positions[OrbAt[Position]] = Position;
		}
		return Positions;
	}

	int32 ShellRuneAtPosition(const FMatchState& State, int32 Position)
	{
		const TArray<int32>& Orbs = State.ShellGame.Orbs;
		const int32 Orb = ShellGameFinalPositions(State.ShellGame.Seed, Orbs.Num(), State.Params.ShellSwapCount).IndexOfByKey(Position);
		return Orbs.IsValidIndex(Orb) ? Orbs[Orb] : INDEX_NONE;
	}

	EStealStart BeginSteal(FMatchState& State, int32 Thief, int32 Victim, double NowSeconds)
	{
		const int32 Runes = State.Seats.IsValidIndex(Victim) ? TotalRunes(State.Seats[Victim]) : 0;
		// R-ROB-4: sin runas no hay trilero; el registro lo deja.
		if (Runes == 0)
		{
			LogEvent(State, EMatchEventType::StealNoRunes, NowSeconds, Thief, INDEX_NONE, Victim);
			return EStealStart::NoRunes;
		}
		EndShellGame(State);
		State.ShellGame.Thief = Thief;
		State.ShellGame.Victim = Victim;
		// R-ROB-5, R-ELE-2: Koschei aparta antes una runa; con una sola, el robo se anula.
		if (HasAffinity(State, Victim, EAffinity::SafeRune))
		{
			if (Runes == 1)
			{
				const int32 Only = State.Seats[Victim].Runes.IndexOfByPredicate([](int32 Count) { return Count > 0; });
				EndShellGame(State);
				LogEvent(State, EMatchEventType::StealCancelled, NowSeconds, Thief, Only, Victim);
				return EStealStart::Cancelled;
			}
			State.ShellGame.Stage = EShellGameStage::SafeRune;
			return EStealStart::SafeRunePending;
		}
		StartShellReveal(State, NowSeconds);
		return EStealStart::Started;
	}

	TArray<int32> SafeRuneOptions(const FMatchState& State)
	{
		TArray<int32> Types;
		const int32 Victim = State.ShellGame.Victim;
		if (State.Seats.IsValidIndex(Victim))
		{
			for (int32 RuneType = 0; RuneType < State.Seats[Victim].Runes.Num(); ++RuneType)
			{
				if (State.Seats[Victim].Runes[RuneType] > 0)
				{
					Types.Add(RuneType);
				}
			}
		}
		return Types;
	}

	void SetAsideSafeRune(FMatchState& State, int32 RuneType, double NowSeconds)
	{
		FShellGameState& Game = State.ShellGame;
		// R-ROB-5: la apartada no sale del Brazal; solo no entra en las esferas. Sin elegir a tiempo, ninguna.
		if (State.Seats.IsValidIndex(Game.Victim) && State.Seats[Game.Victim].RuneCountOf(RuneType) > 0)
		{
			Game.SetAsideRune = RuneType;
			LogEvent(State, EMatchEventType::SafeRuneSetAside, NowSeconds, Game.Victim, RuneType, Game.Thief);
		}
		StartShellReveal(State, NowSeconds);
	}

	void StartShellReveal(FMatchState& State, double NowSeconds)
	{
		FShellGameState& Game = State.ShellGame;
		// R-ROB-2: todas las runas de la víctima, duplicados incluidos, en orden de tipo; menos la apartada (R-ROB-5).
		Game.Orbs.Reset();
		if (State.Seats.IsValidIndex(Game.Victim))
		{
			const FSeatState& Victim = State.Seats[Game.Victim];
			for (int32 RuneType = 0; RuneType < Victim.Runes.Num(); ++RuneType)
			{
				const int32 Count = Victim.Runes[RuneType] - (RuneType == Game.SetAsideRune ? 1 : 0);
				for (int32 Copy = 0; Copy < Count; ++Copy)
				{
					Game.Orbs.Add(RuneType);
				}
			}
		}
		// R-ROB-6: la semilla sale del valor base de la partida y del número de trilero, no del sorteo de los dados.
		Game.Seed = ShellGameSeed(State.ShellSeedBase, State.ShellGameCount);
		++State.ShellGameCount;
		Game.Stage = EShellGameStage::Reveal;
		Game.StageDeadlineSeconds = NowSeconds + State.Params.ShellRevealSeconds;
		LogEvent(State, EMatchEventType::ShellGameStarted, NowSeconds, Game.Thief, Game.Orbs.Num(), Game.Victim);
	}

	void StartShellShuffle(FMatchState& State, double NowSeconds)
	{
		State.ShellGame.Stage = EShellGameStage::Shuffle;
		State.ShellGame.StageDeadlineSeconds = NowSeconds + State.Params.ShellShuffleSeconds;
	}

	void StartShellPick(FMatchState& State)
	{
		State.ShellGame.Stage = EShellGameStage::Pick;
		State.ShellGame.StageDeadlineSeconds = 0.0;
	}

	bool ResolveShellPick(FMatchState& State, int32 Position, double NowSeconds)
	{
		const int32 Thief = State.ShellGame.Thief;
		const int32 Victim = State.ShellGame.Victim;
		const int32 Rune = ShellRuneAtPosition(State, Position);
		// R-ROB-3: la runa de la esfera elegida pasa al ladrón; el resto nunca salió del Brazal de la víctima.
		const bool bMoved = Rune != INDEX_NONE && TransferRune(State, Victim, Thief, Rune);
		if (bMoved)
		{
			LogEvent(State, EMatchEventType::RuneStolen, NowSeconds, Thief, Rune, Victim);
			// R-EFE-8 (S-014): robarle a un Marcado cobra su marca.
			CollectMark(State, Thief, Victim, NowSeconds);
		}
		EndShellGame(State);
		return bMoved;
	}

	void ExpireShellPick(FMatchState& State, double NowSeconds)
	{
		// R-ROB-3: sin elegir a tiempo no roba nada; el registro lo deja. Sin castigo.
		LogEvent(State, EMatchEventType::ShellGameExpired, NowSeconds, State.ShellGame.Thief, INDEX_NONE, State.ShellGame.Victim);
		// R-COM-2 (S-021): no elegir a tiempo es de lento, y el Warden lo dice a quien elegía.
		SpeakRemark(State, EWardenRemarkKind::Slow, State.ShellGame.Thief, NowSeconds);
		EndShellGame(State);
	}
}
