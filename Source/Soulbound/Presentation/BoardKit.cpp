#include "BoardKit.h"

#include "BoardViewActor.h"
#include "BoardViewState.h"
#include "Board/BoardState.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "PresentationShapes.h"

DEFINE_LOG_CATEGORY(LogSoulboundView);

EBoardKitPiece FBoardKit::SlabPiece(EBoardSlab Slab)
{
	switch (Slab)
	{
	case EBoardSlab::Cracked: return EBoardKitPiece::CrackedSlab;
	case EBoardSlab::Niche: return EBoardKitPiece::NicheSlab;
	case EBoardSlab::PowerSource: return EBoardKitPiece::SourceSlab;
	case EBoardSlab::PowerNode: return EBoardKitPiece::NodeSlab;
	case EBoardSlab::Altar: return EBoardKitPiece::Altar;
	case EBoardSlab::Normal:
	default: return EBoardKitPiece::Slab;
	}
}

EBoardKitPiece FBoardKit::StructurePiece(EBoardStructure Structure)
{
	switch (Structure)
	{
	case EBoardStructure::NicheThrone: return EBoardKitPiece::NicheThrone;
	case EBoardStructure::Pylon: return EBoardKitPiece::Pylon;
	case EBoardStructure::PlanarArch: return EBoardKitPiece::PlanarArch;
	case EBoardStructure::DuelSwords: return EBoardKitPiece::DuelSwords;
	case EBoardStructure::OracleOrb: return EBoardKitPiece::OracleOrb;
	case EBoardStructure::None:
	default: return EBoardKitPiece::Count;
	}
}

const TCHAR* FBoardKit::PieceName(EBoardKitPiece Piece)
{
	switch (Piece)
	{
	case EBoardKitPiece::Slab: return TEXT("losa");
	case EBoardKitPiece::CrackedSlab: return TEXT("losa partida");
	case EBoardKitPiece::NicheSlab: return TEXT("losa del Nicho");
	case EBoardKitPiece::SourceSlab: return TEXT("losa de la Fuente");
	case EBoardKitPiece::NodeSlab: return TEXT("losa del Nodo");
	case EBoardKitPiece::PathSegment: return TEXT("sendero");
	case EBoardKitPiece::Pedestal: return TEXT("peana");
	case EBoardKitPiece::PlainFloor: return TEXT("suelo liso");
	case EBoardKitPiece::Altar: return TEXT("Altar");
	case EBoardKitPiece::NicheThrone: return TEXT("trono");
	case EBoardKitPiece::Pylon: return TEXT("pilón");
	case EBoardKitPiece::FloatingRune: return TEXT("runa flotante");
	case EBoardKitPiece::PlanarArch: return TEXT("arco Planar");
	case EBoardKitPiece::DuelSwords: return TEXT("espadas de Duelo");
	case EBoardKitPiece::OracleOrb: return TEXT("orbe del Oráculo");
	case EBoardKitPiece::SealChains: return TEXT("cadenas del Sello");
	case EBoardKitPiece::TokenCard: return TEXT("carta de la ficha");
	case EBoardKitPiece::Count:
	default: return TEXT("?");
	}
}

int32 FBoardKit::Load(const ABoardViewActor& Config, const TCHAR* WhenMissing)
{
	// R-VIS-8: cada pieza que falte se avisa una vez, aquí, al cargar.
	Meshes.Init(nullptr, static_cast<int32>(EBoardKitPiece::Count));
	int32 Missing = 0;
	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		const EBoardKitPiece Piece = static_cast<EBoardKitPiece>(Index);
		const TCHAR* Name = PieceName(Piece);
		const TSoftObjectPtr<UStaticMesh>& Reference = Config.KitReference(Piece);
		if (Reference.IsNull())
		{
			// Vacía: no hay pieza. Con el trono es una variante de diseño ("el Nicho es el trono"), no un fallo.
			if (Piece != EBoardKitPiece::NicheThrone)
			{
				UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: la pieza %s no está configurada; %s."), Name, WhenMissing);
				++Missing;
			}
			continue;
		}
		Meshes[Index] = Reference.LoadSynchronous();
		if (!Meshes[Index])
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: la pieza %s (%s) no existe; %s."), Name, *Reference.ToString(), WhenMissing);
			++Missing;
		}
	}
	bNicheThroneConfigured = !Config.NicheThroneMesh.IsNull();
	if (!bNicheThroneConfigured)
	{
		UE_LOG(LogSoulboundView, Log, TEXT("Vista del tablero: sin trono configurado; el Nicho no lleva estructura."));
	}

	auto LoadMaterial = [&Missing](const TSoftObjectPtr<UMaterialInterface>& Reference, const TCHAR* Name) -> UMaterialInterface*
	{
		UMaterialInterface* Material = Reference.IsNull() ? nullptr : Reference.LoadSynchronous();
		if (!Material)
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: falta el material %s (%s); las piezas llevan el suyo."), Name, *Reference.ToString());
			++Missing;
		}
		return Material;
	};
	VertexColor = LoadMaterial(Config.VertexColorMaterial, TEXT("de color por vértice"));
	Emissive = LoadMaterial(Config.EmissiveMaterial, TEXT("emisivo"));
	Dim = LoadMaterial(Config.DimMaterial, TEXT("tenue"));
	EmissiveSlotTag = Config.EmissiveSlotTag;
	DimSlotTag = Config.DimSlotTag;
	if (Missing > 0)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: faltan %d piezas o materiales del kit; %s."), Missing, WhenMissing);
	}
	return Missing;
}

