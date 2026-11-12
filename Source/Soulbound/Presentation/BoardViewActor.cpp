#include "BoardViewActor.h"

#include "../Match/MatchHostSubsystem.h"
#include "../Profile/PlayerProfileSubsystem.h"
#include "../Spike/SpikeOnlinePlayerController.h"
#include "../Warden/WardenHostSubsystem.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Subsystem/A27OnlineSubsystem.h"
#include "BoardTileActor.h"
#include "BoardViewState.h"
#include "ChroniclePanel.h"
#include "DecisionPanel.h"
#include "EventText.h"
#include "GrimoirePanel.h"
#include "InstructionTracker.h"
#include "LegendPanel.h"
#include "StatusPanel.h"
#include "StatusText.h"
#include "Match/BondRaceRules.h"
#include "Match/RuneRules.h"
#include "Match/VictoryRules.h"
#include "Match/WardenEventRules.h"
#include "PresentationShapes.h"
#include "RingPanel.h"
#include "Warden/WardenBrain.h"
#include "SeatTokenActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Framework/Application/IInputProcessor.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

// S-027: los textos de asiento que usan la leyenda y las etiquetas viven en StatusText (antes, en el espacio anónimo de este archivo).
using Soulbound::Presentation::BracerText;
using Soulbound::Presentation::DomainTitle;
using Soulbound::Presentation::EffectStatusText;
using Soulbound::Presentation::ElectName;
using Soulbound::Presentation::VoidText;

namespace
{
	/**
	 * Entrada del anillo (S-026): preprocesador de Slate que fecha cada clic izquierdo con el reloj real en cuanto Slate lo recibe, antes
	 * del fotograma del juego (R-MIN-18; Unreal no da la hora del mensaje del sistema, así que es lo más temprano que se puede fechar), y
	 * se lo pasa al anfitrión. Nunca consume el clic (condición del autor del 06/11/2026). Con el juego en pausa no pulsa: el menú de
	 * pausa recibe sus clics y los pulsos del anillo, que corren en tiempo real, se pierden.
	 */
	class FRingInputProcessor final : public IInputProcessor
	{
	public:
		explicit FRingInputProcessor(TWeakObjectPtr<UMatchHostSubsystem> InHost) : Host(InHost) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			Press(MouseEvent);
			return false;
		}

		// El segundo clic de dos rápidos llega como doble clic, no como pulsación: también cuenta.
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			Press(MouseEvent);
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("SoulboundRingInput"); }

	private:
		void Press(const FPointerEvent& MouseEvent) const
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return;
			}
			const double PressSeconds = FPlatformTime::Seconds();
			UMatchHostSubsystem* HostPtr = Host.Get();
			const UWorld* World = HostPtr ? HostPtr->GetWorld() : nullptr;
			if (!HostPtr || (World && World->IsPaused()))
			{
				return;
			}
			HostPtr->PressRing(PressSeconds);
		}

		TWeakObjectPtr<UMatchHostSubsystem> Host;
	};

	/** Entradas del anillo registradas en Slate ahora, en todo el proceso: el log lo dice para comprobar que ninguna queda colgada. */
	int32 LiveRingInputs = 0;

	// Medidas de las formas de respaldo que no son parámetros (de S-005 a S-007), por 6,3/175 desde S-019: la mesa mide lo
	// que mide el kit. El grosor de la losa es LayoutParams.SlabTop.
	constexpr float EdgeThickness = 0.15f;
	/** Alto de las marcas de tipo sobre la losa. */
	constexpr float InlayHeight = 0.22f;
	/** Holgura de los textos sobre las marcas. */
	constexpr float TextClearance = 0.1f;
	/** Altura y alto del halo de alcanzable y del aro de Olvidado, bajo la losa. */
	constexpr float HaloZ = 0.07f;
	constexpr float HaloHeight = 0.15f;
	constexpr float ForgottenRingZ = 0.1f;
	constexpr float ForgottenRingHeight = 0.22f;
	/** Segmentos del aro de un Nicho reclamado. */
	constexpr int32 NicheRingSegmentCount = 24;

	/** Ruta inicial de una pieza o un material del kit, según kit.json (R-VIS-8). */
	FSoftObjectPath KitPath(const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/Art/Tablero/%s.%s"), Name, Name));
	}

	/** Atenuado de una casilla de un Dominio Olvidado: mismo tono, más oscuro. */
	FLinearColor Dimmed(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor);
	}

	/** Marca de tiempo de la crónica: minutos y segundos del reloj de partida. */
	FString Stamp(double Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	/** Efecto de un Nodo de Poder para su etiqueta (S-009); vacío si no es un Nodo. */
	const TCHAR* NodeEffectName(EPowerNodeEffect Effect)
	{
		switch (Effect)
		{
		case EPowerNodeEffect::Duel: return TEXT("DUELO");
		case EPowerNodeEffect::Oracle: return TEXT("ORÁCULO");
		case EPowerNodeEffect::Planar: return TEXT("PLANAR");
		case EPowerNodeEffect::None:
		default: return TEXT("");
		}
	}

	/**
	 * Etiqueta de una casilla: identificador y Dominio ("D3·41"), con el prefijo del tipo; el Altar, "ALTAR". Un Nodo de
	 * Poder enseña su efecto debajo desde el Despertar (R-TAB-9, S-009); antes, no.
	 */
	FString TileLabelText(const FBoardState& Board, const FTileState& Tile)
	{
		const FString Plain = Soulbound::Presentation::TileShortName(Board, Tile.TileId);
		switch (Tile.Type)
		{
		case ETileType::Niche: return TEXT("N ") + Plain;
		case ETileType::PowerSource:
			// La reserva no va en la etiqueta: tiene su propio contador azul sobre la Fuente (S-008).
			return TEXT("F ") + Plain;
		case ETileType::PowerNode:
			return Board.AreNodesRevealed() ? FString::Printf(TEXT("X %s\n%s"), *Plain, NodeEffectName(Tile.PowerNodeEffect)) : TEXT("X ") + Plain;
		case ETileType::Altar:
		case ETileType::Normal:
		default: return Plain;
		}
	}

}

ABoardViewActor::ABoardViewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(Soulbound::Presentation::CubePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(Soulbound::Presentation::CylinderPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(Soulbound::Presentation::SpherePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	SphereMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;

	// Piezas y materiales del kit (S-019, R-VIS-8), con los nombres de kit.json. Config los sustituye si DefaultGame.ini los trae.
	SlabMesh = KitPath(TEXT("SM_losa"));
	CrackedSlabMesh = KitPath(TEXT("SM_losa_partida"));
	NicheSlabMesh = KitPath(TEXT("SM_losa_nicho"));
	SourceSlabMesh = KitPath(TEXT("SM_losa_fuente"));
	NodeSlabMesh = KitPath(TEXT("SM_losa_nodo"));
	PathSegmentMesh = KitPath(TEXT("SM_sendero"));
	PedestalMesh = KitPath(TEXT("SM_peana"));
	PlainFloorMesh = KitPath(TEXT("SM_suelo_liso"));
	AltarMesh = KitPath(TEXT("SM_altar"));
	NicheThroneMesh = KitPath(TEXT("SM_trono"));
	PylonMesh = KitPath(TEXT("SM_pilon"));
	FloatingRuneMesh = KitPath(TEXT("SM_runa_flotante"));
	PlanarArchMesh = KitPath(TEXT("SM_nodo_planar"));
	DuelSwordsMesh = KitPath(TEXT("SM_nodo_duelo"));
	OracleOrbMesh = KitPath(TEXT("SM_nodo_oraculo"));
	SealChainsMesh = KitPath(TEXT("SM_sello"));
	TokenCardMesh = KitPath(TEXT("SM_ficha_prueba"));
	VertexColorMaterial = KitPath(TEXT("M_ColorVertice"));
	EmissiveMaterial = KitPath(TEXT("M_Emisivo"));
	DimMaterial = KitPath(TEXT("M_Tenue"));

	// Seis tonos a 60 grados de tono entre sí, del mismo brillo medio, para distinguirse a un metro.
	DomainColors = {
		FLinearColor(0.03f, 0.15f, 0.90f), // 0 azul
		FLinearColor(0.85f, 0.03f, 0.03f), // 1 rojo
		FLinearColor(0.04f, 0.60f, 0.08f), // 2 verde
		FLinearColor(0.75f, 0.05f, 0.60f), // 3 magenta
		FLinearColor(0.03f, 0.60f, 0.70f), // 4 cian
		FLinearColor(0.80f, 0.65f, 0.02f), // 5 amarillo
	};
	// Claros y algo lavados: la ficha se distingue de la casilla saturada que pisa.
	SeatColors = {
		FLinearColor(0.40f, 0.70f, 1.00f), // 0 celeste
		FLinearColor(1.00f, 0.55f, 0.75f), // 1 rosa
		FLinearColor(0.60f, 1.00f, 0.40f), // 2 lima
		FLinearColor(1.00f, 0.95f, 0.60f), // 3 crema
	};
}

FLinearColor ABoardViewActor::DomainColor(int32 DomainId) const
{
	return DomainColors.IsValidIndex(DomainId) ? DomainColors[DomainId] : FLinearColor(0.1f, 0.1f, 0.1f);
}

FLinearColor ABoardViewActor::SeatColor(int32 Seat) const
{
	return SeatColors.IsValidIndex(Seat) ? SeatColors[Seat] : FLinearColor::White;
}

void ABoardViewActor::Initialize(UMatchHostSubsystem* InHost)
{
	Host = InHost;
	if (!InHost || !InHost->HasMatch())
	{
		return;
	}

	// S-019: la colocación es una función pura del tablero; la vista solo la lee.
	Kit.Load(*this, TEXT("se usa la forma básica"));
	Layout = Soulbound::Presentation::ComputeBoardLayout(InHost->GetState().Board, LayoutParams);
	if (!Layout.bRadial)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista del tablero: el trazado no es radial (dos Dominios en el mismo sector o uno repartido en más de 60 grados); las cuñas van de 60 en 60 por identificador de Dominio."));
	}
	TilePositions.Reset(Layout.Tiles.Num());
	for (const FTileLayout& Tile : Layout.Tiles)
	{
		TilePositions.Add(Tile.Location);
	}
	// S-022 (R-CAM-1): la partida arranca sentada, o cenital con bStartSeated=false; la tecla de la vista alterna como siempre.
	bSeatedView = bStartSeated;
	BuildEdges();
	BuildTiles();
	BuildKit();
	BuildTokens();
	BuildCamera();
	BuildLighting();
	BuildOverlay();
	Refresh();

	ChangedHandle = InHost->OnMatchStateChanged.AddUObject(this, &ABoardViewActor::Refresh);
	// S-026: el anillo del humano; la entrada se registra al abrirlo y se retira al cerrarlo.
	RingOpenedHandle = InHost->OnRingOpened.AddUObject(this, &ABoardViewActor::HandleRingOpened);
	RingClosedHandle = InHost->OnRingClosed.AddUObject(this, &ABoardViewActor::HandleRingClosed);
	GetWorldTimerManager().SetTimer(HudTimer, this, &ABoardViewActor::TickHud, FMath::Max(HudIntervalSeconds, 0.05f), true, 0.f);
}

void ABoardViewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMatchHostSubsystem* HostPtr = Host.Get())
	{
		HostPtr->OnMatchStateChanged.Remove(ChangedHandle);
		HostPtr->OnRingOpened.Remove(RingOpenedHandle);
		HostPtr->OnRingClosed.Remove(RingClosedHandle);
	}
	// S-026: ninguna entrada del anillo sobrevive a la vista (fin de PIE, viaje al menú), haya o no anillo abierto.
	UnregisterRingInput();
	GetWorldTimerManager().ClearTimer(HudTimer);
	if (Overlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
			}
		}
		Overlay.Reset();
	}
	if (WardenLineOverlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(WardenLineOverlay.ToSharedRef());
			}
		}
		WardenLineOverlay.Reset();
	}
	if (ReturnToMenuOverlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(ReturnToMenuOverlay.ToSharedRef());
			}
		}
		ReturnToMenuOverlay.Reset();
	}
	if (RingOverlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(RingOverlay.ToSharedRef());
			}
		}
		RingOverlay.Reset();
	}
	// S-027: el panel de estado (con el de depuración) y el Grimorio.
	for (TSharedPtr<SWidget>* Layer : {&StatusOverlay, &GrimoireOverlay})
	{
		UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
		if (Layer->IsValid() && Viewport)
		{
			Viewport->RemoveViewportWidgetContent(Layer->ToSharedRef());
		}
		Layer->Reset();
	}
	Super::EndPlay(EndPlayReason);
}

