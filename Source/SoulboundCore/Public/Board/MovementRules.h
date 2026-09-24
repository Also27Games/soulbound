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
	 * Casillas alcanzables desde FromTile con una tirada Roll, por caminos sin repetir casilla cuyas casillas
	 * son todas transitables para el jugador:
	 *  - R-TAB-14: las casillas a las que llega un camino de longitud exactamente Roll.
	 *  - R-TAB-16: el Altar, si un camino llega a él con longitud menor o igual que Roll; el camino termina ahí
	 *    y no lo cruza. Salir del Altar de origen es un movimiento normal.
	 *  - R-TAB-17: la casilla anterior a una sellada con la que topa un camino, aunque sobre tirada.
	 * El origen nunca es alcanzable; si no queda ninguna, el jugador no se mueve (R-TAB-18).
	 * Devuelve identificadores únicos en orden ascendente. Tirada 0 o casilla inválida: ninguna.
	 * Pendiente para sus stories: casillas ocupadas (R-TAB-19).
	 */
	SOULBOUNDCORE_API TArray<int32> ReachableTiles(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover);

	/** Una casilla es transitable para el jugador si no está sellada o el jugador tiene Paso Libre. */
	SOULBOUNDCORE_API bool IsPassable(const FBoardState& Board, int32 TileId, const FMoverTraits& Mover);
}
