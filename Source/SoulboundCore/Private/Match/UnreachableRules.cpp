#include "Match/UnreachableRules.h"

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/MatchState.h"

namespace Soulbound::Rules
{
	EUnreachableReason UnreachableReason(const FMatchState& State, int32 Seat, int32 TileId)
	{
		if (!State.Seats.IsValidIndex(Seat) || !State.Board.IsValidTile(TileId))
		{
			return EUnreachableReason::Roll;
		}
		const FMoverTraits Mover = MakeMoverTraits(State, Seat);
		// R-TAB-29, en este orden: el sello (R-TAB-17, R-POD-2), el confinamiento (R-TAB-20) y, si no, la tirada.
		if (State.Board.Tiles[TileId].bSealed && !Mover.bFreePassage)
		{
			return EUnreachableReason::Sealed;
		}
		if (IsOutsideConfinement(State.Board, TileId, Mover))
		{
			return EUnreachableReason::Confined;
		}
		return EUnreachableReason::Roll;
	}
}
