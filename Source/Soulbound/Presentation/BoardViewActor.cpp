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
#include "DecisionPanel.h"
#include "ElectCardPanel.h"
#include "EventText.h"
#include "GrimoirePanel.h"
#include "InstructionTracker.h"
#include "LegendPanel.h"
#include "StatusPanel.h"
#include "StatusText.h"
#include "TokenLook.h"
#include "Match/BondRaceRules.h"
#include "Match/RuneRules.h"
#include "Match/VictoryRules.h"
#include "Match/WardenEventRules.h"
#include "PresentationShapes.h"
#include "RingPanel.h"
#include "RingPanelModel.h"
#include "Engine/Texture2D.h"
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
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

// S-027: los textos de asiento que usan la leyenda y las etiquetas viven en StatusText (antes, en el espacio anónimo de este archivo).
using Soulbound::Presentation::BracerText;
using Soulbound::Presentation::DomainLegendText;
using Soulbound::Presentation::DomainTitle;
using Soulbound::Presentation::EffectStatusText;
using Soulbound::Presentation::ElectName;
using Soulbound::Presentation::SeatLegendText;
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
			// S-042 (R-MIN-19, decisión del autor del 16/11/2026): mientras la prueba espera, el clic es del botón "Empezar" de Slate y no se entrega
			// como pulsación del anillo. Así el clic que empieza la prueba nunca cuenta; uno fuera del botón no hace nada ni deja línea.
			const FRingTrialRun* Run = HostPtr->GetRingRun();
			if (Run && Run->IsWaiting())
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
	/**
	 * La marca del Sello (S-037): su cota sobre la losa de la casilla y su alto, en cm. Por encima del halo de alcanzable (cima en 0,145) y del aro de Dominio Olvidado (cima en
	 * 0,21), para que se vea sobre los dos y, por ser menos ancha que el halo, deje ver su borde blanco.
	 */
	constexpr float SealMarkZ = 0.14f;
	constexpr float SealMarkHeight = 0.3f;
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
	// S-029: solo corre mientras hay un destello de una Fuente (Tick); las fichas llevan los suyos.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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
	// S-040 (R-VIS-27): el material del suelo de los Olvidados se carga una vez, como la runa del anillo (S-031) y el dado (S-039). Sin él, el suelo como antes.
	ForgottenFloor = nullptr;
	if (ForgottenFloorMaterial.IsNull())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Suelo de los Olvidados: ForgottenFloorMaterial está vacío; el suelo se pinta con el material del kit, apagado por ForgottenDim."));
	}
	else
	{
		ForgottenFloor = Cast<UMaterialInterface>(ForgottenFloorMaterial.TryLoad());
		if (!ForgottenFloor)
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Suelo de los Olvidados: no existe el material %s; el suelo se pinta con el material del kit, apagado por ForgottenDim."),
				*ForgottenFloorMaterial.ToString());
		}
	}
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
	// S-029 (añadido del autor del 12/11/2026): los destellos y la carta empiezan en el final del registro; lo ya ocurrido no destella.
	FlashScanned = InHost->GetState().EventLog.Num();
	ElectCard.SkipExisting(InHost->GetState());
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
		WardenLineContent.Reset();
		WardenLineBox.Reset();
	}
	if (CutDimOverlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(CutDimOverlay.ToSharedRef());
			}
		}
		CutDimOverlay.Reset();
	}
	if (NoticeOverlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(NoticeOverlay.ToSharedRef());
			}
		}
		NoticeOverlay.Reset();
	}
	if (ElectCardOverlay.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(ElectCardOverlay.ToSharedRef());
			}
		}
		ElectCardOverlay.Reset();
		ElectCardPanel.Reset();
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
	// S-027: la caja de la partida (S-045) con el panel de estado y el de depuración, y el Grimorio.
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
	FitAltarHalo();
}

void ABoardViewActor::FitAltarHalo()
{
	// R-DEC-12 (S-033): el halo del Altar sale del componente real de su pieza ya colocado, no de la malla por la escala del kit.
	const int32 Altar = Host->GetState().Board.FindAltar();
	UStaticMeshComponent* Halo = Halos.IsValidIndex(Altar) ? Halos[Altar].Get() : nullptr;
	UStaticMesh* AltarPiece = Kit.Mesh(EBoardKitPiece::Altar);
	if (!Halo || !AltarPiece)
	{
		return;
	}
	UStaticMeshComponent* Piece = nullptr;
	for (const TObjectPtr<UStaticMeshComponent>& Component : KitFixed.Fixed)
	{
		if (Component && Component->GetStaticMesh() == AltarPiece)
		{
			Piece = Component;
			break;
		}
	}
	if (!Piece)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Halo del Altar: no encuentro el componente de su pieza entre los del kit; se queda el del tablero básico."));
		return;
	}
	AltarPieceComponent = Piece;
	// Los límites en mundo de la pieza: su centro, y la mayor de sus medidas X o Y (el Altar es un hexágono y el AABB lleva sus esquinas).
	const FBoxSphereBounds Bounds = Piece->CalcBounds(Piece->GetComponentTransform());
	const float Width = Soulbound::Presentation::HaloDiameter(Layout.AltarApothem * 2.f, HaloScale, static_cast<float>(Bounds.BoxExtent.X) * 2.f,
		static_cast<float>(Bounds.BoxExtent.Y) * 2.f, AltarHaloMargin);
	Halo->SetWorldLocation(FVector(Bounds.Origin.X, Bounds.Origin.Y, AltarHaloZ));
	Halo->SetWorldScale3D(FVector(Width / 100.f, Width / 100.f, HaloHeight / 100.f));
	UE_LOG(LogSoulboundView, Verbose, TEXT("Halo del Altar: pieza con origen (%.2f, %.2f, %.2f) y semiextensión (%.2f, %.2f, %.2f) en mundo, base en Z %.2f; halo en (%.2f, %.2f, %.2f), diámetro %.2f (casilla %.2f, HaloScale %.2f, AltarHaloMargin %.2f), alto %.2f, KitFloorHeight %.2f."),
		Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z, Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z, Bounds.Origin.Z - Bounds.BoxExtent.Z,
		Bounds.Origin.X, Bounds.Origin.Y, AltarHaloZ, Width, Layout.AltarApothem * 2.f, HaloScale, AltarHaloMargin, HaloHeight, LayoutParams.KitFloorHeight);
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
	SealMarks.Init(nullptr, Board.Tiles.Num());
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
		// El del Altar con la pieza del kit se recoloca y redimensiona en BuildKit (FitAltarHalo), cuando la pieza ya existe (R-DEC-12).
		UStaticMeshComponent* Halo = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, HaloZ), FRotator::ZeroRotator, FVector(Diameter * HaloScale, Diameter * HaloScale, HaloHeight));
		Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, ReachableColor);
		Halo->SetVisibility(false);
		Halos[Tile.TileId] = Halo;

		// Aro de Dominio Olvidado: más alto que el halo, para que se vea también en una casilla alcanzable.
		UStaticMeshComponent* Ring = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, ForgottenRingZ), FRotator::ZeroRotator,
			FVector(Diameter * ForgottenRingScale, Diameter * ForgottenRingScale, ForgottenRingHeight));
		Soulbound::Presentation::SetComponentColor(*Ring, BaseMaterial, ForgottenRingColor);
		Ring->SetVisibility(false);
		ForgottenRings[Tile.TileId] = Ring;

		// S-037 (R-VIS-24): la marca del Sello, otro disco como los dos anteriores (uno por casilla, no un aro de segmentos): del tamaño de la casilla por SealMarkScale,
		// entre el aro de Dominio Olvidado y el halo, por encima de los dos. Oculta hasta que la casilla se sella.
		UStaticMeshComponent* SealMark = AddShape(CylinderMesh, Position + FVector(0.f, 0.f, SealMarkZ), FRotator::ZeroRotator,
			FVector(Diameter * SealMarkScale, Diameter * SealMarkScale, SealMarkHeight));
		Soulbound::Presentation::SetComponentColor(*SealMark, BaseMaterial, SealMarkColor);
		SealMark->SetVisibility(false);
		SealMarks[Tile.TileId] = SealMark;

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
			Token->Configure(TokenLabelSize, ActiveLift, ActiveSpinDegreesPerSecond, RingDiameter, RingWidth, ActiveScale, TokenDiameter, TokenLabelHeight);
			// S-029 (corrección del 13/11/2026): el aro del destello, más grueso y creciendo hacia fuera mientras dura.
			Token->ConfigureFlash(FlashRingScale, FlashRingWidth, FlashGrowth);
			// R-VIS-6: la carta de prueba del kit, con el color del asiento en el aro de su base (sin carta, la esfera o el cono).
			if (UStaticMeshComponent* CardMesh = Token->UseCard(Card))
			{
				Kit.ApplyMaterials(*CardMesh);
			}
			// S-029 (R-VIS-10): el disco de rol, de la huella de la carta, sin pasar del borde interior del aro del turno; el disco de cada asiento, un poco
			// más alto que el del anterior.
			Token->SetRoleDisc(Soulbound::Presentation::RoleDiscDiameter(Token->GetFootprint(), RoleDiscScale, Soulbound::Presentation::RoleDiscMaxDiameter(RingDiameter, RingWidth)),
				Soulbound::Presentation::RoleDiscLift(Seat, RoleDiscLift), RoleDiscBrightness);
			Token->SetRingPulse(ActiveRingPulseSeconds, ActiveRingPulseMin);
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
		bOracleOrbShown = false;
		return;
	}
	// Siempre sobre la ficha del consultante, también si consulta desde un Nodo (decisión del autor del 26/10/2026).
	const ASeatTokenActor* Token = TokenActors.IsValidIndex(Seat) ? TokenActors[Seat].Get() : nullptr;
	const FVector Base = Token ? Token->GetActorLocation() : TokenBase(State.Seats[Seat].TileId);
	OracleOrb->SetWorldLocation(Base + FVector(0.f, 0.f, OracleOrbHeight));
	Soulbound::Presentation::SetComponentColor(*OracleOrb, BaseMaterial, SeatColor(Seat));
	OracleOrb->SetVisibility(true);
	// S-041 (R-VIS-29): mientras la consulta espera, el actor tica para que el orbe lata (Tick, OracleOrbScale).
	bOracleOrbShown = true;
	ScaleOracleOrb(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	SetActorTickEnabled(true);
}

