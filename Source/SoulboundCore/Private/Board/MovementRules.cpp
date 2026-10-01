#include "Board/MovementRules.h"

#include "Board/BoardState.h"

namespace Soulbound::Rules
{
	namespace
	{
		/** Si la casilla es de un Dominio con dueño y el jugador está confinado a los Olvidados (R-TAB-20). El Altar, no. */
		bool IsOutsideConfinement(const FBoardState& Board, int32 TileId, const FMoverTraits& Mover)
		{
			const int32 DomainId = Board.Tiles[TileId].DomainId;
			return Mover.bConfinedToForgotten && Board.Domains.IsValidIndex(DomainId) && Board.Domains[DomainId].State != EDomainState::Forgotten;
		}

		/**
		 * Búsqueda en profundidad de caminos simples. Un camino acaba en una casilla alcanzable si agota la tirada
		 * (R-TAB-14), si entra en el Altar (R-TAB-16) o si topa con una casilla sellada (R-TAB-17). El origen nunca
		 * es alcanzable: ni el Altar de origen detiene nada ni un Sello en el paso 1 lo convierte en destino.
		 * Un Dominio vedado por el confinamiento (R-TAB-20) no detiene antes: para el camino, no existe.
		 * Los tableros son pequeños (decenas de casillas).
		 */
		void CollectPaths(const FBoardState& Board, int32 FromTile, int32 Current, int32 StepsLeft, const FMoverTraits& Mover,
			TArray<bool>& Visited, TSet<int32>& OutTiles)
		{
			const bool bIsOrigin = Current == FromTile;
			if (StepsLeft == 0 || (!bIsOrigin && Board.IsAltar(Current)))
			{
				OutTiles.Add(Current);
				return;
			}

			for (const int32 Next : Board.Tiles[Current].Neighbours)
			{
				if (!Board.IsValidTile(Next) || Visited[Next] || IsOutsideConfinement(Board, Next, Mover))
				{
					continue;
				}
				if (!IsPassable(Board, Next, Mover))
				{
					if (!bIsOrigin)
					{
						OutTiles.Add(Current);
					}
					continue;
				}
				Visited[Next] = true;
				CollectPaths(Board, FromTile, Next, StepsLeft - 1, Mover, Visited, OutTiles);
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
		if (IsOutsideConfinement(Board, TileId, Mover))
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
		CollectPaths(Board, FromTile, FromTile, Roll, Mover, Visited, Found);

		Result = Found.Array();
		Result.Sort();
		return Result;
	}
}
