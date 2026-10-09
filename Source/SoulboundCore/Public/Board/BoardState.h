#pragma once

#include "CoreMinimal.h"
#include "BoardState.generated.h"

namespace Soulbound::Board
{
	/** Número de Dominios del tablero: siempre seis, en anillo alrededor del Altar (R-TAB-3). */
	inline constexpr int32 DomainCount = 6;
}

/**
 * Tipo de casilla (glosario "Casilla": tiene un tipo y conexiones). Es fijo durante la partida;
 * los estados que cambian, como el Sello o el efecto de un Nodo, van aparte.
 */
UENUM()
enum class ETileType : uint8
{
	/** Casilla sin efecto propio sobre el movimiento. */
	Normal,
	/** El Altar: detiene el movimiento de quien entra en él (R-TAB-16). */
	Altar,
	/** Nicho: una por Dominio, al final de un ramal, con una sola conexión (R-TAB-4, R-TAB-4c). */
	Niche,
	/** Fuente de Poder: tantas por Dominio como diga el archivo del tablero (R-TAB-4). */
	PowerSource,
	/** Nodo de Poder: posición fija, efecto sorteado al iniciar la partida (R-TAB-5). */
	PowerNode,
};

/** Efecto de un Nodo de Poder, sorteado al iniciar la partida (R-TAB-5). None en cualquier otra casilla. */
UENUM()
enum class EPowerNodeEffect : uint8
{
	None,
	/** Nodo de Duelo (glosario `DuelNode`). */
	Duel,
	/** Nodo del Oráculo (glosario `OracleNode`). */
	Oracle,
	/** Nodo Planar (glosario `PlanarNode`). */
	Planar,
};

/** Estado de un Dominio (glosario "Dominio"). Lo que se reclama es el Nicho, no el Dominio: su estado va en ENicheState. */
UENUM()
enum class EDomainState : uint8
{
	/** Con dueño: su Elegido, aunque ningún Eco se haya vinculado aún. */
	Owned,
	/** Dominio Olvidado: corrompido por el Warden al inicio (R-TAB-6). */
	Forgotten,
};

/**
 * Estado del tablero (R-TAB-8, R-TAB-9): Primordial desde el inicio hasta que el primer jugador reclama un Nicho;
 * entonces Despierto, sin vuelta atrás, y los Nodos de Poder quedan revelados todos a la vez (Despertar del tablero,
 * glosario `BoardAwakening`). Nombre propuesto en S-006.
 */
UENUM()
enum class EBoardPhase : uint8
{
	/** Primordial: nadie se ha vinculado; los Nodos no están revelados y no hay runas (R-TAB-8). */
	Primordial,
	/** Despierto: desde el primer vínculo hasta el fin de la partida (R-TAB-9). */
	Awakened,
};

/** Estado del Nicho de un Dominio. */
UENUM()
enum class ENicheState : uint8
{
	/** Libre: se puede reclamar. */
	Free,
	/** Reclamado por un Eco que se ha vinculado a su Elegido (R-CAR-4); el asiento está en FDomainState::OwnerSeat. */
	Claimed,
	/** Cerrado: el Nicho de un Dominio Olvidado (R-TAB-6, glosario "Dominio Olvidado"). */
	Closed,
};

/**
 * Casilla (Tile) del tablero: nodo del grafo con sus conexiones (Edges), su tipo y su estado.
 * El ocupante llegará con su story.
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

	/** Dominio al que pertenece la casilla (0 a 5). INDEX_NONE solo en el Altar. */
	UPROPERTY()
	int32 DomainId = INDEX_NONE;

	/**
	 * Reserva de la Fuente de Poder: runas que quedan por obtener en ella (R-RUN-4, R-TAB-4b). 0 en cualquier otra casilla
	 * y mientras el tablero es Primordial; al Despertar arranca con FBoardParams::RuneReservePerSource. Solo la escriben
	 * las reglas de Match/BondRaceRules.h (Despertar) y Match/RuneRules.h. Nombre propuesto en S-008.
	 */
	UPROPERTY()
	int32 RuneReserve = 0;

	/** Efecto sorteado al iniciar la partida si la casilla es un Nodo de Poder (R-TAB-5); None si no. */
	UPROPERTY()
	EPowerNodeEffect PowerNodeEffect = EPowerNodeEffect::None;

	/**
	 * Sello del Warden: no se pisa ni se atraviesa salvo con Paso Libre (R-TAB-17, glosario "Sello"). En partida lo
	 * escriben solo las reglas de Board/SealRules.h, a la vez que FBoardState::SealedTiles.
	 */
	UPROPERTY()
	bool bSealed = false;

	/**
	 * Posición 2D de la casilla en el tablero, en las unidades del archivo (bloque opcional `editor.positions`,
	 * S-005). Solo vale si bHasLayoutPosition; la presentación la escala. Nombre propuesto en S-005.
	 */
	// TODO(R-RED-1): LayoutPosition es un dato de presentación dentro del estado de partida; al replicar el estado,
	// decidir si viaja al cliente o se recarga del archivo del tablero.
	UPROPERTY()
	FVector2D LayoutPosition = FVector2D::ZeroVector;

	/** Si el archivo del tablero traía posición para esta casilla. Sin ella, la presentación reparte las casillas en anillo. */
	UPROPERTY()
	bool bHasLayoutPosition = false;
};

