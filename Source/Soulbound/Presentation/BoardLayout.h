#pragma once

#include "CoreMinimal.h"
#include "BoardLayout.generated.h"

struct FBoardState;
struct FTileState;

/**
 * Losa del kit que lleva una casilla (S-019, R-VIS-3; nombre propuesto): la normal, la partida (una fracción de las
 * normales, sorteada), la del Nicho, la de la Fuente y la del Nodo. El Altar es su pieza propia.
 */
enum class EBoardSlab : uint8
{
	Normal,
	Cracked,
	Niche,
	PowerSource,
	PowerNode,
	Altar,
};

/**
 * Estructura del kit junto a una casilla especial (S-019, R-VIS-5; nombre propuesto): el trono en el Nicho, el pilón en la
 * Fuente y, en el Nodo, la de su efecto. None en las demás casillas y en un Nodo sin efecto.
 */
enum class EBoardStructure : uint8
{
	None,
	NicheThrone,
	Pylon,
	PlanarArch,
	DuelSwords,
	OracleOrb,
};

/**
 * Parámetros de la colocación del tablero sobre la mesa (S-019; nombre propuesto `BoardLayoutParams`). Las medidas son las
 * del kit de la prueba C, ronda 6 (`kit.json`), en cm: un metro del kit es 100 cm en Unreal y las piezas no se escalan.
 */
USTRUCT()
struct SOULBOUND_API FBoardLayoutParams
{
	GENERATED_BODY()

	/** Radio de la mesa: la losa más lejana del Altar queda entera dentro (R-VIS-1). Valor inicial 74. */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float TableRadius = 74.f;

	/**
	 * Radio de mesa para el que está hecho el kit (R-VIS-1): el suelo de cada cuña (`peana` y `suelo_liso`) se escala por
	 * TableRadius / KitTableRadius. Con un valor no positivo no se escala. Valor inicial 74.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float KitTableRadius = 74.f;

	/**
	 * Cota de la cara del suelo del kit sobre la mesa, en cm (R-VIS-3): lo que se coloca sobre el suelo (losas, senderos,
	 * estructuras) sube esa altura; el Altar no, su base está en la mesa. Valor inicial 0: sin kit, las formas de respaldo
	 * van a cota 0. Con el kit, 3,4: la cara de la losa (0,55 más) queda por encima del relieve máximo de la tierra (3,9).
	 */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float KitFloorHeight = 0.f;

	/** Apotema del Altar, de su centro al borde de una cara (sus vértices están a 17,3). Valor inicial 15. */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float AltarApothem = 15.f;

	/**
	 * Hueco que se suma entre el borde del Altar y la distancia escalada de cada casilla (decisión del autor del 20/10/2026:
	 * se escala desde el borde del Altar, no desde su centro). Valor inicial 0: la casilla más cercana ya queda separada por
	 * su propia distancia escalada.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float AltarGap = 0.f;

	/** Media losa: del centro de la losa a su borde. Valor inicial 3,15 (la losa mide 6,3). */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float SlabHalfSize = 3.15f;

	/**
	 * Cota de la grada alta del Altar del kit, donde se posan las fichas, en cm (R-VIS-3): 4,0 la grada baja y 5,1 la alta.
	 * La cara superior del Altar es AltarTop * KitScale, nunca las dimensiones de la malla (la aguja llega a 25,6). Valor inicial 5,1.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float AltarTop = 5.1f;

	/** Altura de la cara superior de la losa, donde se posa la ficha (R-VIS-3). Valor inicial 0,55. */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float SlabTop = 0.55f;

	/** Largo de un tramo de sendero, a lo largo de su x local (R-VIS-4). Valor inicial 2,4. */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	float PathSegmentLength = 2.4f;

	/** Fracción de las losas normales que son la variante partida (R-VIS-3). Valor inicial 0,25. */
	UPROPERTY(EditAnywhere, Category = "Mesa", meta = (ClampMin = "0", ClampMax = "1"))
	float CrackedSlabFraction = 0.25f;

	/** Semilla del sorteo de las losas partidas: la misma en cada partida con el mismo tablero (R-VIS-3). Valor inicial 19. */
	UPROPERTY(EditAnywhere, Category = "Mesa")
	int32 CrackedSlabSeed = 19;

	/** Del borde de la losa al centro del trono: su frente está a 4,8 de su origen, más 0,5 de hueco. Valor inicial 5,3. */
	UPROPERTY(EditAnywhere, Category = "Estructuras")
	float NicheThroneDistance = 5.3f;

	/** Del borde de la losa al centro del pilón (radio 2,2 más 0,5). Valor inicial 2,7. */
	UPROPERTY(EditAnywhere, Category = "Estructuras")
	float PylonDistance = 2.7f;

	/** Del borde de la losa al centro del arco Planar (fondo ±1,0 más 0,5). Valor inicial 1,5. */
	UPROPERTY(EditAnywhere, Category = "Estructuras")
	float PlanarArchDistance = 1.5f;

	/** Del borde de la losa al centro de las espadas de Duelo (fondo ±2,5 más 0,5). Valor inicial 3,0. */
	UPROPERTY(EditAnywhere, Category = "Estructuras")
	float DuelSwordsDistance = 3.f;

	/** Del borde de la losa al centro del orbe del Oráculo (radio 1,8 más 0,5). Valor inicial 2,3. */
	UPROPERTY(EditAnywhere, Category = "Estructuras")
	float OracleOrbDistance = 2.3f;