void ABoardViewActor::ScaleOracleOrb(double Seconds)
{
	if (OracleOrb)
	{
		const float Scale = Soulbound::Presentation::OracleOrbScale(Seconds, OracleOrbPulseSeconds, OracleOrbPulseMin);
		OracleOrb->SetWorldScale3D(FVector(FMath::Max(OracleOrbDiameter, 0.1f) * Scale / 100.f));
	}
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
	// S-027 (R-HUD-3): la letra de los paneles sigue a la altura de la pantalla.
	UpdatePanelScale(Viewport);

	// Cámara cenital fija: el tablero entero, con el margen CameraMargin. Desde S-045 (R-HUD-16) no hay columna: usa todo el ancho (TopCameraFraming).
	const Soulbound::Presentation::FTopCameraFraming Framing = Soulbound::Presentation::TopCameraFraming(BoardHalf, Viewport, CameraMargin);
	Camera->SetActorLocation(FVector(BoardCenter.X, BoardCenter.Y + Framing.ShiftY, Framing.Height));
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

	// S-045 (R-HUD-16): la columna de la derecha, con la leyenda y la crónica, desaparece; su contenido está en las pestañas del Grimorio (S-044).

	// S-027 (R-HUD-3): la letra de todos los paneles se lee en cada pintado, para que siga a la resolución sin rehacerlos.
	const TAttribute<int32> PanelFont = TAttribute<int32>::CreateLambda([this]() { return PanelSlateFontSize; });

	// S-045 (R-HUD-14, R-HUD-15): arriba a la izquierda, la caja de la partida (el reloj y los avisos de mesa), con el aspecto de la caja del objetivo; debajo,
	// el panel de estado y el de depuración, cada uno solo si su tecla lo enseña (T y D), en ese orden (decisión del autor del 17/11/2026). Ninguno recibe
	// clics: el tablero de debajo sigue respondiendo. La capa se oculta con el telón; la caja de la partida, además, en el prólogo y con la partida
	// terminada, y ninguna tecla la oculta. Sin columna, el panel de estado ya no reserva su hueco a la derecha.
	StatusOverlay = SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(8.f, 8.f, 16.f, 8.f))
		.Visibility_Lambda([this]()
		{
			const UMatchHostSubsystem* HostPtr = Host.Get();
			return HostPtr && HostPtr->IsCurtainDown() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(MatchPanel, SStatusPanel)
				.FontSize(PanelFont)
				.LargeScale(StatusLargeFontScale)
				.Visibility_Lambda([this]() { return IsMatchPanelVisibleNow() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SAssignNew(StatusPanel, SStatusPanel)
				.FontSize(PanelFont)
				.LargeScale(StatusLargeFontScale)
				.Visibility_Lambda([this]() { return HudToggles.bStatusPanelShown ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SAssignNew(DebugPanel, SStatusPanel)
				.FontSize(PanelFont)
				.Title(FString::Printf(TEXT("DEPURACIÓN (%s)"), *DebugPanelKey.GetDisplayName().ToString()))
				.Visibility_Lambda([this]() { return HudToggles.bDebugPanelShown ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
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
	// S-044 (R-HUD-10): "Cómo se juega" con el mismo archivo y el mismo cargador que el menú; si falta, la pestaña enseña el mensaje del cargador.
	HowToPlayLoaded = Soulbound::Presentation::LoadHowToPlayText(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath));
	if (!HowToPlayLoaded.IsSuccess())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Grimorio: %s"), *HowToPlayLoaded.Message);
	}
	GrimoireTab.Open();
	GrimoireChronicleCount = -1;
	// S-044: la capa ya no deja pasar los clics por encima del panel (las pestañas y la rueda son del Grimorio); fuera del panel, sí.
	GrimoireOverlay = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(16.f))
		.Visibility_Lambda([this]()
		{
			const UMatchHostSubsystem* HostPtr = Host.Get();
			return bGrimoireOpen && !(HostPtr && HostPtr->IsCurtainDown()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
		})
		[
			SAssignNew(GrimoirePanel, SGrimoirePanel)
			.FontSize(PanelFont)
			.DomainColors(RuneChipColors)
			// Corrección del autor del 17/11/2026 tras PIE: los nombres de las teclas, en español donde lo haya (GrimoireKeysHint).
			.KeysHint(Soulbound::Presentation::GrimoireKeysHint(GrimoireKey.GetDisplayName().ToString(), GrimoirePreviousTabKey.GetDisplayName().ToString(),
				GrimoireNextTabKey.GetDisplayName().ToString()))
			// S-029 (R-VIS-21): ancho fijo, el de la capa; no depende de lo pintado antes.
			.PanelWidth_Lambda([this]() { return Soulbound::Presentation::GrimoirePanelWidth(LayerSlateWidth()); })
			// S-044: alto fijo, una fracción del de la pantalla menos el margen de la capa; no cambia al cambiar de pestaña.
			.PanelHeight_Lambda([this]()
			{
				const float Height = static_cast<float>(LastViewportSize.Y) / FMath::Max(PanelDpiScale, 0.01f);
				return FMath::Max(Height * GrimoireHeightFraction - 32.f, 200.f);
			})
			.ActiveTab_Lambda([this]() { return GrimoireTab.Active; })
			.OnTabClicked_Lambda([this](Soulbound::Presentation::EGrimoireTab Tab) { GrimoireTab.Select(Tab); })
		];
	Viewport->AddViewportWidgetContent(GrimoireOverlay.ToSharedRef(), 14);
	GrimoirePanel->SetBoard(Soulbound::Presentation::BoardLegendRows(BoardLegendLookNow()));
	GrimoirePanel->SetHowToPlay(Soulbound::Presentation::GrimoireHowToPlayText(HowToPlayLoaded, {GrimoireKey.GetDisplayName().ToString(),
		GrimoirePreviousTabKey.GetDisplayName().ToString(), GrimoireNextTabKey.GetDisplayName().ToString(), SeatedViewKey.GetDisplayName().ToString(),
		OverlayToggleKey.GetDisplayName().ToString(), RingStartKey.GetDisplayName().ToString(), ReturnToMenuKey.GetDisplayName().ToString()}));

	// S-029 (R-VIS-13): la carta del Elegido. Como la capa de la frase (R-DEC-13) y la del anillo (R-VIS-20), la capa ocupa el hueco libre (HAlign_Fill,
	// con sus márgenes) y la carta se centra dentro; AlignChild no centra dentro del hueco con márgenes desiguales. Abajo se deja libre una fracción
	// de la pantalla (ElectCardBottomFraction) para el panel de decisión y la frase del Warden. La capa no recibe clics; la carta sí (cierra).
	ElectCardOverlay = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding_Lambda([this]()
		{
			const float Height = static_cast<float>(LastViewportSize.Y) / FMath::Max(PanelDpiScale, 0.01f);
			return FMargin(16.f, 0.f, 16.f, Height * ElectCardBottomFraction);
		})
		.Visibility_Lambda([this]() { return IsElectCardShown() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SAssignNew(ElectCardPanel, SElectCardPanel)
				.FontSize(PanelFont)
				.DomainColors(RuneChipColors)
				.GrimoireKey(GrimoireKey.GetDisplayName().ToString())
				.PanelWidth_Lambda([this]() { return FMath::Max(BoardFreeWidthNow(ElectCardMaxWidth) - 8.f, 1.f); })
				.OnDismissed(FSimpleDelegate::CreateUObject(this, &ABoardViewActor::DismissElectCard))
			]
		];
	Viewport->AddViewportWidgetContent(ElectCardOverlay.ToSharedRef(), 13);

	// S-028 (R-DEC-4): el ancho máximo del panel de decisión, en unidades de Slate; dentro, sus botones van en fila si caben.
	static constexpr float DecisionPanelMaxWidth = 900.f;
	// S-021: la línea del Warden, grande y abajo. Solo texto: ni assets ni widgets del juego.
	// S-028 (R-DEC-2 a R-DEC-4): encima, en la misma columna, el panel de decisión, centrado y con la letra de los paneles; va siempre por
	// encima de la frase. El panel se queda los clics que le caen encima (SDecisionPanel); la capa y la frase siguen sin recibirlos.
	// R-DEC-13 (S-033, corrección del autor del 12/11/2026): la capa ocupa el hueco libre (HAlign_Fill, con sus márgenes) y el contenido se centra dentro,
	// sin márgenes laterales. Con HAlign_Center y márgenes desiguales, AlignChild (LayoutUtils.h) coloca en (ancho - hijo) / 2 + izquierdo - derecho: todo
	// quedaba media columna a la izquierda del centro del hueco y a 1024x768 se cortaba. S-045 (R-HUD-16): sin columna, el hueco es todo el ancho, con 16
	// a cada lado; el panel de decisión, la frase, la caja del objetivo y el botón del Grimorio se colocan en él.
	WardenLineOverlay = SNew(SBox)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(16.f, 0.f, 16.f, 36.f))
	.Visibility(EVisibility::SelfHitTestInvisible)
	[
		SAssignNew(WardenLineContent, SOverlay)
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
					// S-039 (R-DEC-15, R-DEC-16): la cara del dado (con imagen si existe, si no dibujada por código) y la línea del aviso, en el color de la
					// línea del Warden.
					.DiceBrush([this]() -> const FSlateBrush* { return DiceTexture ? &DiceBrush : nullptr; })
					.DiceFaceSize_Lambda([this]() { return DiceFaceSize; })
					.DiceColor_Lambda([this]() { return DiceFaceColor; })
					.DicePipColor_Lambda([this]() { return DiceFacePipColor; })
					.TimeFraction_Lambda([this]() { return DecisionTimeFractionNow(); })
					// Lo más ancho que puede ser el panel: el hueco del tablero (la capa deja 16 a cada lado; desde S-045 no hay columna), con el
					// tope de la caja. Con él decide si sus botones caben en fila (corrección del autor del 11/11/2026).
					.MaxWidth_Lambda([this]()
					{
						return BoardFreeWidthNow(DecisionPanelMaxWidth);
					})
					.OnOption_Lambda([this](const Soulbound::Presentation::FDecisionOption& Option) { AnswerDecision(Option); })
				]
			]
			// S-033 (R-DEC-9): el hueco de la frase, siempre reservado, haya frase o no, como espacio vacío: el panel de encima no se mueve cuando la
			// frase aparece, cambia de largo o se va. Su alto sale de la letra de la frase y de sus márgenes (WardenLineReservedHeight). Desde S-041
			// (R-DEC-17) crece si la frase y su línea pequeña no caben en él, para que nunca tapen el panel (WardenLineBandHeightNow).
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride_Lambda([this]() { return FOptionalSize(WardenLineBandHeightNow()); })
			]
		]
		// S-040 (R-HUD-7; corrección del autor del 16/11/2026 tras PIE): la caja de la línea de objetivo, abajo a la izquierda del hueco del tablero, dentro
		// de la franja que se reserva a la frase (por debajo del panel de decisión, que va encima de esa franja: no se pisan). Fondo y letra grande de los
		// paneles, con GoalLineColor. Alineada a la izquierda para medir lo que su texto, que se parte al ancho libre del hueco (WrapWidth, no AutoWrapText:
		// tras un texto corto, el largo del Eco saldría estrecho) y nunca pasa del margen derecho de esta capa. Ninguna tecla la oculta;
		// IsGoalPanelVisibleNow la quita en el prólogo, con la meta None, mientras hay frase del Warden (cada frame: nunca se ven encimadas) y con el telón.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
		[
			SAssignNew(GoalPanel, SStatusPanel)
			.FontSize(TAttribute<int32>::CreateLambda([this]() { return PanelSlateFontSize; }))
			.LargeScale(StatusLargeFontScale)
			.WrapWidth_Lambda([this]() { return FMath::Max(BoardFreeWidthNow(DecisionPanelMaxWidth) - 16.f, 1.f); })
			.Visibility_Lambda([this]() { return IsGoalPanelVisibleNow() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]
		// S-044 (R-HUD-12; corrección del autor del 17/11/2026): el botón del Grimorio, abajo a la derecha del hueco del tablero, en la misma franja y con la
		// misma regla que la caja de objetivo, en espejo: no se ve en el prólogo, con la partida terminada ni mientras hay frase del Warden (la frase no
		// se estrecha por él), y ninguna tecla de ocultar lo oculta. Abre y cierra como la tecla. El botón se queda su clic: no llega a las casillas. Sin
		// foco, para que no se lleve las teclas de la partida.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
		[
			SAssignNew(GrimoireButton, SBox)
			.Visibility_Lambda([this]() { return IsGrimoireButtonVisibleNow() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(12.f, 6.f))
				.OnClicked_Lambda([this]()
				{
					ToggleGrimoire();
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(Soulbound::Presentation::GrimoireButtonLabel(GrimoireKey.GetDisplayName().ToString())))
					.Font_Lambda([this]() { return FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(FMath::RoundToInt(PanelSlateFontSize * StatusLargeFontScale), 8)); })
				]
			]
		]
		// Capa de arriba: la frase, anclada abajo con el mismo margen que el hueco. Crece hacia arriba, por encima del panel si hace falta (corrección
		// del autor del 11/11/2026: en una caja de alto fijo se recortaba lo que sobraba). No recibe clics: el hueco vacío deja pasar los del tablero.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		[
			SAssignNew(WardenLineBox, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.65f))
			.Padding(FMargin(20.f, 10.f))
			.Visibility_Lambda([this]() { return WardenLineNow().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::FromString(WardenLineNow().Text); })
					// S-041 (R-VOZ-8): el Oráculo como golpe sin palabra de corte en la tabla no tiene frase, solo línea pequeña.
					.Visibility_Lambda([this]() { return WardenLineNow().Text.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
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
				// S-041 (R-VOZ-8): debajo de la frase de un corte, una línea pequeña y neutra que dice qué ha pasado; dura lo que su frase.
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::FromString(WardenLineNow().CutNote); })
					.Visibility_Lambda([this]() { return WardenLineNow().CutNote.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(CutNoteFontSize, 8)))
					.ColorAndOpacity(CutNoteColor)
					.Justification(ETextJustify::Center)
					.WrapTextAt_Lambda([this]()
					{
						return FMath::Max(BoardFreeWidthNow(1000.f) - 40.f, 1.f);
					})
				]
			]
		]
	];
	Viewport->AddViewportWidgetContent(WardenLineOverlay.ToSharedRef(), 11);

	// S-041 (R-VOZ-8): la capa oscura del corte. Llena el viewport sobre el tablero y por debajo de todas las demás capas (la de los paneles va en el 10):
	// negra con la opacidad de CutDimOpacityNow: desde la corrección del autor del 16/11/2026, varios pulsos seguidos que aparecen de golpe y se
	// desvanecen. Sin posproceso (restricción de rendimiento) y sin clics.
	// S-043 (R-VIS-31): la misma capa lleva la penumbra de la Noche de los Ecos; BoardDimColorNow da el más oscuro de los dos con su color.
	CutDimOverlay = SNew(SImage)
		.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.ColorAndOpacity_Lambda([this]() { return FSlateColor(BoardDimColorNow()); })
		.Visibility_Lambda([this]() { return BoardDimColorNow().A > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; });
	Viewport->AddViewportWidgetContent(CutDimOverlay.ToSharedRef(), 9);

	// S-039 (R-DEC-16; corrección del autor del 15/11/2026 tras las pruebas en PIE): el aviso de casilla inalcanzable sale junto al punto del clic, no en
	// el panel de decisión: los ojos están en la casilla pulsada. Una capa que llena el viewport, sin clics (la casilla de debajo sigue respondiendo), con
	// el aviso en la esquina que le da PlaceNotice: dentro del viewport y sin quedar debajo de las zonas de NoticeAvoidZones (S-040; desde S-045, sin columna). Mismo texto y
	// color que la línea del Warden; fondo oscuro como los paneles, para leerse sobre el tablero. Se recoloca cada frame con los mismos datos, así que
	// se queda quieto; el texto y su vida los pone TickHud.
	{
		const int32 NoticeFont = FMath::Max(FMath::RoundToInt(static_cast<float>(PanelSlateFontSize) * 1.2f), 10);
		NoticeOverlay = SNew(SBox)
			.Visibility_Lambda([this]()
			{
				const UMatchHostSubsystem* HostPtr = Host.Get();
				return NoticeText.IsEmpty() || (HostPtr && HostPtr->IsCurtainDown()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
			})
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
				[
					SNew(SBox)
					.Padding_Lambda([this, NoticeFont]()
					{
						return NoticeMarginNow(NoticeFont);
					})
					[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.78f))
					.Padding(FMargin(NoticePadding))
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FText::FromString(NoticeText); })
						.Font_Lambda([NoticeFont]() { return FCoreStyle::GetDefaultFontStyle("Bold", NoticeFont); })
						.ColorAndOpacity_Lambda([this]() { return WardenLineColor; })
						.WrapTextAt_Lambda([this]() { return NoticeWrapAt; })
					]
					]
				]
			];
		Viewport->AddViewportWidgetContent(NoticeOverlay.ToSharedRef(), 13);
	}

	// S-022 (decisión del autor del 29/10/2026): con la partida terminada, un botón grande para volver al menú, con el ratón (el controlador
	// deja el cursor visible y la entrada en juego y UI). El botón va en el centro del tablero (desde S-045, R-HUD-16, a todo el ancho: ya no hay columna);
	// desde S-041 (R-HUD-8), con el resultado encima y la tecla debajo. Antes del final no existe; pedido, se desactiva.
	// R-VIS-20 (S-031, corrección del autor del 12/11/2026): como la capa de la frase (R-DEC-13), la capa ocupa el hueco libre (HAlign_Fill, con sus
	// márgenes) y el botón se centra dentro, sin márgenes laterales; con HAlign_Center y márgenes desiguales, AlignChild lo dejaba media columna a la
	// izquierda del centro del hueco. La caja interior, que no recibe clics por sí misma, solo centra.
	ReturnToMenuOverlay = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(16.f, 0.f, 16.f, 0.f))
		.Visibility_Lambda([this]() { return IsMatchOverShown() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				// S-041 (R-HUD-8, añadido del autor del 16/11/2026): el resultado en grande encima del botón, con su color (MatchResultLine), y la tecla
				// debajo, en pequeño; antes los dos iban arriba a la izquierda, en el panel de estado. Los textos no reciben clics.
				// Ajuste del autor del 16/11/2026 tras PIE: los dos textos llevan el fondo oscuro de los paneles (negro a 0,72, como StatusPanel), porque
				// sin fondo el rojo se mezclaba con las casillas y sus números.
				SNew(SVerticalBox)
				.Visibility(EVisibility::SelfHitTestInvisible)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 16.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
					.Padding(FMargin(16.f, 8.f))
					.Visibility(EVisibility::HitTestInvisible)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FText::FromString(MatchResultLineNow().Text); })
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(MatchResultLineNow().Color); })
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(MatchResultFontSize, 8)))
						.ShadowOffset(FVector2D(2.f, 2.f))
						.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
						.Justification(ETextJustify::Center)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
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
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
					.Padding(FMargin(8.f, 4.f))
					.Visibility(EVisibility::HitTestInvisible)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Soulbound::Presentation::ReturnToMenuHint(ReturnToMenuKey.GetDisplayName().ToString())))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(ReturnToMenuHintFontSize, 8)))
						.ShadowOffset(FVector2D(1.f, 1.f))
						.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
						.Justification(ETextJustify::Center)
					]
				]
			]
		];
	Viewport->AddViewportWidgetContent(ReturnToMenuOverlay.ToSharedRef(), 12);

	// S-026: el panel del anillo, centrado en el hueco del tablero como el botón del final. No recibe clics: la entrada va por RingInput (desde S-042,
	// salvo el botón "Empezar" mientras la prueba espera).
	// R-VIS-20: igual que el botón de volver: la capa llena el hueco y la caja interior centra el panel.
	// S-031 (R-VIS-18): la textura de la runa se carga aquí, una vez; el pincel la apunta y el actor la mantiene viva (RuneTexture es UPROPERTY).
	RuneTexture = nullptr;
	RuneBrush = FSlateBrush();
	if (RingRuneTexture.IsNull())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Anillo: RingRuneTexture está vacío; la runa se pinta como un disco relleno."));
	}
	else
	{
		RuneTexture = Cast<UTexture2D>(RingRuneTexture.TryLoad());
		if (RuneTexture)
		{
			RuneBrush.SetResourceObject(RuneTexture);
			RuneBrush.ImageSize = FVector2f(RuneTexture->GetSizeX(), RuneTexture->GetSizeY());
			RuneBrush.DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Anillo: no existe la textura de la runa %s; se pinta un disco relleno."), *RingRuneTexture.ToString());
		}
	}
	// S-039 (R-DEC-15): la imagen de las caras del dado se carga aquí, una vez, como la runa. Sin imagen, el panel de decisión dibuja la cara por código.
	DiceTexture = nullptr;
	DiceBrush = FSlateBrush();
	if (DiceFacesTexture.IsNull())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Dado: DiceFacesTexture está vacío; la cara del dado se dibuja por código."));
	}
	else
	{
		DiceTexture = Cast<UTexture2D>(DiceFacesTexture.TryLoad());
		if (DiceTexture)
		{
			DiceBrush.SetResourceObject(DiceTexture);
			DiceBrush.ImageSize = FVector2f(DiceTexture->GetSizeX(), DiceTexture->GetSizeY());
			DiceBrush.DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			UE_LOG(LogSoulboundView, Warning, TEXT("Dado: no existe la textura de las caras %s; la cara del dado se dibuja por código."), *DiceFacesTexture.ToString());
		}
	}
	// S-042 (R-MIN-19): mientras la prueba espera, el botón "Empezar", su tecla y la cuenta atrás van en una fila dentro del panel, en su franja bajo el
	// aro, entre el círculo y la barra (corrección del autor del 16/11/2026: en el centro tapaban la runa y la franja verde). La fila es el hijo del
	// panel y la coloca él (SRingPanel::OnArrangeChildren) con RingPanelLayout y su tamaño de verdad, el mismo con que pinta: la vista no hace la
	// cuenta aparte (segunda corrección del 16/11/2026: un hueco de SOverlay con margen la dejaba por debajo del panel). El panel reserva la franja
	// siempre, así que no cambia de tamaño ni se mueve al empezar. Solo el botón recibe clics: el panel y el resto de la capa los dejan pasar. Desde S-045
	// (decisión del autor del 17/11/2026) el anillo se centra a todo el ancho, como la carta del Elegido: ya no hay columna.
	const int32 StartHintFontSize = FMath::Max(FMath::RoundToInt(RingFontSize * 0.8f), 8);
	const int32 RingPanelFont = FMath::Max(RingFontSize, 8);
	RingOverlay = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(16.f, 0.f, 16.f, 0.f))
		.Visibility_Lambda([this]() { return RingPanelModel().bVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SNew(SRingPanel)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.FontSize(RingFontSize)
				.Model([this]() { return RingPanelModel(); })
				.RuneBrush([this]() -> const FSlateBrush* { return RuneTexture ? &RuneBrush : nullptr; })
				[
					SNew(SHorizontalBox)
					.Visibility_Lambda([this]() { return RingPanelModel().StartLabel.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton)
						.IsFocusable(false)
						.ContentPadding(FMargin(24.f, 8.f))
						.OnClicked_Lambda([this]()
						{
							StartRingNow();
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return FText::FromString(RingPanelModel().StartLabel); })
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", RingPanelFont))
							.Justification(ETextJustify::Center)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
						.Padding(FMargin(8.f, 4.f))
						.Visibility(EVisibility::HitTestInvisible)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return FText::FromString(RingPanelModel().StartKeyHint); })
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", StartHintFontSize))
								.Justification(ETextJustify::Center)
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return FText::FromString(RingPanelModel().StartCountdown); })
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", StartHintFontSize))
								.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
								.Justification(ETextJustify::Center)
							]
						]
					]
				]
			]
		];
	Viewport->AddViewportWidgetContent(RingOverlay.ToSharedRef(), 13);
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
	// S-037 (R-VIS-24): las casillas con marca del Sello, del estado de vista.
	const TSet<int32> SealMarked(Soulbound::Presentation::SealMarkedTiles(State));
	TSet<int32> Reachable;
	if (State.TurnPhase == ETurnPhase::RollAndChoose)
	{
		Reachable.Append(State.ReachableOptions);
	}
	else if (State.PendingChoice.Kind == EChoiceKind::PlanarJump)
	{
		Reachable.Append(State.PendingChoice.Options);
	}

	// S-039 (R-VIS-26): mientras hay halos a la vista el actor tica para que latan; Tick lo apaga cuando ya no hay halos ni destellos.
	bReachableHalosShown = !Reachable.IsEmpty();
	if (bReachableHalosShown)
	{
		SetActorTickEnabled(true);
	}

	// S-023 (R-PRO-1): en el prólogo, un Dominio Olvidado se ve en juego hasta que se enseña su frase de corrupción; después, lo del estado.
	bDomainsShownInPrologue = HostPtr->IsInPrologue();
	DomainsShownOpeningLines = WardenLineQueue.ShownCount();
	DomainsShownForgotten = Soulbound::Presentation::ForgottenDomainsShown(State, bDomainsShownInPrologue, DomainsShownOpeningLines);

	// El suelo de cada cuña lleva el color de su Dominio, apagado si está Olvidado (S-019, R-VIS-2, R-VIS-5); desde S-040 (R-VIS-27), el suelo liso de
	// un Olvidado, con su material propio si está.
	Kit.TintWedgeFloors(KitFixed, DomainsShownForgotten, *this, ForgottenFloor.Get());

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
		if (UStaticMeshComponent* SealMarkDisc = SealMarks.IsValidIndex(Tile.TileId) ? SealMarks[Tile.TileId].Get() : nullptr)
		{
			// S-037 (R-VIS-24): la marca se ve mientras la casilla está sellada, y mientras dura un destello de Sello aunque ya no lo esté.
			SealMarkDisc->SetVisibility(SealMarked.Contains(Tile.TileId) || IsSealMarkFlashing(Tile.TileId));
			Soulbound::Presentation::SetComponentColor(*SealMarkDisc, BaseMaterial, SealMarkColor);
		}
		if (UStaticMeshComponent* Halo = Halos.IsValidIndex(Tile.TileId) ? Halos[Tile.TileId].Get() : nullptr)
		{
			const bool bHaloShown = Reachable.Contains(Tile.TileId);
			Halo->SetVisibility(bHaloShown);
			if (Tile.Type == ETileType::Altar && LoggedAltarHalo != (bHaloShown ? 1 : 0))
			{
				LoggedAltarHalo = bHaloShown ? 1 : 0;
				const FBoxSphereBounds PieceBounds = AltarPieceComponent.IsValid() ? AltarPieceComponent->CalcBounds(AltarPieceComponent->GetComponentTransform()) : FBoxSphereBounds(ForceInit);
				const FVector HaloLocation = Halo->GetComponentLocation();
				const FVector HaloScale3D = Halo->GetComponentScale();
				UE_LOG(LogSoulboundView, Verbose, TEXT("Halo del Altar: visible %d; halo en (%.2f, %.2f, %.2f), diámetro %.2f, alto %.2f; pieza con origen (%.2f, %.2f, %.2f) y semiextensión (%.2f, %.2f, %.2f)."),
					bHaloShown ? 1 : 0, HaloLocation.X, HaloLocation.Y, HaloLocation.Z, HaloScale3D.X * 100.0, HaloScale3D.Z * 100.0,
					PieceBounds.Origin.X, PieceBounds.Origin.Y, PieceBounds.Origin.Z, PieceBounds.BoxExtent.X, PieceBounds.BoxExtent.Y, PieceBounds.BoxExtent.Z);
			}
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
	RefreshEchoNightDim(State);
	RefreshFlashes(State);
	RefreshElectCard(State);
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
	// S-041: las frases nuevas del registro, con su duración (R-VOZ-6) y cada corte emparejado con su frase (R-VOZ-8), las mismas que cuentan la
	// espera y el prólogo del anfitrión. Un corte del Warden oscurece el tablero desde ahora; uno nuevo reinicia la capa.
	const double NowReal = FPlatformTime::Seconds();
	for (const Soulbound::Presentation::FWardenLineItem& Item :
		Soulbound::Presentation::WardenLineItems(State, WardenLineScanned, HumanSeat, WardenLineSeconds, WardenLineSecondsPerChar))
	{
		WardenLineQueue.Push(Item, NowReal);
		if (Item.bWardenCut)
		{
			CutDimStartReal = NowReal;
		}
	}
	WardenLineScanned = State.EventLog.Num();

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
		// S-041 (R-VOZ-6): la instrucción dura según su longitud; ni detiene la mesa ni es un comentario.
		Soulbound::Presentation::FWardenLineItem Item;
		Item.Text = Line;
		Item.Seconds = Soulbound::Presentation::WardenLineDuration(Line, FString(), WardenLineSeconds, WardenLineSecondsPerChar);
		Item.GameClock = Clock;
		WardenLineQueue.Push(Item, FPlatformTime::Seconds());
		InstructionNotes.Add({State.EventLog.Num() - 1, Clock, Line});
		// La crónica del Grimorio se reconstruye solo si crece el registro: la nota no lo hace crecer (desde S-045, la única crónica en pantalla).
		GrimoireChronicleCount = -1;
	}
}

