#include "Board/SealRules.h"

#include "Board/MovementRules.h"
#include "Match/MatchState.h"

namespace Soulbound::Rules
{
	namespace
	{
		void LogSealEvent(FMatchState& State, EMatchEventType Type, int32 TileId, double NowSeconds)
		{
			FMatchEvent& Event = State.EventLog.AddDefaulted_GetRef();
			Event.Type = Type;
			Event.ClockSeconds = NowSeconds;
			Event.Value = TileId;
		}

		/** Sello que cae por el tope si se sella una casilla más (R-TAB-22), o INDEX_NONE si cabe sin liberar ninguno. */
		int32 SealToDropFor(const FBoardState& Board)
		{
			return Board.SealedTiles.Num() >= Board.Params.MaxSealedTiles && !Board.SealedTiles.IsEmpty() ? Board.SealedTiles[0] : INDEX_NONE;
		}

		/**
		 * Si toda casilla no sellada tiene camino al Altar para un jugador sin Paso Libre (R-TAB-22). Búsqueda en
		 * anchura desde el Altar con IsPassable, el mismo criterio de transitabilidad que el movimiento (R-TAB-17).
		 */
		bool AllTilesReachAltar(const FBoardState& Board)
		{
			const int32 Altar = Board.FindAltar();
			if (!Board.IsValidTile(Altar))
			{
				return false;
			}
			const FMoverTraits WithoutFreePassage;
			TArray<bool> Reached;
			Reached.Init(false, Board.Tiles.Num());
			Reached[Altar] = true;
			TArray<int32> Queue = {Altar};
			for (int32 i = 0; i < Queue.Num(); ++i)
			{
				for (const int32 Next : Board.Tiles[Queue[i]].Neighbours)
				{
					if (Board.IsValidTile(Next) && !Reached[Next] && IsPassable(Board, Next, WithoutFreePassage))
					{
						Reached[Next] = true;
						Queue.Add(Next);
					}
				}
			}
			for (const FTileState& Tile : Board.Tiles)
			{
				if (!Tile.bSealed && !Reached[Tile.TileId])
				{
					return false;
				}
			}
			return true;
		}
	}

	ESealResult CanSealTile(const FMatchState& State, int32 TileId)
	{
		const FBoardState& Board = State.Board;
		if (!Board.IsValidTile(TileId))
		{
			return ESealResult::InvalidTile;
		}
		if (Board.Params.MaxSealedTiles <= 0)
		{
			return ESealResult::NoSealsAllowed;
		}
		const FTileState& Tile = Board.Tiles[TileId];
		if (Tile.bSealed)
		{
			return ESealResult::AlreadySealed;
		}
		switch (Tile.Type)
		{
		case ETileType::Altar: return ESealResult::Altar;
		case ETileType::Niche: return ESealResult::Niche;
		case ETileType::PowerSource: return ESealResult::PowerSource;
		default: break;
		}
		if (State.IsTileOccupied(TileId))
		{
			return ESealResult::Occupied;
		}

		// El tablero tal como quedaría: con el Sello nuevo y sin el que caería por el tope. Los tableros son pequeños
		// (decenas de casillas): copiar es más claro que simular la transitabilidad a un lado.
		FBoardState After = Board;
		const int32 Dropped = SealToDropFor(Board);
		if (Dropped != INDEX_NONE)
		{
			After.Tiles[Dropped].bSealed = false;
		}
		After.Tiles[TileId].bSealed = true;
		return AllTilesReachAltar(After) ? ESealResult::Accepted : ESealResult::CutsOffAltar;
	}

	ESealResult SealTile(FMatchState& State, int32 TileId, double NowSeconds)
	{
		const ESealResult Result = CanSealTile(State, TileId);
		if (Result != ESealResult::Accepted)
		{
			return Result;
		}
		const int32 Dropped = SealToDropFor(State.Board);
		if (Dropped != INDEX_NONE)
		{
			ReleaseSeal(State, Dropped, NowSeconds);
		}
		State.Board.Tiles[TileId].bSealed = true;
		State.Board.SealedTiles.Add(TileId);
		LogSealEvent(State, EMatchEventType::SealPlaced, TileId, NowSeconds);
		return ESealResult::Accepted;
	}

	bool ReleaseSeal(FMatchState& State, int32 TileId, double NowSeconds)
	{
		FBoardState& Board = State.Board;
		if (!Board.IsValidTile(TileId) || !Board.Tiles[TileId].bSealed)
		{
			return false;
		}
		Board.Tiles[TileId].bSealed = false;
		Board.SealedTiles.Remove(TileId);
		LogSealEvent(State, EMatchEventType::SealReleased, TileId, NowSeconds);
		return true;
	}
}
