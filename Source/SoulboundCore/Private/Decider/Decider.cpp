#include "Decider/Decider.h"

#include "Match/MatchState.h"

namespace
{
	/**
	 * Pasos del grafo desde cada casilla hasta el Nicho Libre más cercano, por búsqueda en anchura desde todos los
	 * Nichos Libres a la vez. Sin Sellos ni Altar: el dummy de carrera es un doble de prueba, no un bot.
	 * MAX_int32 en las casillas desde las que no se llega a ninguno.
	 */
	TArray<int32> StepsToNearestFreeNiche(const FBoardState& Board)
	{
		TArray<int32> Steps;
		Steps.Init(MAX_int32, Board.Tiles.Num());
		TArray<int32> Queue;
		for (const FDomainState& Domain : Board.Domains)
		{
			const int32 Niche = Domain.NicheState == ENicheState::Free ? Board.FindNiche(Domain.DomainId) : INDEX_NONE;
			if (Board.IsValidTile(Niche))
			{
				Steps[Niche] = 0;
				Queue.Add(Niche);
			}
		}
		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			for (const int32 Next : Board.Tiles[Queue[i]].Neighbours)
			{
				if (Board.IsValidTile(Next) && Steps[Next] == MAX_int32)
				{
					Steps[Next] = Steps[Queue[i]] + 1;
					Queue.Add(Next);
				}
			}
		}
		return Steps;
	}
}

int32 FDummyDecider::ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng)
{
	if (Options.Num() == 0)
	{
		return INDEX_NONE;
	}
	return Options[Rng.RandRange(0, Options.Num() - 1)];
}

int32 FRaceDummyDecider::ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng)
{
	const bool bRacing = State.MatchPhase == EMatchPhase::BondRace && State.Seats.IsValidIndex(Seat) && State.Seats[Seat].Role == ESeatRole::LostEcho;
	if (!bRacing)
	{
		return FDummyDecider().ChooseDestination(State, Seat, Options, Rng);
	}

	const TArray<int32> Steps = StepsToNearestFreeNiche(State.Board);
	TArray<int32> Closest;
	int32 Best = MAX_int32;
	for (const int32 Option : Options)
	{
		const int32 OptionSteps = Steps.IsValidIndex(Option) ? Steps[Option] : MAX_int32;
		if (OptionSteps < Best)
		{
			Best = OptionSteps;
			Closest.Reset();
		}
		if (OptionSteps == Best && OptionSteps != MAX_int32)
		{
			Closest.Add(Option);
		}
	}
	// Sin Nicho Libre al alcance de ninguna opción, al azar como el dummy.
	return FDummyDecider().ChooseDestination(State, Seat, Closest.IsEmpty() ? Options : Closest, Rng);
}

// TODO(R-RUN-2): esqueleto de S-008 para que los tests compilen y fallen; la implementación llega en el siguiente commit.
bool FDummyDecider::PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng)
{
	return false;
}

bool FRaceDummyDecider::PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng)
{
	return false;
}
