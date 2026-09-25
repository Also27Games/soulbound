#include "WardenHostSubsystem.h"

#include "../Spike/SpikeOnlineGameMode.h"
#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundWarden, Log, All);

bool UWardenHostSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// Solo mundos de juego (empaquetado o PIE), nunca el editor ni las previsualizaciones.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWardenHostSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Solo con autoridad: el creador de la partida es el servidor (R-RED-1). Un cliente no evalúa nunca.
	if (InWorld.GetNetMode() == NM_Client)
	{
		UE_LOG(LogSoulboundWarden, Log, TEXT("Anfitrion del Warden inactivo: este proceso es un cliente."));
		return;
	}

	// Solo en una partida: el anfitrión pertenece a la partida, no al mundo. En el spike, "partida" es el mapa
	// con nuestro GameMode; en los menús de UGT no hay Warden. En el motor real lo arrancará el GameMode de la Cripta.
	if (!InWorld.GetAuthGameMode<ASpikeOnlineGameMode>())
	{
		UE_LOG(LogSoulboundWarden, Log, TEXT("Anfitrion del Warden inactivo: este mapa no es una partida."));
		return;
	}

	State = FMatchState::MakeSpikeMatch();
	Rng.Initialize(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));

	InWorld.GetTimerManager().SetTimer(EvaluationTimer, this, &UWardenHostSubsystem::Evaluate, EvaluationPeriodSeconds, true);
	UE_LOG(LogSoulboundWarden, Log, TEXT("Anfitrion del Warden activo: evalua cada %.0f s."), EvaluationPeriodSeconds);
}

void UWardenHostSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
	}
	Super::Deinitialize();
}

void UWardenHostSubsystem::Evaluate()
{
	if (const UWorld* World = GetWorld())
	{
		State.ClockSeconds = World->GetTimeSeconds();
	}
	++EvaluationCount;

	const FWardenDecision Decision = Brain.Evaluate(State, Rng);
	FWardenBrain::Apply(State, Decision);

	int32 SealedCount = 0;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		SealedCount += Tile.bSealed ? 1 : 0;
	}

	const FString Text = Decision.IsIntervention()
		? FString::Printf(TEXT("Warden (eval %d, t=%.0f s): accion %d en casilla %d, \"%s\". Sellos: %d"),
			EvaluationCount, State.ClockSeconds, static_cast<int32>(Decision.Action), Decision.TileId, *Decision.Line, SealedCount)
		: FString::Printf(TEXT("Warden (eval %d, t=%.0f s): observa y calla. Sellos: %d"), EvaluationCount, State.ClockSeconds, SealedCount);

	UE_LOG(LogSoulboundWarden, Log, TEXT("%s"), *Text);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, EvaluationPeriodSeconds, Decision.IsIntervention() ? FColor::Orange : FColor::Silver, Text);
	}
}