const TSoftObjectPtr<UStaticMesh>& ABoardViewActor::KitReference(EBoardKitPiece Piece) const
{
	switch (Piece)
	{
	case EBoardKitPiece::Slab: return SlabMesh;
	case EBoardKitPiece::CrackedSlab: return CrackedSlabMesh;
	case EBoardKitPiece::NicheSlab: return NicheSlabMesh;
	case EBoardKitPiece::SourceSlab: return SourceSlabMesh;
	case EBoardKitPiece::NodeSlab: return NodeSlabMesh;
	case EBoardKitPiece::PathSegment: return PathSegmentMesh;
	case EBoardKitPiece::Pedestal: return PedestalMesh;
	case EBoardKitPiece::PlainFloor: return PlainFloorMesh;
	case EBoardKitPiece::Altar: return AltarMesh;
	case EBoardKitPiece::NicheThrone: return NicheThroneMesh;
	case EBoardKitPiece::Pylon: return PylonMesh;
	case EBoardKitPiece::FloatingRune: return FloatingRuneMesh;
	case EBoardKitPiece::PlanarArch: return PlanarArchMesh;
	case EBoardKitPiece::DuelSwords: return DuelSwordsMesh;
	case EBoardKitPiece::OracleOrb: return OracleOrbMesh;
	case EBoardKitPiece::SealChains: return SealChainsMesh;
	case EBoardKitPiece::TokenCard:
	default: return TokenCardMesh;
	}
}

void ABoardViewActor::BuildKit()
{
	// R-VIS-2 a 5: la colocación es del kit, la misma de la vista previa; aquí las estructuras salen ocultas hasta que el estado de vista las pide (RefreshKit).
	const FBoardState& Board = Host->GetState().Board;
	Kit.PlaceFixed(*this, Board, Layout, KitFixed);
	Kit.PlaceStructures(*this, Board, Layout, LayoutParams, FBoardKitStructureOptions(), KitStructures);
}

void ABoardViewActor::BuildEdges()
{
	// S-019: con la pieza del sendero, los tramos del kit (BuildKit); la tira básica es el respaldo.
	if (Kit.Mesh(EBoardKitPiece::PathSegment))
	{
		return;
	}
	const FBoardState& Board = Host->GetState().Board;
	for (const FTileState& Tile : Board.Tiles)
	{
		for (const int32 Other : Tile.Neighbours)
		{
			if (Other <= Tile.TileId)
			{
				continue; // cada conexión una sola vez
			}
			const FVector A = TilePositions[Tile.TileId];
			const FVector B = TilePositions[Other];
			const FVector Delta = B - A;

			UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(this);
			Strip->SetStaticMesh(CubeMesh);
			Strip->SetupAttachment(GetRootComponent());
			Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Strip->SetCastShadow(false);
			Strip->RegisterComponent();
			// Una forma básica estirada entre las dos casillas (el cubo del motor mide 100 cm).
			Strip->SetWorldLocation((A + B) * 0.5f + FVector(0.f, 0.f, EdgeThickness * 0.5f));
			Strip->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.f));
			Strip->SetWorldScale3D(FVector(Delta.Size() / 100.f, EdgeWidth / 100.f, EdgeThickness / 100.f));
			Soulbound::Presentation::SetComponentColor(*Strip, BaseMaterial, FLinearColor(0.04f, 0.04f, 0.05f));
			EdgeStrips.Add(Strip);
		}
	}
}

void ABoardViewActor::BuildTiles()
{
	const FBoardState& Board = Host->GetState().Board;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TileTops.Init(LayoutParams.SlabTop, Board.Tiles.Num());
	TileHasKitSlab.Init(false, Board.Tiles.Num());
	Inlays.Init(nullptr, Board.Tiles.Num());
	InlayOutlines.Init(nullptr, Board.Tiles.Num());
	InlayOutlinesB.Init(nullptr, Board.Tiles.Num());
	Halos.Init(nullptr, Board.Tiles.Num());
	ForgottenRings.Init(nullptr, Board.Tiles.Num());
	NicheNames.Init(nullptr, Board.Tiles.Num());
	NicheNumbers.Init(nullptr, Board.Tiles.Num());
	SourceReserves.Init(nullptr, Board.Tiles.Num());
	NicheRingFirst.Init(INDEX_NONE, Board.Tiles.Num());
	InlayHoles.Init(nullptr, Board.Tiles.Num());
	Labels.Init(nullptr, Board.Tiles.Num());

	// Las marcas y las etiquetas son componentes de la vista, en coordenadas del mundo: la losa se aplasta y se
	// escala, y lo que colgara de ella se deformaría con ella.
	auto AddShape = [this](UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Size)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->SetupAttachment(GetRootComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->RegisterComponent();
		Component->SetWorldLocation(Location);
		Component->SetWorldRotation(Rotation);
		Component->SetWorldScale3D(Size / 100.f);
		return Component;
	};

	for (const FTileState& Tile : Board.Tiles)
	{
		const FVector Position = TilePositions[Tile.TileId];
		const bool bAltar = Tile.Type == ETileType::Altar;
		// S-019: si está la pieza de la losa (o del Altar), la forma básica queda oculta y solo recibe el clic.
		UStaticMesh* KitSlab = Layout.Tiles.IsValidIndex(Tile.TileId) ? Kit.Mesh(FBoardKit::SlabPiece(Layout.Tiles[Tile.TileId].Slab)) : nullptr;
		TileHasKitSlab[Tile.TileId] = KitSlab != nullptr;
		// La cara superior es la cota de la ficha (R-VIS-3): la de la losa del kit, o el alto de la pieza del Altar.
		float Thickness = bAltar ? AltarThickness : LayoutParams.SlabTop;
		if (bAltar && KitSlab)
		{
			// De los datos (AltarTop * KitScale), no de la malla: sus dimensiones llegan a la punta de la aguja.
			Thickness = Layout.Tiles[Tile.TileId].Top;
		}
		const float Diameter = bAltar ? Layout.AltarApothem * 2.f : SlabSize();
		TileTops[Tile.TileId] = Thickness;

		// El tipo se ve por la silueta: Nicho cuadrado, Nodo en rombo, Altar grande y alto, el resto redondo.
		FRotator Rotation = FRotator::ZeroRotator;
		float Width = Diameter;
		if (Tile.Type == ETileType::PowerNode)
		{
			Rotation = FRotator(0.f, 45.f, 0.f);
			Width = Diameter * 0.8f;
		}
		else if (Tile.Type == ETileType::Niche)
		{
			Width = Diameter * 0.9f;
		}
		// El cilindro del motor mide 100 cm de diámetro y 100 de alto: se aplasta a una losa.
		const FTransform Transform(Rotation, Position + FVector(0.f, 0.f, Thickness * 0.5f), FVector(Width / 100.f, Width / 100.f, Thickness / 100.f));
		ABoardTileActor* Actor = GetWorld()->SpawnActor<ABoardTileActor>(ABoardTileActor::StaticClass(), Transform, Params);
		if (Actor)
		{
			Actor->SetTileId(Tile.TileId);
			Actor->SetSquareShape(Tile.Type == ETileType::Niche || Tile.Type == ETileType::PowerNode);
			Actor->SetKitSlabShown(KitSlab != nullptr);
			Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
		TileActors.Add(Actor);

		// Halo de "alcanzable": un disco por debajo de la losa, más ancho, que solo se ve en el borde. La losa
		// conserva su color; el halo se enciende cuando la tirada la permite.
		// R-DEC-12 (S-033): el halo asoma por el borde de la pieza del kit también en el Altar, que es mayor y hexagonal (la mayor de X e Y).
		FVector PieceSize = FVector::ZeroVector;
		if (bAltar && KitSlab)
		{
			PieceSize = KitSlab->GetBounds().BoxExtent * 2.f * Layout.KitScale;
		}
		const float HaloWidth = Soulbound::Presentation::HaloDiameter(Diameter, HaloScale, static_cast<float>(PieceSize.X), static_cast<float>(PieceSize.Y));
		UStaticMeshComponent* Halo = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, HaloZ), FRotator::ZeroRotator, FVector(HaloWidth, HaloWidth, HaloHeight));
		Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, ReachableColor);
		Halo->SetVisibility(false);
		Halos[Tile.TileId] = Halo;

		// Aro de Dominio Olvidado: más alto que el halo, para que se vea también en una casilla alcanzable.
		UStaticMeshComponent* Ring = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, ForgottenRingZ), FRotator::ZeroRotator,
			FVector(Diameter * ForgottenRingScale, Diameter * ForgottenRingScale, ForgottenRingHeight));
		Soulbound::Presentation::SetComponentColor(*Ring, BaseMaterial, ForgottenRingColor);
		Ring->SetVisibility(false);
		ForgottenRings[Tile.TileId] = Ring;

		// Marca del tipo sobre la losa, del tamaño justo para no tapar la ficha del centro, con contorno negro
		// para que se distinga sobre cualquier Dominio. Con la losa del kit no hace falta: la pieza ya dice el tipo.
		const float Inlay = SlabSize() * InlayScale;
		const float Outline = Inlay * OutlineScale;
		const FVector OnTop = Position + FVector(0.f, 0.f, Thickness);
		const FVector Mid = OnTop + FVector(0.f, 0.f, InlayHeight * 0.5f);
		switch (KitSlab ? ETileType::Normal : Tile.Type)
		{
		case ETileType::Niche:
			InlayOutlines[Tile.TileId] = AddShape(CubeMesh, OnTop + FVector(0.f, 0.f, InlayHeight * 0.4f), FRotator::ZeroRotator, FVector(Outline, Outline, InlayHeight * 0.8f));
			Inlays[Tile.TileId] = AddShape(CubeMesh, Mid, FRotator::ZeroRotator, FVector(Inlay, Inlay, InlayHeight));
			break;
		case ETileType::PowerSource:
		{
			// Cruz: dos brazos cruzados, con contorno negro. No se parece al rombo con esfera del Nodo.
			const float Arm = Inlay * CrossArmThickness;
			const float OutlineArm = Arm + (Outline - Inlay);
			const FVector LowMid = OnTop + FVector(0.f, 0.f, InlayHeight * 0.4f);
			InlayOutlines[Tile.TileId] = AddShape(CubeMesh, LowMid, FRotator::ZeroRotator, FVector(Outline, OutlineArm, InlayHeight * 0.8f));
			InlayOutlinesB[Tile.TileId] = AddShape(CubeMesh, LowMid, FRotator::ZeroRotator, FVector(OutlineArm, Outline, InlayHeight * 0.8f));
			Inlays[Tile.TileId] = AddShape(CubeMesh, Mid, FRotator::ZeroRotator, FVector(Inlay, Arm, InlayHeight));
			InlayHoles[Tile.TileId] = AddShape(CubeMesh, Mid, FRotator::ZeroRotator, FVector(Arm, Inlay, InlayHeight));
			break;
		}
		case ETileType::PowerNode:
			InlayOutlines[Tile.TileId] = AddShape(SphereMesh, OnTop + FVector(0.f, 0.f, InlayHeight * 0.4f), FRotator::ZeroRotator, FVector(Outline, Outline, InlayHeight * 1.6f));
			Inlays[Tile.TileId] = AddShape(SphereMesh, Mid, FRotator::ZeroRotator, FVector(Inlay, Inlay, InlayHeight * 2.f));
			break;
		default:
			break;
		}
		if (UStaticMeshComponent* OutlineMesh = InlayOutlines[Tile.TileId].Get())
		{
			Soulbound::Presentation::SetComponentColor(*OutlineMesh, BaseMaterial, OutlineColor);
		}
		if (UStaticMeshComponent* OutlineMeshB = InlayOutlinesB[Tile.TileId].Get())
		{
			Soulbound::Presentation::SetComponentColor(*OutlineMeshB, BaseMaterial, OutlineColor);
		}

		// Etiqueta plana sobre la losa, en la franja de abajo (la parte baja de la pantalla), orientada a la cámara cenital.
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(GetRootComponent());
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCastShadow(false);
		Label->RegisterComponent();
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(LabelSize);
		Label->SetTextRenderColor(FColor::White);
		// El texto del motor se ve desde +X: tumbado con el frente hacia arriba (pitch 90) y la cabeza hacia +X, arriba
		// de la pantalla (yaw 180). Con pitch -90 se veía por detrás, como en un espejo.
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetWorldLocation(OnTop + FVector(-Diameter * LabelOffsetFraction, 0.f, InlayHeight + TextClearance));
		Label->SetText(FText::FromString(TileLabelText(Board, Tile)));
		Labels[Tile.TileId] = Label;

		if (Tile.Type == ETileType::PowerSource)
		{
			// Contador de reserva en el centro de la Fuente, en azul: se distingue de la etiqueta blanca (S-008).
			UTextRenderComponent* Reserve = NewObject<UTextRenderComponent>(this);
			Reserve->SetupAttachment(GetRootComponent());
			Reserve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Reserve->SetCastShadow(false);
			Reserve->RegisterComponent();
			Reserve->SetHorizontalAlignment(EHTA_Center);
			Reserve->SetVerticalAlignment(EVRTA_TextCenter);
			Reserve->SetWorldSize(NicheNumberSize);
			Reserve->SetTextRenderColor(SourceReserveColor);
			Reserve->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
			Reserve->SetWorldLocation(OnTop + FVector(0.f, 0.f, InlayHeight + TextClearance));
			Reserve->SetText(FText::GetEmpty());
			SourceReserves[Tile.TileId] = Reserve;
		}

		if (Tile.Type == ETileType::Niche)
		{
			// Nombre del Elegido (de elegidos.json) arriba de la casilla; número del asiento sobre el cubo.
			auto AddText = [this, &OnTop](float Size, const FVector& Offset)
			{
				UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
				Text->SetupAttachment(GetRootComponent());
				Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Text->SetCastShadow(false);
				Text->RegisterComponent();
				Text->SetHorizontalAlignment(EHTA_Center);
				Text->SetVerticalAlignment(EVRTA_TextCenter);
				Text->SetWorldSize(Size);
				Text->SetTextRenderColor(FColor::White);
				Text->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
				Text->SetWorldLocation(OnTop + Offset);
				return Text;
			};
			NicheNames[Tile.TileId] = AddText(NicheNameSize, FVector(Diameter * NicheNameOffsetFraction, 0.f, InlayHeight + TextClearance));
			NicheNames[Tile.TileId]->SetText(FText::FromString(ElectName(Host->GetState(), Tile.DomainId)));
			NicheNumbers[Tile.TileId] = AddText(NicheNumberSize, FVector(0.f, 0.f, InlayHeight + TextClearance));
			NicheNumbers[Tile.TileId]->SetText(FText::GetEmpty());

			// Aro del asiento: segmentos en círculo alrededor de la casilla, apagados hasta que se reclama.
			const float RingRadius = SlabSize() * NicheRingRadiusScale;
			const float SegmentLength = 2.f * PI * RingRadius / NicheRingSegmentCount * 1.15f;
			NicheRingFirst[Tile.TileId] = NicheRingSegments.Num();
			for (int32 Index = 0; Index < NicheRingSegmentCount; ++Index)
			{
				const float Angle = 2.f * PI * Index / NicheRingSegmentCount;
				UStaticMeshComponent* Segment = AddShape(CubeMesh, Position + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, ForgottenRingZ),
					FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f), FVector(SegmentLength, NicheRingWidth, ForgottenRingHeight));
				Soulbound::Presentation::SetComponentColor(*Segment, BaseMaterial, FLinearColor::White);
				Segment->SetVisibility(false);
				NicheRingSegments.Add(Segment);
			}
		}
	}
}