bool ABoardViewActor::IsGoalPanelVisibleNow()
{
	// S-040 (R-HUD-7): la regla, en IsGoalPanelShown (desde S-045, a través de HudShown); el telón lo tapa todo, como a las demás capas.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || HostPtr->IsCurtainDown())
	{
		return false;
	}
	return HudShownNow().bGoalPanel;
}

bool ABoardViewActor::IsGrimoireButtonVisibleNow()
{
	// S-044 (R-HUD-12): la regla, en IsGrimoireButtonShown (desde S-045, a través de HudShown), con el prólogo de la caja de objetivo (el mismo contexto
	// de TickHud); el telón lo tapa todo.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch() || HostPtr->IsCurtainDown())
	{
		return false;
	}
	return HudShownNow().bGrimoireButton;
}

Soulbound::Presentation::FHudShown ABoardViewActor::HudShownNow()
{
	// S-045 (R-HUD-15): una sola función decide qué piezas se ven; el prólogo y la meta los guarda TickHud, la frase del Warden es la de este frame.
	return Soulbound::Presentation::HudShown(HudToggles, bGoalPanelPrologue, IsMatchOverShown(), GoalPanelGoal, !WardenLineNow().IsEmpty());
}

bool ABoardViewActor::IsMatchPanelVisibleNow()
{
	// S-045 (R-HUD-14): la regla, en IsMatchPanelShown a través de HudShown; sin partida o con el telón, nada.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch() || HostPtr->IsCurtainDown())
	{
		return false;
	}
	return HudShownNow().bMatchPanel;
}

