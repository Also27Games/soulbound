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
#include "Match/WardenEventRules.h"
#include "Match/WardenEvents.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Warden/WardenRules.h"

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
	const FWardenEventsLoadResult Events = Soulbound::Rules::LoadWardenEventsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), WardenEventsRelativePath));
	const FWardenParamsLoadResult Warden = Soulbound::Rules::LoadWardenParamsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), WardenParamsRelativePath));
	if (!Board.IsSuccess() || !Params.IsSuccess() || !Elects.IsSuccess() || !Events.IsSuccess() || !Warden.IsSuccess())
	{
		const FString Error = !Board.IsSuccess() ? Board.Message : !Params.IsSuccess() ? Params.Message : !Elects.IsSuccess() ? Elects.Message
			: !Events.IsSuccess() ? Events.Message : Warden.Message;
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

	// S-016: el Warden viaja en el estado, con su sorteo propio (semilla + 2), igual que en la simulación.
	FMatchStartResult Start = Soulbound::Rules::StartMatch(Board.Board, Params.Params, Elects.Table, Soulbound::Match::MaxPlayers, MatchRng, Events.Table, Warden.Params);
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
		// Paso a paso: al entrar en BetweenTurns la partida se detiene hasta que se concede un paso; con un evento del Warden
		// en curso, no, hasta que termina (S-014).
		if (StepClock.ShouldHold(bStepMode, State.TurnPhase, State.MatchPhase, State.CurrentEvent.IsActive()))
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
		if (Pending.bPending && (Pending.Seat != ChoiceSeat || Pending.TurnCount != State.TurnCount || Pending.ChoiceKind != State.PendingChoice.Kind
			|| Pending.bDuel != (State.TurnPhase == ETurnPhase::Duel)))
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
				Pending.bDuel = false;
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
				Pending.bDuel = false;
				Pending.ReadyAtSeconds = Now + DummyResponseSeconds;
				Pending.TileId = Deciders[Seat]->ChooseDestination(State, Seat, State.ReachableOptions, DeciderRng);
			}
			if (Now >= Pending.ReadyAtSeconds && Pending.TileId != INDEX_NONE)
			{
				// S-020 (decisión del autor del 21/10/2026): el camino no se entrega aquí; lo recorre la ficha del dummy, que entrega
				// el avance de cada casilla como la del humano (SubmitMoveProgress), y sin presentación lo cubre la salvaguarda.
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
				Pending.bDuel = false;
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
		else if (State.TurnPhase == ETurnPhase::Duel && State.Duel.IsActive())
		{
			// Duelo (S-012): cada participante rinde con su decisor; por el humano, mientras no exista el minijuego de duelo, un
			// dummy al azar (como el minijuego de la Fuente, S-008). Los dos rendimientos se deciden a la vez, primero el del
			// retador, y se entregan juntos tras el retardo del dummy: una sola espera por duelo.
			if (!Pending.bPending)
			{
				const int32 Challenger = State.Duel.Challenger;
				const int32 Challenged = State.Duel.Challenged;
				auto Perform = [this](int32 DuelSeat) -> double
				{
					IDecider& Decider = Deciders.IsValidIndex(DuelSeat) && Deciders[DuelSeat].IsValid() ? *Deciders[DuelSeat] : static_cast<IDecider&>(HumanTrialDecider);
					return Decider.DuelPerformance(State, DuelSeat, DeciderRng);
				};
				Pending.bPending = true;
				Pending.Seat = Seat;
				Pending.TurnCount = State.TurnCount;
				Pending.ChoiceKind = EChoiceKind::None;
				Pending.bDuel = true;
				Pending.ReadyAtSeconds = Now + DummyResponseSeconds;
				// Contra el Warden (S-015) no hay asiento retador: su rendimiento es un dato del motor y solo rinde el retado.
				Pending.ChallengerPerformance = State.Duel.bAgainstWarden ? 0.0 : Perform(Challenger);
				Pending.ChallengedPerformance = Perform(Challenged);
#if !UE_BUILD_SHIPPING
				// SbDuel: el resultado fijado sustituye a los dos rendimientos; los decisores ya consumieron su sorteo. Con un
				// margen menor que 1 (validado en partida.json), 1 contra 0 gana siempre y 0 contra 1 pierde siempre.
				if (ForcedDuelResult.IsSet() && State.Duel.IsParticipant(HumanSeat))
				{
					const bool bHumanWins = ForcedDuelResult.GetValue();
					const bool bHumanChallenges = Challenger == HumanSeat;
					Pending.ChallengerPerformance = bHumanChallenges == bHumanWins ? 1.0 : 0.0;
					Pending.ChallengedPerformance = 1.0 - Pending.ChallengerPerformance;
					ForcedDuelResult.Reset();
				}
#endif
			}
			if (Now >= Pending.ReadyAtSeconds)
			{
				const int32 Challenger = State.Duel.Challenger;
				const int32 Challenged = State.Duel.Challenged;
				if (!State.Duel.bAgainstWarden)
				{
					bChanged |= Soulbound::Rules::SubmitDuelPerformance(State, Challenger, Pending.ChallengerPerformance, Now) == EIntentResult::Accepted;
				}
				bChanged |= Soulbound::Rules::SubmitDuelPerformance(State, Challenged, Pending.ChallengedPerformance, Now) == EIntentResult::Accepted;
				Pending.bPending = false;
				// Tras el duelo puede abrirse el trilero del botín, con sus elecciones: se atienden en la vuelta siguiente.
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

EIntentResult UMatchHostSubsystem::SubmitMoveProgress(int32 Seat, int32 TileId)
{
	if (!bHasMatch)
	{
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitMoveProgress(State, Seat, TileId, Now);
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

FWardenDecision UMatchHostSubsystem::EvaluateWarden()
{
	if (!bHasMatch)
	{
		return FWardenDecision();
	}
	State.ClockSeconds = MatchSeconds();
	// S-016: el motor decide y aplica (cerebro, registro, corte y evento); aquí solo se escribe y se avisa.
	const FWardenDecision Decision = Soulbound::Rules::EvaluateWarden(State, State.ClockSeconds, MatchRng);
	OnMatchStateChanged.Broadcast();
	Advance();
	return Decision;
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

FDebugEditOutcome UMatchHostSubsystem::DebugFixDuel(bool bWin)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	// Como SbTrial: el resultado vive aquí; el motor solo deja el evento de la edición.
	const EDebugEditResult Result = Soulbound::Debug::FixDuel(State, HumanSeat, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	if (bApplied)
	{
		ForcedDuelResult = bWin;
	}
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("el humano %s el próximo duelo en que participe"), bWin ? TEXT("gana") : TEXT("pierde")) : Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugBanish(int32 Seat)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EEffectResult Result = Soulbound::Debug::BanishSeat(State, Seat, State.ClockSeconds);
	const bool bApplied = Result == EEffectResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("asiento %d desterrado al Vacío"), Seat) : Soulbound::Presentation::DescribeEffectResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugSetInsolence(int32 Seat, int32 Level)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::SetInsolence(State, Seat, Level, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied
		? FString::Printf(TEXT("asiento %d con insolencia %d: la siguiente subida trae el castigo de su nivel%s"), Seat, Level, Level == Soulbound::Match::MaxInsolence - 1 ? TEXT(" (la Furia)") : TEXT(""))
		: Result == EDebugEditResult::InvalidAmount ? FString::Printf(TEXT("la insolencia se fija de 0 a %d"), Soulbound::Match::MaxInsolence - 1)
		: Soulbound::Presentation::DescribeDebugResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugRelease()
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EEffectResult Result = Soulbound::Debug::ReleaseVoidEdit(State, State.ClockSeconds);
	const bool bApplied = Result == EEffectResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString(TEXT("Liberación: todos los del Vacío vuelven a su hogar")) : Soulbound::Presentation::DescribeEffectResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugSendHome(int32 Seat)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EEffectResult Result = Soulbound::Debug::SendHomeEdit(State, Seat, State.ClockSeconds);
	const bool bApplied = Result == EEffectResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("asiento %d vuelve a su hogar"), Seat) : Soulbound::Presentation::DescribeEffectResult(Result));
}

FDebugEditOutcome UMatchHostSubsystem::DebugLaunchEvent(const FString& EventId)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	// S-016: el sorteo del Warden es el del estado; un evento lanzado a mano no gasta usos ni cuenta para la garantía.
	FRandomStream WardenRandom = Soulbound::Rules::WardenDraw(State);
	const EWardenEventStart Result = Soulbound::Debug::LaunchEvent(State, EventId, State.ClockSeconds, WardenRandom);
	const bool bApplied = Result == EWardenEventStart::Started;
	// S-014: con la partida detenida, el reloj vuelve a correr para que corran los plazos del evento; el turno siguiente
	// sigue esperando su paso (ShouldHold vuelve a detener al terminar el evento).
	if (bApplied && State.CurrentEvent.IsActive() && StepClock.bHeld)
	{
		StepClock.Resume(GetWorld()->GetTimeSeconds());
	}
	const FDebugEditOutcome Outcome = FinishDebugEdit(bApplied, bApplied
		? FString::Printf(TEXT("evento \"%s\" lanzado%s"), *EventId, State.CurrentEvent.IsActive() ? TEXT(": espera una elección") : TEXT(" y aplicado"))
		: Soulbound::Presentation::DescribeWardenEventStart(Result));
	if (bApplied)
	{
		Advance();
	}
	return Outcome;
}

FDebugEditOutcome UMatchHostSubsystem::DebugStrikeEvent(const FString& EventId)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const bool bTurnInProgress = State.TurnPhase != ETurnPhase::BetweenTurns;
	FRandomStream WardenRandom = Soulbound::Rules::WardenDraw(State);
	const EWardenEventStart Result = Soulbound::Debug::StrikeEvent(State, EventId, State.ClockSeconds, MatchRng, WardenRandom);
	const bool bApplied = Result == EWardenEventStart::Started;
	if (bApplied && State.CurrentEvent.IsActive() && StepClock.bHeld)
	{
		StepClock.Resume(GetWorld()->GetTimeSeconds());
	}
	// Con un turno en curso, NotBetweenTurns es que está en una fase que no se corta (decisión del autor del 20/10/2026).
	const FString Rejection = bTurnInProgress && Result == EWardenEventStart::NotBetweenTurns
		? FString(TEXT("el turno está en un duelo, un trilero o un minijuego, que no se cortan: prueba cuando termine"))
		: Soulbound::Presentation::DescribeWardenEventStart(Result);
	const FDebugEditOutcome Outcome = FinishDebugEdit(bApplied, bApplied
		? FString::Printf(TEXT("golpe \"%s\" %s%s"), *EventId, bTurnInProgress ? TEXT("corta el turno") : TEXT("lanzado entre turnos"),
			State.CurrentEvent.IsActive() ? TEXT(": espera una elección") : TEXT(" y aplicado"))
		: Rejection);
	if (bApplied)
	{
		Advance();
	}
	return Outcome;
}
#endif
