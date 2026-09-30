#include "WardenHostSubsystem.h"

#include "../Match/MatchHostSubsystem.h"
#include "Engine/Engine.h"
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

void UWardenHostSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// El anfitrión de partida existe antes que este: se evalúa sobre su estado.
	Collection.InitializeDependency<UMatchHostSubsystem>();
	Super::Initialize(Collection);
}

void UWardenHostSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Mismas condiciones que el anfitrión de partida: solo con autoridad y solo en una partida (no en los menús).
	if (!UMatchHostSubsystem::IsMatchWorld(InWorld))
	{
		UE_LOG(LogSoulboundWarden, Log, TEXT("Anfitrion del Warden inactivo: este mundo no es una partida con autoridad."));
		return;
	}

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
	UMatchHostSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (!Match || !Match->HasMatch())
	{
		return;
	}
	if (!bSeeded)
	{
		// Semilla derivada de la de la partida (+2; +1 es la de los decisores): fijarla repite también al Warden.
		Rng.Initialize(Match->GetSeed() + 2);
		bSeeded = true;
	}
	++EvaluationCount;

	const FMatchState& State = Match->GetState();
	const FWardenDecision Decision = Brain.Evaluate(State, Rng);
	Match->ApplyWardenDecision(Decision);

	const FString Text = Decision.IsIntervention()
		? FString::Printf(TEXT("Warden (eval %d, t=%.0f s): accion %d en casilla %d, \"%s\". Sellos: %d"),
			EvaluationCount, State.ClockSeconds, static_cast<int32>(Decision.Action), Decision.TileId, *Decision.Line, State.Board.SealedTiles.Num())
		: FString::Printf(TEXT("Warden (eval %d, t=%.0f s): observa y calla. Sellos: %d"), EvaluationCount, State.ClockSeconds, State.Board.SealedTiles.Num());

	UE_LOG(LogSoulboundWarden, Log, TEXT("%s"), *Text);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, EvaluationPeriodSeconds, Decision.IsIntervention() ? FColor::Orange : FColor::Silver, Text);
	}
}