void ABoardViewActor::BuildTokens()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UStaticMesh* Card = Kit.Mesh(EBoardKitPiece::TokenCard);
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		ASeatTokenActor* Token = GetWorld()->SpawnActor<ASeatTokenActor>(ASeatTokenActor::StaticClass(), FTransform::Identity, Params);
		if (Token)
		{
			Token->SetSeat(Seat);
			Token->Configure(TokenLabelSize, ActiveLift, ActiveSpinDegreesPerSecond, RingDiameter, RingWidth, ActiveScale, TokenDiameter);
			// R-VIS-6: la carta de prueba del kit, con el color del asiento en el aro de su base (sin carta, la esfera o el cono).
			if (UStaticMeshComponent* CardMesh = Token->UseCard(Card, Kit.EmissiveMaterial(), TokenRingScale, TokenRingWidth, EchoRingBrightness))
			{
				Kit.ApplyMaterials(*CardMesh);
			}
			Token->SetColor(SeatColor(Seat));
			Token->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Token->OnTileEntered.BindUObject(this, &ABoardViewActor::OnTokenTileEntered);
		}
		TokenActors.Add(Token);
	}

	// S-017: el orbe de la consulta del Oráculo, una esfera que solo existe para verse y recibir el clic (vista de depuración, sin
	// asset: la presentación del Oráculo con su barra es de la interfaz, GDD 07).
	OracleOrb = NewObject<UStaticMeshComponent>(this);
	OracleOrb->SetStaticMesh(SphereMesh);
	OracleOrb->SetupAttachment(GetRootComponent());
	OracleOrb->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OracleOrb->SetCollisionResponseToAllChannels(ECR_Ignore);
	OracleOrb->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	OracleOrb->SetCastShadow(false);
	OracleOrb->RegisterComponent();
	OracleOrb->SetWorldScale3D(FVector(OracleOrbDiameter / 100.f));
	OracleOrb->SetVisibility(false);

	// S-018: los halos del aspirante, discos de formas básicas sin colisión bajo el Altar (la marca de la vista de depuración; el latido
	// del Altar y su zoom son de la interfaz). Uno por asiento, de su color; los coloca RefreshAspirantHalos.
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		UStaticMeshComponent* Halo = NewObject<UStaticMeshComponent>(this);
		Halo->SetStaticMesh(CylinderMesh);
		Halo->SetupAttachment(GetRootComponent());
		Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Halo->SetCastShadow(false);
		Halo->RegisterComponent();
		Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, SeatColor(Seat));
		Halo->SetVisibility(false);
		AspirantHalos.Add(Halo);
	}
}

bool ABoardViewActor::IsOracleOrb(const UPrimitiveComponent* Component) const
{
	return Component && Component == OracleOrb.Get() && OracleOrb->IsVisible();
}

void ABoardViewActor::RefreshOracleOrb(const FMatchState& State)
{
	if (!OracleOrb)
	{
		return;
	}
	const int32 Seat = State.Oracle.Consultant;
	if (!State.Oracle.IsWaiting() || !State.Seats.IsValidIndex(Seat))
	{
		OracleOrb->SetVisibility(false);
		return;
	}
	// Siempre sobre la ficha del consultante, también si consulta desde un Nodo (decisión del autor del 26/10/2026).
	const ASeatTokenActor* Token = TokenActors.IsValidIndex(Seat) ? TokenActors[Seat].Get() : nullptr;
	const FVector Base = Token ? Token->GetActorLocation() : TokenBase(State.Seats[Seat].TileId);
	OracleOrb->SetWorldLocation(Base + FVector(0.f, 0.f, OracleOrbHeight));
	Soulbound::Presentation::SetComponentColor(*OracleOrb, BaseMaterial, SeatColor(Seat));
	OracleOrb->SetVisibility(true);
}

void ABoardViewActor::BuildCamera()
{
	// R-VIS-9: la cenital de siempre, ajustada a la mesa, con el Altar en el centro.
	BoardCenter = FVector2D::ZeroVector;
	BoardHalf = FVector2D(LayoutParams.TableRadius);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator(-90.f, 0.f, 0.f), FVector(BoardCenter.X, BoardCenter.Y, 1000.0)), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(60.f);
		Camera->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	UpdateCamera();

	// La vista sentada del asiento humano. No es la cámara de la interfaz (R-CAM-1 a 5).
	SeatedCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	if (SeatedCamera)
	{
		SeatedCamera->GetCameraComponent()->SetFieldOfView(60.f);
		SeatedCamera->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	PlaceSeatedCamera();
}

void ABoardViewActor::PlaceSeatedCamera()
{
	if (!SeatedCamera)
	{
		return;
	}
	// Fuera del borde de la mesa, sobre ella, inclinada hacia abajo y girada al centro (R-VIS-9).
	const float Yaw = SeatedViewYawDegrees;
	const FVector2D Direction(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const FVector Location(Direction * (LayoutParams.TableRadius + SeatedViewDistance), SeatedViewHeight);
	SeatedCamera->SetActorLocationAndRotation(Location, FRotator(-SeatedViewPitchDegrees, Yaw + 180.f, 0.f));
}

void ABoardViewActor::SetSeatedView(float Height, float Distance, float PitchDegrees, TOptional<float> YawDegrees)
{
	SeatedViewHeight = Height;
	SeatedViewDistance = Distance;
	SeatedViewPitchDegrees = PitchDegrees;
	if (YawDegrees.IsSet())
	{
		SeatedViewYawDegrees = YawDegrees.GetValue();
	}
	PlaceSeatedCamera();
	bSeatedView = true;
	ApplyViewTarget();
}

FString ABoardViewActor::DescribeSeatedView() const
{
	return FString::Printf(TEXT("altura %.1f cm, distancia al borde %.1f cm, inclinación %.1f°, ángulo del asiento %.1f° (%s)"), SeatedViewHeight, SeatedViewDistance,
		SeatedViewPitchDegrees, SeatedViewYawDegrees, bSeatedView ? TEXT("activa") : TEXT("inactiva: tecla C"));
}

void ABoardViewActor::UpdateCamera()
{
	if (!Camera)
	{
		return;
	}
	FVector2D Viewport(1280.0, 720.0);
	if (const UGameViewportClient* Client = GetWorld()->GetGameViewport())
	{
		Client->GetViewportSize(Viewport);
	}
	if (Viewport.X < 1.0 || Viewport.Y < 1.0)
	{
		return;
	}
	LastViewportSize = Viewport;
	// S-027 (R-HUD-3): la letra y el ancho de los paneles siguen a la altura de la pantalla.
	UpdatePanelScale(Viewport);

	// Cámara cenital fija: el tablero entero, con el margen CameraMargin, en el hueco que dejan los paneles a la derecha.
	// Con FOV horizontal de 60 grados, a la altura H se ve media anchura H*tan(30); la media altura es esa entre el aspecto.
	constexpr float Fov = 60.f;
	const float Aspect = Viewport.X / Viewport.Y;
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	// Con los paneles ocultos (tecla H), el tablero usa todo el ancho. S-027: el ancho que ocupa de verdad la columna, en píxeles de pantalla.
	const float PanelPixels = bPanelsHidden ? 0.f : PanelScreenWidth;
	const float FreeFraction = 1.f - FMath::Clamp(PanelPixels / static_cast<float>(Viewport.X), 0.f, 0.6f);
	const float Height = CameraMargin * FMath::Max(BoardHalf.Y / (FreeFraction * HalfTan), BoardHalf.X * Aspect / HalfTan);
	// La pantalla crece hacia +Y a la derecha: para que el tablero quede a la izquierda, la cámara se corre hacia +Y.
	const float VisibleHalfWidth = Height * HalfTan;
	const float ShiftY = VisibleHalfWidth * (1.f - FreeFraction);
	Camera->SetActorLocation(FVector(BoardCenter.X, BoardCenter.Y + ShiftY, Height));
}

void ABoardViewActor::BuildLighting()
{
	// Exposición manual fija en un volumen sin límites, generado por código: el nivel del spike lleva la suya
	// automática y los colores salían pastel. Prioridad alta para ganar a cualquier otro volumen del mapa.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PostProcess = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	if (PostProcess)
	{
		PostProcess->bUnbound = true;
		PostProcess->Priority = 1000.f;
		PostProcess->Settings.bOverride_AutoExposureMethod = true;
		PostProcess->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PostProcess->Settings.bOverride_AutoExposureBias = true;
		PostProcess->Settings.AutoExposureBias = ExposureEV100;
	}

	// R-VIS-7: la lámpara sobre la mesa, un foco cenital, y un cielo tenue; sin sol ni cielo visible. Se crean en partida,
	// así que son luces móviles.
	// TODO(R-VIS-7): las luces estáticas (precalculadas, por la Steam Deck) llegan con la Cripta; hasta entonces, móviles.
	Lamp = NewObject<USpotLightComponent>(this);
	Lamp->SetupAttachment(GetRootComponent());
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, LampHeight), FRotator(-90.f, 0.f, 0.f));
	Lamp->SetIntensityUnits(ELightUnits::Candelas);
	Lamp->SetIntensity(LampIntensityCandelas);
	Lamp->SetInnerConeAngle(LampInnerConeDegrees);
	Lamp->SetOuterConeAngle(LampOuterConeDegrees);
	Lamp->RegisterComponent();

	SkyLight = NewObject<USkyLightComponent>(this);
	SkyLight->SetupAttachment(GetRootComponent());
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->SetIntensity(SkyLightIntensity);
	SkyLight->RegisterComponent();
	SkyLight->RecaptureSky();
}