Soulbound::Presentation::FBoardLegendLook ABoardViewActor::BoardLegendLookNow() const
{
	Soulbound::Presentation::FBoardLegendLook Look;
	Look.bKit = TileHasKitSlab.Contains(true);
	Look.bForgottenFloor = ForgottenFloor != nullptr;
	Look.Altar = AltarColor;
	Look.NicheFree = NicheFreeColor;
	Look.Source = SourceColor;
	Look.Node = NodeColor;
	Look.NicheClaimed = SeatColor(0);
	Look.NicheClosed = NicheClosedColor;
	Look.Reachable = ReachableColor;
	Look.SealMark = SealMarkColor;
	Look.ForgottenRing = ForgottenRingColor;
	return Look;
}

Soulbound::Presentation::FChronicleLook ABoardViewActor::ChronicleLookNow() const
{
	Soulbound::Presentation::FChronicleLook Look;
	// S-026: los resultados del humano dicen "con el anillo" si lo juega (interruptor encendido); los de los dummies, al azar.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	Look.RingSeat = HostPtr && HostPtr->HumanPlaysSkillTrials() ? HostPtr->GetHumanSeat() : INDEX_NONE;
	Look.MaxEvents = ChronicleMaxLines;
	Look.SeatColors = SeatColors;
	Look.WardenColor = WardenLineColor;
	Look.RaceColor = RaceLineColor;
	Look.DebugColor = DebugLineColor;
	return Look;
}

