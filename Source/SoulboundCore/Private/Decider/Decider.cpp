#include "Decider/Decider.h"

#include "Match/EffectCatalog.h"
#include "Match/MatchState.h"
#include "Match/VictoryRules.h"

namespace
{
	/**
	 * Pasos del grafo desde cada casilla hasta la casilla objetivo más cercana, por búsqueda en anchura desde todos los
	 * objetivos a la vez. Sin Sellos ni Altar: el dummy de carrera es un doble de prueba, no un bot.
	 * MAX_int32 en las casillas desde las que no se llega a ninguno.
	 */
	TArray<int32> StepsToNearest(const FBoardState& Board, const TArray<int32>& Targets)
	{
		TArray<int32> Steps;
		Steps.Init(MAX_int32, Board.Tiles.Num());
		TArray<int32> Queue;
		for (const int32 Target : Targets)
		{
			if (Board.IsValidTile(Target))
			{
				Steps[Target] = 0;
				Queue.Add(Target);
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

	/** Nichos Libres: lo que busca un Eco durante la carrera. */
	TArray<int32> FreeNiches(const FBoardState& Board)
	{
		TArray<int32> Niches;
		for (const FDomainState& Domain : Board.Domains)
		{
			if (Domain.NicheState == ENicheState::Free)
			{
				Niches.Add(Board.FindNiche(Domain.DomainId));
			}
		}
		return Niches;
	}

	/**
	 * Fuentes con reserva donde el asiento puede obtener una runa (R-RUN-2): un Elegido, cualquiera; un Eco, solo las de
	 * los Dominios Olvidados (R-ECO-3).
	 */
	TArray<int32> SourcesWhereSeatCanObtain(const FMatchState& State, int32 Seat)
	{
		const bool bElect = State.Seats[Seat].Role == ESeatRole::Elect;
		TArray<int32> Sources;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			const bool bForgotten = State.Board.Domains.IsValidIndex(Tile.DomainId) && State.Board.Domains[Tile.DomainId].State == EDomainState::Forgotten;
			if (Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0 && (bElect || bForgotten))
			{
				Sources.Add(Tile.TileId);
			}
		}
		return Sources;
	}

	/**
	 * De las Options, las que dejan al asiento a menos pasos del objetivo más cercano; los empates, todas. Vacío si
	 * ninguna opción lleva a un objetivo.
	 */
	TArray<int32> ClosestTo(const FBoardState& Board, const TArray<int32>& Targets, const TArray<int32>& Options)
	{
		const TArray<int32> Steps = StepsToNearest(Board, Targets);
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
		return Closest;
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
	if (!State.Seats.IsValidIndex(Seat))
	{
		return FDummyDecider().ChooseDestination(State, Seat, Options, Rng);
	}
	// Durante la carrera, un Eco busca el Nicho Libre más cercano; acabada, cualquiera busca la Fuente con reserva más
	// cercana donde pueda obtener (S-008). Sin objetivo al alcance de ninguna opción, al azar como el dummy.
	TArray<int32> Targets;
	if (State.MatchPhase == EMatchPhase::BondRace && State.Seats[Seat].Role == ESeatRole::LostEcho)
	{
		Targets = FreeNiches(State.Board);
	}
	else if (State.MatchPhase == EMatchPhase::RuneHunt && Soulbound::Rules::RuneTypeCount(State.Seats[Seat]) == Soulbound::Board::DomainCount)
	{
		// S-018: con los seis tipos, al Altar, para que en la simulación haya aspirantes y victorias.
		Targets = {State.Board.FindAltar()};
	}
	else if (State.MatchPhase == EMatchPhase::RuneHunt && State.Board.Phase == EBoardPhase::Awakened)
	{
		Targets = SourcesWhereSeatCanObtain(State, Seat);
	}
	const TArray<int32> Closest = ClosestTo(State.Board, Targets, Options);
	return FDummyDecider().ChooseDestination(State, Seat, Closest.IsEmpty() ? Options : Closest, Rng);
}

bool FDummyDecider::PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng)
{
	// S-018: el desafío final con su propia probabilidad; un solo valor del generador en los dos casos.
	const double Chance = Trial == ESkillTrial::FinalChallenge ? Soulbound::Decider::DummyFinalChallengeSuccessChance : Soulbound::Decider::DummySkillSuccessChance;
	return Rng.FRand() < Chance;
}

bool FRaceDummyDecider::PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng)
{
	return FDummyDecider().PassSkillMinigame(State, Seat, Trial, Difficulty, Rng);
}

int32 FDummyDecider::ChooseOption(const FMatchState& State, int32 Seat, const FPendingChoice& Choice, FRandomStream& Rng)
{
	// Al azar entre las opciones y "no" (INDEX_NONE) si la elección lo admite, todas con la misma probabilidad. Las opciones
	// se cuentan por posición (S-010): así la acción contra un jugador, que se contesta por índice, entra igual.
	// S-017 (R-DIS-2, decisión del autor del 23/10/2026): el rival sugerido por un disparador, si está entre las opciones, lo sigue.
	const int32 Suggested = State.CurrentEvent.SuggestedRival;
	if (Choice.Kind == EChoiceKind::ChosenRival && Suggested != INDEX_NONE && Choice.Options.Contains(Suggested))
	{
		return Suggested;
	}
	const int32 Count = Choice.AnswerCount() + (Choice.CanDecline() ? 1 : 0);
	if (Count == 0)
	{
		return INDEX_NONE;
	}
	return Choice.AnswerAt(Rng.RandRange(0, Count - 1));
}

int32 FRaceDummyDecider::ChooseOption(const FMatchState& State, int32 Seat, const FPendingChoice& Choice, FRandomStream& Rng)
{
	return FDummyDecider().ChooseOption(State, Seat, Choice, Rng);
}

double FDummyDecider::DuelPerformance(const FMatchState& State, int32 Seat, FRandomStream& Rng)
{
	// Doble de prueba del minijuego de duelo (S-012): al azar con el generador que le pasan, de 0 a 1.
	return Rng.FRand();
}

double FRaceDummyDecider::DuelPerformance(const FMatchState& State, int32 Seat, FRandomStream& Rng)
{
	return FDummyDecider().DuelPerformance(State, Seat, Rng);
}

bool FDummyDecider::RefusesOracleSentence(const FMatchState& State, int32 Seat, FRandomStream& Rng)
{
	// R-ORA-5, decisión del autor del 23/10/2026: el dummy de Tifón nunca rechaza; consulta en el acto.
	return false;
}

bool FRaceDummyDecider::RefusesOracleSentence(const FMatchState& State, int32 Seat, FRandomStream& Rng)
{
	return FDummyDecider().RefusesOracleSentence(State, Seat, Rng);
}

int32 FDummyDecider::ChooseVote(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng)
{
	if (Options.IsEmpty())
	{
		return INDEX_NONE;
	}
	// R-VOT-5 (decisión del autor del 22/10/2026): en un castigo (todo lo que no es regalo), al que más tipos de runa distintos
	// tiene; en un regalo, al que menos; con empate, al azar con su sorteo.
	const FCurrentWardenEvent& Event = State.CurrentEvent;
	const FWardenEventDef* Def = State.WardenEvents.Events.IsValidIndex(Event.EventIndex) ? &State.WardenEvents.Events[Event.EventIndex] : nullptr;
	const TArray<FEffectApplication>* Applications = Def && Def->Options.IsValidIndex(Event.Option) ? &Def->Options[Event.Option].Applications : nullptr;
	const FEffectApplication* Application = Applications && Applications->IsValidIndex(Event.NextApplication - 1) ? &(*Applications)[Event.NextApplication - 1] : nullptr;
	const bool bGift = Application && Soulbound::Rules::IsGift(Application->Effect);
	auto Types = [&State](int32 Voted) { return State.Seats[Voted].Runes.FilterByPredicate([](int32 Count) { return Count > 0; }).Num(); };
	TArray<int32> Best;
	int32 BestTypes = 0;
	for (const int32 Voted : Options)
	{
		const int32 VotedTypes = Types(Voted);
		if (Best.IsEmpty() || (bGift ? VotedTypes < BestTypes : VotedTypes > BestTypes))
		{
			Best = {Voted};
			BestTypes = VotedTypes;
		}
		else if (VotedTypes == BestTypes)
		{
			Best.Add(Voted);
		}
	}
	return Best[Rng.RandRange(0, Best.Num() - 1)];
}

int32 FRaceDummyDecider::ChooseVote(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng)
{
	return FDummyDecider().ChooseVote(State, Seat, Options, Rng);
}