void ABoardViewActor::BuildOverlay()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport)
	{
		return;
	}
	const FBoardState& Board = Host->GetState().Board;

	// La leyenda sale de los mismos colores que la vista: no pueden contradecirse.
	TArray<FLegendRow> Rows;
	FLegendRow Domains;
	Domains.Title = TEXT("Dominios y su Elegido (color de la casilla, D<n> en la etiqueta). Fuera = Olvidado");
	TWeakObjectPtr<UMatchHostSubsystem> WeakHost = Host;
	TWeakObjectPtr<ABoardViewActor> WeakView = this;
	for (int32 Id = 0; Id < Board.Domains.Num(); ++Id)
	{
		FLegendChip Chip;
		Chip.Color = DomainColor(Id);
		// Al día: fuera de la partida (Olvidado), en juego, o en juego con el asiento que reclamó su Nicho.
		Chip.TextGetter = [WeakHost, WeakView, Id]() -> FString
		{
			const UMatchHostSubsystem* HostPtr = WeakHost.Get();
			const ABoardViewActor* ViewPtr = WeakView.Get();
			if (!HostPtr || !ViewPtr || !HostPtr->HasMatch() || !HostPtr->GetState().Board.Domains.IsValidIndex(Id))
			{
				return FString::Printf(TEXT("D%d"), Id);
			}
			const FDomainState& Domain = HostPtr->GetState().Board.Domains[Id];
			// S-023: lo que pinta la vista, que en el prólogo va con la frase de corrupción.
			if (ViewPtr->IsDomainShownForgotten(Id))
			{
				return FString::Printf(TEXT("D%d %s: OLVIDADO (fuera)"), Id, *DomainTitle(HostPtr->GetState(), Id));
			}
			return Domain.OwnerSeat == INDEX_NONE
				? FString::Printf(TEXT("D%d %s: en juego"), Id, *DomainTitle(HostPtr->GetState(), Id))
				: FString::Printf(TEXT("D%d %s: en juego, asiento %d"), Id, *DomainTitle(HostPtr->GetState(), Id), Domain.OwnerSeat);
		};
		Domains.Chips.Add(Chip);
	}
	Rows.Add(Domains);
	Rows.Add({TEXT("Tipo = forma (etiqueta: ALTAR, N, F, X)"),
		{{AltarColor, TEXT("ALTAR: casilla grande y alta")},
			{NicheFreeColor, TEXT("N Nicho: casilla cuadrada con cubo")},
			{SourceColor, TEXT("F Fuente: casilla redonda con cruz")},
			{NodeColor, TEXT("X Nodo: rombo con esfera")}}});
	// S-019: con el kit, la losa de cada tipo y su estructura dicen el tipo (las formas básicas quedan donde falta la pieza).
	if (TileHasKitSlab.Contains(true))
	{
		Rows.Add({TEXT("Con el kit (color por vértice: sin color de Dominio en la losa)"),
			{{NicheFreeColor, TEXT("Nicho: losa con cabecera y trono")},
				{SourceColor, TEXT("Fuente: pilón, con runa mientras tiene reserva")},
				{NodeColor, TEXT("Nodo: losa con puntas; desde el Despertar, arco, espadas u orbe")},
				{SealedColor, TEXT("sellada: cadenas sobre la losa")}}});
	}
	Rows.Add({TEXT("Nicho (el cubo)"),
		{{NicheFreeColor, TEXT("Libre")}, {SeatColor(0), TEXT("Reclamado: aro y número del asiento")}, {NicheClosedColor, TEXT("Cerrado")}}});
	Rows.Add({TEXT("Estado de la casilla"),
		{{ReachableColor, TEXT("alcanzable: halo blanco bajo la casilla")},
			{SealedColor, TEXT("sellada (SELLO)")},
			{ForgottenRingColor, TEXT("Dominio Olvidado: casillas apagadas con aro gris oscuro")}}});
	// Una fila por asiento, con su rol al día: Eco (esfera) o Elegido (cono), y el Dominio que lleva.
	FLegendRow Seats;
	// S-019: con la carta del kit, el rol va en el aro de su base (decisión del autor del 20/10/2026).
	Seats.Title = Kit.Mesh(EBoardKitPiece::TokenCard)
		? TEXT("Asientos (el número va sobre la carta): aro tenue = Eco, aro vivo = Elegido. El activo crece, sube, gira y lleva aro")
		: TEXT("Asientos (el número va en la ficha): esfera = Eco, cono = Elegido. El activo crece, sube, gira y lleva aro");
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		FLegendChip Chip;
		Chip.Color = SeatColor(Seat);
		Chip.TextGetter = [WeakHost, Seat]() -> FString
		{
			const UMatchHostSubsystem* HostPtr = WeakHost.Get();
			if (!HostPtr || !HostPtr->HasMatch() || !HostPtr->GetState().Seats.IsValidIndex(Seat))
			{
				return FString::Printf(TEXT("%d"), Seat);
			}
			const FSeatState& SeatState = HostPtr->GetState().Seats[Seat];
			const TCHAR* Who = Seat == HostPtr->GetHumanSeat() ? TEXT("humano") : TEXT("dummy");
			const FString DomainName = DomainTitle(HostPtr->GetState(), SeatState.DomainId);
			// S-013: la ficha desterrada no está en el tablero; aquí se dice dónde está.
			const FString Void = VoidText(HostPtr->GetState(), Seat);
			// S-014: Marcado y Brazal revelado van detrás del Vacío.
			const FString InVoid = (Void.IsEmpty() ? FString() : TEXT(" | ") + Void) + EffectStatusText(HostPtr->GetState(), Seat);
			if (SeatState.Role == ESeatRole::Elect)
			{
				return FString::Printf(TEXT("%d (%s): ELEGIDO, D%d %s%s"), Seat, Who, SeatState.DomainId, *DomainName, *InVoid);
			}
			return SeatState.DomainId == INDEX_NONE
				? FString::Printf(TEXT("%d (%s): ECO%s"), Seat, Who, *InVoid)
				: FString::Printf(TEXT("%d (%s): ECO, hogar en D%d %s%s"), Seat, Who, SeatState.DomainId, *DomainName, *InVoid);
		};
		Seats.Chips.Add(Chip);
	}
	Rows.Add(Seats);
	// Brazal de cada asiento (S-008): runas por tipo y poderes activos, al día.
	FLegendRow Bracers;
	Bracers.Title = TEXT("Brazal por asiento: runas (D<n>×cantidad, una por Dominio) y poderes activos. Fuente: F, con su reserva");
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		FLegendChip Chip;
		Chip.Color = SeatColor(Seat);
		Chip.TextGetter = [WeakHost, Seat]() -> FString
		{
			const UMatchHostSubsystem* HostPtr = WeakHost.Get();
			if (!HostPtr || !HostPtr->HasMatch() || !HostPtr->GetState().Seats.IsValidIndex(Seat))
			{
				return FString::Printf(TEXT("%d"), Seat);
			}
			return BracerText(HostPtr->GetState(), Seat);
		};
		Bracers.Chips.Add(Chip);
	}
	Rows.Add(Bracers);
	// S-027: las teclas de los paneles, también en la leyenda.
	Rows.Add({FString::Printf(TEXT("Teclas: %s Grimorio | %s depuración | %s panel de estado | %s paneles"), *GrimoireKey.GetDisplayName().ToString(),
		*DebugPanelKey.GetDisplayName().ToString(), *DebugTextToggleKey.GetDisplayName().ToString(), *PanelsToggleKey.GetDisplayName().ToString()), {}});

	// S-027 (R-HUD-3): la letra y el ancho de todos los paneles se leen en cada pintado, para que sigan a la resolución sin rehacerlos.
	const TAttribute<int32> PanelFont = TAttribute<int32>::CreateLambda([this]() { return PanelSlateFontSize; });
	ChroniclePanel = SNew(SChroniclePanel).FontSize(PanelFont);
	Overlay = SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.f, 8.f, 8.f, 8.f))
		// S-024: con el telón bajado (o en su margen) los paneles no se dibujan, se vean o no a través de la pantalla de carga; la tecla de
		// los paneles sigue mandando después.
		.Visibility_Lambda([this]()
		{
			const UMatchHostSubsystem* HostPtr = Host.Get();
			return bPanelsHidden || (HostPtr && HostPtr->IsCurtainDown()) ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
		})
		[
			SNew(SBox)
			.WidthOverride_Lambda([this]() { return FOptionalSize(PanelSlateWidth); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SLegendPanel).FontSize(PanelFont).Rows(Rows)
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 6.f, 0.f, 0.f)
				[
					ChroniclePanel.ToSharedRef()
				]
			]
		];
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);

	// S-027 (R-HUD-1, R-HUD-2): el panel de estado arriba a la izquierda, en el hueco del tablero, y el de depuración debajo (decisión del autor
	// del 9/11/2026). Sustituyen a los mensajes de pantalla del motor. No reciben clics: el tablero de debajo sigue respondiendo. Se ocultan con
	// los paneles (H) y con el telón, como la leyenda; el de estado, también con T; el de depuración solo se ve con D.
	StatusOverlay = SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding_Lambda([this]() { return FMargin(8.f, 8.f, PanelSlateWidth + 16.f, 8.f); })
		.Visibility_Lambda([this]()
		{
			const UMatchHostSubsystem* HostPtr = Host.Get();
			return bPanelsHidden || (HostPtr && HostPtr->IsCurtainDown()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(StatusPanel, SStatusPanel)
				.FontSize(PanelFont)
				.LargeScale(StatusLargeFontScale)
				.Visibility_Lambda([this]() { return bDebugTextHidden ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SAssignNew(DebugPanel, SStatusPanel)
				.FontSize(PanelFont)
				.Title(FString::Printf(TEXT("DEPURACIÓN (%s)"), *DebugPanelKey.GetDisplayName().ToString()))
				.Visibility_Lambda([this]() { return bDebugPanelShown ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			]
		];
	Viewport->AddViewportWidgetContent(StatusOverlay.ToSharedRef(), 10);

	// S-027 (R-HUD-4): el Grimorio, centrado y por encima de todo. No es pausa; los clics del tablero los ignora el controlador mientras está
	// abierto. Los textos de afinidades y poderes salen de grimorio.json; lo que falte se enseña con el nombre del enumerado y se dice aquí.
	const Soulbound::Presentation::FGrimoireTextsLoadResult Texts =
		Soulbound::Presentation::LoadGrimoireTexts(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::GrimoireRelativePath));
	GrimoireTexts = Texts.Texts;
	if (!Texts.IsComplete())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Grimorio: %s"), *Texts.Message);
	}
	TArray<FLinearColor> RuneChipColors;
	for (int32 Id = 0; Id < Soulbound::Board::DomainCount; ++Id)
	{
		RuneChipColors.Add(DomainColor(Id));
	}
	GrimoireOverlay = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(16.f))
		.Visibility_Lambda([this]()
		{
			const UMatchHostSubsystem* HostPtr = Host.Get();
			return bGrimoireOpen && !(HostPtr && HostPtr->IsCurtainDown()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SAssignNew(GrimoirePanel, SGrimoirePanel)
			.FontSize(PanelFont)
			.DomainColors(RuneChipColors)
			.CloseKeys(FString::Printf(TEXT("%s o %s"), *GrimoireKey.GetDisplayName().ToString(), *PanelsToggleKey.GetDisplayName().ToString()))
		];
	Viewport->AddViewportWidgetContent(GrimoireOverlay.ToSharedRef(), 14);

	// S-028 (R-DEC-4): el ancho máximo del panel de decisión, en unidades de Slate; dentro, sus botones van en fila si caben.
	static constexpr float DecisionPanelMaxWidth = 900.f;
	// S-021: la línea del Warden, grande y abajo, a la izquierda de la columna de paneles. Solo texto: ni assets ni widgets del juego.
	// S-028 (R-DEC-2 a R-DEC-4): encima, en la misma columna, el panel de decisión, centrado y con la letra de los paneles; va siempre por
	// encima de la frase. El panel se queda los clics que le caen encima (SDecisionPanel); la capa y la frase siguen sin recibirlos.
	WardenLineOverlay = SNew(SBox)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding_Lambda([this]() { return FMargin(16.f, 0.f, PanelSlateWidth + 16.f, 36.f); })
	.Visibility(EVisibility::SelfHitTestInvisible)
	[
		SNew(SOverlay)
		// Capa de abajo: la columna del panel de decisión y, debajo, el hueco de la frase.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SBox)
				.MaxDesiredWidth(DecisionPanelMaxWidth)
				.Visibility_Lambda([this]() { return IsDecisionPanelVisible() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
				[
					SAssignNew(DecisionPanel, SDecisionPanel)
					.FontSize(PanelFont)
					.LargeScale(StatusLargeFontScale)
					.TimeFraction_Lambda([this]() { return DecisionTimeFractionNow(); })
					// Lo más ancho que puede ser el panel: el hueco del tablero (la capa deja 16 a la izquierda y la columna más 16 a la derecha), con el
					// tope de la caja. Con él decide si sus botones caben en fila (corrección del autor del 11/11/2026).
					.MaxWidth_Lambda([this]()
					{
						return BoardFreeWidthNow(DecisionPanelMaxWidth);
					})
					.OnOption_Lambda([this](const Soulbound::Presentation::FDecisionOption& Option) { AnswerDecision(Option); })
				]
			]
			// S-033 (R-DEC-9): el hueco de la frase, siempre reservado, haya frase o no, como espacio vacío de alto fijo: el panel de encima no se
			// mueve cuando la frase aparece, cambia de largo o se va. Su alto sale de la letra de la frase y de sus márgenes (WardenLineReservedHeight).
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride_Lambda([this]() { return FOptionalSize(WardenLineReservedHeight()); })
			]
		]
		// Capa de arriba: la frase, anclada abajo con el mismo margen que el hueco. Crece hacia arriba, por encima del panel si hace falta (corrección
		// del autor del 11/11/2026: en una caja de alto fijo se recortaba lo que sobraba). No recibe clics: el hueco vacío deja pasar los del tablero.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.65f))
			.Padding(FMargin(20.f, 10.f))
			.Visibility_Lambda([this]() { return WardenLineNow().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(WardenLineNow()); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(WardenLineFontSize, 8)))
				.ColorAndOpacity(WardenLineColor)
				.Justification(ETextJustify::Center)
				// R-HUD-3 (corrección del autor del 11/11/2026): con AutoWrapText el texto se parte al ancho con que se pintó la frase anterior
				// (FSlateTextBlockLayout::CalculateWrappingWidth usa CachedSize, el de la última pasada de pintado), así que tras una frase corta
				// todas salían estrechas. Con WrapTextAt solo y el hueco libre (el tope de 1000 menos los 40 de margen del fondo), el ancho no
				// depende de lo pintado antes. Nunca los dos a la vez: manda el menor. Desde R-DEC-13 (12/11/2026) el hueco sale de la geometría real
				// de la capa (BoardFreeWidthNow), no de la ventana y la escala de DPI: a 1024x768 la cuenta daba de más y la frase se cortaba.
				.WrapTextAt_Lambda([this]()
				{
					return FMath::Max(BoardFreeWidthNow(1000.f) - 40.f, 1.f);
				})
			]
		]
	];
	Viewport->AddViewportWidgetContent(WardenLineOverlay.ToSharedRef(), 11);

	// S-022 (decisión del autor del 29/10/2026): con la partida terminada, un botón grande para volver al menú, con el ratón (el controlador
	// deja el cursor visible y la entrada en juego y UI). El resultado es texto de depuración arriba a la izquierda, de altura variable: el botón
	// va en el centro del hueco del tablero, a la izquierda de la columna de paneles. Antes del final no existe; pedido, se desactiva.
	ReturnToMenuOverlay = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding_Lambda([this]() { return FMargin(16.f, 0.f, PanelSlateWidth + 16.f, 0.f); })
		.Visibility_Lambda([this]() { return IsMatchOverShown() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SButton)
			.ContentPadding(FMargin(32.f, 14.f))
			.IsEnabled_Lambda([this]() { return !bReturningToMenu; })
			.OnClicked_Lambda([this]()
			{
				ReturnToMenu();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Volver al menú")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(ReturnToMenuFontSize, 8)))
				.Justification(ETextJustify::Center)
			]
		];
	Viewport->AddViewportWidgetContent(ReturnToMenuOverlay.ToSharedRef(), 12);

	// S-026: el panel del anillo, centrado en el hueco del tablero como el botón del final. No recibe clics: la entrada va por RingInput.
	RingOverlay = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding_Lambda([this]() { return FMargin(16.f, 0.f, PanelSlateWidth + 16.f, 0.f); })
		.Visibility_Lambda([this]() { return RingPanelModel().bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SRingPanel)
			.FontSize(RingFontSize)
			.Model([this]() { return RingPanelModel(); })
		];
	Viewport->AddViewportWidgetContent(RingOverlay.ToSharedRef(), 13);
}

void ABoardViewActor::RefreshChronicle(const FMatchState& State)
{
	if (!ChroniclePanel.IsValid() || State.EventLog.Num() == ChronicleLineCount)
	{
		return;
	}
	ChronicleLineCount = State.EventLog.Num();
	// S-026: los resultados del humano dicen "con el anillo" si lo juega (interruptor encendido); los de los dummies, al azar.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	const int32 RingSeat = HostPtr && HostPtr->HumanPlaysSkillTrials() ? HostPtr->GetHumanSeat() : INDEX_NONE;

	// La más reciente primero; se conservan solo las últimas ChronicleMaxLines.
	TArray<FChronicleLine> Lines;
	const int32 First = FMath::Max(0, State.EventLog.Num() - FMath::Max(ChronicleMaxLines, 1));
	for (int32 Index = State.EventLog.Num() - 1; Index >= First; --Index)
	{
		// S-021 (A3): las instrucciones enseñadas justo después de este evento van encima de él (la crónica es de la más reciente a la más vieja).
		for (int32 Note = InstructionNotes.Num() - 1; Note >= 0; --Note)
		{
			if (InstructionNotes[Note].After == Index)
			{
				Lines.Add({FString::Printf(TEXT("[%s] %s"), *Stamp(InstructionNotes[Note].ClockSeconds), *InstructionNotes[Note].Text), WardenLineColor});
			}
		}
		const FMatchEvent& Event = State.EventLog[Index];
		FLinearColor Color = SeatColor(Event.Seat);
		switch (Soulbound::Presentation::EventSource(Event.Type))
		{
		case Soulbound::Presentation::EEventSource::Warden: Color = WardenLineColor; break;
		case Soulbound::Presentation::EEventSource::Race: Color = RaceLineColor; break;
		case Soulbound::Presentation::EEventSource::Debug: Color = DebugLineColor; break;
		case Soulbound::Presentation::EEventSource::Seat: break;
		}
		// S-016: las evaluaciones del Warden que no hacen nada no tienen línea (una cada 15 s taparía la crónica). S-017: la decisión
		// con efecto sí, con sus disparadores entre paréntesis (los WardenTriggered que la siguen, que no tienen línea propia).
		FString Text;
		if (Event.Type == EMatchEventType::WardenDecided)
		{
			TArray<FMatchEvent> Triggers;
			bool bViaOracle = false;
			for (int32 Next = Index + 1; Next < State.EventLog.Num(); ++Next)
			{
				const FMatchEvent& After = State.EventLog[Next];
				if (After.Type == EMatchEventType::WardenTriggered)
				{
					Triggers.Add(After);
					continue;
				}
				// Lo que aplica la decisión, en el mismo instante: el corte, el fin del turno y, si va por el Oráculo, su consulta.
				if (After.ClockSeconds != Event.ClockSeconds || After.Type == EMatchEventType::WardenEventStarted || After.Type == EMatchEventType::WardenEvaluated)
				{
					break;
				}
				bViaOracle |= After.Type == EMatchEventType::OracleOpened && After.Value == static_cast<int32>(EOracleOrigin::Strike);
			}
			Text = Soulbound::Presentation::DescribeWardenDecision(Event, Triggers, bViaOracle, &State.WardenEvents);
		}
		else
		{
			Text = Soulbound::Presentation::DescribeChronicleEvent(State, Index, RingSeat);
		}
		if (!Text.IsEmpty())
		{
			Lines.Add({FString::Printf(TEXT("[%s] %s"), *Stamp(Event.ClockSeconds), *Text), Color});
		}
	}
	ChroniclePanel->SetLines(Lines);
}

void ABoardViewActor::Refresh()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	const FMatchState& State = HostPtr->GetState();

	// Alcanzables: las que guarda el estado, calculadas por el motor (ReachableTiles) al tirar. Solo valen mientras
	// se espera el destino. Con un salto planar pendiente (S-009), el halo marca los Nodos a los que se puede saltar.
	TSet<int32> Reachable;
	if (State.TurnPhase == ETurnPhase::RollAndChoose)
	{
		Reachable.Append(State.ReachableOptions);
	}
	else if (State.PendingChoice.Kind == EChoiceKind::PlanarJump)
	{
		Reachable.Append(State.PendingChoice.Options);
	}

	// S-023 (R-PRO-1): en el prólogo, un Dominio Olvidado se ve en juego hasta que se enseña su frase de corrupción; después, lo del estado.
	bDomainsShownInPrologue = HostPtr->IsInPrologue();
	DomainsShownOpeningLines = WardenLineQueue.ShownCount();
	DomainsShownForgotten = Soulbound::Presentation::ForgottenDomainsShown(State, bDomainsShownInPrologue, DomainsShownOpeningLines);

	// El suelo de cada cuña lleva el color de su Dominio, apagado si está Olvidado (S-019, R-VIS-2, R-VIS-5).
	Kit.TintWedgeFloors(KitFixed, DomainsShownForgotten, *this);

	for (const FTileState& Tile : State.Board.Tiles)
	{
		ABoardTileActor* Actor = TileActors.IsValidIndex(Tile.TileId) ? TileActors[Tile.TileId].Get() : nullptr;
		if (!Actor)
		{
			continue;
		}
		const FDomainState* Domain = State.Board.Domains.IsValidIndex(Tile.DomainId) ? &State.Board.Domains[Tile.DomainId] : nullptr;
		const bool bForgotten = IsDomainShownForgotten(Tile.DomainId);
		const float Dim = bForgotten ? ForgottenDim : 1.f;

		// La losa lleva el color del Dominio (el del tipo, en el Altar); la marca de encima, el del tipo.
		FLinearColor Base = Tile.Type == ETileType::Altar ? AltarColor : Dimmed(DomainColor(Tile.DomainId), Dim);
		if (Tile.bSealed)
		{
			Base = SealedColor;
		}
		Actor->SetColor(Base);
		if (UStaticMeshComponent* ForgottenRing = ForgottenRings.IsValidIndex(Tile.TileId) ? ForgottenRings[Tile.TileId].Get() : nullptr)
		{
			ForgottenRing->SetVisibility(bForgotten);
		}
		if (UStaticMeshComponent* Halo = Halos.IsValidIndex(Tile.TileId) ? Halos[Tile.TileId].Get() : nullptr)
		{
			Halo->SetVisibility(Reachable.Contains(Tile.TileId));
		}

		UStaticMeshComponent* Inlay = Inlays.IsValidIndex(Tile.TileId) ? Inlays[Tile.TileId].Get() : nullptr;
		if (Inlay)
		{
			FLinearColor Mark = Base;
			switch (Tile.Type)
			{
			case ETileType::Niche:
				// El cubo no cambia de color por quien lo reclama: el Dominio se sigue viendo. Libre y Reclamado, el color de
				// tipo; Cerrado, gris oscuro. El Reclamado lleva el aro y el número del asiento (más abajo).
				// S-023: el Nicho Cerrado de un Dominio que la apertura aún no ha corrompido se ve Libre, como el resto del Dominio.
				Mark = Domain && Domain->NicheState == ENicheState::Closed && bForgotten ? NicheClosedColor : NicheFreeColor;
				break;
			case ETileType::PowerSource:
				// Agotada (S-008): la cruz se apaga; viva, el color del tipo.
				Mark = Dimmed(SourceColor, State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve <= 0 ? Dim * SourceDepletedDim : Dim);
				break;
			case ETileType::PowerNode: Mark = Dimmed(NodeColor, Dim); break;
			default: break;
			}
			Soulbound::Presentation::SetComponentColor(*Inlay, BaseMaterial, Mark);
		}
		if (UStaticMeshComponent* Hole = InlayHoles.IsValidIndex(Tile.TileId) ? InlayHoles[Tile.TileId].Get() : nullptr)
		{
			// Segundo brazo de la cruz de la Fuente: el mismo color que el primero.
			const bool bDepleted = State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve <= 0;
			Soulbound::Presentation::SetComponentColor(*Hole, BaseMaterial, Dimmed(SourceColor, bDepleted ? Dim * SourceDepletedDim : Dim));
		}
		if (Tile.Type == ETileType::Niche && NicheRingFirst.IsValidIndex(Tile.TileId) && NicheRingFirst[Tile.TileId] != INDEX_NONE)
		{
			const bool bClaimed = Domain && Domain->NicheState == ENicheState::Claimed;
			const FLinearColor RingColor = bClaimed ? SeatColor(Domain->OwnerSeat) : FLinearColor::White;
			const int32 First = NicheRingFirst[Tile.TileId];
			for (int32 Index = First; Index < First + NicheRingSegmentCount && NicheRingSegments.IsValidIndex(Index); ++Index)
			{
				Soulbound::Presentation::SetComponentColor(*NicheRingSegments[Index], BaseMaterial, RingColor);
				NicheRingSegments[Index]->SetVisibility(bClaimed);
			}
			if (UTextRenderComponent* Number = NicheNumbers[Tile.TileId].Get())
			{
				Number->SetText(bClaimed ? FText::AsNumber(Domain->OwnerSeat) : FText::GetEmpty());
			}
		}
		if (UTextRenderComponent* Reserve = SourceReserves.IsValidIndex(Tile.TileId) ? SourceReserves[Tile.TileId].Get() : nullptr)
		{
			// Desde el Despertar: lo que queda sobre lo que tenía. Agotada: 0/n, y la cruz y la etiqueta se apagan.
			Reserve->SetText(State.Board.Phase == EBoardPhase::Awakened
				? FText::FromString(FString::Printf(TEXT("%d/%d"), Tile.RuneReserve, State.Board.Params.RuneReservePerSource))
				: FText::GetEmpty());
		}
		if (UTextRenderComponent* Label = Labels.IsValidIndex(Tile.TileId) ? Labels[Tile.TileId].Get() : nullptr)
		{
			const FString Text = TileLabelText(State.Board, Tile) + (Tile.bSealed ? TEXT("\nSELLO") : TEXT(""));
			Label->SetText(FText::FromString(Text));
			const bool bDepletedSource = Tile.Type == ETileType::PowerSource && State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve <= 0;
			Label->SetTextRenderColor(Tile.bSealed ? FColor::Red : bDepletedSource ? FColor(130, 130, 130) : FColor::White);
		}
		// S-019 (R-VIS-9): la tecla de las capturas oculta todas las etiquetas de la casilla.
		for (const TArray<TObjectPtr<UTextRenderComponent>>* Texts : {&Labels, &NicheNames, &NicheNumbers, &SourceReserves})
		{
			if (UTextRenderComponent* Text = Texts->IsValidIndex(Tile.TileId) ? (*Texts)[Tile.TileId].Get() : nullptr)
			{
				Text->SetVisibility(!bOverlayHidden);
			}
		}
	}

	// S-019: estructuras, runas flotantes, cadenas y fichas fuera en el Vacío, según el estado de vista.
	const FBoardViewState View = Soulbound::Presentation::ComputeBoardViewState(State, Kit.IsNicheThroneConfigured());
	RefreshKit(View);

	RefreshTokens(State);
	RefreshOracleOrb(State);
	RefreshAspirantHalos(State);
	RefreshWardenLine(State);
	RefreshChronicle(State);
}

