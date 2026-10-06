#include "MatchHostSubsystem.h"

#include "../Presentation/BoardViewActor.h"
#include "../Presentation/EventText.h"
#include "../Spike/SpikeOnlineGameMode.h"
#include "Board/BoardLoader.h"
#include "Elects/ElectsLoader.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Match/MatchParams.h"
#include "Match/DebugEdits.h"
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
	const FElectsLoadResult Elects = Soulbound::Rules::LoadElectsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), ElectsRelativePath));
	if (!Board.IsSuccess() || !Params.IsSuccess() || !Elects.IsSuccess())
	{
		const FString Error = !Board.IsSuccess() ? Board.Message : !Params.IsSuccess() ? Params.Message : Elects.Message;
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

	FMatchStartResult Start = Soulbound::Rules::StartMatch(Board.Board, Params.Params, Elects.Table, Soulbound::Match::MaxPlayers, MatchRng);
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
		// Los demás, dummies de carrera (S-006): buscan el Nicho Libre más cercano. Doble de prueba hasta el bot básico.
		Deciders.Add(Seat == HumanSeat ? TSharedPtr<IDecider>() : TSharedPtr<IDecider>(MakeShared<FRaceDummyDecider>()));
	}
	StepClock = FStepClock();
	StepClock.OriginWorldSeconds = World.GetTimeSeconds();
	State.ClockSeconds = 0.0;
	bHasMatch = true;

	UE_LOG(LogSoulboundMatch, Log, TEXT("Partida iniciada: %d casillas, semilla %d, humano en el asiento %d."), State.Board.Tiles.Num(), UsedSeed, HumanSeat);
	return true;
}

double UMatchHostSubsystem::MatchSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? StepClock.MatchSeconds(World->GetTimeSeconds()) : 0.0;
}