/**
 * Dominio del tablero. Su identificador es también su posición en el anillo: el Dominio i es vecino
 * de i-1 e i+1 (módulo seis). Los GDD posteriores asignan Elegidos y Ecos por este identificador.
 */
USTRUCT()
struct SOULBOUNDCORE_API FDomainState
{
	GENERATED_BODY()

	/** Identificador estable, de 0 a 5, en el orden del anillo. */
	UPROPERTY()
	int32 DomainId = INDEX_NONE;

	/** Nombre del Dominio, de los datos (domains[].name). El nombre del Elegido vive en elegidos.json (S-008). */
	UPROPERTY()
	FString Name;

	UPROPERTY()
	EDomainState State = EDomainState::Owned;

	UPROPERTY()
	ENicheState NicheState = ENicheState::Free;

	/**
	 * Dueño del Dominio: el asiento que reclamó su Nicho y es su Elegido (R-CAR-4), o INDEX_NONE si nadie lo ha
	 * reclamado. Lo escriben solo las reglas de Match/BondRaceRules.h. Nombre propuesto en S-006.
	 */
	UPROPERTY()
	int32 OwnerSeat = INDEX_NONE;
};

/** Parámetros de juego que dependen del tablero. Se cargan del mismo archivo; ningún valor vive en el código. */
USTRUCT()
struct SOULBOUNDCORE_API FBoardParams
{
	GENERATED_BODY()

	/** Fuentes de Poder por Dominio (R-TAB-4), en datos. */
	UPROPERTY()
	int32 PowerSourcesPerDomain = 0;

	/** Cantidad de Nodos de cada efecto (R-TAB-5). Su suma es el número de Nodos del tablero. En datos. */
	UPROPERTY()
	int32 DuelNodeCount = 0;

	UPROPERTY()
	int32 OracleNodeCount = 0;

	UPROPERTY()
	int32 PlanarNodeCount = 0;

	/** Máximo de casillas selladas a la vez (R-TAB-22), en datos. Con 0 no se sella ninguna. */
	UPROPERTY()
	int32 MaxSealedTiles = 0;

	/** Runas en reserva por Fuente (R-TAB-4b), en datos. Cada Fuente arranca con ellas al Despertar (S-008). */
	UPROPERTY()
	int32 RuneReservePerSource = 0;

	/** Número total de Nodos de Poder que piden los datos. */
	int32 TotalPowerNodeCount() const { return DuelNodeCount + OracleNodeCount + PlanarNodeCount; }
};

/** Estado del tablero: el grafo de casillas, sus Dominios y sus parámetros. Datos puros, sin mundo. */
USTRUCT()
struct SOULBOUNDCORE_API FBoardState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTileState> Tiles;

	/** Los seis Dominios, indexados por su identificador. Vacío en los tableros mínimos de los tests de movimiento. */
	UPROPERTY()
	TArray<FDomainState> Domains;

	UPROPERTY()
	FBoardParams Params;

	/**
	 * Casillas selladas, de la más antigua a la más nueva (R-TAB-22: al pasarse del máximo cae la primera).
	 * Coherente con FTileState::bSealed; solo la escriben las reglas de Board/SealRules.h. Nombre propuesto en S-004.
	 */
	UPROPERTY()
	TArray<int32> SealedTiles;

	/** Primordial o Despierto (R-TAB-8, R-TAB-9). Solo lo escriben las reglas de Match/BondRaceRules.h. */
	UPROPERTY()
	EBoardPhase Phase = EBoardPhase::Primordial;

	/** Si los efectos de los Nodos de Poder están revelados: desde el Despertar, todos a la vez (R-TAB-9). */
	bool AreNodesRevealed() const { return Phase == EBoardPhase::Awakened; }

	/** Añade una casilla y devuelve su identificador. */
	int32 AddTile();

	/** Conecta dos casillas en ambos sentidos. Ignora duplicados e identificadores inválidos. */
	void Connect(int32 TileA, int32 TileB);

	/** Desconecta dos casillas en ambos sentidos. Ignora identificadores inválidos. */
	void Disconnect(int32 TileA, int32 TileB);

	bool IsValidTile(int32 TileId) const { return Tiles.IsValidIndex(TileId); }

	bool IsAltar(int32 TileId) const { return IsValidTile(TileId) && Tiles[TileId].Type == ETileType::Altar; }

	/** Identificador de la primera casilla Altar, o INDEX_NONE si no hay. */
	int32 FindAltar() const;

	/** Identificador del Nicho del Dominio indicado, o INDEX_NONE si no tiene. */
	int32 FindNiche(int32 DomainId) const;

	/**
	 * Identificador de la primera Fuente de Poder del Dominio indicado, o INDEX_NONE si no tiene: la Fuente a la que vuelve
	 * una runa de ese tipo (R-RUN-6). Nombre propuesto en S-013.
	 */
	int32 FindPowerSource(int32 DomainId) const;
};