void ABoardViewActor::RefreshWardenLine(const FMatchState& State)
{
	if (WardenLineScanned > State.EventLog.Num())
	{
		WardenLineScanned = 0;
		WardenLineQueue = Soulbound::Presentation::FWardenLineQueue();
		InstructionNotes.Empty();
	}
	const int32 HumanSeat = Host.IsValid() ? Host->GetHumanSeat() : INDEX_NONE;
	for (; WardenLineScanned < State.EventLog.Num(); ++WardenLineScanned)
	{
		const FMatchEvent& Event = State.EventLog[WardenLineScanned];
		const FString Spoken = Event.Type == EMatchEventType::WardenSpoke ? Soulbound::Presentation::SpokenText(State.WardenLines, Event) : FString();
		if (!Spoken.IsEmpty())
		{
			WardenLineQueue.Push(Soulbound::Presentation::WardenLinePrefix(Event.Seat, HumanSeat) + TEXT(" ") + Spoken, Event.ClockSeconds);
		}
	}

	// S-021 (A3): las instrucciones, después de las frases del registro. El seguimiento dice qué momento acaba de darse; el perfil, si se enseña.
	TArray<int32> HumanSeats;
	if (HumanSeat != INDEX_NONE)
	{
		HumanSeats.Add(HumanSeat);
	}
	TeachInstructions(State, InstructionTracker.Observe(State, HumanSeats));
}

