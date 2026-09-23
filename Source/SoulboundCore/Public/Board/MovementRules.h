#pragma once

#include "CoreMinimal.h"

struct FBoardState;

/** Lo que el motor necesita saber del jugador que mueve para decidir qué casillas puede transitar. */
struct SOULBOUNDCORE_API FMoverTraits
{
	/** Paso Libre: atraviesa y pisa casillas selladas (glosario "Paso Libre"). */
	bool bFreePassage = false;
};

namespace Soulbound::Rules
{
	/**
	 * R-TAB-14. Casillas alcanzables desde FromTile con una tirada Roll: aquellas a las que llega un camino
	 * sin repetir casillas, de longitud exactamente Roll, cuyas casillas son todas transitables para el jugador.
	 * Devuelve identificadores únicos en orden ascendente. Tirada 0 o casilla inválida: ninguna.
	 * Pendiente para sus stories: el Altar detiene siempre el movimiento; casillas ocupadas.
	 */
	SOULBOUNDCORE_API TArray<int32> ReachableTiles(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover);

	/** Una casilla es transitable para el jugador si no está sellada o el jugador tiene Paso Libre. */
	SOULBOUNDCORE_API bool IsPassable(const FBoardState& Board, int32 TileId, const FMoverTraits& Mover);
}
