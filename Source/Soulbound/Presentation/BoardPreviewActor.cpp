#include "BoardPreviewActor.h"

#include "../Match/MatchHostSubsystem.h"
#include "BoardKit.h"
#include "BoardViewActor.h"
#include "Board/BoardLoader.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

ABoardPreviewActor::ABoardPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	// No se cocina: es una herramienta del editor.
	bIsEditorOnlyActor = true;
}

void ABoardPreviewActor::BuildPreview()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}
	ClearPreview();

	// Vacía, la ruta del anfitrión de partida, que se lee de DefaultGame.ini en su CDO.
	const FString Relative = BoardRelativePath.IsEmpty() ? GetDefault<UMatchHostSubsystem>()->BoardRelativePath : BoardRelativePath;
	const FBoardLoadResult Loaded = Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(FPaths::ProjectContentDir(), Relative));
	if (!Loaded.IsSuccess())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista previa del tablero: no se pudo cargar %s (%s)."), *Relative, *Loaded.Message);
		return;
	}

	// Los LayoutParams, las piezas y el tinte salen del CDO de la vista de partida: en el editor no hay ninguna instancia.
	const ABoardViewActor& Config = *GetDefault<ABoardViewActor>();
	const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Loaded.Board, Config.LayoutParams);
	if (Layout.Tiles.IsEmpty())
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista previa del tablero: %s no tiene casillas."), *Relative);
		return;
	}
	if (!Layout.bRadial)
	{
		UE_LOG(LogSoulboundView, Warning, TEXT("Vista previa del tablero: el trazado no es radial; las cuñas van de 60 en 60 por identificador de Dominio."));
	}

	FBoardKit Kit;
	Kit.Load(Config, TEXT("no se coloca"));

	// Transitorios: no se guardan en el mapa.
	FBoardKitFixedPieces Fixed;
	Kit.PlaceFixed(*this, Loaded.Board, Layout, Fixed, RF_Transient);
	// Recién cargado ningún Dominio está Olvidado (lo decide StartMatch): todas las cuñas, con su color entero.
	TArray<bool> NoneForgotten;
	NoneForgotten.Init(false, Loaded.Board.Domains.Num());
	Kit.TintWedgeFloors(Fixed, NoneForgotten, Config);

	FBoardKitStructureOptions Options;
	Options.bVisible = true;
	Options.bFloatingRunes = false;
	Options.bSealChains = false;
	FBoardKitStructures Structures;
	Kit.PlaceStructures(*this, Loaded.Board, Layout, Config.LayoutParams, Options, Structures, RF_Transient);
#endif
}

void ABoardPreviewActor::ClearPreview()
{
#if WITH_EDITOR
	// Todo componente de malla del actor es una pieza de la vista previa; el root no lo es.
	TInlineComponentArray<UStaticMeshComponent*> Pieces(this);
	for (UStaticMeshComponent* Piece : Pieces)
	{
		Piece->DestroyComponent();
	}
#endif
}