void ABoardViewActor::TeachInstructions(const FMatchState& State, const TArray<Soulbound::Presentation::FInstructionCue>& Cues)
{
	UPlayerProfileSubsystem* Profile = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>() : nullptr;
	if (!Profile)
	{
		return;
	}
	const int32 HumanSeat = Host.IsValid() ? Host->GetHumanSeat() : INDEX_NONE;
	for (const Soulbound::Presentation::FInstructionCue& Cue : Cues)
	{
		const FString Text = Soulbound::Presentation::InstructionText(State.WardenLines, Cue.Moment);
		// El momento ya está consumido. Sin frase en la tabla no se enseña ni se anota en el perfil; con ella, el perfil decide (y lo anota).
		if (Text.IsEmpty() || !Profile->TeachInstruction(Cue.Moment))
		{
			continue;
		}
		const double Clock = State.EventLog.IsEmpty() ? 0.0 : State.EventLog.Last().ClockSeconds;
		const FString Line = Soulbound::Presentation::WardenLinePrefix(Cue.Seat, HumanSeat) + TEXT(" ") + Text;
		WardenLineQueue.Push(Line, Clock);
		InstructionNotes.Add({State.EventLog.Num() - 1, Clock, Line});
		// La crónica se reconstruye solo si crece el registro: la nota no lo hace crecer.
		ChronicleLineCount = -1;
	}
}

FString ABoardViewActor::WardenLineNow()
{
	// S-024 (R-PRO-1 precisión): con el telón bajado no se pide la frase a la cola, así que su primera frase empieza a contar cuando sube.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (HostPtr && HostPtr->IsCurtainDown())
	{
		return FString();
	}
	return WardenLineQueue.Current(FPlatformTime::Seconds(), WardenLineSeconds);
}

void ABoardViewActor::RefreshAspirantHalos(const FMatchState& State)
{
	// Sobre el halo de alcanzable del Altar: mismo sitio, un poco más abajo para no pisarlo, y más ancho.
	const int32 Altar = State.Board.FindAltar();
	const UStaticMeshComponent* Base = Halos.IsValidIndex(Altar) ? Halos[Altar].Get() : nullptr;
	int32 Shown = 0;
	for (int32 Seat = 0; Seat < AspirantHalos.Num(); ++Seat)
	{
		UStaticMeshComponent* Halo = AspirantHalos[Seat].Get();
		if (!Halo)
		{
			continue;
		}
		const bool bShow = Base && !State.IsMatchOver() && Soulbound::Rules::IsAspirant(State, Seat);
		if (bShow)
		{
			const FVector Scale = Base->GetComponentScale();
			const float Factor = AspirantHaloScale + AspirantHaloStep * Shown++;
			Halo->SetWorldLocation(Base->GetComponentLocation() - FVector(0.f, 0.f, 0.5f));
			Halo->SetWorldScale3D(FVector(Scale.X * Factor, Scale.Y * Factor, Scale.Z));
		}
		Halo->SetVisibility(bShow);
	}
}

FVector ABoardViewActor::TokenBase(int32 TileId) const
{
	return TilePositions.IsValidIndex(TileId) ? TilePositions[TileId] + FVector(0.f, 0.f, TileTops.IsValidIndex(TileId) ? TileTops[TileId] : 0.f) : FVector::ZeroVector;
}

void ABoardViewActor::OnTokenTileEntered(int32 Seat, int32 TileId, bool bLast)
{
	UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	// R-TAB-26: la presentación dice al motor en qué casilla ha entrado la ficha, sea del humano o de un dummy (decisión del autor
	// del 21/10/2026). Aceptada, el anfitrión avisa y la vista se refresca sola; si no (la salvaguarda llegó antes, o el turno se
	// cortó), al acabar el recorrido se recoloca.
	if (HostPtr->SubmitMoveProgress(Seat, TileId) == EIntentResult::Accepted)
	{
		return;
	}
	if (bLast)
	{
		RefreshTokens(HostPtr->GetState());
	}
}

void ABoardViewActor::RefreshTokens(const FMatchState& State)
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr)
	{
		return;
	}
	// Asiento activo: solo mientras dura su turno; entre turnos no juega nadie.
	const bool bSomeoneActive = !State.IsMatchOver() && State.TurnPhase != ETurnPhase::BetweenTurns;
	// S-013, S-019: qué fichas están en el tablero (fuera, en el Vacío). Se mira aquí porque las fichas también se recolocan al
	// acabar un recorrido, sin pasar por Refresh.
	const FBoardViewState View = Soulbound::Presentation::ComputeBoardViewState(State, Kit.IsNicheThroneConfigured());

	// S-020: el asiento que se mueve, humano o dummy (decisión del autor del 21/10/2026).
	const int32 MovingSeat = State.TurnPhase == ETurnPhase::Moving ? State.ActiveSeat : INDEX_NONE;
	// R-VIS-6 (decisión del autor del 22/10/2026): el trono del Warden ocupa el centro del Altar, así que la ficha no cruza de
	// centro a centro. Al salir del Altar va primero por el círculo de la grada alta, desde su sitio hasta el punto que mira a la
	// primera casilla, en pasos de 30° como mucho; al llegar a él, al revés. Los puntos del círculo (casilla INDEX_NONE) son solo
	// presentación: las intenciones de avance se entregan una por casilla, al llegar a su punto final.
	const FBoardState& Board = Host->GetState().Board;
	auto CirclePoint = [this, &Board](float Angle)
	{
		return TokenBase(Board.FindAltar()) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Layout.AltarApothem * 0.6f);
	};
	auto FacingAngle = [this](int32 Tile) -> float
	{
		const FVector2D Direction(Layout.Tiles[Tile].Location);
		return Direction.IsNearlyZero() ? 0.f : static_cast<float>(FMath::Atan2(Direction.Y, Direction.X));
	};
	// Los ángulos desde From hasta To (el camino corto) en pasos de 30° como mucho, sin From y con To.
	auto ArcAngles = [](float From, float To)
	{
		TArray<float> Angles;
		const float Delta = FMath::UnwindRadians(To - From);
		const int32 Steps = FMath::CeilToInt(FMath::Abs(Delta) / FMath::DegreesToRadians(30.f) - UE_KINDA_SMALL_NUMBER);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			Angles.Add(From + Delta * Step / Steps);
		}
		return Angles;
	};
	auto WalkOf = [&](int32 Seat, int32 CurrentTile, const TArray<int32>& Tiles, TArray<int32>& OutTiles, TArray<FVector>& OutPoints)
	{
		const int32 AltarTile = Board.FindAltar();
		const float SeatAngle = 2.f * PI * Seat / Soulbound::Match::MaxPlayers;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			const int32 Tile = Tiles[Index];
			if (Index == 0 && CurrentTile == AltarTile)
			{
				for (const float Angle : ArcAngles(SeatAngle, FacingAngle(Tile)))
				{
					OutTiles.Add(INDEX_NONE);
					OutPoints.Add(CirclePoint(Angle));
				}
			}
			if (Index == Tiles.Num() - 1 && Tile == AltarTile)
			{
				TArray<float> Angles{FacingAngle(Index > 0 ? Tiles[Index - 1] : CurrentTile)};
				Angles.Append(ArcAngles(Angles[0], SeatAngle));
				for (int32 Step = 0; Step < Angles.Num(); ++Step)
				{
					OutTiles.Add(Step == Angles.Num() - 1 ? Tile : INDEX_NONE);
					OutPoints.Add(CirclePoint(Angles[Step]));
				}
				continue;
			}
			OutTiles.Add(Tile);
			OutPoints.Add(TokenBase(Tile));
		}
	};

	// Fichas: sobre su casilla y, si comparten casilla, repartidas en un círculo pequeño.
	for (int32 Seat = 0; Seat < State.Seats.Num() && Seat < TokenActors.Num(); ++Seat)
	{
		ASeatTokenActor* Token = TokenActors[Seat];
		const int32 TileId = State.Seats[Seat].TileId;
		if (!Token)
		{
			continue;
		}
		// S-013 (R-TAB-24): en el Vacío la ficha sale del tablero; se oculta y reaparece en su hogar al salir.
		Token->SetActorHiddenInGame(!View.TokensOnBoard.IsValidIndex(Seat) || !View.TokensOnBoard[Seat]);
		Token->SetLabelVisible(!bOverlayHidden);
		if (!TilePositions.IsValidIndex(TileId))
		{
			Token->StopWalk();
			continue;
		}
		Token->SetElectShape(State.Seats[Seat].Role == ESeatRole::Elect);
		Token->SetActive(bSomeoneActive && Seat == State.ActiveSeat);
		// S-017: durante una votación, el clic en la ficha de otro es el voto del humano.
		Token->SetClickable(Soulbound::Rules::VoteOptions(State, HostPtr->GetHumanSeat()).Contains(Seat));

		// S-020 (R-TAB-26): la ficha del que se mueve recorre lo que le queda del camino, y cada llegada es su intención de avance.
		// Si el motor va por delante (la salvaguarda entró en una casilla a la que la ficha no había llegado), la ficha se coloca
		// donde dice el estado, sin animar, y sigue desde ahí.
		if (Seat == MovingSeat)
		{
			const TArray<int32>& Path = State.Movement.Path;
			const int32 Done = FMath::Clamp(State.Movement.EnteredCount, 0, Path.Num());
			const TArray<int32> Remaining(Path.GetData() + Done, Path.Num() - Done);
			if (Token->RemainingWalk() != Remaining)
			{
				if (Token->IsWalking())
				{
					Token->StopWalk();
					Token->MoveTo(TokenBase(TileId), 0.f);
				}
				TArray<int32> WalkTiles;
				TArray<FVector> WalkPoints;
				WalkOf(Seat, TileId, Remaining, WalkTiles, WalkPoints);
				Token->WalkPath(WalkTiles, WalkPoints, TokenSpeed);
			}
			continue;
		}
		if (Token->IsWalking())
		{
			// Un recorrido que el estado deja atrás: cortado a medio camino o llegado por la salvaguarda. La ficha va donde dice el
			// estado, sin animar.
			Token->StopWalk();
			Token->MoveTo(TokenBase(TileId), 0.f);
		}

		// Cada asiento tiene siempre su sitio en la casilla, esté sola o acompañada: si una ficha se va, las demás no se mueven
		// (decisión del autor del 22/10/2026). En el Altar, en el círculo de la grada alta, porque el centro es el trono vacío
		// del Warden (R-VIS-6).
		const bool bOnAltar = Host->GetState().Board.IsAltar(TileId);
		const float Angle = 2.f * PI * Seat / Soulbound::Match::MaxPlayers;
		const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (bOnAltar ? Layout.AltarApothem * 0.6f : SlabSize() * 0.28f);
		// La base de la ficha, posada en la cara superior de la losa (R-VIS-3, R-VIS-6). Lo que no es un movimiento (Vacío, hogar,
		// Expulsión, salto planar) desliza en línea recta; tras recorrer un camino, solo el sitio del asiento en la casilla.
		Token->MoveTo(TokenBase(TileId) + Offset, TokenSlideSeconds);
	}
}

