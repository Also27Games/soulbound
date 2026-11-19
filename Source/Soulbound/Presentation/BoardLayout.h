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

	/**
	 * El diámetro del halo de una casilla alcanzable (S-033, R-DEC-12, nombre propuesto): el de siempre, el de la casilla por HaloScale, o, si su pieza
	 * del kit es mayor, la mayor de sus medidas X e Y por el margen (AltarHaloMargin), para que el halo asome por su borde también en las esquinas
	 * (el Altar, hexagonal, lo tapaba).
	 * @param TileDiameter El diámetro de la casilla.
	 * @param HaloScale El factor del halo (HaloScale).
	 * @param PieceSizeX La medida X de la pieza ya escalada; 0 sin pieza.
	 * @param PieceSizeY La medida Y de la pieza ya escalada; 0 sin pieza.
	 * @param Margin Cuánto sobresale el halo de la pieza, por su mayor medida; valor inicial 1,1 (un 10 %). Menos de 1 se toma como 1.
	 * @return El diámetro del halo.
	 */
	SOULBOUND_API float HaloDiameter(float TileDiameter, float HaloScale, float PieceSizeX, float PieceSizeY, float Margin);

	/**
	 * Parámetros del rótulo del Dominio (S-047, R-VIS-34, nombre propuesto `DomainNameplate`): sitio y tamaño, relativos a la cuña. Los valores
	 * iniciales están en ABoardViewActor (DomainNameplate...), en Config. "Profundidad de la cuña" es TableRadius menos la apotema del Altar.
	 */
	struct FDomainNameplateParams
	{
		/** Sitio preferido, a lo largo del radio: 0 en el borde del Altar (su radio circunscrito), 1 en el borde de la mesa. Valor inicial 0,27. */
		float RadiusFraction = 0.27f;
		/** Sitio preferido, de lado: fracción de media cuña desde su eje (negativo, a un lado; positivo, al otro). Valor inicial -0,8. */
		float AngleFraction = -0.8f;
		/** Tope de la letra del nombre, en fracción de la profundidad de la cuña. Valor inicial 0,08 (7,7 cm con la mesa de 120). */
		float MaxLetterSizeFraction = 0.08f;
		/**
		 * Ancho medio de un carácter, en fracción de la letra. Valor inicial 0,5: el autor midió 0,45 sobre una captura cenital ("Encadenado", 10
		 * caracteres con letra de 3,27 cm, unos 14 cm), con holgura (ronda del 19/11/2026; antes, 0,6 a ciegas).
		 */
		float CharWidthFraction = 0.5f;
		/** Alto de una línea, en fracción de su letra. Valor inicial 1,0 (ronda del 19/11/2026; antes, 1,2 a ciegas). */
		float LineHeightFraction = 1.f;
		/** Letra de "Olvidado", en fracción de la del nombre. Valor inicial 0,6. */
		float ForgottenScale = 0.6f;
		/** Holgura del rectángulo con losas, el Altar y los bordes de la cuña, en fracción de la profundidad de la cuña. Valor inicial 0,01. */
		float MarginFraction = 0.01f;
		/** Paso de la búsqueda del sitio libre, en fracción de la profundidad de la cuña. Valor inicial 0,01. */
		float SearchStepFraction = 0.01f;
		/**
		 * Cuánto puede alejarse el centro del rótulo del sitio preferido, en fracción de la profundidad de la cuña (ronda del 19/11/2026: los seis en la
		 * misma zona). Valor inicial 0,2 (unos 19 cm con la mesa de 120).
		 */
		float MaxOffsetFraction = 0.2f;
		/** Un nombre no se parte si alguna de sus dos líneas quedaría con menos caracteres (ronda del 19/11/2026: "El" no va suelto). Valor inicial 3. */
		int32 MinSplitLineChars = 3;
	};

	/**
	 * Dónde va el rótulo de un Dominio (S-047, R-VIS-34, nombre propuesto): el rectángulo que ocupa el texto, tumbado sobre la mesa, con el mismo giro
	 * en las seis cuñas (el de la vista sentada). En el espacio de la mesa, con el Altar en el origen.
	 */
	struct FDomainNameplateSpot
	{
		/** Si hay sitio en la cuña para el rectángulo. */
		bool bFound = false;
		/** Centro del rectángulo. */
		FVector2D Center = FVector2D::ZeroVector;
		/** Medio ancho (a lo largo de Right) y medio alto (a lo largo de Up) del rectángulo, sin la holgura. */
		FVector2D HalfExtent = FVector2D::ZeroVector;
		/** Dirección de lectura del texto (hacia la derecha de quien lo lee desde la vista sentada) y hacia arriba del texto (lo que se aleja de ella). */
		FVector2D Right = FVector2D::ZeroVector;
		FVector2D Up = FVector2D::ZeroVector;
		/** Giro del texto en grados (yaw del UTextRenderComponent tumbado, pitch 90): el de la vista sentada. */
		float Yaw = 0.f;
		/** Centro de cada línea del nombre, de arriba abajo. */
		TArray<FVector2D> NameLineCenters;
		/** Centro de la línea de "Olvidado", debajo del nombre; reservada siempre, para que el nombre no se mueva cuando aparece. */
		FVector2D ForgottenLineCenter = FVector2D::ZeroVector;
	};

	/** Los rótulos de los seis Dominios (S-047, R-VIS-34, nombre propuesto): la misma letra en todos; cada uno, sus líneas y su rectángulo. */
	struct FDomainNameplateLayout
	{
		/** Si sale la forma partida: cada nombre en dos líneas donde se puede (SplitNameplateName). Si no, todos en una línea. */
		bool bSplit = false;
		/** Letra del nombre, en cm: la mayor que cabe en las seis cuñas, con el tope de MaxLetterSizeFraction. 0 si en alguna no cabe ninguna. */
		float LetterSize = 0.f;
		/** Las líneas del nombre de cada Dominio, indexadas por su identificador. */
		TArray<TArray<FString>> NameLines;
		/** El sitio de cada Dominio, indexado por su identificador. */
		TArray<FDomainNameplateSpot> Spots;
		/** La letra mayor que cabe en cada cuña con su nombre, por forma (para el informe): en una línea y partido donde se puede. */
		TArray<float> MaxLetterOneLine;
		TArray<float> MaxLetterSplit;
	};

	/**
	 * El nombre de un Dominio partido en dos líneas por el espacio más cercano a la mitad (S-047, R-VIS-34, nombre propuesto): el que deja las dos
	 * líneas más parejas; a igual distancia, el primero. Sin espacio, o si alguna de las dos líneas quedaría con menos de MinLineChars caracteres
	 * (ronda del 19/11/2026: "El Banquete" no deja "El" suelto), una línea.
	 * @param Name El nombre ("El Fiordo Encadenado").
	 * @param MinLineChars Los caracteres mínimos de cada línea para partirlo (FDomainNameplateParams::MinSplitLineChars, 3).
	 * @return Las líneas ("El Fiordo", "Encadenado"); una si no se parte; vacío si el nombre está vacío.
	 */
	SOULBOUND_API TArray<FString> SplitNameplateName(const FString& Name, int32 MinLineChars);

	/**
	 * El tamaño estimado del rectángulo de un rótulo (S-047, R-VIS-34): ancho, la línea más larga (del nombre, o "Olvidado" con su letra) por la letra
	 * y por CharWidthFraction; alto, las líneas del nombre más la de "Olvidado", cada una por LineHeightFraction.
	 * @param NameLines Las líneas del nombre.
	 * @param LetterSize La letra del nombre, en cm.
	 * @param Params Los parámetros del rótulo.
	 * @return El ancho (X) y el alto (Y), en cm.
	 */
	SOULBOUND_API FVector2D NameplateSize(const TArray<FString>& NameLines, float LetterSize, const FDomainNameplateParams& Params);

	/**
	 * Dónde cabe un rectángulo de rótulo en la cuña de un Dominio (S-047, R-VIS-34, nombre propuesto): el sitio libre más cercano al preferido en el
	 * que el rectángulo, con su holgura y con el giro de la vista sentada, queda entero dentro del sector de la cuña (su eje más y menos media cuña,
	 * hasta el borde de la mesa) y no toca ninguna losa (de ningún Dominio) ni el Altar; los senderos se pueden cruzar. El centro no se aleja del
	 * preferido más de MaxOffsetFraction de la profundidad de la cuña (ronda del 19/11/2026).
	 * @param Board El tablero.
	 * @param Layout Su colocación (ComputeBoardLayout).
	 * @param LayoutParams Los parámetros con que se colocó (radio de mesa, media losa).
	 * @param DomainId El Dominio.
	 * @param Size El ancho y el alto del rectángulo (NameplateSize).
	 * @param ViewYawDegrees El ángulo de la vista sentada alrededor de la mesa (SeatedViewYawDegrees, 180).
	 * @param Params Los parámetros del rótulo.
	 * @return El sitio; bFound falso si no cabe en ninguna parte.
	 */
	SOULBOUND_API FDomainNameplateSpot DomainNameplateSpot(const FBoardState& Board, const FBoardLayout& Layout, const FBoardLayoutParams& LayoutParams,
		int32 DomainId, const FVector2D& Size, float ViewYawDegrees, const FDomainNameplateParams& Params);

	/**
	 * Los rótulos de los seis Dominios (S-047, R-VIS-34, nombre propuesto). Prueba las dos formas, todos los nombres en una línea y cada uno partido
	 * en dos donde se puede (SplitNameplateName); en cada una, cada rótulo mide el rectángulo de sus propias líneas más la de "Olvidado" (ronda del
	 * 19/11/2026), y la letra es la mayor que cabe en las seis cuñas, con el tope. Se queda la forma de letra mayor (a igualdad, en una línea) y coloca
	 * cada rótulo con DomainNameplateSpot.
	 * @param Board El tablero, con los nombres de sus Dominios.
	 * @param Layout Su colocación.
	 * @param LayoutParams Los parámetros con que se colocó.
	 * @param ViewYawDegrees El ángulo de la vista sentada.
	 * @param Params Los parámetros del rótulo.
	 * @return La forma, la letra y el sitio de cada Dominio.
	 */
	SOULBOUND_API FDomainNameplateLayout ComputeDomainNameplates(const FBoardState& Board, const FBoardLayout& Layout, const FBoardLayoutParams& LayoutParams,
		float ViewYawDegrees, const FDomainNameplateParams& Params);
}
