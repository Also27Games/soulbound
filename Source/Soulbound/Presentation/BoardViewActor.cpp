#include "BoardViewActor.h"

#include "../Match/MatchHostSubsystem.h"
#include "BoardTileActor.h"
#include "PresentationShapes.h"
#include "SeatTokenActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Casilla a la altura del suelo; las conexiones y las fichas se miden desde aquí. */
	constexpr float TileThickness = 10.f;
	constexpr float EdgeThickness = 4.f;

	/** Clave base de los mensajes de pantalla de esta vista: una línea por clave, se sustituyen al refrescar. */
	constexpr uint64 TextKeyBase = 0x53420500;

	FLinearColor TileTypeColor(ETileType Type)
	{
		switch (Type)
		{
		case ETileType::Altar: return FLinearColor(0.9f, 0.5f, 0.1f);
		case ETileType::Niche: return FLinearColor(0.6f, 0.3f, 0.85f);
		case ETileType::PowerSource: return FLinearColor(0.2f, 0.8f, 0.9f);
		case ETileType::PowerNode: return FLinearColor(0.3f, 0.85f, 0.35f);
		case ETileType::Normal:
		default: return FLinearColor(0.35f, 0.35f, 0.4f);
		}
	}

	FLinearColor SeatColor(int32 Seat)
	{
		static const FLinearColor Colors[] = {
			FLinearColor(0.2f, 0.5f, 1.f), FLinearColor(1.f, 0.3f, 0.4f), FLinearColor(0.3f, 0.9f, 0.4f), FLinearColor(1.f, 0.9f, 0.2f)};
		return Colors[FMath::Clamp(Seat, 0, UE_ARRAY_COUNT(Colors) - 1)];
	}

	/** Atenuado de una casilla de un Dominio Olvidado: mismo tono, mucho más oscuro. */
	FLinearColor Dimmed(const FLinearColor& Color)
	{
		return FLinearColor(Color.R * 0.25f, Color.G * 0.25f, Color.B * 0.25f);
	}

	const TCHAR* PhaseName(ETurnPhase Phase)
	{
		switch (Phase)
		{
		case ETurnPhase::BetweenTurns: return TEXT("entre turnos");
		case ETurnPhase::Start: return TEXT("inicio");
		case ETurnPhase::PreRoll: return TEXT("antes de tirar");
		case ETurnPhase::RollAndChoose: return TEXT("tirada y destino");
		case ETurnPhase::MoveAndResolve: return TEXT("movimiento");
		case ETurnPhase::End: return TEXT("fin de turno");
		}
		return TEXT("?");
	}

	/** Texto de un evento del registro (R-TUR-15), para la línea "último suceso". */
	FString DescribeEvent(const FMatchEvent& Event)
	{
		switch (Event.Type)
		{
		case EMatchEventType::TurnStarted: return FString::Printf(TEXT("empieza el turno del asiento %d"), Event.Seat);
		case EMatchEventType::Rolled: return FString::Printf(TEXT("el asiento %d tira %d"), Event.Seat, Event.Value);
		case EMatchEventType::TurnClockExpired: return FString::Printf(TEXT("el asiento %d AGOTA el reloj de turno: no se mueve"), Event.Seat);
		case EMatchEventType::TurnSkipped: return FString::Printf(TEXT("el asiento %d pierde el turno"), Event.Seat);
		case EMatchEventType::TurnEnded: return FString::Printf(TEXT("termina el turno del asiento %d"), Event.Seat);
		case EMatchEventType::SealPlaced: return FString::Printf(TEXT("el Warden sella la casilla %d"), Event.Value);
		case EMatchEventType::SealReleased: return FString::Printf(TEXT("se libera el Sello de la casilla %d"), Event.Value);
		}
		return FString();
	}
}

ABoardViewActor::ABoardViewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(Soulbound::Presentation::CubePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;
}

void ABoardViewActor::Initialize(UMatchHostSubsystem* InHost)
{
	Host = InHost;
	if (!InHost || !InHost->HasMatch())
	{
		return;
	}

	TilePositions = ComputeTilePositions();
	BuildEdges();
	BuildTiles();
	BuildTokens();
	BuildCamera();
	Refresh();

	ChangedHandle = InHost->OnMatchStateChanged.AddUObject(this, &ABoardViewActor::Refresh);
	GetWorldTimerManager().SetTimer(HudTimer, this, &ABoardViewActor::TickHud, FMath::Max(HudIntervalSeconds, 0.05f), true, 0.f);
}

void ABoardViewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMatchHostSubsystem* HostPtr = Host.Get())
	{
		HostPtr->OnMatchStateChanged.Remove(ChangedHandle);
	}
	GetWorldTimerManager().ClearTimer(HudTimer);
	Super::EndPlay(EndPlayReason);
}