void ABoardViewActor::RefreshKit(const FBoardViewState& View)
{
	// R-VIS-5: lo que cambia con la partida se lee del estado de vista; la vista no decide nada.
	for (int32 TileId = 0; TileId < View.Tiles.Num(); ++TileId)
	{
		const FTileViewState& Shown = View.Tiles[TileId];
		if (UStaticMeshComponent* Structure = KitStructures.Structures.IsValidIndex(TileId) ? KitStructures.Structures[TileId].Get() : nullptr)
		{
			Structure->SetVisibility(Shown.Structure != EBoardStructure::None);
		}
		if (UStaticMeshComponent* Rune = KitStructures.FloatingRunes.IsValidIndex(TileId) ? KitStructures.FloatingRunes[TileId].Get() : nullptr)
		{
			Rune->SetVisibility(Shown.bFloatingRune);
		}
		if (UStaticMeshComponent* Chains = KitStructures.SealChains.IsValidIndex(TileId) ? KitStructures.SealChains[TileId].Get() : nullptr)
		{
			Chains->SetVisibility(Shown.bSealChains);
		}
	}
}

void ABoardViewActor::ToggleSeatedView()
{
	bSeatedView = !bSeatedView;
	ApplyViewTarget();
}

void ABoardViewActor::ToggleOverlay()
{
	bOverlayHidden = !bOverlayHidden;
	Refresh();
}

void ABoardViewActor::TogglePanels()
{
	// La visibilidad de los paneles la calcula su capa (bPanelsHidden y el telón, S-024); aquí solo se recoloca el tablero. S-027 (R-HUD-4): H
	// también cierra el Grimorio, para que uno invisible no siga bloqueando los clics (decisión aceptada por el autor el 9/11/2026).
	bPanelsHidden = !bPanelsHidden;
	bGrimoireOpen = false;
	UpdateCamera();
}

void ABoardViewActor::ToggleDebugText()
{
	// S-027: el panel de estado se oculta en su capa; al volver, se rellena en el acto.
	bDebugTextHidden = !bDebugTextHidden;
	TickHud();
}

FString ABoardViewActor::DescribeViewKeys() const
{
	return Soulbound::Presentation::ViewKeysText(bSeatedView, SeatedViewKey.GetDisplayName().ToString(), OverlayToggleKey.GetDisplayName().ToString(),
		PanelsToggleKey.GetDisplayName().ToString(), DebugTextToggleKey.GetDisplayName().ToString(), GrimoireKey.GetDisplayName().ToString(),
		DebugPanelKey.GetDisplayName().ToString());
}

bool ABoardViewActor::IsMatchOverShown() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	return HostPtr && HostPtr->HasMatch() && HostPtr->GetState().IsMatchOver();
}

void ABoardViewActor::ReturnToMenu()
{
	// Antes del final no hay botón y la tecla no hace nada; una vez pedido, ni el segundo clic ni la tecla repiten.
	if (bReturningToMenu || !IsMatchOverShown())
	{
		return;
	}
	bReturningToMenu = true;
	// TODO(R-RED-1): con red, solo el servidor tiene vista; cuando los clientes la tengan, cada uno cerrará su sesión y volverá a su menú.
	UA27OnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UA27OnlineSubsystem>() : nullptr;
	if (Online && !Online->GetOnlineSessioCode().IsEmpty())
	{
		UE_LOG(LogSoulboundView, Log, TEXT("Volver al menú: se cierra la sesión en línea %s."), *Online->GetOnlineSessioCode());
		Online->OnLeaveSessionComplete.AddDynamic(this, &ABoardViewActor::HandleSessionLeftForMenu);
		Online->LeaveSession();
		return;
	}
	TravelToMenu();
}

void ABoardViewActor::HandleSessionLeftForMenu(bool bWasSuccessful)
{
	if (UA27OnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UA27OnlineSubsystem>() : nullptr)
	{
		Online->OnLeaveSessionComplete.RemoveDynamic(this, &ABoardViewActor::HandleSessionLeftForMenu);
	}
	if (!bWasSuccessful)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Volver al menú: no se pudo cerrar la sesión en línea; se vuelve igual."));
	}
	TravelToMenu();
}

void ABoardViewActor::TravelToMenu()
{
	// S-027 (R-MEN-1): el mapa del menú es ReturnToMenuMap (el menú principal de UGT); vacío o inexistente, el de arranque del juego
	// (GameDefaultMap, el lanzador con los logos), para no quedarse nunca sin salida. Los dos se leen; ninguna ruta va en el código.
	const bool bReturnMapExists = !ReturnToMenuMap.IsNull() && FPackageName::DoesPackageExist(ReturnToMenuMap.GetLongPackageName());
	const FString Map = Soulbound::Presentation::MenuMapPackage(ReturnToMenuMap.IsNull() ? FString() : ReturnToMenuMap.ToString(), bReturnMapExists,
		UGameMapsSettings::GetGameDefaultMap());
	if (!bReturnMapExists)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Volver al menú: ReturnToMenuMap %s; se vuelve al mapa de arranque (GameDefaultMap)."),
			ReturnToMenuMap.IsNull() ? TEXT("está vacío") : *FString::Printf(TEXT("(%s) no existe"), *ReturnToMenuMap.ToString()));
	}
	UE_LOG(LogSoulboundView, Log, TEXT("Volver al menú: %s."), *Map);
	UGameplayStatics::OpenLevel(this, FName(*Map));
}

void ABoardViewActor::ApplyViewTarget()
{
	ACameraActor* Wanted = bSeatedView && SeatedCamera ? SeatedCamera.Get() : Camera.Get();
	if (!Wanted)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalController())
		{
			continue;
		}
		if (Controller->GetViewTarget() != Wanted)
		{
			Controller->SetViewTarget(Wanted);
		}
		// S-019: a escala de mesa, el peón por defecto del modo de juego (una esfera de 70 cm en el PlayerStart) tapa las
		// cámaras. La vista no lo usa: lo oculta y le quita la colisión, para que tampoco se coma el clic en las casillas.
		APawn* Pawn = Controller->GetPawn();
		if (Pawn && !Pawn->IsHidden())
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
		}
		// S-019 (R-VIS-9): las teclas de la vista, enlazadas a la tecla sin assets de entrada, como las del controlador.
		if (!bInputBound)
		{
			EnableInput(Controller);
			if (InputComponent)
			{
				InputComponent->BindKey(SeatedViewKey, IE_Pressed, this, &ABoardViewActor::ToggleSeatedView);
				InputComponent->BindKey(OverlayToggleKey, IE_Pressed, this, &ABoardViewActor::ToggleOverlay);
				InputComponent->BindKey(PanelsToggleKey, IE_Pressed, this, &ABoardViewActor::TogglePanels);
				InputComponent->BindKey(DebugTextToggleKey, IE_Pressed, this, &ABoardViewActor::ToggleDebugText);
				// S-027: el Grimorio (G) y el panel de depuración (D).
				InputComponent->BindKey(GrimoireKey, IE_Pressed, this, &ABoardViewActor::ToggleGrimoire);
				InputComponent->BindKey(DebugPanelKey, IE_Pressed, this, &ABoardViewActor::ToggleDebugPanel);
				// S-022: la tecla de volver al menú, alternativa al botón, no se consume: antes del final sigue llegando a quien la use.
				InputComponent->BindKey(ReturnToMenuKey, IE_Pressed, this, &ABoardViewActor::ReturnToMenu).bConsumeInput = false;
				bInputBound = true;
			}
		}
	}
}

void ABoardViewActor::UpdatePanelScale(const FVector2D& ViewportSize)
{
	// S-027 (R-HUD-3): el tamaño se calcula en píxeles de pantalla y se pasa a Slate dividido por la escala de DPI que Unreal ya aplica a toda
	// la interfaz del viewport (la regla de UUserInterfaceSettings, por defecto según el lado corto), para no escalar dos veces.
	const FIntPoint Size(FMath::RoundToInt(ViewportSize.X), FMath::RoundToInt(ViewportSize.Y));
	const UUserInterfaceSettings* Settings = GetDefault<UUserInterfaceSettings>();
	const float Dpi = Settings ? Settings->GetDPIScaleBasedOnSize(Size) : 1.f;
	PanelDpiScale = Dpi > KINDA_SMALL_NUMBER ? Dpi : 1.f;
	const int32 ScreenFont = Soulbound::Presentation::ScaledPanelFontSize(ViewportSize.Y, PanelFontSize, PanelMinFontSize);
	const int32 SlateFont = FMath::Max(1, FMath::RoundToInt(ScreenFont / PanelDpiScale));
	const float ScreenWidth = Soulbound::Presentation::ScaledPanelWidth(ViewportSize.Y, PanelWidthPixels);
	const bool bChanged = SlateFont != PanelSlateFontSize || !FMath::IsNearlyEqual(ScreenWidth, PanelScreenWidth, 0.5f) || !bPanelScaleLogged;
	PanelSlateFontSize = SlateFont;
	PanelScreenWidth = ScreenWidth;
	PanelSlateWidth = ScreenWidth / PanelDpiScale;
	if (bChanged)
	{
		bPanelScaleLogged = true;
		UE_LOG(LogSoulboundView, Log, TEXT("Paneles: viewport %dx%d, escala DPI de Unreal %.3f; letra %d px en pantalla (%d en Slate, mínimo %d); columna %.0f px en pantalla (%.0f en Slate)."),
			Size.X, Size.Y, PanelDpiScale, ScreenFont, PanelSlateFontSize, PanelMinFontSize, PanelScreenWidth, PanelSlateWidth);
	}
}

void ABoardViewActor::ToggleDebugPanel()
{
	bDebugPanelShown = !bDebugPanelShown;
	TickHud();
}

void ABoardViewActor::ToggleGrimoire()
{
	bGrimoireOpen = !bGrimoireOpen;
	TickHud();
}

Soulbound::Presentation::FStatusContext ABoardViewActor::MakeStatusContext() const
{
	Soulbound::Presentation::FStatusContext Context;
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr)
	{
		return Context;
	}
	// S-021 (A3): el texto que explica qué hacer ("<- tu turno: ...") sigue el ajuste de ayudas; las opciones numeradas y el tiempo, nunca.
	const UPlayerProfileSubsystem* Profile = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>() : nullptr;
	const UWardenHostSubsystem* WardenHost = GetWorld() ? GetWorld()->GetSubsystem<UWardenHostSubsystem>() : nullptr;
	Context.HumanSeat = HostPtr->GetHumanSeat();
	Context.bHints = !Profile || Profile->ShowsRequestHints();
	Context.bPrologue = HostPtr->IsInPrologue();
	Context.bWardenSpeaking = HostPtr->IsWardenSpeaking();
	Context.bHumanRing = HostPtr->HumanPlaysSkillTrials();
	Context.bStepWait = HostPtr->IsWaitingForStep();
	Context.bStepMode = HostPtr->IsStepMode();
	Context.bWardenOn = WardenHost && WardenHost->GetMode() == EWardenHostMode::Auto;
	Context.Seed = HostPtr->GetSeed();
	Context.ViewKeys = DescribeViewKeys();
	Context.ReturnKey = ReturnToMenuKey.GetDisplayName().ToString();
	Context.SeatColors = SeatColors;
	return Context;
}

