#include "MatchHostSubsystem.h"

#include "../Presentation/BoardViewActor.h"
#include "../Spike/SpikeOnlineGameMode.h"
#include "Board/BoardLoader.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Match/MatchParams.h"
#include "Match/MatchStart.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Warden/WardenBrain.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundMatch, Log, All);

bool UMatchHostSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// Solo mundos de juego (empaquetado o PIE), nunca el editor ni las previsualizaciones.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

bool UMatchHostSubsystem::IsMatchWorld(const UWorld& World)
{
	// Solo con autoridad: el creador de la partida es el servidor (R-RED-1). Un cliente no la lleva nunca.
	// Solo en una partida: en el spike, "partida" es el mapa con nuestro GameMode; en los menús de UGT no hay.
	return World.GetNetMode() != NM_Client && World.GetAuthGameMode<ASpikeOnlineGameMode>() != nullptr;
}

void UMatchHostSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!IsMatchWorld(InWorld))
	{
		UE_LOG(LogSoulboundMatch, Log, TEXT("Anfitrion de partida inactivo: este mundo no es una partida con autoridad."));
		return;
	}
	if (!StartFromData(InWorld))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	View = InWorld.SpawnActor<ABoardViewActor>(ABoardViewActor::StaticClass(), FTransform::Identity, Params);
	if (View)
	{
		View->Initialize(this);
	}

	InWorld.GetTimerManager().SetTimer(AdvanceTimer, this, &UMatchHostSubsystem::Advance, FMath::Max(AdvanceIntervalSeconds, 0.01f), true);
}

void UMatchHostSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AdvanceTimer);
	}
	OnMatchStateChanged.Clear();
	Super::Deinitialize();
}

bool UMatchHostSubsystem::StartFromData(UWorld& World)
{
	const FString BoardPath = FPaths::Combine(FPaths::ProjectContentDir(), BoardRelativePath);
	const FString ParamsPath = FPaths::Combine(FPaths::ProjectContentDir(), MatchParamsRelativePath);

	const FBoardLoadResult Board = Soulbound::Rules::LoadBoardFromFile(BoardPath);
	const FMatchParamsLoadResult Params = Soulbound::Rules::LoadMatchParamsFromFile(ParamsPath);
	if (!Board.IsSuccess() || !Params.IsSuccess())
	{
		const FString Error = !Board.IsSuccess() ? Board.Message : Params.Message;
		UE_LOG(LogSoulboundMatch, Error, TEXT("No se pudo iniciar la partida: %s"), *Error);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red, FString::Printf(TEXT("Partida no iniciada: %s"), *Error));
		}
		return false;
	}

	UsedSeed = FixedSeed >= 0 ? FixedSeed : static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff);
	MatchRng.Initialize(UsedSeed);
	// Los decisores tienen su propio generador (semilla + 1), como en la simulación: no tocan los dados de la partida.
	DeciderRng.Initialize(UsedSeed + 1);

	FMatchStartResult Start = Soulbound::Rules::StartMatch(Board.Board, Params.Params, Soulbound::Match::MaxPlayers, MatchRng);
	if (!Start.IsSuccess())
	{
		UE_LOG(LogSoulboundMatch, Error, TEXT("StartMatch rechaza la partida: %s"), *Start.Message);
		return false;
	}
	State = MoveTemp(Start.State);

	Deciders.Reset();
	for (int32 Seat = 0; Seat < Soulbound::Match::SeatCount; ++Seat)
	{
		// El asiento del humano no tiene decisor: su intención llega por SubmitHumanIntent.
		Deciders.Add(Seat == HumanSeat ? TSharedPtr<IDecider>() : TSharedPtr<IDecider>(MakeShared<FDummyDecider>()));
	}
	OriginWorldSeconds = World.GetTimeSeconds();
	State.ClockSeconds = 0.0;
	bHasMatch = true;

	UE_LOG(LogSoulboundMatch, Log, TEXT("Partida iniciada: %d casillas, semilla %d, humano en el asiento %d."), State.Board.Tiles.Num(), UsedSeed, HumanSeat);
	return true;
}

double UMatchHostSubsystem::MatchSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() - OriginWorldSeconds : 0.0;
}

void UMatchHostSubsystem::Advance()
{
	if (!bHasMatch)
	{
		return;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	bool bChanged = false;

	// Las reglas son del motor: aquí solo se pregunta al decisor, se entrega su intención y se avanza el turno.
	while (true)
	{
		const int32 Seat = State.ActiveSeat;
		if (State.TurnPhase == ETurnPhase::RollAndChoose && Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid())
		{
			// Una decisión pendiente de otro turno (otro asiento u otro TurnCount) ya no vale: se descarta.
			if (Pending.bPending && (Pending.Seat != Seat || Pending.TurnCount != State.TurnCount))
			{
				Pending.bPending = false;
			}
			if (!Pending.bPending)
			{
				Pending.bPending = true;
				Pending.Seat = Seat;
				Pending.TurnCount = State.TurnCount;
				Pending.ReadyAtSeconds = Now + DummyResponseSeconds;
				Pending.TileId = Deciders[Seat]->ChooseDestination(State, Seat, State.ReachableOptions, DeciderRng);
			}
			if (Now >= Pending.ReadyAtSeconds && Pending.TileId != INDEX_NONE)
			{
				bChanged |= Soulbound::Rules::SubmitDestinationIntent(State, Seat, Pending.TileId, Now) == EIntentResult::Accepted;
				Pending.bPending = false;
			}
		}
		else
		{
			Pending.bPending = false;
		}

		// Falso cuando espera una intención, o si la partida no tiene orden de turno; en ambos casos no hay más que hacer.
		if (!Soulbound::Rules::AdvanceTurn(State, Now, MatchRng))
		{
			break;
		}
		bChanged = true;
	}

	if (bChanged)
	{
		OnMatchStateChanged.Broadcast();
	}
}

EIntentResult UMatchHostSubsystem::SubmitHumanIntent(int32 TileId)
{
	if (!bHasMatch)
	{
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitDestinationIntent(State, HumanSeat, TileId, Now);
	if (Result == EIntentResult::Accepted)
	{
		OnMatchStateChanged.Broadcast();
	}
	return Result;
}

void UMatchHostSubsystem::ApplyWardenDecision(const FWardenDecision& Decision)
{
	if (!bHasMatch)
	{
		return;
	}
	State.ClockSeconds = MatchSeconds();
	FWardenBrain::Apply(State, Decision);
	// Sin intervención el estado no cambia: no se avisa a la presentación.
	if (Decision.IsIntervention())
	{
		LastWardenLine = Decision.Line;
		OnMatchStateChanged.Broadcast();
	}
}