Soulbound::Presentation::FWardenLineItem ABoardViewActor::WardenLineNow()
{
	// S-024 (R-PRO-1 precisión): con el telón bajado no se pide la frase a la cola, así que su primera frase empieza a contar cuando sube.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (HostPtr && HostPtr->IsCurtainDown())
	{
		return Soulbound::Presentation::FWardenLineItem();
	}
	return WardenLineQueue.Current(FPlatformTime::Seconds());
}

float ABoardViewActor::WardenLineBandHeightNow()
{
	// S-041 (R-DEC-17): lo que necesita la frase es el alto que Slate le calcula a su fondo (partida al ancho de la franja, con la línea pequeña y los
	// márgenes); es el de la pasada anterior, así que el panel sube un frame después de que llegue una frase larga.
	const bool bHasLine = !WardenLineNow().IsEmpty();
	const float Needed = WardenLineBox.IsValid() ? static_cast<float>(WardenLineBox->GetDesiredSize().Y) : 0.f;
	return Soulbound::Presentation::WardenLineBandHeight(WardenLineReservedHeight(), bHasLine, Needed);
}

float ABoardViewActor::CutDimOpacityNow() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (HostPtr && HostPtr->IsCurtainDown())
	{
		return 0.f;
	}
	return Soulbound::Presentation::CutDimOpacity(FPlatformTime::Seconds(), CutDimStartReal, CutDimMaxOpacity, CutDimPulseCount, CutDimPulseSeconds);
}

float ABoardViewActor::EchoNightDimOpacityNow() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (EchoNightDimChangedReal < 0.0 || (HostPtr && HostPtr->IsCurtainDown()))
	{
		return 0.f;
	}
	return Soulbound::Presentation::EchoNightDimOpacity(bEchoNightDimOn, EchoNightDimFrom, FPlatformTime::Seconds() - EchoNightDimChangedReal,
		EchoNightDimMaxOpacity, EchoNightDimFadeSeconds);
}

FLinearColor ABoardViewActor::BoardDimColorNow() const
{
	return Soulbound::Presentation::BoardDimColor(EchoNightDimColor, EchoNightDimOpacityNow(), CutDimOpacityNow());
}

void ABoardViewActor::RefreshEchoNightDim(const FMatchState& State)
{
	// R-VIS-31: la transición empieza desde la opacidad que hay ahora, así una Noche que termina mientras aún sube no salta.
	const bool bShown = Soulbound::Presentation::EchoNightDimShown(State);
	if (bShown != bEchoNightDimOn)
	{
		EchoNightDimFrom = EchoNightDimOpacityNow();
		EchoNightDimChangedReal = FPlatformTime::Seconds();
		bEchoNightDimOn = bShown;
	}
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
	// S-029 (R-VIS-11): los asientos con el aro del turno. Solo durante el turno; entre turnos, ninguno; en un duelo o un trilero que juega un evento
	// del Warden, sus dos participantes (el ActiveSeat de entonces es el del último turno).
	const TArray<int32> RingedSeats = Soulbound::Presentation::RingedSeats(State);
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
		// S-029 (R-VIS-10): el disco de rol bajo la ficha (la propia ficha, sin carta) toma el gris del Eco o el color del Dominio del Elegido.
		Token->SetRoleColor(Soulbound::Presentation::TokenColor(State.Seats[Seat].Role, State.Seats[Seat].DomainId, EchoTokenColor, DomainColors));
		if (!TilePositions.IsValidIndex(TileId))
		{
			Token->StopWalk();
			continue;
		}
		Token->SetElectShape(State.Seats[Seat].Role == ESeatRole::Elect);
		Token->SetActive(RingedSeats.Contains(Seat));
		// S-017: durante una votación, el clic en la ficha de otro es el voto del humano.
		Token->SetClickable(Soulbound::Rules::VoteOptions(State, HostPtr->GetHumanSeat()).Contains(Seat));

		// S-020 (R-TAB-26): la ficha del que se mueve recorre lo que le queda del camino, y cada llegada es su intención de avance.
		// Si el motor va por delante (la salvaguarda entró en una casilla a la que la ficha no había llegado), la ficha no salta
		// (S-046, R-VIS-32, precisión del autor del 17/11/2026): va en línea recta hasta la casilla del estado como primer punto del
		// recorrido, a la velocidad de caminar y sin avisar (el motor ya entró), y sigue desde ahí.
		if (Seat == MovingSeat)
		{
			const TArray<int32>& Path = State.Movement.Path;
			const int32 Done = FMath::Clamp(State.Movement.EnteredCount, 0, Path.Num());
			const TArray<int32> Remaining(Path.GetData() + Done, Path.Num() - Done);
			if (Token->RemainingWalk() != Remaining)
			{
				const bool bBehind = Token->IsWalking()
					&& Soulbound::Presentation::TokenArrival(TileId, Token->StandingTile(), Token->RemainingWalk()) != Soulbound::Presentation::ETokenArrival::Stay;
				TArray<int32> WalkTiles;
				TArray<FVector> WalkPoints;
				WalkOf(Seat, TileId, Remaining, WalkTiles, WalkPoints);
				if (bBehind)
				{
					WalkTiles.Insert(INDEX_NONE, 0);
					WalkPoints.Insert(TokenBase(TileId), 0);
				}
				Token->WalkPath(WalkTiles, WalkPoints, TokenSpeed);
			}
			continue;
		}
		if (Token->IsWalking())
		{
			// Un recorrido que el estado deja atrás: cortado a medio camino o llegado por la salvaguarda (S-046, R-VIS-32). Si la casilla del
			// estado es la siguiente del camino y la última que le queda, sigue caminando hasta ella; al llegar, OnTokenTileEntered la coloca.
			// Si no, deja el camino y va deslizándose a su sitio en esa casilla (abajo), en vez de aparecer en ella de golpe.
			const Soulbound::Presentation::ETokenArrival Arrival = Soulbound::Presentation::TokenArrival(TileId, Token->StandingTile(), Token->RemainingWalk());
			if (Arrival == Soulbound::Presentation::ETokenArrival::Walk && Token->RemainingWalk() == TArray<int32>{TileId})
			{
				continue;
			}
			// TODO(R-VIS-32): si la siguiente es la del estado pero el camino seguía más allá (un corte justo cuando la salvaguarda entra), se
			// desliza en vez de caminar: haría falta recortar el recorrido conservando sus puntos de paso. Caso raro; se anota para PIE.
			Token->StopWalk();
		}
		Token->SetTile(TileId);

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

void ABoardViewActor::ToggleDebugText()
{
	// S-045 (R-HUD-15): el panel de estado es de depuración y empieza oculto; se oculta en su capa y, al volver, se rellena en el acto.
	HudToggles.ToggleStatusPanel();
	TickHud();
}

FString ABoardViewActor::DescribeViewKeys() const
{
	return Soulbound::Presentation::ViewKeysText(bSeatedView, SeatedViewKey.GetDisplayName().ToString(), OverlayToggleKey.GetDisplayName().ToString(),
		GrimoireKey.GetDisplayName().ToString(), ReturnToMenuKey.GetDisplayName().ToString(), DebugTextToggleKey.GetDisplayName().ToString(),
		DebugPanelKey.GetDisplayName().ToString());
}

bool ABoardViewActor::IsMatchOverShown() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	return HostPtr && HostPtr->HasMatch() && HostPtr->GetState().IsMatchOver();
}

Soulbound::Presentation::FPanelLine ABoardViewActor::MatchResultLineNow() const
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	return HostPtr && HostPtr->HasMatch() ? Soulbound::Presentation::MatchResultLine(HostPtr->GetState(), MakeStatusContext()) : Soulbound::Presentation::FPanelLine();
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
				// S-045 (R-HUD-16): ya no hay tecla de ocultar paneles (H) ni de pestaña de la columna (P). T enseña el panel de estado, de depuración.
				InputComponent->BindKey(DebugTextToggleKey, IE_Pressed, this, &ABoardViewActor::ToggleDebugText);
				// S-027: el Grimorio (G) y el panel de depuración (D).
				InputComponent->BindKey(GrimoireKey, IE_Pressed, this, &ABoardViewActor::ToggleGrimoire);
				// S-044 (R-HUD-10): las pestañas del Grimorio; cerrado no hacen nada, y no se consumen.
				InputComponent->BindKey(GrimoirePreviousTabKey, IE_Pressed, this, &ABoardViewActor::PreviousGrimoireTab).bConsumeInput = false;
				InputComponent->BindKey(GrimoireNextTabKey, IE_Pressed, this, &ABoardViewActor::NextGrimoireTab).bConsumeInput = false;
				InputComponent->BindKey(DebugPanelKey, IE_Pressed, this, &ABoardViewActor::ToggleDebugPanel);
				// S-022: la tecla de volver al menú, alternativa al botón, no se consume: antes del final sigue llegando a quien la use.
				InputComponent->BindKey(ReturnToMenuKey, IE_Pressed, this, &ABoardViewActor::ReturnToMenu).bConsumeInput = false;
				// S-042 (R-MIN-19): la tecla de "Empezar" del anillo; fuera de la espera no hace nada y tampoco se consume.
				InputComponent->BindKey(RingStartKey, IE_Pressed, this, &ABoardViewActor::StartRingNow).bConsumeInput = false;
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
	// S-045 (R-HUD-16): sin columna, no hay ancho de columna que calcular.
	const bool bChanged = SlateFont != PanelSlateFontSize || !bPanelScaleLogged;
	PanelSlateFontSize = SlateFont;
	if (bChanged)
	{
		bPanelScaleLogged = true;
		UE_LOG(LogSoulboundView, Log, TEXT("Paneles: viewport %dx%d, escala DPI de Unreal %.3f; letra %d px en pantalla (%d en Slate, mínimo %d)."),
			Size.X, Size.Y, PanelDpiScale, ScreenFont, PanelSlateFontSize, PanelMinFontSize);
	}
}