void ABoardViewActor::TickHud()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	const FMatchState& State = HostPtr->GetState();

	// S-023: en el prólogo no cambia el estado, pero cada frase de corrupción apaga su Dominio: se repinta al enseñarse una frase de la
	// apertura y al terminar el prólogo. WardenLineNow avanza la cola aunque la capa de la línea no se haya dibujado aún.
	WardenLineNow();
	if (HostPtr->IsInPrologue() != bDomainsShownInPrologue || (bDomainsShownInPrologue && WardenLineQueue.ShownCount() != DomainsShownOpeningLines))
	{
		Refresh();
	}

	// La cámara es la vista del jugador local. Se pone al iniciar la partida, o en cuanto exista el controlador. Desde
	// S-019, la cenital o la sentada, según la tecla.
	if (Camera)
	{
		ApplyViewTarget();
		// Si la ventana cambia de tamaño, el tablero se recoloca en el hueco de los paneles; desde S-027, los paneles también cambian de tamaño.
		if (const UGameViewportClient* Client = GetWorld()->GetGameViewport())
		{
			FVector2D Viewport;
			Client->GetViewportSize(Viewport);
			if (!Viewport.Equals(LastViewportSize, 0.5))
			{
				UpdateCamera();
			}
		}
	}

	// S-033 (R-DEC-13): los números para medir a 1024x768, en la línea "Paneles:" cuando cambia el ancho de la capa de la frase: su geometría real,
	// lo que da la cuenta con la ventana y la escala de DPI, y el hueco libre que sale de cada una (tope 1000, el de la frase).
	if (WardenLineOverlay.IsValid())
	{
		const FVector2D LayerSize = FVector2D(WardenLineOverlay->GetTickSpaceGeometry().GetLocalSize());
		if (LayerSize.X >= 1.0 && !FMath::IsNearlyEqual(static_cast<float>(LayerSize.X), LoggedLayerWidth, 0.5f))
		{
			LoggedLayerWidth = static_cast<float>(LayerSize.X);
			const float ByWindow = static_cast<float>(LastViewportSize.X) / PanelDpiScale;
			UE_LOG(LogSoulboundView, Log, TEXT("Paneles: capa de la frase %.1fx%.1f en Slate (geometría real); la cuenta con la ventana da %.1f de ancho (%.0f px / escala %.3f); hueco libre con tope 1000: %.1f por geometría, %.1f por la cuenta; columna %.1f."),
				LayerSize.X, LayerSize.Y, ByWindow, LastViewportSize.X, PanelDpiScale, Soulbound::Presentation::BoardFreeWidth(static_cast<float>(LayerSize.X), PanelSlateWidth, 1000.f),
				Soulbound::Presentation::BoardFreeWidth(ByWindow, PanelSlateWidth, 1000.f), PanelSlateWidth);
		}
	}

	// S-024 (decisión del autor del 4/11): con el telón bajado, o en su margen, no se rellena nada: los paneles se ocultan en su capa.
	if (HostPtr->IsCurtainDown())
	{
		return;
	}

	// S-027 (R-HUD-1, R-HUD-2, R-HUD-4): lo que antes eran mensajes de pantalla del motor, en los paneles; el texto lo construye StatusText.
	// S-028 (R-DEC-1 a R-DEC-3): la decisión del humano, en su panel, con el reloj de la barra; con el panel a la vista, el de estado no repite
	// la petición. S-033 (R-DEC-6): el mismo panel enseña, si el humano no tiene nada que decidir, el resultado de una votación reciente o a quién
	// espera la mesa; el panel de estado solo deja de repetir la petición cuando lo que se ve es una decisión propia.
	const TOptional<Soulbound::Presentation::FDecisionView> Panel = PanelViewNow();
	Soulbound::Presentation::AdvanceDecisionClock(DecisionClock, Panel);
	DecisionDeadlineSeconds = Panel.IsSet() ? Panel->DeadlineSeconds : 0.0;
	const bool bRingOpen = HostPtr->GetRingRun() != nullptr;
	bDecisionShown = Soulbound::Presentation::IsDecisionPanelShown(Panel, bRingOpen);
	if (DecisionPanel.IsValid())
	{
		DecisionPanel->SetView(bDecisionShown ? Panel : TOptional<Soulbound::Presentation::FDecisionView>());
	}

	Soulbound::Presentation::FStatusContext Context = MakeStatusContext();
	Context.bDecisionPanelShown = Soulbound::Presentation::IsOwnDecisionShown(Panel, bRingOpen);
	if (StatusPanel.IsValid())
	{
		StatusPanel->SetLines(Soulbound::Presentation::StatusLines(State, Context));
	}
	if (DebugPanel.IsValid())
	{
		DebugPanel->SetLines(bDebugPanelShown ? Soulbound::Presentation::DebugLines(State, Context) : TArray<Soulbound::Presentation::FPanelLine>());
	}
	if (GrimoirePanel.IsValid() && bGrimoireOpen)
	{
		GrimoirePanel->SetContent(Soulbound::Presentation::GrimoireRows(State, Context.HumanSeat, GrimoireTexts, SeatColors),
			Soulbound::Presentation::GrimoirePowerLines(GrimoireTexts));
	}
}

void ABoardViewActor::HandleRingOpened()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr)
	{
		return;
	}
	// El título se fija al abrir: al cerrarse, el anfitrión ya no tiene la prueba.
	LastRingTitle = RingTitle(HostPtr->GetRingTrial(), HostPtr->IsRingDuel(), HostPtr->GetRingDifficulty());
	LastRingShownUntil = 0.0;
	// S-027 (R-INS-1): la primera vez que se abre el anillo para el humano, el Warden lo explica. El anillo no es un evento del motor: lo avisa
	// la vista al seguimiento.
	if (HostPtr->HasMatch() && HostPtr->GetHumanSeat() != INDEX_NONE)
	{
		TeachInstructions(HostPtr->GetState(), InstructionTracker.RingOpened({HostPtr->GetHumanSeat()}));
	}
	if (RingInput.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	RingInput = MakeShared<FRingInputProcessor>(Host);
	FSlateApplication::Get().RegisterInputPreProcessor(RingInput);
	++LiveRingInputs;
	UE_LOG(LogSoulboundView, Log, TEXT("Anillo: entrada registrada (activas: %d)."), LiveRingInputs);
}

void ABoardViewActor::UnregisterRingInput()
{
	if (!RingInput.IsValid())
	{
		return;
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(RingInput);
	}
	RingInput.Reset();
	--LiveRingInputs;
	UE_LOG(LogSoulboundView, Log, TEXT("Anillo: entrada retirada (activas: %d)."), LiveRingInputs);
}

void ABoardViewActor::HandleRingClosed(const TOptional<FRingOutcome>& Outcome)
{
	UnregisterRingInput();
	if (!Outcome.IsSet())
	{
		LastRingShownUntil = 0.0; // descartado: no hay resultado que enseñar
		return;
	}
	LastRingPrecisions = Outcome->Precisions;
	if (Outcome->bDuel)
	{
		LastRingResult = FString::Printf(TEXT("RENDIMIENTO %.2f"), Outcome->Performance);
		LastRingColor = SRingPanel::PrecisionColor(Outcome->Performance);
		LastRingNote = Outcome->bForced ? TEXT("resultado fijado con SbDuel") : FString();
	}
	else
	{
		LastRingResult = Outcome->bPassed ? TEXT("SUPERADA") : TEXT("FALLADA");
		LastRingColor = Outcome->bPassed ? SRingPanel::PrecisionColor(1.0) : SRingPanel::PrecisionColor(0.0);
		LastRingNote = Outcome->bForced ? TEXT("resultado fijado con SbTrial") : FString();
	}
	LastRingShownUntil = FPlatformTime::Seconds() + FMath::Max(RingResultSeconds, 0.f);
}

TOptional<Soulbound::Presentation::FDecisionView> ABoardViewActor::PanelViewNow() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return {};
	}
	return Soulbound::Presentation::PanelView(HostPtr->GetState(), HostPtr->GetHumanSeat(), HostPtr->IsWardenSpeaking(), VoteResultSeconds, OutcomeSeconds);
}

float ABoardViewActor::LayerSlateWidth() const
{
	// La geometría de la capa en el último tick (en unidades de Slate, ya sin la escala de DPI); vacía hasta que se dibuja por primera vez.
	if (WardenLineOverlay.IsValid())
	{
		const float Local = static_cast<float>(WardenLineOverlay->GetTickSpaceGeometry().GetLocalSize().X);
		if (Local >= 1.f)
		{
			return Local;
		}
	}
	return static_cast<float>(LastViewportSize.X) / PanelDpiScale;
}

float ABoardViewActor::BoardFreeWidthNow(float Cap) const
{
	return Soulbound::Presentation::BoardFreeWidth(LayerSlateWidth(), PanelSlateWidth, Cap);
}

float ABoardViewActor::WardenLineReservedHeight() const
{
	// R-DEC-9: WardenLineReservedLines líneas de la letra de la frase, más el margen de arriba y de abajo de su fondo (los 10 del SBorder).
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(WardenLineFontSize, 8));
	float LineHeight = static_cast<float>(FMath::Max(WardenLineFontSize, 8));
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		LineHeight = static_cast<float>(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->GetMaxCharacterHeight(Font, 1.f));
	}
	return FMath::Max(WardenLineReservedLines, 0) * LineHeight + 2.f * 10.f;
}

bool ABoardViewActor::IsDecisionPanelVisible() const
{
	// La regla (decisión abierta, sin anillo) la calcula TickHud; la tecla de los paneles y el telón mandan en el acto, como en las demás capas.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	return bDecisionShown && !bPanelsHidden && HostPtr && !HostPtr->IsCurtainDown() && !HostPtr->GetRingRun();
}

float ABoardViewActor::DecisionTimeFractionNow() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return 0.f;
	}
	return static_cast<float>(Soulbound::Presentation::DecisionTimeFraction(DecisionClock.TotalSeconds, DecisionDeadlineSeconds, HostPtr->GetState().ClockSeconds));
}

void ABoardViewActor::AnswerDecision(const Soulbound::Presentation::FDecisionOption& Option)
{
	// R-DEC-2: el botón entrega lo mismo que la tecla, por el controlador del jugador local; nunca al motor directamente.
	if (ASpikeOnlinePlayerController* Controller = GetWorld() ? Cast<ASpikeOnlinePlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr)
	{
		Controller->AnswerDecisionOption(Option);
	}
}

FRingPanelModel ABoardViewActor::RingPanelModel() const
{
	FRingPanelModel Model;
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return Model;
	}
	const double Now = FPlatformTime::Seconds();
	if (const FRingTrialRun* Run = HostPtr->GetRingRun())
	{
		const int32 Total = Run->NumPulses();
		const TArray<double>& Done = Run->Precisions();
		Model.bVisible = true;
		Model.bShowRing = true;
		Model.Title = LastRingTitle;
		// En el anuncio, la instrucción bajo el título; los pulsos que vienen ya se ven en las marcas de abajo.
		Model.Counter = Run->IsAnnouncing(Now)
			? FString(TEXT("Haz clic cuando el anillo toque la runa"))
			: FString::Printf(TEXT("pulso %d de %d"), FMath::Min(Done.Num() + 1, Total), Total);
		Model.CloseFraction = Run->CloseFraction(Now);
		Model.ZoneFraction = Run->Sequence.CloseSeconds > 0.0 ? 0.5 * Run->Sequence.SweetZoneSeconds / Run->Sequence.CloseSeconds : 0.0;
		for (int32 Pulse = 0; Pulse < Total; ++Pulse)
		{
			Model.Pulses.Add(Done.IsValidIndex(Pulse) ? TOptional<double>(Done[Pulse]) : TOptional<double>());
		}
		// Resuelto un pulso, su precisión se enseña hasta que empieza el siguiente.
		if (!Done.IsEmpty() && Run->CurrentPulse(Now) == INDEX_NONE)
		{
			Model.Big = FString::Printf(TEXT("%.2f"), Done.Last());
			Model.BigColor = SRingPanel::PrecisionColor(Done.Last());
		}
		return Model;
	}
	if (Now < LastRingShownUntil)
	{
		Model.bVisible = true;
		Model.Title = LastRingTitle;
		Model.Counter = LastRingNote;
		Model.Big = LastRingResult;
		Model.BigColor = LastRingColor;
		for (const double Precision : LastRingPrecisions)
		{
			Model.Pulses.Add(Precision);
		}
	}
	return Model;
}

FString ABoardViewActor::RingTitle(ESkillTrial Trial, bool bDuel, ESkillDifficulty Difficulty)
{
	const TCHAR* Use = bDuel ? TEXT("DUELO")
		: Trial == ESkillTrial::ForgottenSource ? TEXT("FUENTE DE UN DOMINIO OLVIDADO")
		: Trial == ESkillTrial::WardenTrial ? TEXT("PRUEBA DEL WARDEN")
		: Trial == ESkillTrial::VoidEscape ? TEXT("ESCAPE DEL VACÍO")
		: Trial == ESkillTrial::FinalChallenge ? TEXT("DESAFÍO FINAL")
		: TEXT("PRUEBA");
	return FString::Printf(TEXT("%s · %s"), Use, *Soulbound::Presentation::DifficultyText(static_cast<int32>(Difficulty)));
}
