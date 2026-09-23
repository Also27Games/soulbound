#include "Board/MovementRules.h"

#include "Board/BoardState.h"

namespace Soulbound::Rules
{
	namespace
	{
		/** Búsqueda en profundidad de caminos simples de longitud exacta. Los tableros son pequeños (decenas de casillas). */
		void CollectExactPaths(const FBoardState& Board, int32 Current, int32 StepsLeft, const FMoverTraits& Mover,
			TArray<bool>& Visited, TSet<int32>& OutTiles)
		{
			if (StepsLeft == 0)
			{
				OutTiles.Add(Current);
				return;
			}

			for (const int32 Next : Board.Tiles[Current].Neighbours)
			{
				if (!Board.IsValidTile(Next) || Visited[Next] || !IsPassable(Board, Next, Mover))
				{
					continue;
				}
				Visited[Next] = true;
				CollectExactPaths(Board, Next, StepsLeft - 1, Mover, Visited, OutTiles);
				Visited[Next] = false;
			}
		}
	}

	bool IsPassable(const FBoardState& Board, int32 TileId, const FMoverTraits& Mover)
	{
		if (!Board.IsValidTile(TileId))
		{
			return false;
		}
		return !Board.Tiles[TileId].bSealed || Mover.bFreePassage;
	}

	TArray<int32> ReachableTiles(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover)
	{
		TArray<int32> Result;
		if (!Board.IsValidTile(FromTile) || Roll <= 0)
		{
			return Result;
		}

		TArray<bool> Visited;
		Visited.Init(false, Board.Tiles.Num());
		Visited[FromTile] = true;

		TSet<int32> Found;
		CollectExactPaths(Board, FromTile, Roll, Mover, Visited, Found);

		Result = Found.Array();
		Result.Sort();
		return Result;
	}
}
