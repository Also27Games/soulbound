#pragma once

#include "CoreMinimal.h"
#include "BoardState.generated.h"

/**
 * Casilla (Tile) del tablero: nodo del grafo con sus conexiones (Edges) y su estado.
 * Spike, paso 2: solo lo necesario para la regla de casillas alcanzables (R-TAB-14).
 * El tipo de casilla, el ocupante y los Nodos llegarán con sus stories.
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

	/** Sello del Warden: no se pisa ni se atraviesa salvo con Paso Libre (R-TAB-16, glosario "Sello"). */
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
};