TArray<FVector> ABoardViewActor::ComputeTilePositions() const
{
	const FBoardState& Board = Host->GetState().Board;
	TArray<FVector> Positions;
	Positions.SetNum(Board.Tiles.Num());

	const bool bAllPositioned = Board.Tiles.FindByPredicate([](const FTileState& Tile) { return !Tile.bHasLayoutPosition; }) == nullptr;
	if (bAllPositioned)
	{
		// El editor dibuja con la Y hacia abajo; la cámara cenital tiene el +X del mundo hacia arriba de la pantalla.
		FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
		FVector2D Max(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
		for (const FTileState& Tile : Board.Tiles)
		{
			Min = FVector2D(FMath::Min(Min.X, Tile.LayoutPosition.X), FMath::Min(Min.Y, Tile.LayoutPosition.Y));
			Max = FVector2D(FMath::Max(Max.X, Tile.LayoutPosition.X), FMath::Max(Max.Y, Tile.LayoutPosition.Y));
		}
		const FVector2D Center = (Min + Max) * 0.5;
		for (const FTileState& Tile : Board.Tiles)
		{
			Positions[Tile.TileId] = FVector(-(Tile.LayoutPosition.Y - Center.Y) * PositionScale, (Tile.LayoutPosition.X - Center.X) * PositionScale, 0.0);
		}
		return Positions;
	}

	// Sin posiciones en el archivo: el Altar en el centro y cada Dominio en un sector del anillo, en filas de cinco.
	constexpr int32 PerRow = 5;
	constexpr float FirstRowRadius = 600.f;
	constexpr float RowSpacing = 350.f;
	constexpr float AngleStepDegrees = 12.f;
	TArray<int32> IndexInDomain;
	IndexInDomain.Init(0, Soulbound::Board::DomainCount);
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Tile.DomainId == INDEX_NONE)
		{
			Positions[Tile.TileId] = FVector::ZeroVector;
			continue;
		}
		const int32 K = IndexInDomain[Tile.DomainId]++;
		const float Angle = FMath::DegreesToRadians(Tile.DomainId * (360.f / Soulbound::Board::DomainCount) + ((K % PerRow) - PerRow / 2) * AngleStepDegrees);
		const float Radius = FirstRowRadius + (K / PerRow) * RowSpacing;
		Positions[Tile.TileId] = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	}
	return Positions;
}

void ABoardViewActor::BuildEdges()
{
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
			Soulbound::Presentation::SetComponentColor(*Strip, BaseMaterial, FLinearColor(0.75f, 0.75f, 0.8f));
			EdgeStrips.Add(Strip);
		}
	}
}

void ABoardViewActor::BuildTiles()
{
	const FBoardState& Board = Host->GetState().Board;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FTileState& Tile : Board.Tiles)
	{
		// El cilindro del motor mide 100 cm de diámetro y 100 de alto: se aplasta a una losa.
		const FTransform Transform(FRotator::ZeroRotator, TilePositions[Tile.TileId] + FVector(0.f, 0.f, TileThickness * 0.5f),
			FVector(TileDiameter / 100.f, TileDiameter / 100.f, TileThickness / 100.f));
		ABoardTileActor* Actor = GetWorld()->SpawnActor<ABoardTileActor>(ABoardTileActor::StaticClass(), Transform, Params);
		if (Actor)
		{
			Actor->SetTileId(Tile.TileId);
			Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
		TileActors.Add(Actor);
	}
}

void ABoardViewActor::BuildTokens()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Seat = 0; Seat < Host->GetState().Seats.Num(); ++Seat)
	{
		ASeatTokenActor* Token = GetWorld()->SpawnActor<ASeatTokenActor>(ASeatTokenActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(TokenDiameter / 100.f)), Params);
		if (Token)
		{
			Token->SetSeat(Seat);
			Token->SetColor(SeatColor(Seat));
			Token->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
		TokenActors.Add(Token);
	}
}

