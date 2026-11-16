#include "Match/SeatGoalRules.h"

#include "Board/BoardState.h"
#include "Match/MatchState.h"
#include "Match/RuneRules.h"
#include "Match/VictoryRules.h"

namespace Soulbound::Rules
{
	FSeatGoal SeatGoal(const FMatchState& State, int32 Seat)
	{
		FSeatGoal Goal;
		if (!State.Seats.IsValidIndex(Seat) || State.IsMatchOver())
		{
			return Goal;
		}
		const FSeatState& SeatState = State.Seats[Seat];
		const FBoardState& Board = State.Board;

		// R-VIC-5: en la carrera, quien no se ha vinculado busca un Nicho Libre; los reclamados y los cerrados de los Olvidados no cuentan.
		if (State.MatchPhase == EMatchPhase::BondRace && SeatState.Role != ESeatRole::Elect)
		{
			Goal.Kind = ESeatGoal::Niche;
			for (const FDomainState& Domain : Board.Domains)
			{
				const int32 Niche = Domain.NicheState == ENicheState::Free ? Board.FindNiche(Domain.DomainId) : INDEX_NONE;
				if (Niche != INDEX_NONE)
				{
					Goal.Tiles.Add(Niche);
				}
			}
			Goal.Tiles.Sort();
			return Goal;
		}

		// R-VIC-1, R-VIC-5: con los seis tipos, a ganar al Altar.
		if (RuneTypeCount(SeatState) >= Soulbound::Board::DomainCount)
		{
			Goal.Kind = ESeatGoal::Altar;
			const int32 Altar = Board.FindAltar();
			if (Altar != INDEX_NONE)
			{
				Goal.Tiles.Add(Altar);
			}
			return Goal;
		}

		// R-VIC-5: las Fuentes donde la llegada daría (o jugaría la prueba por) una runa que le falta. La condición es la de la Fuente, no una copia.
		Goal.Kind = ESeatGoal::Runes;
		for (int32 TileId = 0; TileId < Board.Tiles.Num(); ++TileId)
		{
			const ERuneLanding Landing = PreviewPowerSourceLanding(State, Seat, TileId);
			const bool bGivesRune = Landing == ERuneLanding::Obtained || Landing == ERuneLanding::TrialPending;
			if (bGivesRune && SeatState.RuneCountOf(Board.Tiles[TileId].DomainId) == 0)
			{
				Goal.Tiles.Add(TileId);
			}
		}
		return Goal;
	}
}
