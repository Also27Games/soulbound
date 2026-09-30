#pragma once

#include "CoreMinimal.h"

struct FMatchState;

/** Qué pasa con un intento de sellar una casilla (R-TAB-22). Solo Accepted cambia el estado. Nombre propuesto en S-004. */
enum class ESealResult : uint8
{
	/** Se puede sellar: SealTile la sella y, si se pasa del máximo, libera la más antigua. */
	Accepted,
	/** La casilla no existe en el tablero. */
	InvalidTile,
	/** El máximo de casillas selladas de los parámetros del tablero es 0: no se sella ninguna. */
	NoSealsAllowed,
	/** La casilla ya está sellada: un Sello no se renueva. */
	AlreadySealed,
	/** No se sella el Altar. */
	Altar,
	/** No se sella un Nicho. */
	Niche,
	/** No se sella una Fuente de Poder. */
	PowerSource,
	/** No se sella una casilla ocupada (FMatchState::IsTileOccupied). */
	Occupied,
	/** Con el Sello puesto, alguna casilla no sellada quedaría sin camino al Altar para un jugador sin Paso Libre. */
	CutsOffAltar,
};

/**
 * Sellos del Warden (R-TAB-22, R-TAB-23, GDD 01). Viven a nivel de partida porque una casilla ocupada no se sella.
 * Solo estas funciones escriben FBoardState::SealedTiles y FTileState::bSealed, y lo hacen siempre a la vez.
 * Los jugadores no tienen ninguna vía para mover ni retirar un Sello: las llama quien tenga la autoridad del Warden.
 * El tiempo es un dato (ADR-001): cada cambio deja un evento en el registro con la marca NowSeconds.
 */
namespace Soulbound::Rules
{
	/**
	 * Dice si la casilla TileId se puede sellar ahora (R-TAB-22) y, si no, por qué. No cambia nada.
	 * El camino al Altar se comprueba sobre el tablero tal como quedaría: con este Sello puesto y, si se pasa del
	 * máximo, con el más antiguo liberado; con la transitabilidad de un jugador sin Paso Libre (IsPassable).
	 * Devuelve Accepted o el primer motivo de rechazo, en el orden de ESealResult.
	 */
	SOULBOUNDCORE_API ESealResult CanSealTile(const FMatchState& State, int32 TileId);

	/**
	 * Sella la casilla TileId si CanSealTile la acepta (R-TAB-22). Si con ella se pasa del máximo de los parámetros
	 * del tablero, libera antes la más antigua (R-TAB-23) y registra SealReleased; después registra SealPlaced.
	 * NowSeconds: instante del reloj de partida, marca de tiempo de los eventos.
	 * Devuelve Accepted si selló, o el motivo del rechazo sin tocar el estado.
	 */
	SOULBOUNDCORE_API ESealResult SealTile(FMatchState& State, int32 TileId, double NowSeconds);

	/**
	 * Libera el Sello de la casilla TileId y registra SealReleased. Reservada al Warden: es un evento suyo definido
	 * en su GDD (R-TAB-23); ninguna acción de jugador la llama.
	 * NowSeconds: instante del reloj de partida, marca de tiempo del evento.
	 * Devuelve false, sin cambiar nada, si la casilla no existe o no está sellada.
	 */
	SOULBOUNDCORE_API bool ReleaseSeal(FMatchState& State, int32 TileId, double NowSeconds);
}