void UMatchHostSubsystem::Advance()
{
	if (!bHasMatch || StepClock.bHeld)
	{
		return; // detenida en el paso a paso: ni dummies, ni reloj, ni turno
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	bool bChanged = false;

	// Las reglas son del motor: aquí solo se pregunta al decisor, se entrega su intención y se avanza el turno.
	while (true)
	{
		// Paso a paso: al entrar en BetweenTurns la partida se detiene hasta que se concede un paso.
		if (StepClock.ShouldHold(bStepMode, State.TurnPhase, State.MatchPhase))
		{
			StepClock.Hold(GetWorld()->GetTimeSeconds());
			UE_LOG(LogSoulboundMatch, Log, TEXT("Paso a paso: partida detenida en t=%.1f s, esperando el siguiente turno."), StepClock.FrozenMatchSeconds);
			bChanged = true;
			break;
		}

		const int32 Seat = State.ActiveSeat;
		const bool bDummy = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid();
		// Quien decide ahora: el asiento de la elección abierta, que puede no tener el turno (S-010: Koschei víctima aparta
		// una runa); sin elección, el activo.
		const int32 ChoiceSeat = State.PendingChoice.IsOpen() ? State.PendingChoice.Seat : Seat;
		const bool bChoiceDummy = Deciders.IsValidIndex(ChoiceSeat) && Deciders[ChoiceSeat].IsValid();
		// Una decisión pendiente de otro turno (otro asiento u otro TurnCount) o de otra cosa (otra elección, o el destino
		// tras una elección) ya no vale: se descarta.
		if (Pending.bPending && (Pending.Seat != ChoiceSeat || Pending.TurnCount != State.TurnCount || Pending.ChoiceKind != State.PendingChoice.Kind))
		{
			Pending.bPending = false;
		}
		if (State.PendingChoice.IsOpen() && bChoiceDummy)
		{
			// Elecciones (S-009, S-010): dados, repetir, salto, acción, runa de Koschei y esfera. Se pregunta al decisor del
			// asiento al que se le pide y se entrega su respuesta a su hora, como el destino; la repetición se contesta antes
			// de elegir destino. La hora es el retardo del dummy o, si el plazo es más corto (la esfera del trilero), la mitad
			// de lo que queda: un dummy nunca agota una elección.
			if (!Pending.bPending)
			{
				Pending.bPending = true;
				Pending.Seat = ChoiceSeat;
				Pending.TurnCount = State.TurnCount;
				Pending.ChoiceKind = State.PendingChoice.Kind;
				Pending.ReadyAtSeconds = Now + FMath::Min(static_cast<double>(DummyResponseSeconds), 0.5 * (State.PendingChoice.DeadlineSeconds - Now));
				Pending.Option = Deciders[ChoiceSeat]->ChooseOption(State, ChoiceSeat, State.PendingChoice, DeciderRng);
			}
			if (Now >= Pending.ReadyAtSeconds)
			{
				bChanged |= Soulbound::Rules::SubmitChoiceIntent(State, ChoiceSeat, Pending.ChoiceKind, Pending.Option, Now, MatchRng) == EIntentResult::Accepted;
				Pending.bPending = false;
				continue;
			}
		}
		else if (State.TurnPhase == ETurnPhase::RollAndChoose && bDummy)
		{
			if (!Pending.bPending)
			{
				Pending.bPending = true;
				Pending.Seat = Seat;
				Pending.TurnCount = State.TurnCount;
				Pending.ChoiceKind = EChoiceKind::None;
				Pending.ReadyAtSeconds = Now + DummyResponseSeconds;
				Pending.TileId = Deciders[Seat]->ChooseDestination(State, Seat, State.ReachableOptions, DeciderRng);
			}
			if (Now >= Pending.ReadyAtSeconds && Pending.TileId != INDEX_NONE)
			{
				bChanged |= Soulbound::Rules::SubmitDestinationIntent(State, Seat, Pending.TileId, Now) == EIntentResult::Accepted;
				Pending.bPending = false;
			}
		}
		else if (State.TurnPhase == ETurnPhase::SkillTrial)
		{
			// El minijuego se resuelve como el destino: se pregunta a un decisor y se entrega su resultado. Mientras el
			// minijuego real del humano no exista (su story), su prueba la responde un dummy, al azar con probabilidad 0,5 (S-008).
			if (!Pending.bPending)
			{
				IDecider& Decider = bDummy ? *Deciders[Seat] : static_cast<IDecider&>(HumanTrialDecider);
				Pending.bPending = true;
				Pending.Seat = Seat;
				Pending.TurnCount = State.TurnCount;
				Pending.ChoiceKind = EChoiceKind::None;
				Pending.ReadyAtSeconds = Now + DummyResponseSeconds;
				Pending.bPassedTrial = Decider.PassSkillMinigame(State, Seat, State.PendingTrial, State.PendingDifficulty, DeciderRng);
#if !UE_BUILD_SHIPPING
				// S-011: el resultado fijado sustituye a la respuesta del dummy del humano; el decisor ya consumió su sorteo.
				if (!bDummy && ForcedTrialResult.IsSet())
				{
					Pending.bPassedTrial = ForcedTrialResult.GetValue();
					ForcedTrialResult.Reset();
				}
#endif
			}
			if (Now >= Pending.ReadyAtSeconds)
			{
				bChanged |= Soulbound::Rules::SubmitSkillTrialResult(State, Seat, Pending.bPassedTrial, Now) == EIntentResult::Accepted;
				Pending.bPending = false;
				// Tras el minijuego puede quedar abierta una elección (S-009, R-INT-1): se atiende en la vuelta siguiente.
				continue;
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
		StepClock.NoteTurnPhase(State.TurnPhase);
		bChanged = true;
	}

	if (bChanged)
	{
		OnMatchStateChanged.Broadcast();
	}
}

void UMatchHostSubsystem::SetStepMode(bool bEnabled)
{
	bStepMode = bEnabled;
	if (!bHasMatch)
	{
		return;
	}
	if (!bEnabled)
	{
		StepClock.Disable(GetWorld()->GetTimeSeconds());
		UE_LOG(LogSoulboundMatch, Log, TEXT("Paso a paso desactivado: la partida sigue sola."));
		OnMatchStateChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogSoulboundMatch, Log, TEXT("Paso a paso activado: la partida se detendra al acabar el turno en curso."));
	}
	Advance();
}

bool UMatchHostSubsystem::StepOnce()
{
	if (!bHasMatch || !StepClock.Step(GetWorld()->GetTimeSeconds()))
	{
		return false;
	}
	UE_LOG(LogSoulboundMatch, Log, TEXT("Paso a paso: se juega un turno."));
	OnMatchStateChanged.Broadcast();
	Advance();
	return true;
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

EIntentResult UMatchHostSubsystem::SubmitHumanChoice(EChoiceKind Kind, int32 Option)
{
	if (!bHasMatch)
	{
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitChoiceIntent(State, HumanSeat, Kind, Option, Now, MatchRng);
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

#if !UE_BUILD_SHIPPING
FDebugEditOutcome UMatchHostSubsystem::FinishDebugEdit(bool bApplied, const FString& Message)
{
	UE_LOG(LogSoulboundMatch, Log, TEXT("Depuración: %s%s"), bApplied ? TEXT("") : TEXT("RECHAZADA, "), *Message);
	if (bApplied)
	{
		OnMatchStateChanged.Broadcast();
	}
	return {bApplied, Message};
}

FDebugEditOutcome UMatchHostSubsystem::DebugGiveRune(int32 Seat, int32 RuneType, int32 Count)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::GiveRune(State, Seat, RuneType, Count, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("%d runas de tipo %d al asiento %d"), Count, RuneType, Seat) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugTakeRune(int32 Seat, int32 RuneType, int32 Count)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::TakeRune(State, Seat, RuneType, Count, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("%d runas de tipo %d quitadas al asiento %d y devueltas a su Fuente"), Count, RuneType, Seat) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugMove(int32 Seat, int32 TileId)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::PlaceToken(State, Seat, TileId, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("asiento %d a la casilla %d, sin resolverla"), Seat, TileId) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugBond(int32 Seat, int32 DomainId)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::BondSeat(State, Seat, DomainId, State.ClockSeconds, MatchRng);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("asiento %d vinculado al Elegido del Dominio %d"), Seat, DomainId) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugSeal(int32 TileId)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const ESealResult Result = Soulbound::Debug::SealTileEdit(State, TileId, State.ClockSeconds);
	const bool bApplied = Result == ESealResult::Accepted;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("casilla %d sellada"), TileId) : Soulbound::Presentation::DescribeSealResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugUnseal(int32 TileId)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::UnsealTile(State, TileId, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("Sello de la casilla %d liberado"), TileId) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugEndRace()
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::EndRace(State, State.ClockSeconds, MatchRng);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString(TEXT("carrera terminada")) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugFixRoll(int32 Value)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::FixNextRoll(State, Value, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("la próxima tirada será %d"), Value) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugFixNextSeat(int32 Seat)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::FixNextSeat(State, Seat, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("el siguiente turno que empiece será del asiento %d"), Seat) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugFixTrial(bool bPass)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	// El resultado vive aquí; el motor solo deja el evento de la edición en el registro, y con él la vista se refresca.
	const EDebugEditResult Result = Soulbound::Debug::FixTrial(State, HumanSeat, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	if (bApplied)
	{
		ForcedTrialResult = bPass;
	}
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("el próximo minijuego del humano se %s"), bPass ? TEXT("supera") : TEXT("falla")) : Soulbound::Presentation::DescribeDebugResult(Result));
}
#endif