	/** Altura de la runa flotante sobre la base del pilón (R-VIS-5). Valor inicial 3,4. */
	UPROPERTY(EditAnywhere, Category = "Estructuras")
	float FloatingRuneHeight = 3.4f;

	/** Distancia del borde de la losa al centro de la estructura indicada; 0 con None. */
	float StructureDistance(EBoardStructure Structure) const;
};

/** Colocación de una casilla (S-019): su losa y, si es especial, su estructura. En el espacio de la mesa, con el Altar en el origen. */
struct SOULBOUND_API FTileLayout
{
	/** Cota de la cara superior, donde se posa la ficha (R-VIS-3): KitFloorHeight + SlabTop en una losa, AltarTop * KitScale en el Altar. */
	float Top = 0.f;

	/** Centro de la losa, sobre la mesa (z = 0, la base de la pieza). */
	FVector Location = FVector::ZeroVector;

	/** Giro de la losa en grados. La x local sigue el camino; en el Nicho apunta a su estructura (la cabecera). */
	float Yaw = 0.f;

	EBoardSlab Slab = EBoardSlab::Normal;

	/** Lado de la estructura: unitario, hacia fuera del camino (R-VIS-5). Cero en el Altar sin conexiones. */
	FVector2D OutwardDirection = FVector2D::ZeroVector;

	/** La estructura que le toca por tipo y efecto, se vea o no (la visibilidad es del estado de vista). */
	EBoardStructure Structure = EBoardStructure::None;

	/** Centro de la estructura, en la base (z = 0), a su distancia del borde de la losa por OutwardDirection. */
	FVector StructureLocation = FVector::ZeroVector;

	/** Giro de la estructura en grados: su frente (-y local) mira a la losa. */
	float StructureYaw = 0.f;
};

/** Un tramo de sendero de una conexión (S-019, R-VIS-4; nombre propuesto `PathSegment`). */
struct SOULBOUND_API FPathSegmentLayout
{
	/** Las dos casillas de la conexión, con TileA < TileB. */
	int32 TileA = INDEX_NONE;
	int32 TileB = INDEX_NONE;

	/** Centro del tramo, sobre la mesa. */
	FVector Location = FVector::ZeroVector;

	/** Giro en grados: la x local del tramo, a lo largo de la conexión. */
	float Yaw = 0.f;
};

/** Cuña de un Dominio (S-019, R-VIS-2; nombre propuesto `DomainWedge`): la peana y el suelo liso, girados a su sector. */
struct SOULBOUND_API FDomainWedgeLayout
{
	int32 DomainId = INDEX_NONE;

	/** Giro en grados, desde el Altar: el ángulo medio de las casillas del Dominio, o el identificador por 60 si no es radial. */
	float Yaw = 0.f;
};

/**
 * Colocación del tablero en la mesa (S-019; nombre propuesto `BoardLayout`): todo lo que no cambia con la partida, como
 * función pura del tablero y de los parámetros. La vista la lee; no contiene reglas de juego.
 */
struct SOULBOUND_API FBoardLayout
{
	/** Una por casilla, indexada por su identificador. */
	TArray<FTileLayout> Tiles;

	TArray<FPathSegmentLayout> PathSegments;

	/** Una por Dominio, indexada por su identificador. */
	TArray<FDomainWedgeLayout> Wedges;

	/** Centímetros de la mesa por unidad de posición del editor (R-VIS-1). */
	float Scale = 0.f;

	/**
	 * Escala del kit a la mesa (R-VIS-1): TableRadius / KitTableRadius, 1 si éste no es positivo. El suelo de las cuñas
	 * (`peana` y `suelo_liso`) la lleva en X e Y y el Altar en X, Y y Z; las losas y el resto, no.
	 */
	float KitScale = 1.f;

	/** Apotema del Altar ya escalada (AltarApothem * KitScale): el borde interior de las casillas y el Altar de respaldo (R-VIS-1). */
	float AltarApothem = 0.f;

	/** Falso si el trazado no es radial (R-VIS-2): la vista lo avisa y las cuñas van de 60 en 60. */
	bool bRadial = true;
};

namespace Soulbound::Presentation
{
	/**
	 * Estructura que corresponde a la casilla por su tipo y el efecto de su Nodo (R-VIS-5), sin mirar si se ve.
	 * @param Tile Casilla del tablero.
	 * @return NicheThrone en el Nicho, Pylon en la Fuente, la del efecto en un Nodo; None en las demás.
	 */
	SOULBOUND_API EBoardStructure StructureFor(const FTileState& Tile);

	/**
	 * Coloca el tablero en la mesa (R-VIS-1 a 5): posición y giro de cada losa y su tipo de losa, el lado y la posición de
	 * cada estructura, los tramos de sendero de cada conexión y el giro de cada cuña. Sin mundo ni assets.
	 * Las casillas se colocan con el ángulo que tienen en el editor alrededor del Altar y a la distancia apotema + hueco +
	 * distancia en el editor por la escala, con la escala que deja la losa más lejana dentro del radio de mesa. Si alguna
	 * casilla no trae posición, se reparten en anillo por Dominio.
	 * @param Board Tablero con sus posiciones del editor (`LayoutPosition`).
	 * @param Params Medidas de la mesa y del kit.
	 * @return La colocación; vacía si el tablero no tiene casillas.
	 */
	SOULBOUND_API FBoardLayout ComputeBoardLayout(const FBoardState& Board, const FBoardLayoutParams& Params);
}
