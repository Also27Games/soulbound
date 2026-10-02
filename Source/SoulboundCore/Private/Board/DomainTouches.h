#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"

// Cabecera privada del motor: consulta del grafo que comparten el validador de tableros (Board/BoardLoader.cpp) y su test
// (Tests/BoardLoaderTest.cpp). No es API del módulo.
namespace Soulbound::BoardGraph
{
	/** Si alguna casilla del Dominio DomainId tiene una conexión con una casilla que cumple Pred. */
	template <typename PredType>
	bool DomainTouches(const FBoardState& Board, int32 DomainId, PredType Pred)
	{
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.DomainId == DomainId && Tile.Neighbours.ContainsByPredicate([&](int32 Other) { return Pred(Board.Tiles[Other]); }))
			{
				return true;
			}
		}
		return false;
	}
}