UStaticMesh* FBoardKit::Mesh(EBoardKitPiece Piece) const
{
	const int32 Index = static_cast<int32>(Piece);
	return Meshes.IsValidIndex(Index) ? Meshes[Index].Get() : nullptr;
}

void FBoardKit::ApplyMaterials(UStaticMeshComponent& Component) const
{
	const UStaticMesh* StaticMesh = Component.GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}
	const TArray<FStaticMaterial>& Slots = StaticMesh->GetStaticMaterials();
	for (int32 Index = 0; Index < FMath::Max(1, Slots.Num()); ++Index)
	{
		const FString Slot = Slots.IsValidIndex(Index) ? Slots[Index].MaterialSlotName.ToString() : FString();
		UMaterialInterface* Material = !EmissiveSlotTag.IsEmpty() && Slot.Contains(EmissiveSlotTag) ? Emissive.Get()
			: !DimSlotTag.IsEmpty() && Slot.Contains(DimSlotTag) ? Dim.Get()
			: VertexColor.Get();
		if (Material)
		{
			Component.SetMaterial(Index, Material);
		}
	}
}

UStaticMeshComponent* FBoardKit::AddPiece(AActor& Owner, UStaticMesh* StaticMesh, const FVector& Location, float Yaw, bool bVisible, EObjectFlags Flags) const
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(&Owner, NAME_None, Flags);
	Component->SetStaticMesh(StaticMesh);
	Component->SetupAttachment(Owner.GetRootComponent());
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Las piezas del kit no se escalan (R-VIS-1), salvo el suelo de la cuña (PlaceFixed): un metro del kit son 100 cm.
	Component->SetRelativeLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
	Component->SetVisibility(bVisible);
	ApplyMaterials(*Component);
	Component->RegisterComponent();
	return Component;
}