void ABoardViewActor::ToggleDebugPanel()
{
	HudToggles.ToggleDebugPanel();
	TickHud();
}

Soulbound::Presentation::FGrimoireKeyNames ABoardViewActor::GrimoireKeyNames() const
{
	Soulbound::Presentation::FGrimoireKeyNames Keys;
	Keys.Grimoire = GrimoireKey.GetDisplayName().ToString();
	Keys.PreviousTab = GrimoirePreviousTabKey.GetDisplayName().ToString();
	Keys.NextTab = GrimoireNextTabKey.GetDisplayName().ToString();
	Keys.View = SeatedViewKey.GetDisplayName().ToString();
	Keys.Overlay = OverlayToggleKey.GetDisplayName().ToString();
	Keys.RingStart = RingStartKey.GetDisplayName().ToString();
	Keys.ReturnToMenu = ReturnToMenuKey.GetDisplayName().ToString();
	Keys.PreviousPage = GrimoirePreviousPageKey.GetDisplayName().ToString();
	Keys.NextPage = GrimoireNextPageKey.GetDisplayName().ToString();
	return Keys;
}

void ABoardViewActor::ToggleGrimoire()
{
	if (bGrimoireOpen)
	{
		CloseGrimoire();
		return;
	}
	// S-044 (R-HUD-11, decisión del autor del 17/11/2026): durante un anillo del humano ni la tecla ni el botón lo abren.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!Soulbound::Presentation::CanOpenGrimoire(HostPtr && HostPtr->GetRingRun() != nullptr))
	{
		return;
	}
	bGrimoireOpen = true;
	// S-044 (R-HUD-10): siempre en "Tú"; la crónica, al día.
	GrimoireTab.Open();
	GrimoireChronicleCount = -1;
	// S-029 (R-VIS-13): abrir el Grimorio cierra la carta del Elegido.
	ElectCard.SetGrimoireOpen(true);
	TickHud();
}

void ABoardViewActor::CloseGrimoire()
{
	bGrimoireOpen = false;
	ElectCard.SetGrimoireOpen(false);
}

void ABoardViewActor::PreviousGrimoireTab()
{
	if (bGrimoireOpen)
	{
		GrimoireTab.Previous();
	}
}

void ABoardViewActor::NextGrimoireTab()
{
	if (bGrimoireOpen)
	{
		GrimoireTab.Next();
	}
}

void ABoardViewActor::RefreshElectCard(const FMatchState& State)
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	const UWorld* World = GetWorld();
	if (!HostPtr || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// Un Bonded del asiento humano abre la carta; el de un dummy, no (R-VIS-13).
	ElectCard.Observe(State, HostPtr->GetHumanSeat(), Now);
	if (ElectCardPanel.IsValid() && ElectCard.IsVisible(Now, ElectCardSeconds))
	{
		const TArray<Soulbound::Presentation::FGrimoireRow> Rows = Soulbound::Presentation::GrimoireRows(State, HostPtr->GetHumanSeat(), GrimoireTexts, SeatColors);
		if (Rows.IsValidIndex(HostPtr->GetHumanSeat()))
		{
			ElectCardPanel->SetRow(Rows[HostPtr->GetHumanSeat()]);
		}
	}
}

void ABoardViewActor::DismissElectCard()
{
	ElectCard.Dismiss();
}

bool ABoardViewActor::IsElectCardShown() const
{
	const UWorld* World = GetWorld();
	const UMatchHostSubsystem* HostPtr = Host.Get();
	return World && HostPtr && !bGrimoireOpen && !HostPtr->IsCurtainDown() && ElectCard.IsVisible(World->GetTimeSeconds(), ElectCardSeconds);
}

