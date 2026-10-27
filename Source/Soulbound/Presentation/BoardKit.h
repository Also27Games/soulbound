#pragma once

#include "CoreMinimal.h"
#include "BoardLayout.h"
#include "UObject/GCObject.h"

struct FBoardState;
class ABoardViewActor;
class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

SOULBOUND_API DECLARE_LOG_CATEGORY_EXTERN(LogSoulboundView, Log, All);

/** Pieza del kit de arte del tablero (S-019, R-VIS-8): una por referencia configurable de la vista. */
enum class EBoardKitPiece : uint8
{
	Slab,
	CrackedSlab,
	NicheSlab,
	SourceSlab,
	NodeSlab,
	PathSegment,
	Pedestal,
	PlainFloor,
	Altar,
	NicheThrone,
	Pylon,
	FloatingRune,
	PlanarArch,
	DuelSwords,
	OracleOrb,
	SealChains,
	TokenCard,
	Count,
};

/** Lo que `FBoardKit::PlaceFixed` coloca y no cambia con la partida: suelos de cuña, losas, senderos y Altar. */
struct FBoardKitFixedPieces
{
	/** Losas y senderos del kit, una malla instanciada por pieza. */
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Instances;

	/** Cuñas y Altar del kit. */
	TArray<TObjectPtr<UStaticMeshComponent>> Fixed;

	/** Suelos de cuña (`peana` y `suelo_liso`) que se tiñen por Dominio, y el Dominio de cada uno (mismo índice). */
	TArray<TObjectPtr<UStaticMeshComponent>> WedgeFloors;
	TArray<int32> WedgeFloorDomains;
};

/** Qué estructuras de casilla crea `FBoardKit::PlaceStructures`, y si salen a la vista. */
struct FBoardKitStructureOptions
{
	/** Visibilidad inicial de lo que se crea: la vista de partida lo oculta hasta que el estado de vista lo pide. */
	bool bVisible = false;

	/** Crea la runa flotante sobre el pilón de cada Fuente. */
	bool bFloatingRunes = true;

	/** Crea las cadenas del Sello sobre cada casilla que se puede sellar. */
	bool bSealChains = true;
};

/** Las estructuras de casilla que crea `FBoardKit::PlaceStructures`, indexadas por casilla; nula la que no lleva o falta la pieza. */
struct FBoardKitStructures
{
	TArray<TObjectPtr<UStaticMeshComponent>> Structures;
	TArray<TObjectPtr<UStaticMeshComponent>> FloatingRunes;
	TArray<TObjectPtr<UStaticMeshComponent>> SealChains;
};

/**
 * El kit de arte del tablero cargado (S-019, R-VIS-8; extraído de ABoardViewActor): las piezas y los materiales que
 * configura ABoardViewActor, y la colocación de lo que no cambia con la partida sobre un actor. Lo comparten la vista de
 * partida y la vista previa del editor, para que el tablero se coloque igual en las dos. No contiene reglas: la
 * colocación sale de FBoardLayout.
 */
class SOULBOUND_API FBoardKit : public FGCObject
{
public:
	/** La pieza del kit de una losa. */
	static EBoardKitPiece SlabPiece(EBoardSlab Slab);

	/** La pieza del kit de una estructura; Count con None. */
	static EBoardKitPiece StructurePiece(EBoardStructure Structure);

	/** Nombre de la pieza para el registro. */
	static const TCHAR* PieceName(EBoardKitPiece Piece);

	/**
	 * Carga las piezas y los materiales que Config tiene configurados y avisa una vez en el registro por cada uno que falte.
	 * @param Config Quien guarda las referencias: la vista de partida, o el CDO en el editor.
	 * @param WhenMissing Lo que pasa con una pieza que falta, para el aviso ("se usa la forma básica").
	 * @return Cuántas piezas o materiales faltan.
	 */
	int32 Load(const ABoardViewActor& Config, const TCHAR* WhenMissing);

	/** La pieza cargada, o nula si no está. */
	UStaticMesh* Mesh(EBoardKitPiece Piece) const;

	UMaterialInterface* VertexColorMaterial() const { return VertexColor; }
	UMaterialInterface* EmissiveMaterial() const { return Emissive; }

	/** Si el trono está configurado: referencia no vacía, exista o no el asset (R-VIS-5, R-VIS-8). */
	bool IsNicheThroneConfigured() const { return bNicheThroneConfigured; }

	/** Pone a cada ranura de la pieza su material del kit: emisivo, tenue o color por vértice según el nombre de la ranura. */
	void ApplyMaterials(UStaticMeshComponent& Component) const;

	/** Un componente de una pieza del kit sobre Owner, sin colisión; oculto si bVisible es falso. */
	UStaticMeshComponent* AddPiece(AActor& Owner, UStaticMesh* Mesh, const FVector& Location, float Yaw, bool bVisible, EObjectFlags Flags = RF_NoFlags) const;

	/**
	 * Coloca sobre Owner lo que no cambia con la partida (R-VIS-2 a 4): el suelo de cada cuña, una losa por casilla que tenga
	 * pieza (las demás se saltan), el Altar y los tramos de sendero.
	 */
	void PlaceFixed(AActor& Owner, const FBoardState& Board, const FBoardLayout& Layout, FBoardKitFixedPieces& Out, EObjectFlags Flags = RF_NoFlags) const;

	/** Coloca sobre Owner, según Options, las estructuras, las runas flotantes y las cadenas de las casillas (R-VIS-5). */
	void PlaceStructures(AActor& Owner, const FBoardState& Board, const FBoardLayout& Layout, const FBoardLayoutParams& Params,
		const FBoardKitStructureOptions& Options, FBoardKitStructures& Out, EObjectFlags Flags = RF_NoFlags) const;

	/** Tiñe el suelo de cada cuña con el color de su Dominio, apagado si está Olvidado (R-VIS-2, R-VIS-5). */
	void TintWedgeFloors(const FBoardKitFixedPieces& Pieces, const FBoardState& Board, const ABoardViewActor& Config) const;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FBoardKit"); }

private:
	TArray<TObjectPtr<UStaticMesh>> Meshes;
	TObjectPtr<UMaterialInterface> VertexColor;
	TObjectPtr<UMaterialInterface> Emissive;
	TObjectPtr<UMaterialInterface> Dim;
	FString EmissiveSlotTag;
	FString DimSlotTag;
	bool bNicheThroneConfigured = false;
};