void FBoardKit::PlaceFixed(AActor& Owner, const FBoardState& Board, const FBoardLayout& Layout, FBoardKitFixedPieces& Out, EObjectFlags Flags) const
{
	// R-VIS-2: la peana y el suelo liso de cada Dominio, girados a su sector, con el centro en el Altar y a la escala del suelo.
	for (const FDomainWedgeLayout& Wedge : Layout.Wedges)
	{
		for (const EBoardKitPiece Piece : {EBoardKitPiece::Pedestal, EBoardKitPiece::PlainFloor})
		{
			if (UStaticMesh* StaticMesh = Mesh(Piece))
			{
				UStaticMeshComponent* Floor = AddPiece(Owner, StaticMesh, FVector::ZeroVector, Wedge.Yaw, true, Flags);
				// R-VIS-1: el suelo de la cuña se escala en X e Y al radio de la mesa; el resto del kit, no.
				Floor->SetRelativeScale3D(FVector(Layout.KitScale, Layout.KitScale, 1.f));
				Out.Fixed.Add(Floor);
				Out.WedgeFloors.Add(Floor);
				Out.WedgeFloorDomains.Add(Wedge.DomainId);
			}
		}
	}

	// R-VIS-3: una losa por casilla, una malla instanciada por pieza; el Altar es su pieza propia.
	TMap<EBoardKitPiece, UInstancedStaticMeshComponent*> Instances;
	auto InstancesOf = [this, &Owner, &Out, &Instances, Flags](EBoardKitPiece Piece) -> UInstancedStaticMeshComponent*
	{
		if (UInstancedStaticMeshComponent** Found = Instances.Find(Piece))
		{
			return *Found;
		}
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(&Owner, NAME_None, Flags);
		Component->SetStaticMesh(Mesh(Piece));
		Component->SetupAttachment(Owner.GetRootComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ApplyMaterials(*Component);
		Component->RegisterComponent();
		Out.Instances.Add(Component);
		Instances.Add(Piece, Component);
		return Component;
	};
	for (const FTileState& Tile : Board.Tiles)
	{
		const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		const EBoardKitPiece Piece = SlabPiece(Placed.Slab);
		if (!Mesh(Piece))
		{
			continue; // sin pieza, la forma básica de la vista de partida hace de respaldo
		}
		if (Placed.Slab == EBoardSlab::Altar)
		{
			// R-VIS-1: el Altar crece con el kit, en X, Y y Z.
			UStaticMeshComponent* Altar = AddPiece(Owner, Mesh(Piece), Placed.Location, Placed.Yaw, true, Flags);
			Altar->SetRelativeScale3D(FVector(Layout.KitScale));
			Out.Fixed.Add(Altar);
		}
		else
		{
			InstancesOf(Piece)->AddInstance(FTransform(FRotator(0.f, Placed.Yaw, 0.f), Placed.Location), false);
		}
	}

	// R-VIS-4: los tramos de sendero de cada conexión, en una sola malla instanciada.
	if (Mesh(EBoardKitPiece::PathSegment))
	{
		UInstancedStaticMeshComponent* Paths = InstancesOf(EBoardKitPiece::PathSegment);
		for (const FPathSegmentLayout& Segment : Layout.PathSegments)
		{
			Paths->AddInstance(FTransform(FRotator(0.f, Segment.Yaw, 0.f), Segment.Location), false);
		}
	}
}

void FBoardKit::PlaceStructures(AActor& Owner, const FBoardState& Board, const FBoardLayout& Layout, const FBoardLayoutParams& Params,
	const FBoardKitStructureOptions& Options, FBoardKitStructures& Out, EObjectFlags Flags) const
{
	// R-VIS-5: estructuras, runas flotantes y cadenas.
	Out.Structures.Init(nullptr, Board.Tiles.Num());
	Out.FloatingRunes.Init(nullptr, Board.Tiles.Num());
	Out.SealChains.Init(nullptr, Board.Tiles.Num());
	for (const FTileState& Tile : Board.Tiles)
	{
		const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		if (UStaticMesh* StructureMesh = Mesh(StructurePiece(Placed.Structure)))
		{
			Out.Structures[Tile.TileId] = AddPiece(Owner, StructureMesh, Placed.StructureLocation, Placed.StructureYaw, Options.bVisible, Flags);
		}
		if (Options.bFloatingRunes && Placed.Structure == EBoardStructure::Pylon && Mesh(EBoardKitPiece::FloatingRune))
		{
			Out.FloatingRunes[Tile.TileId] = AddPiece(Owner, Mesh(EBoardKitPiece::FloatingRune), Placed.StructureLocation + FVector(0.f, 0.f, Params.FloatingRuneHeight),
				Placed.StructureYaw, Options.bVisible, Flags);
		}
		// El Altar no se sella (glosario "Sello"); las cadenas, sobre la cara superior de la losa.
		if (Options.bSealChains && Tile.Type != ETileType::Altar && Mesh(EBoardKitPiece::SealChains))
		{
			Out.SealChains[Tile.TileId] = AddPiece(Owner, Mesh(EBoardKitPiece::SealChains), Placed.Location + FVector(0.f, 0.f, Params.SlabTop), Placed.Yaw, Options.bVisible, Flags);
		}
	}
}

void FBoardKit::TintWedgeFloors(const FBoardKitFixedPieces& Pieces, const TArray<bool>& Forgotten, const ABoardViewActor& Config) const
{
	for (int32 Index = 0; Index < Pieces.WedgeFloors.Num(); ++Index)
	{
		const int32 DomainId = Pieces.WedgeFloorDomains[Index];
		UStaticMeshComponent* Floor = Pieces.WedgeFloors[Index];
		if (Floor && Forgotten.IsValidIndex(DomainId))
		{
			const bool bForgotten = Forgotten[DomainId];
			Soulbound::Presentation::SetComponentColor(*Floor, VertexColor.Get(),
				Soulbound::Presentation::WedgeFloorTint(Config.DomainColor(DomainId), bForgotten, Config.FloorTintStrength, Config.ForgottenDim));
		}
	}
}

void FBoardKit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Meshes);
	Collector.AddReferencedObject(VertexColor);
	Collector.AddReferencedObject(Emissive);
	Collector.AddReferencedObject(Dim);
}