void ABoardViewActor::RefreshFlashes(const FMatchState& State)
{
	// Un registro más corto que lo leído es otra partida.
	if (FlashScanned > State.EventLog.Num())
	{
		FlashScanned = 0;
	}
	const TArray<Soulbound::Presentation::FFlashCue> Cues = Soulbound::Presentation::FlashCuesFromEvents(State, FlashScanned, FlashSeconds);
	FlashScanned = State.EventLog.Num();
	const UWorld* World = GetWorld();
	if (Cues.IsEmpty() || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (const Soulbound::Presentation::FFlashCue& Cue : Cues)
	{
		const FLinearColor Color = Soulbound::Presentation::FlashColorOf(Cue, DomainColors, FlashGainColor, FlashLossColor, SealMarkColor);
		if (Cue.Target == Soulbound::Presentation::EFlashTarget::Token)
		{
			if (TokenActors.IsValidIndex(Cue.Id) && TokenActors[Cue.Id])
			{
				TokenActors[Cue.Id]->Flash(Color, Cue.DurationSeconds, Cue.DelaySeconds, Cue.Direction == Soulbound::Presentation::EFlashDirection::Inward);
			}
			continue;
		}
		// La Fuente (o, desde S-037, la casilla de un Sello): la vista encadena sus destellos, uno tras otro, con el mismo criterio que la ficha.
		const bool bTileCue = Cue.Target == Soulbound::Presentation::EFlashTarget::Tile;
		double& Busy = bTileCue ? SealFlashBusy.FindOrAdd(Cue.Id, 0.0) : SourceFlashBusy.FindOrAdd(Cue.Id, 0.0);
		FSourceFlash Flash;
		Flash.bTile = bTileCue;
		Flash.RuneType = Cue.Id;
		Flash.bInward = Cue.Direction == Soulbound::Presentation::EFlashDirection::Inward;
		Flash.Color = Color;
		Flash.Start = FMath::Max(Now + Cue.DelaySeconds, Busy);
		Flash.End = Flash.Start + Cue.DurationSeconds;
		Busy = Flash.End;
		SourceFlashes.Add(Flash);
		SetActorTickEnabled(true);
	}
}

bool ABoardViewActor::IsSealMarkFlashing(int32 TileId) const
{
	for (const FSourceFlash& Flash : SourceFlashes)
	{
		if (Flash.bTile && Flash.RuneType == TileId)
		{
			return true;
		}
	}
	return false;
}

void ABoardViewActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UMatchHostSubsystem* HostPtr = Host.Get();
	const UWorld* World = GetWorld();
	if (!HostPtr || !World || !HostPtr->HasMatch())
	{
		return;
	}
	// S-029 (R-VIS-12): el destello de una Fuente tiñe su cruz (la marca de la casilla) con el color del destello, con subida y bajada suaves, y al
	// acabar el último se repinta la vista para devolver a la cruz su color de siempre.
	const double Now = World->GetTimeSeconds();
	const FMatchState& State = HostPtr->GetState();
	const int32 Before = SourceFlashes.Num();
	SourceFlashes.RemoveAll([Now](const FSourceFlash& Flash) { return Flash.End <= Now; });
	// S-039 (R-VIS-26): las casillas cuyo halo está en un destello de Fuente en este instante; ahí manda el destello y el latido no pinta.
	TSet<int32> FlashedHalos;
	for (const FSourceFlash& Flash : SourceFlashes)
	{
		if (Flash.Start > Now)
		{
			continue;
		}
		if (!Flash.bTile)
		{
			FlashedHalos.Add(State.Board.FindPowerSource(Flash.RuneType));
		}
		const float Intensity = static_cast<float>(FMath::Sin(UE_DOUBLE_PI * (Now - Flash.Start) / FMath::Max(Flash.End - Flash.Start, static_cast<double>(KINDA_SMALL_NUMBER))));
		if (Flash.bTile)
		{
			// S-037 (R-VIS-25): el destello de un Sello escala el disco de la marca de su casilla (como el de la Fuente con su halo): hacia dentro se cierra al
			// sellarse y hacia fuera se abre al liberarse. Sellada, el disco conserva su color y lo anima solo el tamaño; liberada, ya no hay marca y el
			// destello la enciende y la apaga con la subida y bajada de siempre.
			if (UStaticMeshComponent* Mark = SealMarks.IsValidIndex(Flash.RuneType) ? SealMarks[Flash.RuneType].Get() : nullptr)
			{
				const bool bStillSealed = State.Board.IsValidTile(Flash.RuneType) && State.Board.Tiles[Flash.RuneType].bSealed;
				Soulbound::Presentation::SetComponentColor(*Mark, BaseMaterial,
					bStillSealed ? Flash.Color : FLinearColor(Flash.Color.R * Intensity, Flash.Color.G * Intensity, Flash.Color.B * Intensity));
				Mark->SetVisibility(true);
				const FVector Original = SealMarkBaseScales.FindOrAdd(Flash.RuneType, Mark->GetRelativeScale3D());
				const float Grown = Soulbound::Presentation::FlashScale(static_cast<float>((Now - Flash.Start) / FMath::Max(Flash.End - Flash.Start, static_cast<double>(KINDA_SMALL_NUMBER))), FlashGrowth, Flash.bInward);
				Mark->SetRelativeScale3D(FVector(Original.X * Grown, Original.Y * Grown, Original.Z));
			}
			continue;
		}
		const int32 TileId = State.Board.FindPowerSource(Flash.RuneType);
		const FLinearColor Color = FMath::Lerp(SourceColor, Flash.Color, Intensity);
		for (const TArray<TObjectPtr<UStaticMeshComponent>>* Marks : {&Inlays, &InlayHoles})
		{
			if (UStaticMeshComponent* Mark = Marks->IsValidIndex(TileId) ? (*Marks)[TileId].Get() : nullptr)
			{
				Soulbound::Presentation::SetComponentColor(*Mark, BaseMaterial, Color);
			}
		}
		// La cruz de la Fuente es blanca de por sí, y un destello claro no se vería en ella: el halo de la casilla (el de las alcanzables) se
		// enciende con el color del destello mientras dura, y con él se ve también el claro.
		if (UStaticMeshComponent* Halo = Halos.IsValidIndex(TileId) ? Halos[TileId].Get() : nullptr)
		{
			Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, FLinearColor(Flash.Color.R * Intensity, Flash.Color.G * Intensity, Flash.Color.B * Intensity));
			Halo->SetVisibility(true);
			// El halo se abre (la runa sale de la Fuente) o se cierra (vuelve a ella) como el aro de la ficha (FlashScale); su escala de siempre se guarda la primera
			// vez y se devuelve al acabar.
			const FVector Original = HaloBaseScales.FindOrAdd(TileId, Halo->GetRelativeScale3D());
			const float Grown = Soulbound::Presentation::FlashScale(static_cast<float>((Now - Flash.Start) / FMath::Max(Flash.End - Flash.Start, static_cast<double>(KINDA_SMALL_NUMBER))), FlashGrowth, Flash.bInward);
			Halo->SetRelativeScale3D(FVector(Original.X * Grown, Original.Y * Grown, Original.Z));
		}
	}
	if (SourceFlashes.Num() != Before)
	{
		// Acabó algún destello: los halos recuperan su color de alcanzable y su tamaño, y la vista se repinta (cruz y visibilidad de siempre).
		for (UStaticMeshComponent* Halo : Halos)
		{
			if (Halo)
			{
				Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, ReachableColor);
			}
		}
		for (const TPair<int32, FVector>& Scaled : HaloBaseScales)
		{
			if (UStaticMeshComponent* Halo = Halos.IsValidIndex(Scaled.Key) ? Halos[Scaled.Key].Get() : nullptr)
			{
				Halo->SetRelativeScale3D(Scaled.Value);
			}
		}
		HaloBaseScales.Reset();
		for (const TPair<int32, FVector>& Scaled : SealMarkBaseScales)
		{
			if (UStaticMeshComponent* Mark = SealMarks.IsValidIndex(Scaled.Key) ? SealMarks[Scaled.Key].Get() : nullptr)
			{
				Mark->SetRelativeScale3D(Scaled.Value);
			}
		}
		SealMarkBaseScales.Reset();
		Refresh();
	}
	// S-039 (R-VIS-26): el latido de los halos de casilla alcanzable. El color es ReachableColor por el pulso del aro del turno (ReachableHaloColor); el
	// halo no cambia de tamaño ni de sitio. Solo los halos visibles, y no los que está pintando un destello.
	if (bReachableHalosShown)
	{
		const FLinearColor Pulse = Soulbound::Presentation::ReachableHaloColor(ReachableColor, Now, ReachablePulseSeconds, ReachablePulseMin);
		for (int32 TileId = 0; TileId < Halos.Num(); ++TileId)
		{
			if (UStaticMeshComponent* Halo = Halos[TileId].Get(); Halo && Halo->IsVisible() && !FlashedHalos.Contains(TileId))
			{
				Soulbound::Presentation::SetComponentColor(*Halo, BaseMaterial, Pulse);
			}
		}
	}
	// S-041 (R-VIS-29): el latido del orbe del Oráculo mientras la consulta espera.
	if (bOracleOrbShown)
	{
		ScaleOracleOrb(Now);
	}
	if (SourceFlashes.IsEmpty())
	{
		SourceFlashBusy.Reset();
		SealFlashBusy.Reset();
		if (!bReachableHalosShown && !bOracleOrbShown)
		{
			SetActorTickEnabled(false);
		}
	}
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
	Context.SeatColors = SeatColors;
	Context.GoalLineColor = GoalLineColor;
	Context.WardenLineColor = WardenLineColor;
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

	// S-033 (R-DEC-13): los números para medir a 1024x768, en la línea "Paneles:" cuando cambian: la geometría real de la capa de la frase, de su contenido
	// (dónde empieza y cuánto mide dentro de la capa), lo que da la cuenta con la ventana y la escala de DPI, y el hueco libre que sale de cada una (tope
	// 1000, el de la frase). El centro del contenido y el del hueco tienen que coincidir. Desde S-045 (R-HUD-16) no hay columna: el hueco es todo el ancho.
	if (WardenLineOverlay.IsValid() && WardenLineContent.IsValid())
	{
		const FGeometry& LayerGeometry = WardenLineOverlay->GetTickSpaceGeometry();
		const FGeometry& ContentGeometry = WardenLineContent->GetTickSpaceGeometry();
		const FVector2D LayerSize = FVector2D(LayerGeometry.GetLocalSize());
		if (LayerSize.X >= 1.0)
		{
			const FVector2D ContentAt = FVector2D(LayerGeometry.AbsoluteToLocal(ContentGeometry.GetAbsolutePosition()));
			const FVector2D ContentSize = FVector2D(ContentGeometry.GetLocalSize());
			const FString Key = FString::Printf(TEXT("%.0f|%.0f|%.0f"), LayerSize.X, ContentAt.X, ContentSize.X);
			if (Key != LoggedLayoutKey)
			{
				LoggedLayoutKey = Key;
				const float ByWindow = static_cast<float>(LastViewportSize.X) / PanelDpiScale;
				const float HoleCenter = 16.f + (static_cast<float>(LayerSize.X) - 32.f) * 0.5f;
				UE_LOG(LogSoulboundView, Verbose, TEXT("Paneles: capa de la frase %.1fx%.1f en Slate (geometría real); contenido en x %.1f con ancho %.1f (centro %.1f; centro del hueco %.1f); la cuenta con la ventana da %.1f de ancho (%.0f px / escala %.3f); hueco libre con tope 1000: %.1f por geometría, %.1f por la cuenta."),
					LayerSize.X, LayerSize.Y, ContentAt.X, ContentSize.X, ContentAt.X + ContentSize.X * 0.5, HoleCenter, ByWindow, LastViewportSize.X, PanelDpiScale,
					Soulbound::Presentation::BoardFreeWidth(static_cast<float>(LayerSize.X), 1000.f), Soulbound::Presentation::BoardFreeWidth(ByWindow, 1000.f));
			}
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
	TOptional<Soulbound::Presentation::FDecisionView> Panel = PanelViewNow();
	Soulbound::Presentation::AdvanceDecisionClock(DecisionClock, Panel);
	// S-039 (R-DEC-16): el aviso de casilla inalcanzable. Un rechazo nuevo del anfitrión (otro número de orden, aunque el motivo y la tirada se repitan) lo
	// enseña o lo sustituye; se quita solo al pasar sus segundos, o ya si el turno de tirada deja de ser del humano (destino aceptado, turno cortado o
	// terminado). Va en la vista sin cambiar su identidad: la barra del plazo no se reinicia. La primera vez solo se pone al día el número.
	{
		const UMatchHostSubsystem::FHumanRejection& Rejection = HostPtr->GetLastHumanRejection();
		const double RealNow = FPlatformTime::Seconds();
		if (SeenRejectionSerial == INDEX_NONE)
		{
			SeenRejectionSerial = Rejection.Serial;
		}
		else if (Rejection.Serial != SeenRejectionSerial)
		{
			// El punto lo capturó el anfitrión en el momento del clic (coordenadas absolutas de Slate) y se guarda así: lo pasa a la capa NoticeMarginNow,
			// con la geometría del frame. S-040, corrección tras PIE de un defecto de S-039: convertirlo aquí usaba la geometría de la capa del aviso, que
			// está plegada sin aviso y, antes del primero, no se había pintado nunca: el primer aviso de la partida salía desplazado.
			SeenRejectionSerial = Rejection.Serial;
			UnreachableNotice.Show(Soulbound::Presentation::UnreachableNoticeText(Rejection.Reason, Rejection.Roll), Rejection.ScreenPosition, RealNow);
		}
		if (!Panel.IsSet() || Panel->Input != Soulbound::Presentation::EDecisionInput::Tile)
		{
			UnreachableNotice.Dismiss();
		}
		NoticeText = UnreachableNotice.Current(RealNow, UnreachableNoticeSeconds);
	}
	DecisionDeadlineSeconds = Panel.IsSet() ? Panel->DeadlineSeconds : 0.0;
	const bool bRingOpen = HostPtr->GetRingRun() != nullptr;
	// S-044 (R-HUD-11): una decisión propia nueva (también bloqueada porque habla el Warden) o un anillo del humano cierran el Grimorio. Se mira siempre,
	// abierto o no: lo que ya estaba a la vista al abrirlo no lo cierra.
	if (GrimoireCloseWatch.Observe(Panel, bRingOpen) && bGrimoireOpen)
	{
		CloseGrimoire();
	}
	bDecisionShown = Soulbound::Presentation::IsDecisionPanelShown(Panel, bRingOpen);
	if (DecisionPanel.IsValid())
	{
		DecisionPanel->SetView(bDecisionShown ? Panel : TOptional<Soulbound::Presentation::FDecisionView>());
	}

	Soulbound::Presentation::FStatusContext Context = MakeStatusContext();
	Context.bDecisionPanelShown = Soulbound::Presentation::IsOwnDecisionShown(Panel, bRingOpen);
	if (MatchPanel.IsValid())
	{
		// S-045 (R-HUD-14): la caja de la partida, arriba a la izquierda; cuándo se ve lo dice HudShownNow cada frame.
		MatchPanel->SetLines(Soulbound::Presentation::MatchPanelLines(State, Context));
	}
	if (StatusPanel.IsValid())
	{
		// S-045 (R-HUD-15): el panel de estado es de depuración; oculto, no se rellena (su tecla lo rellena en el acto al enseñarlo).
		StatusPanel->SetLines(HudToggles.bStatusPanelShown ? Soulbound::Presentation::StatusLines(State, Context) : TArray<Soulbound::Presentation::FPanelLine>());
	}
	if (GoalPanel.IsValid())
	{
		// S-040 (R-HUD-7): el texto de la caja de la línea de objetivo, y lo que su visibilidad necesita del estado; la frase del Warden la mira cada frame.
		GoalPanel->SetLines(Soulbound::Presentation::GoalPanelLines(State, Context));
		bGoalPanelPrologue = Context.bPrologue;
		GoalPanelGoal = Soulbound::Rules::SeatGoal(State, Context.HumanSeat).Kind;
	}
	if (DebugPanel.IsValid())
	{
		DebugPanel->SetLines(HudToggles.bDebugPanelShown ? Soulbound::Presentation::DebugLines(State, Context) : TArray<Soulbound::Presentation::FPanelLine>());
	}
	if (GrimoirePanel.IsValid() && bGrimoireOpen)
	{
		RefreshGrimoire(State, Context);
	}
	RefreshElectCard(State);
}

void ABoardViewActor::RefreshGrimoire(const FMatchState& State, const Soulbound::Presentation::FStatusContext& Context)
{
	// S-044 (R-HUD-10): cada pestaña con la función que ya lo calcula; los paneles solo se rehacen si su contenido cambia (R-VIS-21). "El tablero" y
	// "Cómo se juega" no cambian en la partida: se rellenan al construir.
	GrimoirePanel->SetYou(Soulbound::Presentation::GrimoireYouView(State, Context.HumanSeat, GrimoireTexts, SeatColors));
	TArray<bool> Forgotten;
	TArray<FLinearColor> Domains;
	for (int32 Id = 0; Id < State.Board.Domains.Num(); ++Id)
	{
		Forgotten.Add(IsDomainShownForgotten(Id));
		Domains.Add(DomainColor(Id));
	}
	GrimoirePanel->SetTable(Soulbound::Presentation::GrimoireTableRows(State, Context.HumanSeat, Forgotten, SeatColors, Domains));
	if (State.EventLog.Num() != GrimoireChronicleCount)
	{
		GrimoireChronicleCount = State.EventLog.Num();
		GrimoirePanel->SetChronicle(Soulbound::Presentation::ChronicleLines(State, InstructionNotes, ChronicleLookNow()));
	}
}

void ABoardViewActor::HandleRingOpened()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr)
	{
		return;
	}
	// S-044 (R-HUD-11): el anillo del humano cierra el Grimorio en el acto (TickHud también lo ve, en su siguiente pasada).
	CloseGrimoire();
	// El título se fija al abrir: al cerrarse, el anfitrión ya no tiene la prueba. Desde S-042 (R-VIS-30) dice qué te juegas, también en el resultado.
	LastRingTitle = Soulbound::Presentation::RingStakeTitle(HostPtr->GetRingTrial(), HostPtr->IsRingDuel());
	LastRingShownUntil = 0.0;
	// S-031 (R-VIS-16): criterio y umbral de la prueba, con las mismas funciones de datos con que el anfitrión la abrió (RingParamsForTrial/ForDuel);
	// no duplica la regla, la lee. Sin parámetros (prueba None fuera del duelo), un criterio sin marca.
	const FRingParams& RingData = HostPtr->GetState().Params.Ring;
	LastRingParams = FRingTrialParams();
	LastRingParams.Criterion = ERingCriterion::Performance;
	if (HostPtr->IsRingDuel())
	{
		LastRingParams = Soulbound::Rules::RingParamsForDuel(RingData);
	}
	else if (const TOptional<FRingTrialParams> Params = Soulbound::Rules::RingParamsForTrial(RingData, HostPtr->GetRingTrial(), HostPtr->GetRingDifficulty()))
	{
		LastRingParams = Params.GetValue();
	}
	// R-VIS-18: la runa de una Fuente se tiñe con el Dominio de la casilla del humano; en el resto, el azul de siempre.
	const int32 RuneDomain = Soulbound::Presentation::RingRuneDomain(HostPtr->GetState(), HostPtr->GetHumanSeat(), HostPtr->GetRingTrial());
	LastRingRuneColor = RuneDomain != INDEX_NONE ? DomainColor(RuneDomain) : FLinearColor(0.55f, 0.75f, 1.f);
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

