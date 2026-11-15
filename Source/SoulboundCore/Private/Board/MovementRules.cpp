#include "Board/MovementRules.h"

#include "Board/BoardState.h"
#include "Templates/Function.h"

namespace Soulbound::Rules
{
	bool IsOutsideConfinement(const FBoardState& Board, int32 TileId, const FMoverTraits& Mover)
	{
		const int32 DomainId = Board.Tiles[TileId].DomainId;
		return Mover.bConfinedToForgotten && Board.Domains.IsValidIndex(DomainId) && Board.Domains[DomainId].State != EDomainState::Forgotten;
	}

	namespace
	{

		/**
		 * Búsqueda en profundidad de caminos simples, la de ReachableTiles y MovementPath (S-020: una sola, nada duplicado). Un
		 * camino acaba en una casilla alcanzable si agota la tirada (R-TAB-14), si entra en el Altar (R-TAB-16) o si topa con una
		 * casilla sellada (R-TAB-17). El origen nunca es alcanzable: ni el Altar de origen detiene nada ni un Sello en el paso 1
		 * lo convierte en destino. Un Dominio vedado por el confinamiento (R-TAB-20) no detiene antes: para el camino, no existe.
		 * Los vecinos se recorren en orden ascendente, así el primer camino que encuentra es siempre el mismo (S-020).
		 * Path lleva las casillas del camino en curso, sin el origen; OnEnd recibe cada camino al acabar (su última casilla es la
		 * alcanzable) y devuelve true para parar la búsqueda. Devuelve true si se paró. Los tableros son pequeños (decenas de
		 * casillas).
		 */
		bool WalkPaths(const FBoardState& Board, int32 FromTile, int32 Current, int32 StepsLeft, const FMoverTraits& Mover,
			TArray<bool>& Visited, TArray<int32>& Path, TFunctionRef<bool(const TArray<int32>&)> OnEnd)
		{
			const bool bIsOrigin = Current == FromTile;
			if (StepsLeft == 0 || (!bIsOrigin && Board.IsAltar(Current)))
			{
				return OnEnd(Path);
			}

			TArray<int32, TInlineAllocator<8>> Neighbours(Board.Tiles[Current].Neighbours);
			Neighbours.Sort();
			for (const int32 Next : Neighbours)
			{
				if (!Board.IsValidTile(Next) || Visited[Next] || IsOutsideConfinement(Board, Next, Mover))
				{
					continue;
				}
				if (!IsPassable(Board, Next, Mover))
				{
					if (!bIsOrigin && OnEnd(Path))
					{
						return true;
					}
					continue;
				}
				Visited[Next] = true;
				Path.Push(Next);
				const bool bStop = WalkPaths(Board, FromTile, Next, StepsLeft - 1, Mover, Visited, Path, OnEnd);
				Path.Pop();
				Visited[Next] = false;
				if (bStop)
				{
					return true;
				}
			}
			return false;
		}

		/** Lanza WalkPaths desde FromTile con la tirada Roll. Sin nada que buscar (casilla inválida, tirada 0), no llama a OnEnd. */
		void WalkPathsFrom(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover, TFunctionRef<bool(const TArray<int32>&)> OnEnd)
		{
			if (!Board.IsValidTile(FromTile) || Roll <= 0)
			{
				return;
			}
			TArray<bool> Visited;
			Visited.Init(false, Board.Tiles.Num());
			Visited[FromTile] = true;
			TArray<int32> Path;
			WalkPaths(Board, FromTile, FromTile, Roll, Mover, Visited, Path, OnEnd);
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

	TArray<int32> MovementPath(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover, int32 ToTile)
	{
		// El primer camino de la búsqueda que acaba en ToTile; la casilla de origen nunca acaba un camino.
		TArray<int32> Result;
		WalkPathsFrom(Board, FromTile, Roll, Mover, [ToTile, &Result](const TArray<int32>& Path)
		{
			if (Path.IsEmpty() || Path.Last() != ToTile)
			{
				return false;
			}
			Result = Path;
			return true;
		});
		return Result;
	}

	TArray<int32> ReachableTiles(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover)
	{
		TSet<int32> Found;
		WalkPathsFrom(Board, FromTile, Roll, Mover, [&Found](const TArray<int32>& Path)
		{
			Found.Add(Path.Last());
			return false;
		});
		TArray<int32> Result = Found.Array();
		Result.Sort();
		return Result;
	}
}
