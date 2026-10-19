#include "WardenHostSubsystem.h"

#include "../Match/MatchHostSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Warden/WardenRules.h"

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

	InWorld.GetTimerManager().SetTimer(PollTimer, this, &UWardenHostSubsystem::Poll, PollSeconds, true);
	UE_LOG(LogSoulboundWarden, Log, TEXT("Anfitrion del Warden activo: evalua en el instante que devuelve el cerebro (warden.json)."));
}

void UWardenHostSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
	}
	Super::Deinitialize();
}

void UWardenHostSubsystem::SetMode(EWardenHostMode InMode)
{
	Mode = InMode;
	UE_LOG(LogSoulboundWarden, Log, TEXT("Anfitrion del Warden: modo %s."), Mode == EWardenHostMode::Auto ? TEXT("auto") : TEXT("off"));
}

#if !UE_BUILD_SHIPPING
FDebugEditOutcome UWardenHostSubsystem::DebugLaunchEvent(const FString& EventId)
{
	UMatchHostSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (!Match || !Match->HasMatch())
	{
		return {false, TEXT("sin partida")};
	}
	// S-014: los sorteos del evento salen del sorteo del Warden, no del de la partida; desde S-016, el del estado.
	return Match->DebugLaunchEvent(EventId);
}
#endif

void UWardenHostSubsystem::Poll()
{
	UMatchHostSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	// Detenida en el paso a paso: el Warden no evalúa (el reloj de partida está congelado; S-007). Apagado (S-016), tampoco.
	if (Mode != EWardenHostMode::Auto || !Match || !Match->HasMatch() || Match->IsWaitingForStep())
	{
		return;
	}
	// R-WAR-1, R-WAR-3: el instante lo dio el cerebro en la evaluación anterior; una aplazada, en cuanto termina lo que la aplazó.
	if (!Soulbound::Rules::IsWardenEvaluationDue(Match->GetState(), Match->GetMatchSeconds()))
	{
		return;
	}
	++EvaluationCount;
	const FWardenDecision Decision = Match->EvaluateWarden();
	const FMatchState& State = Match->GetState();
	UE_LOG(LogSoulboundWarden, Log, TEXT("Warden (eval %d, t=%.1f s, acto %d): decisión %d, evento %d%s; siguiente en %.1f s"), EvaluationCount, State.ClockSeconds,
		static_cast<int32>(Decision.Act) + 1, static_cast<int32>(Decision.Kind), Decision.EventIndex, Decision.bForcedByGuarantee ? TEXT(", forzada por la garantía") : TEXT(""),
		State.Warden.NextEvaluationSeconds);
}