void ABoardViewActor::StartRingNow()
{
	// S-042 (R-MIN-19): lo mismo por el botón y por la tecla. La hora es la del reloj real, la del anillo; con el juego en pausa, nada.
	UMatchHostSubsystem* HostPtr = Host.Get();
	const UWorld* World = GetWorld();
	if (!HostPtr || (World && World->IsPaused()))
	{
		return;
	}
	HostPtr->StartRing(FPlatformTime::Seconds());
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
		// R-VIS-17: en el duelo no hay texto ni cifra de rendimiento; quien gana lo dice el panel de decisión (R-DEC-10). Solo la barra y las marcas.
		LastRingResult.Reset();
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

FMargin ABoardViewActor::NoticeMarginNow(int32 FontSize) const
{
	if (!NoticeOverlay.IsValid() || NoticeText.IsEmpty())
	{
		return FMargin(0.f);
	}
	const FGeometry& Layer = NoticeOverlay->GetTickSpaceGeometry();
	const FVector2f ViewportSize(Layer.GetLocalSize());
	if (ViewportSize.X < 1.f || ViewportSize.Y < 1.f)
	{
		return FMargin(0.f);
	}
	// S-045 (R-HUD-16): el tablero usa todo el ancho; el aviso se coloca en toda la capa (NoticeBounds), sin columna que lo recorte.
	const FBox2f Bounds = Soulbound::Presentation::NoticeBounds(ViewportSize);
	TArray<FBox2f> Avoid;
	// La zona de un widget a la vista, en las coordenadas de la capa del aviso.
	auto AddWidgetZone = [&](const TSharedPtr<SWidget>& Widget)
	{
		if (Widget.IsValid() && Widget->GetVisibility().IsVisible())
		{
			const FGeometry& Zone = Widget->GetTickSpaceGeometry();
			const FVector2f At(Layer.AbsoluteToLocal(Zone.GetAbsolutePosition()));
			Avoid.Add(FBox2f(At, At + FVector2f(Zone.GetLocalSize())));
		}
	};
	// S-045: qué zonas esquiva lo dice NoticeAvoidZones (la caja de la partida, la del objetivo, el panel de decisión y el botón del Grimorio; los paneles
	// de estado y de depuración, solo con su tecla). Antes, desde S-040: la caja del objetivo; S-044, el botón; y el panel de decisión, corrección tras
	// PIE del 16/11/2026 de un defecto de S-039 (un clic cerca del panel ponía el aviso encima de "Tu turno"). Cada widget cuenta si además se ve (telón).
	const Soulbound::Presentation::FHudShown Shown = const_cast<ABoardViewActor*>(this)->HudShownNow();
	for (const Soulbound::Presentation::EHudZone Zone : Soulbound::Presentation::NoticeAvoidZones(Shown, IsDecisionPanelVisible()))
	{
		switch (Zone)
		{
		case Soulbound::Presentation::EHudZone::MatchPanel: AddWidgetZone(MatchPanel); break;
		case Soulbound::Presentation::EHudZone::GoalPanel: AddWidgetZone(GoalPanel); break;
		case Soulbound::Presentation::EHudZone::DecisionPanel: AddWidgetZone(DecisionPanel); break;
		case Soulbound::Presentation::EHudZone::GrimoireButton: AddWidgetZone(GrimoireButton); break;
		case Soulbound::Presentation::EHudZone::StatusPanel: AddWidgetZone(StatusPanel); break;
		case Soulbound::Presentation::EHudZone::DebugPanel: AddWidgetZone(DebugPanel); break;
		}
	}

	// El tamaño del aviso: el texto medido con su letra, partido al ancho libre, más el margen del fondo.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);
	FVector2f TextSize(static_cast<float>(NoticeText.Len() * FontSize) * 0.6f, static_cast<float>(FontSize) * 1.3f);
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		TextSize = FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(NoticeText, Font));
	}
	const float WrapAt = FMath::Max(FMath::Min(TextSize.X, Bounds.GetSize().X - 2.f * NoticePadding - 16.f), 1.f);
	const int32 Lines = FMath::Max(FMath::CeilToInt(TextSize.X / WrapAt), 1);
	const FVector2f Size(WrapAt + 2.f * NoticePadding, TextSize.Y * Lines + 2.f * NoticePadding);
	const_cast<ABoardViewActor*>(this)->NoticeWrapAt = WrapAt;

	// El punto del clic, absoluto, en las coordenadas de la capa con la geometría de este frame (S-040, corrección del defecto de S-039 del primer aviso).
	const FVector2f Click(Layer.AbsoluteToLocal(FVector2D(UnreachableNotice.Position())));
	const FVector2f Corner = Soulbound::Presentation::PlaceNotice(Click, Size, Bounds, Avoid, NoticeClickGap);
	return FMargin(Corner.X, Corner.Y, 0.f, 0.f);
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
	return Soulbound::Presentation::BoardFreeWidth(LayerSlateWidth(), Cap);
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
	// La regla (decisión abierta, sin anillo) la calcula TickHud; el telón manda en el acto, como en las demás capas. Desde S-045 no hay tecla de los paneles.
	const UMatchHostSubsystem* HostPtr = Host.Get();
	return bDecisionShown && HostPtr && !HostPtr->IsCurtainDown() && !HostPtr->GetRingRun();
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
		// R-VIS-17: la instrucción, en dos líneas, durante toda la prueba; cuántos pulsos hay y cuántos van ya lo dicen las marcas de abajo.
		Model.Instruction = Soulbound::Presentation::RingInstructionLines();
		Model.CloseFraction = Run->CloseFraction(Now);
		Model.ZoneFraction = Run->Sequence.CloseSeconds > 0.0 ? 0.5 * Run->Sequence.SweetZoneSeconds / Run->Sequence.CloseSeconds : 0.0;
		Model.bInSweetZone = Run->IsInSweetZone(Now);
		Model.ZoneColor = RingZoneColor;
		Model.RuneColor = LastRingRuneColor;
		for (int32 Pulse = 0; Pulse < Total; ++Pulse)
		{
			Model.Pulses.Add(Done.IsValidIndex(Pulse) ? TOptional<double>(Done[Pulse]) : TOptional<double>());
		}
		// R-VIS-16: la barra enseña la suma de precisiones y el umbral del motor; el relleno toma el color de la última precisión. R-VIS-17: ya no
		// hay número de precisión ni texto de puntuación; las marcas de color de cada pulso lo dicen.
		const FRingProgress Progress = Soulbound::Presentation::RingProgress(Done, Total, LastRingParams.Criterion, LastRingParams.PassThreshold);
		Model.ProgressFill = Progress.Fill;
		Model.ProgressMark = Progress.Mark;
		Model.ProgressColor = Done.IsEmpty() ? FLinearColor::White : SRingPanel::PrecisionColor(Done.Last());
		// S-042 (R-MIN-19): mientras la prueba espera, el botón "Empezar", su tecla y la cuenta atrás; empezada, nada de eso.
		Soulbound::Presentation::AddRingStartPrompt(Model, *Run, Now, RingStartKey.GetDisplayName().ToString());
		return Model;
	}
	if (Now < LastRingShownUntil)
	{
		Model.bVisible = true;
		Model.Title = LastRingTitle;
		Model.Note = LastRingNote;
		Model.Big = LastRingResult;
		Model.BigColor = LastRingColor;
		for (const double Precision : LastRingPrecisions)
		{
			Model.Pulses.Add(Precision);
		}
		// R-VIS-16/17: al acabar, la barra con el resultado final y las marcas; el duelo no lleva marca ni cifra.
		const FRingProgress Progress = Soulbound::Presentation::RingProgress(LastRingPrecisions, LastRingParams.Pulses, LastRingParams.Criterion, LastRingParams.PassThreshold);
		Model.ProgressFill = Progress.Fill;
		Model.ProgressMark = Progress.Mark;
		Model.ProgressColor = LastRingPrecisions.IsEmpty() ? FLinearColor::White : SRingPanel::PrecisionColor(LastRingPrecisions.Last());
	}
	return Model;
}