void ABoardViewActor::BuildCamera()
{
	FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D Max(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
	for (const FVector& Position : TilePositions)
	{
		Min = FVector2D(FMath::Min(Min.X, Position.X), FMath::Min(Min.Y, Position.Y));
		Max = FVector2D(FMath::Max(Max.X, Position.X), FMath::Max(Max.Y, Position.Y));
	}
	const FVector2D Center = (Min + Max) * 0.5;
	const FVector2D Half = (Max - Min) * 0.5 + FVector2D(TileDiameter);

	// Cámara cenital fija: el tablero entero a 16:9 con un 10 % de margen. Con FOV horizontal de 60 grados, a la
	// altura H se ve media anchura H*tan(30); la media altura es esa entre 16/9.
	constexpr float Fov = 60.f;
	constexpr float Aspect = 16.f / 9.f;
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	const float Height = 1.1f * FMath::Max(Half.Y / HalfTan, Half.X * Aspect / HalfTan);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
		FTransform(FRotator(-90.f, 0.f, 0.f), FVector(Center.X, Center.Y, Height)), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(Fov);
		Camera->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
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
	// se espera el destino.
	TSet<int32> Reachable;
	if (State.TurnPhase == ETurnPhase::RollAndChoose)
	{
		Reachable.Append(State.ReachableOptions);
	}

	for (const FTileState& Tile : State.Board.Tiles)
	{
		ABoardTileActor* Actor = TileActors.IsValidIndex(Tile.TileId) ? TileActors[Tile.TileId].Get() : nullptr;
		if (!Actor)
		{
			continue;
		}
		const bool bForgotten = State.Board.Domains.IsValidIndex(Tile.DomainId) && State.Board.Domains[Tile.DomainId].State == EDomainState::Forgotten;
		FLinearColor Color = TileTypeColor(Tile.Type);
		if (Reachable.Contains(Tile.TileId))
		{
			Color = FLinearColor::White;
		}
		else if (Tile.bSealed)
		{
			Color = FLinearColor(0.9f, 0.05f, 0.05f);
		}
		else if (bForgotten)
		{
			Color = Dimmed(Color);
		}
		Actor->SetColor(Color);
	}

	// Fichas: sobre su casilla y, si comparten casilla, repartidas en un círculo pequeño.
	for (int32 Seat = 0; Seat < State.Seats.Num() && Seat < TokenActors.Num(); ++Seat)
	{
		ASeatTokenActor* Token = TokenActors[Seat];
		const int32 TileId = State.Seats[Seat].TileId;
		if (!Token || !TilePositions.IsValidIndex(TileId))
		{
			continue;
		}
		int32 Sharing = 0;
		int32 MyIndex = 0;
		for (int32 Other = 0; Other < State.Seats.Num(); ++Other)
		{
			if (State.Seats[Other].TileId == TileId)
			{
				MyIndex = Other == Seat ? Sharing : MyIndex;
				++Sharing;
			}
		}
		FVector Offset = FVector::ZeroVector;
		if (Sharing > 1)
		{
			const float Angle = 2.f * PI * MyIndex / Sharing;
			Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (TileDiameter * 0.28f);
		}
		Token->SetActorLocation(TilePositions[TileId] + Offset + FVector(0.f, 0.f, TileThickness + TokenDiameter * 0.5f));
	}
}

void ABoardViewActor::ShowText(int32 Line, const FColor& Color, const FString& Text) const
{
	if (GEngine)
	{
		// Se sustituye por clave en cada refresco; si la vista deja de refrescar, el mensaje caduca a los dos intervalos.
		GEngine->AddOnScreenDebugMessage(TextKeyBase + Line, HudIntervalSeconds * 2.f + 0.1f, Color, Text);
	}
}

void ABoardViewActor::TickHud()
{
	const UMatchHostSubsystem* HostPtr = Host.Get();
	if (!HostPtr || !HostPtr->HasMatch())
	{
		return;
	}
	const FMatchState& State = HostPtr->GetState();

	// La cámara es la vista del jugador local. Se pone al iniciar la partida, o en cuanto exista el controlador.
	if (Camera)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController() && Controller->GetViewTarget() != Camera)
			{
				Controller->SetViewTarget(Camera);
			}
		}
	}

	const bool bWaiting = State.TurnPhase == ETurnPhase::RollAndChoose;
	const bool bHumanTurn = bWaiting && State.ActiveSeat == HostPtr->GetHumanSeat();
	const int32 TurnNumber = State.TurnPhase == ETurnPhase::BetweenTurns ? State.TurnCount : State.TurnCount + 1;

	ShowText(0, FColor::White, FString::Printf(TEXT("Turno %d | asiento activo: %d (%s) | fase: %s"),
		TurnNumber, State.ActiveSeat, State.ActiveSeat == HostPtr->GetHumanSeat() ? TEXT("humano") : TEXT("dummy"), PhaseName(State.TurnPhase)));

	const FString Clock = bWaiting ? FString::Printf(TEXT("%.0f s"), FMath::Max(0.0, State.TurnDeadlineSeconds - State.ClockSeconds)) : FString(TEXT("-"));
	ShowText(1, bHumanTurn ? FColor::Green : FColor::White, FString::Printf(TEXT("Tirada: %d | reloj de turno: %s%s"),
		State.Roll, *Clock, bHumanTurn ? TEXT("  <- tu turno: clic en una casilla blanca") : TEXT("")));

	ShowText(2, FColor::Yellow, State.EventLog.IsEmpty()
		? FString(TEXT("Ultimo suceso: -"))
		: FString::Printf(TEXT("Ultimo suceso: %s"), *DescribeEvent(State.EventLog.Last())));

	const FString Sealed = State.Board.SealedTiles.IsEmpty()
		? FString(TEXT("ninguno"))
		: FString::JoinBy(State.Board.SealedTiles, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); });
	ShowText(3, FColor::Red, FString::Printf(TEXT("Sellos activos: %d (casillas: %s)"), State.Board.SealedTiles.Num(), *Sealed));

	ShowText(4, FColor::Orange, HostPtr->GetLastWardenLine().IsEmpty()
		? FString(TEXT("Warden: aun no ha dicho nada"))
		: FString::Printf(TEXT("Warden: \"%s\""), *HostPtr->GetLastWardenLine()));

	ShowText(5, FColor::Silver, FString::Printf(TEXT("Semilla: %d (fijala con FixedSeed para repetir la partida) | t=%.0f s"), HostPtr->GetSeed(), State.ClockSeconds));
}
