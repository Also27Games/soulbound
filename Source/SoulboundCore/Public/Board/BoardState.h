#pragma once

#include "CoreMinimal.h"
#include "BoardState.generated.h"

/**
 * Tipo de casilla (glosario "Casilla": tiene un tipo y conexiones). Es fijo durante la partida;
 * los estados que cambian, como el Sello, van aparte. Nichos, Fuentes y Nodos llegarán con sus stories.
 */
UENUM()
enum class ETileType : uint8
{
	/** Casilla sin efecto propio sobre el movimiento. */
	Normal,
	/** El Altar: detiene el movimiento de quien entra en él (R-TAB-16). */
	Altar,
};

/**
 * Casilla (Tile) del tablero: nodo del grafo con sus conexiones (Edges), su tipo y su estado.
 * El ocupante y los Nodos llegarán con sus stories.
 */
USTRUCT()
struct SOULBOUNDCORE_API FTileState
{
	GENERATED_BODY()

	/** Identificador estable de la casilla; coincide con su índice en FBoardState::Tiles. */
	UPROPERTY()
	int32 TileId = INDEX_NONE;

	/** Identificadores de las casillas conectadas (conexiones sin dirección: si A lista a B, B lista a A). */
	UPROPERTY()
	TArray<int32> Neighbours;

	UPROPERTY()
	ETileType Type = ETileType::Normal;

	/** Sello del Warden: no se pisa ni se atraviesa salvo con Paso Libre (R-TAB-17, glosario "Sello"). */
	UPROPERTY()
	bool bSealed = false;
};

/** Estado del tablero: el grafo de casillas. Datos puros, sin mundo. */
USTRUCT()
struct SOULBOUNDCORE_API FBoardState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTileState> Tiles;

	/** Añade una casilla y devuelve su identificador. */
	int32 AddTile();

	/** Conecta dos casillas en ambos sentidos. Ignora duplicados e identificadores inválidos. */
	void Connect(int32 TileA, int32 TileB);

	bool IsValidTile(int32 TileId) const { return Tiles.IsValidIndex(TileId); }

	bool IsAltar(int32 TileId) const { return IsValidTile(TileId) && Tiles[TileId].Type == ETileType::Altar; }
};
