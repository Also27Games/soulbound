#pragma once

#include "CoreMinimal.h"

struct FBoardState;

/** Lo que el motor necesita saber del jugador que mueve para decidir qué casillas puede transitar. */
struct SOULBOUNDCORE_API FMoverTraits
{
	/** Paso Libre: atraviesa y pisa casillas selladas (glosario "Paso Libre"). */
	bool bFreePassage = false;

	/**
	 * Confinado a los Dominios Olvidados: un Eco tras la carrera solo transita por el Altar y los Dominios Olvidados
	 * (R-TAB-20, R-ECO-2). Lo calcula MakeMoverTraits (Match/BondRaceRules.h). Nombre propuesto en S-006.
	 */
	bool bConfinedToForgotten = false;
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
	 *  - R-TAB-20: con bConfinedToForgotten, las casillas de un Dominio con dueño no existen para el camino: ni se
	 *    pisan ni se atraviesan, y no detienen antes como un Sello (R-TAB-17 habla solo de Sellos).
	 *  - R-TAB-19: las fichas nunca bloquean; la ocupación no se mira.
	 * El origen nunca es alcanzable; si no queda ninguna, el jugador no se mueve (R-TAB-18).
	 * Devuelve identificadores únicos en orden ascendente. Tirada 0 o casilla inválida: ninguna.
	 */
	SOULBOUNDCORE_API TArray<int32> ReachableTiles(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover);

	/**
	 * Camino del movimiento desde FromTile hasta ToTile con la tirada Roll (R-TAB-14, S-020): uno de los caminos que hacen a
	 * ToTile alcanzable, con las mismas reglas y la misma búsqueda que ReachableTiles (R-TAB-16, R-TAB-17, R-TAB-20): sin repetir
	 * casillas, todas transitables, de longitud exactamente Roll, o menor si acaba en el Altar o ante un Sello. Determinista: el
	 * primero que encuentra la búsqueda recorriendo los vecinos en orden ascendente.
	 * Devuelve las casillas en orden, sin el origen y acabando en ToTile; vacío si ToTile no está entre las alcanzables.
	 */
	SOULBOUNDCORE_API TArray<int32> MovementPath(const FBoardState& Board, int32 FromTile, int32 Roll, const FMoverTraits& Mover, int32 ToTile);

	/**
	 * Criterio único de transitabilidad (R-TAB-21): una casilla es transitable para el jugador si no está sellada o
	 * tiene Paso Libre, y, si está confinado a los Olvidados, si no pertenece a un Dominio con dueño (el Altar sí).
	 */
	SOULBOUNDCORE_API bool IsPassable(const FBoardState& Board, int32 TileId, const FMoverTraits& Mover);
}
