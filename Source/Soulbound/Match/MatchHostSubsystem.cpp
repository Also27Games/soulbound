#include "MatchHostSubsystem.h"

#include "../Presentation/BoardViewActor.h"
#include "../Presentation/EventText.h"
#include "../Presentation/WardenLineQueue.h"
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
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Warden/WardenLines.h"
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
	// S-026: el anillo no sobrevive al mundo; la vista retira su entrada en su EndPlay.
	HumanRing.Reset();
	OnRingOpened.Clear();
	OnRingClosed.Clear();
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
	// S-021 (R-VOZ-5): las frases se cargan contra los eventos y los Elegidos de los archivos; falta una clave y la partida no empieza.
	const FWardenLinesLoadResult Lines = Events.IsSuccess() && Elects.IsSuccess()
		? Soulbound::Rules::LoadWardenLinesFromFile(FPaths::Combine(FPaths::ProjectContentDir(), WardenLinesRelativePath), Soulbound::Rules::RequiredWardenLineKeys(Events.Table, Elects.Table))
		: FWardenLinesLoadResult();
	if (!Board.IsSuccess() || !Params.IsSuccess() || !Elects.IsSuccess() || !Events.IsSuccess() || !Warden.IsSuccess() || !Lines.IsSuccess())
	{
		const FString Error = !Board.IsSuccess() ? Board.Message : !Params.IsSuccess() ? Params.Message : !Elects.IsSuccess() ? Elects.Message
			: !Events.IsSuccess() ? Events.Message : !Warden.IsSuccess() ? Warden.Message : Lines.Message;
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
	// S-026: las secuencias del anillo del humano, con su propio sorteo (semilla + 5; la simulación usa de + 1 a + 4).
	RingRng.Initialize(UsedSeed + 5);
	HumanRing.Reset();

	// S-016: el Warden viaja en el estado, con su sorteo propio (semilla + 2), igual que en la simulación.
	FMatchStartResult Start = Soulbound::Rules::StartMatch(Board.Board, Params.Params, Elects.Table, Soulbound::Match::MaxPlayers, MatchRng, Events.Table, Warden.Params, Lines.Table);
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

	// S-022 (R-PRO-1, decisión del autor del 29/10/2026): el prólogo. La partida no empieza hasta que el Warden termina la apertura; los
	// segundos de línea son los de la vista, que es quien la enseña. Sin frases no hay apertura ni prólogo.
	PrologueSeconds = Soulbound::Presentation::OpeningSeconds(State, GetDefault<ABoardViewActor>()->WardenLineSeconds);
	bInPrologue = PrologueSeconds > 0.0;
	// S-023: la apertura es del prólogo; la espera del Warden mira lo que se diga desde aquí.
	bWardenSpeaking = false;
	WardenLinesScanned = State.EventLog.Num();
	Curtain = FCurtain();
	if (bInPrologue)
	{
		// S-024 (R-PRO-1 precisión): el prólogo no empieza hasta que la presentación sube el telón (RaiseCurtain) o vence el tope; el
		// final del prólogo se fija entonces (LiftCurtain), no aquí.
		StepClock.Hold(World.GetTimeSeconds());
		Curtain.Lower(World.GetTimeSeconds(), CurtainTimeoutSeconds, CurtainGraceSeconds);
		UE_LOG(LogSoulboundMatch, Log, TEXT("Telón bajado: el prólogo empieza cuando la presentación lo suba o, como tarde, a los %.0f s."), CurtainTimeoutSeconds);
	}

	UE_LOG(LogSoulboundMatch, Log, TEXT("Partida iniciada: %d casillas, semilla %d, humano en el asiento %d."), State.Board.Tiles.Num(), UsedSeed, HumanSeat);
	return true;
}

double UMatchHostSubsystem::MatchSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? StepClock.MatchSeconds(World->GetTimeSeconds()) : 0.0;
}

void UMatchHostSubsystem::EndPrologue(const TCHAR* Why)
{
	if (!bInPrologue)
	{
		return;
	}
	// S-024: las herramientas de depuración (SbEvent, SbStrike, SbClock) acaban el prólogo con el telón aún bajado: se sube antes.
	if (Curtain.Raise(GetWorld()->GetTimeSeconds()))
	{
		UE_LOG(LogSoulboundMatch, Log, TEXT("Telón subido (%s)."), Why);
	}
	// Sin margen: las herramientas de depuración no esperan, y la vista no debe seguir con el telón bajado tras acabar el prólogo.
	Curtain = FCurtain();
	bInPrologue = false;
	StepClock.Resume(GetWorld()->GetTimeSeconds());
	UE_LOG(LogSoulboundMatch, Log, TEXT("Prólogo terminado (%s): empieza la partida."), Why);
	OnMatchStateChanged.Broadcast();
}

bool UMatchHostSubsystem::IsCurtainDown() const
{
	const UWorld* World = GetWorld();
	return World && Curtain.IsDown(World->GetTimeSeconds());
}

void UMatchHostSubsystem::RaiseCurtain()
{
	LiftCurtain(TEXT("presentación"));
}

void UMatchHostSubsystem::LiftCurtain(const TCHAR* Why)
{
	const UWorld* World = GetWorld();
	if (!World || !Curtain.Raise(World->GetTimeSeconds()))
	{
		return;
	}
	// El prólogo dura lo mismo (OpeningSeconds); solo cambia desde cuándo se cuenta: desde la subida más el margen.
	PrologueEndsWorldSeconds = World->GetTimeSeconds() + Curtain.GraceSeconds + PrologueSeconds;
	UE_LOG(LogSoulboundMatch, Log, TEXT("Telón subido (%s)."), Why);
	UE_LOG(LogSoulboundMatch, Log, TEXT("Prólogo: la partida empieza dentro de %.1f s (margen %.1f s y apertura), cuando el Warden termine la apertura."), Curtain.GraceSeconds + PrologueSeconds, Curtain.GraceSeconds);
}

bool UMatchHostSubsystem::NoteWardenLines()
{
	const double Extra = Soulbound::Presentation::WardenWaitSeconds(State, WardenLinesScanned, GetDefault<ABoardViewActor>()->WardenLineSeconds);
	WardenLinesScanned = State.EventLog.Num();
	if (Extra <= 0.0)
	{
		return false;
	}
	const double WorldNow = GetWorld()->GetTimeSeconds();
	if (!bWardenSpeaking)
	{
		bWardenSpeaking = true;
		WardenWaitStartedWorldSeconds = WorldNow;
		WardenWaitEndsWorldSeconds = WorldNow + Extra;
		UE_LOG(LogSoulboundMatch, Log, TEXT("El Warden habla: la mesa espera %.0f s (t=%.1f s; el reloj de partida sigue)."), Extra, MatchSeconds());
	}
	else
	{
		// Decisión del autor del 29/10/2026: una frase dicha más tarde sustituye en pantalla a las que quedaban; la espera acaba con ella.
		WardenWaitEndsWorldSeconds = FMath::Max(WardenWaitEndsWorldSeconds, WorldNow + Extra);
		UE_LOG(LogSoulboundMatch, Log, TEXT("El Warden sigue hablando: la mesa espera hasta dentro de %.0f s."), WardenWaitEndsWorldSeconds - WorldNow);
	}
	return true;
}

void UMatchHostSubsystem::EndWardenWait()
{
	if (!bWardenSpeaking)
	{
		return;
	}
	bWardenSpeaking = false;
	const double Waited = GetWorld()->GetTimeSeconds() - WardenWaitStartedWorldSeconds;
	// La votación de PendingVotes se reconoce por su plazo: si es la abierta, su plazo se mueve con ella.
	const bool bVotesOfOpenVote = State.Vote.IsOpen() && PendingVotesDeadline == State.Vote.DeadlineSeconds;
	const int32 Shifted = Soulbound::Rules::ShiftOpenDeadlines(State, Waited);
	if (bVotesOfOpenVote)
	{
		PendingVotesDeadline = State.Vote.DeadlineSeconds;
	}
	// Las respuestas programadas de los dummies se retrasan lo mismo: contestan a la misma altura de su plazo que sin la espera.
	if (Pending.bPending)
	{
		Pending.ReadyAtSeconds += Waited;
	}
	for (TOptional<TPair<double, int32>>& Vote : PendingVotes)
	{
		if (Vote.IsSet())
		{
			Vote->Key += Waited;
		}
	}
	UE_LOG(LogSoulboundMatch, Log, TEXT("El Warden ha terminado: la mesa ha esperado %.1f s; %d plazos desplazados (t=%.1f s)."), Waited, Shifted, MatchSeconds());
	OnMatchStateChanged.Broadcast();
}

bool UMatchHostSubsystem::RejectWhileWardenSpeaks(const TCHAR* What) const
{
	if (!bWardenSpeaking)
	{
		return false;
	}
	UE_LOG(LogSoulboundMatch, Log, TEXT("Rechazado (%s): la mesa espera a que el Warden termine de hablar."), What);
	return true;
}

void UMatchHostSubsystem::Advance()
{
	if (bHasMatch && bInPrologue)
	{
		// S-024: con el telón bajado nada avanza, salvo que haya vencido el tope y se suba solo.
		if (Curtain.bDown)
		{
			if (!Curtain.TimedOut(GetWorld()->GetTimeSeconds()))
			{
				return;
			}
			LiftCurtain(TEXT("tope"));
		}
		if (GetWorld()->GetTimeSeconds() < PrologueEndsWorldSeconds)
		{
			return; // el Warden aún dice la apertura: ni dummies, ni reloj, ni turno
		}
		EndPrologue(TEXT("el Warden ha dicho la apertura"));
	}
	if (!bHasMatch || StepClock.bHeld)
	{
		return; // detenida en el paso a paso: ni dummies, ni reloj, ni turno
	}
	// S-023: la mesa espera al Warden. Lo dicho fuera de Advance (SbStrike, SbSay, la intención de un humano) también la detiene; el reloj
	// de partida sigue, y al acabar se desplazan los plazos.
	NoteWardenLines();
	if (bWardenSpeaking)
	{
		if (GetWorld()->GetTimeSeconds() < WardenWaitEndsWorldSeconds)
		{
			return; // el Warden habla: ni dummies, ni turno, ni evaluación; el reloj sigue
		}
		EndWardenWait();
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	bool bChanged = false;

	// Las reglas son del motor: aquí solo se pregunta al decisor, se entrega su intención y se avanza el turno.
	while (true)
	{
		// S-023: lo que acaba de pasar puede traer una frase que detiene la mesa (un golpe, una presentación): se para aquí, sin seguir.
		if (NoteWardenLines())
		{
			bChanged = true;
			break;
		}

		// Paso a paso: al entrar en BetweenTurns la partida se detiene hasta que se concede un paso; con un evento del Warden
		// en curso, no, hasta que termina (S-014).
		// S-017: una consulta del Oráculo entre turnos es como un evento: tiene plazo y la partida no se detiene en ella.
		if (StepClock.ShouldHold(bStepMode, State.TurnPhase, State.MatchPhase, State.CurrentEvent.IsActive() || State.Oracle.IsBusy()))
		{
			StepClock.Hold(GetWorld()->GetTimeSeconds());
			UE_LOG(LogSoulboundMatch, Log, TEXT("Paso a paso: partida detenida en t=%.1f s, esperando el siguiente turno."), StepClock.FrozenMatchSeconds);
			bChanged = true;
			break;
		}

		// S-017 (R-VOT-5): en una votación abierta, cada dummy vota a su hora, como contesta una elección: su retardo, o la mitad de lo
		// que queda si el plazo es más corto. El humano vota con un clic en la ficha del votado (SubmitHumanVote).
		if (State.Vote.IsOpen())
		{
			if (PendingVotesDeadline != State.Vote.DeadlineSeconds)
			{
				PendingVotes.Reset();
				PendingVotes.SetNum(State.Seats.Num());
				PendingVotesDeadline = State.Vote.DeadlineSeconds;
			}
			bool bVoted = false;
			for (int32 Voter = 0; Voter < State.Seats.Num() && State.Vote.IsOpen(); ++Voter)
			{
				const TArray<int32> Options = Soulbound::Rules::VoteOptions(State, Voter);
				if (!Deciders.IsValidIndex(Voter) || !Deciders[Voter].IsValid() || State.Vote.Votes[Voter] != INDEX_NONE || Options.IsEmpty())
				{
					continue;
				}
				if (!PendingVotes[Voter].IsSet())
				{
					const double ReadyAt = Now + FMath::Min(static_cast<double>(DummyResponseSeconds), 0.5 * (State.Vote.DeadlineSeconds - Now));
					PendingVotes[Voter] = TPair<double, int32>(ReadyAt, Deciders[Voter]->ChooseVote(State, Voter, Options, DeciderRng));
				}
				if (Now >= PendingVotes[Voter]->Key)
				{
					bVoted |= Soulbound::Rules::SubmitVote(State, Voter, PendingVotes[Voter]->Value, Now) == EIntentResult::Accepted;
					PendingVotes[Voter].Reset();
				}
			}
			if (bVoted)
			{
				bChanged = true;
				continue;
			}
		}

		const int32 Seat = State.ActiveSeat;
		const bool bDummy = Deciders.IsValidIndex(Seat) && Deciders[Seat].IsValid();
		// Quien decide ahora: el asiento de la elección abierta, que puede no tener el turno (S-010: Koschei víctima aparta
		// una runa); sin elección, el que consulta el Oráculo (S-017), que tampoco tiene por qué tenerlo; si no, el activo.
		const bool bOracleWaiting = State.Oracle.IsWaiting() && !State.PendingChoice.IsOpen();
		const int32 ChoiceSeat = State.PendingChoice.IsOpen() ? State.PendingChoice.Seat : bOracleWaiting ? State.Oracle.Consultant : Seat;
		const bool bChoiceDummy = Deciders.IsValidIndex(ChoiceSeat) && Deciders[ChoiceSeat].IsValid();
		// Una decisión pendiente de otro turno (otro asiento u otro TurnCount) o de otra cosa (otra elección, o el destino
		// tras una elección, o la consulta del Oráculo) ya no vale: se descarta.
		if (Pending.bPending && (Pending.Seat != ChoiceSeat || Pending.TurnCount != State.TurnCount || Pending.ChoiceKind != State.PendingChoice.Kind
			|| Pending.bDuel != (State.TurnPhase == ETurnPhase::Duel) || Pending.bOracle != bOracleWaiting))
		{
			Pending = FPendingDecision();
		}
		// S-026: el anillo del humano vive lo que su respuesta pendiente; descartada esta (cambió la fase o el turno), se descarta.
		if (HumanRing.IsSet() && !(Pending.bPending && Pending.bRing))
		{
			CloseRing(TOptional<FRingOutcome>(), TEXT("la fase ha cambiado"));
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
		else if (bOracleWaiting && bChoiceDummy)
		{
			// S-017 (R-VOT-5, R-ORA-3): el dummy consulta tras su retardo, o a la mitad de lo que queda si el plazo es más corto: nunca
			// se le agota. El humano consulta por la vista (SubmitHumanOracleConsult).
			if (!Pending.bPending)
			{
				Pending.bPending = true;
				Pending.Seat = ChoiceSeat;
				Pending.TurnCount = State.TurnCount;
				Pending.ChoiceKind = EChoiceKind::None;
				Pending.bDuel = false;
				Pending.bOracle = true;
				Pending.ReadyAtSeconds = Now + FMath::Min(static_cast<double>(DummyResponseSeconds), 0.5 * (State.Oracle.DeadlineSeconds - Now));
				Pending.bRefuseOracle = State.Oracle.bRefusable && Deciders[ChoiceSeat]->RefusesOracleSentence(State, ChoiceSeat, DeciderRng);
			}
			if (Now >= Pending.ReadyAtSeconds)
			{
				const EIntentResult Result = Pending.bRefuseOracle ? Soulbound::Rules::SubmitOracleRefuse(State, ChoiceSeat, Now) : Soulbound::Rules::SubmitOracleConsult(State, ChoiceSeat, Now);
				bChanged |= Result == EIntentResult::Accepted;
				Pending.bPending = false;
				Pending.bOracle = false;
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
		else if (State.TurnPhase == ETurnPhase::SkillTrial && !bDummy && bHumanPlaysSkillTrials)
		{
			// S-026: el humano juega su prueba con el anillo. Mientras dura, no se entrega nada al motor por él: la máquina espera en
			// SkillTrial, como hoy. Su resultado lo dan las reglas del anillo (RingRules) al cerrarse; SbTrial manda sobre él.
			// TODO(R-MIN): el reloj de turno sigue corriendo durante la Prueba del Warden, como antes de S-026 (cuestión abierta del GDD 09).
			// Una prueba de tres pulsos dura de 5 a 8 s y el reloj es de 30; si el autor decide pararlo, lo hará el motor, no el anfitrión.
			if (!Pending.bPending)
			{
				Pending = FPendingDecision();
				Pending.bPending = true;
				Pending.bRing = true;
				Pending.Seat = Seat;
				Pending.TurnCount = State.TurnCount;
				Pending.ReadyAtSeconds = Now;
#if !UE_BUILD_SHIPPING
				// Condición del autor del 06/11/2026: con un resultado fijado antes de empezar, el anillo no se abre.
				if (ForcedTrialResult.IsSet())
				{
					Pending.bPassedTrial = ForcedTrialResult.GetValue();
					ForcedTrialResult.Reset();
					UE_LOG(LogSoulboundMatch, Log, TEXT("Anillo: no se abre; SbTrial fija la prueba como %s."), Pending.bPassedTrial ? TEXT("superada") : TEXT("fallada"));
				}
				else
#endif
				{
					const TOptional<FRingTrialParams> RingParams = Soulbound::Rules::RingParamsForTrial(State.Params.Ring, State.PendingTrial, State.PendingDifficulty);
					if (RingParams.IsSet())
					{
						OpenRing(*RingParams, State.PendingTrial, false);
					}
				}
			}
			if (SettleRing())
			{
				bChanged |= Soulbound::Rules::SubmitSkillTrialResult(State, Seat, Pending.bPassedTrial, Now) == EIntentResult::Accepted;
				Pending.bPending = false;
				// Tras el minijuego puede quedar abierta una elección (S-009, R-INT-1): se atiende en la vuelta siguiente.
				continue;
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
				// S-026: con el interruptor encendido, el humano rinde con el anillo y su decisor no se consulta; el rival dummy sigue con el
				// suyo y el Warden, en su duelo, con su valor de datos.
				const bool bHumanRing = bHumanPlaysSkillTrials && State.Duel.IsParticipant(HumanSeat);
				auto Perform = [this, bHumanRing](int32 DuelSeat) -> double
				{
					if (bHumanRing && DuelSeat == HumanSeat)
					{
						return 0.0; // lo pone el anillo al cerrarse (SettleRing)
					}
					IDecider& Decider = Deciders.IsValidIndex(DuelSeat) && Deciders[DuelSeat].IsValid() ? *Deciders[DuelSeat] : static_cast<IDecider&>(HumanTrialDecider);
					return Decider.DuelPerformance(State, DuelSeat, DeciderRng);
				};
				Pending = FPendingDecision();
				Pending.bPending = true;
				Pending.bRing = bHumanRing;
				Pending.Seat = Seat;
				Pending.TurnCount = State.TurnCount;
				Pending.ChoiceKind = EChoiceKind::None;
				Pending.bDuel = true;
				// Con el anillo, la espera es la del propio anillo: se entrega en cuanto se cierra.
				Pending.ReadyAtSeconds = bHumanRing ? Now : Now + DummyResponseSeconds;
				// Contra el Warden (S-015) no hay asiento retador: su rendimiento es un dato del motor y solo rinde el retado.
				Pending.ChallengerPerformance = State.Duel.bAgainstWarden ? 0.0 : Perform(Challenger);
				Pending.ChallengedPerformance = Perform(Challenged);
				bool bForced = false;
#if !UE_BUILD_SHIPPING
				// SbDuel: el resultado fijado sustituye a los dos rendimientos; los decisores ya consumieron su sorteo. Con un
				// margen menor que 1 (validado en partida.json), 1 contra 0 gana siempre y 0 contra 1 pierde siempre. Con el anillo,
				// fijado antes de empezar, el anillo no se abre (condición del autor del 06/11/2026).
				if (ForcedDuelResult.IsSet() && State.Duel.IsParticipant(HumanSeat))
				{
					const bool bHumanWins = ForcedDuelResult.GetValue();
					const bool bHumanChallenges = Challenger == HumanSeat;
					Pending.ChallengerPerformance = bHumanChallenges == bHumanWins ? 1.0 : 0.0;
					Pending.ChallengedPerformance = 1.0 - Pending.ChallengerPerformance;
					ForcedDuelResult.Reset();
					bForced = true;
				}
#endif
				if (bHumanRing && !bForced)
				{
					OpenRing(Soulbound::Rules::RingParamsForDuel(State.Params.Ring), ESkillTrial::None, true);
				}
			}
			if (SettleRing() && Now >= Pending.ReadyAtSeconds)
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
			// S-026: fuera de una prueba o un duelo no hay anillo que jugar.
			CloseRing(TOptional<FRingOutcome>(), TEXT("la fase ha cambiado"));
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
	if (!bEnabled && bInPrologue)
	{
		// S-022: el prólogo no es una espera del paso a paso; salir del modo no lo acorta.
		UE_LOG(LogSoulboundMatch, Log, TEXT("Paso a paso desactivado: el prólogo sigue hasta que el Warden termine la apertura."));
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
	// S-022: en el prólogo no hay turno que soltar.
	if (!bHasMatch || bInPrologue || !StepClock.Step(GetWorld()->GetTimeSeconds()))
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
	if (!bHasMatch || RejectWhileWardenSpeaks(TEXT("intención de destino")))
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
	if (!bHasMatch || RejectWhileWardenSpeaks(TEXT("avance de ficha")))
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
	if (!bHasMatch || RejectWhileWardenSpeaks(TEXT("elección")))
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

EIntentResult UMatchHostSubsystem::SubmitHumanOracleConsult()
{
	if (!bHasMatch || RejectWhileWardenSpeaks(TEXT("consulta del Oráculo")))
	{
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitOracleConsult(State, HumanSeat, Now);
	if (Result == EIntentResult::Accepted)
	{
		OnMatchStateChanged.Broadcast();
		// La sentencia puede terminar la consulta y dejar que el turno siga: se avanza ya, sin esperar al siguiente tick.
		Advance();
	}
	return Result;
}

EIntentResult UMatchHostSubsystem::SubmitHumanVote(int32 Voted)
{
	if (!bHasMatch || RejectWhileWardenSpeaks(TEXT("voto")))
	{
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitVote(State, HumanSeat, Voted, Now);
	if (Result == EIntentResult::Accepted)
	{
		OnMatchStateChanged.Broadcast();
		// El último voto resuelve la votación y el evento sigue: se avanza ya.
		Advance();
	}
	return Result;
}

EIntentResult UMatchHostSubsystem::SubmitHumanOracleRefuse()
{
	if (!bHasMatch || RejectWhileWardenSpeaks(TEXT("rechazo de la sentencia")))
	{
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitOracleRefuse(State, HumanSeat, Now);
	if (Result == EIntentResult::Accepted)
	{
		OnMatchStateChanged.Broadcast();
		Advance();
	}
	return Result;
}

bool UMatchHostSubsystem::PressRing(double PressSeconds)
{
	if (!HumanRing.IsSet())
	{
		return false;
	}
	const int32 Before = HumanRing->Run.Precisions().Num();
	const bool bCounted = HumanRing->Run.Press(PressSeconds);
	if (bCounted)
	{
		UE_LOG(LogSoulboundMatch, Log, TEXT("Anillo: pulso %d de %d, precisión %.2f."), HumanRing->Run.Precisions().Num(), HumanRing->Run.NumPulses(),
			HumanRing->Run.Precisions().Last());
	}
	else if (HumanRing->Run.Precisions().Num() > Before)
	{
		UE_LOG(LogSoulboundMatch, Log, TEXT("Anillo: la pulsación cae entre pulsos; el pulso vencido queda en 0."));
	}
	return bCounted;
}

void UMatchHostSubsystem::OpenRing(const FRingTrialParams& Params, ESkillTrial Trial, bool bDuel)
{
	// La semilla de cada prueba sale del sorteo del anillo, derivado de la semilla de la partida: la misma partida da las mismas secuencias.
	const int32 RingSeed = static_cast<int32>(RingRng.GetUnsignedInt() & 0x7fffffff);
	FHumanRing Ring;
	Ring.Params = Params;
	Ring.Trial = Trial;
	Ring.bDuel = bDuel;
	Ring.Run.Begin(Soulbound::Rules::MakeRingSequence(State.Params.Ring, Params, RingSeed), FPlatformTime::Seconds() + FMath::Max(RingAnnounceSeconds, 0.f));
	HumanRing = MoveTemp(Ring);
	UE_LOG(LogSoulboundMatch, Log, TEXT("Anillo abierto (%s): %d pulsos, cierre %.2f s, zona %.2f, semilla %d."),
		bDuel ? TEXT("duelo") : *UEnum::GetValueAsString(Trial), Params.Pulses, Params.CloseSeconds, Params.SweetZone, RingSeed);
	OnRingOpened.Broadcast();
}

void UMatchHostSubsystem::CloseRing(const TOptional<FRingOutcome>& Outcome, const TCHAR* Why)
{
	if (!HumanRing.IsSet())
	{
		return;
	}
	HumanRing.Reset();
	UE_LOG(LogSoulboundMatch, Log, TEXT("Anillo cerrado: %s."), Why);
	OnRingClosed.Broadcast(Outcome);
}

FRingOutcome UMatchHostSubsystem::MakeRingOutcome(bool bForced) const
{
	FRingOutcome Outcome;
	if (!HumanRing.IsSet())
	{
		return Outcome;
	}
	Outcome.Trial = HumanRing->Trial;
	Outcome.bDuel = HumanRing->bDuel;
	Outcome.bForced = bForced;
	Outcome.Precisions = HumanRing->Run.Precisions();
	if (Outcome.bDuel)
	{
		Outcome.Performance = Soulbound::Rules::RingDuelPerformance(Outcome.Precisions);
	}
	else
	{
		Outcome.bPassed = Soulbound::Rules::RingTrialPassed(HumanRing->Params, Outcome.Precisions);
	}
	return Outcome;
}

bool UMatchHostSubsystem::SettleRing()
{
	if (!HumanRing.IsSet())
	{
		return true;
	}
	HumanRing->Run.Advance(FPlatformTime::Seconds());
	if (!HumanRing->Run.IsFinished())
	{
		return false;
	}
	const FRingOutcome Outcome = MakeRingOutcome(false);
	if (Outcome.bDuel)
	{
		// R-MIN-4: el rendimiento del humano es la media; el del rival ya lo decidió su decisor al empezar.
		(State.Duel.Challenger == HumanSeat ? Pending.ChallengerPerformance : Pending.ChallengedPerformance) = Outcome.Performance;
	}
	else
	{
		Pending.bPassedTrial = Outcome.bPassed;
	}
	CloseRing(Outcome, Outcome.bDuel ? TEXT("duelo jugado") : Outcome.bPassed ? TEXT("prueba superada") : TEXT("prueba fallada"));
	return true;
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
		// S-026 (condición del autor del 06/11/2026): fijado durante la prueba, el anillo se cierra en el acto con ese resultado; el
		// motor lo recibe en el siguiente avance.
		if (HumanRing.IsSet() && !HumanRing->bDuel && Pending.bPending && Pending.bRing)
		{
			Pending.bPassedTrial = bPass;
			ForcedTrialResult.Reset();
			FRingOutcome Outcome = MakeRingOutcome(true);
			Outcome.bPassed = bPass;
			CloseRing(Outcome, TEXT("SbTrial fija el resultado"));
		}
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
		// S-026 (condición del autor del 06/11/2026): fijado durante el duelo, el anillo se cierra en el acto con ese resultado, como
		// hace SbDuel con los rendimientos: 1 contra 0 o al revés.
		if (HumanRing.IsSet() && HumanRing->bDuel && Pending.bPending && Pending.bRing && State.Duel.IsParticipant(HumanSeat))
		{
			const bool bHumanChallenges = State.Duel.Challenger == HumanSeat;
			Pending.ChallengerPerformance = bHumanChallenges == bWin ? 1.0 : 0.0;
			Pending.ChallengedPerformance = 1.0 - Pending.ChallengerPerformance;
			ForcedDuelResult.Reset();
			FRingOutcome Outcome = MakeRingOutcome(true);
			Outcome.Performance = bWin ? 1.0 : 0.0;
			CloseRing(Outcome, TEXT("SbDuel fija el resultado"));
		}
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

FDebugEditOutcome UMatchHostSubsystem::DebugSetClock(double Seconds)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	// S-022: adelantar el reloj con el prólogo en marcha lo acaba antes; si no, el reloj seguiría detenido por él.
	EndPrologue(TEXT("SbClock"));
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EDebugEditResult Result = Soulbound::Debug::SetClock(State, Seconds, Now);
	const bool bApplied = Result == EDebugEditResult::Applied;
	if (bApplied)
	{
		StepClock.Skip(Seconds - Now);
	}
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("reloj de partida en %02d:%02d"), FMath::FloorToInt32(Seconds) / 60, FMath::FloorToInt32(Seconds) % 60)
		: FString::Printf(TEXT("el reloj solo se adelanta: ahora marca %.0f s"), Now));
}

FDebugEditOutcome UMatchHostSubsystem::DebugSay(const FString& Key)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	State.ClockSeconds = MatchSeconds();
	const EDebugEditResult Result = Soulbound::Debug::SayLine(State, Key, State.ClockSeconds);
	const bool bApplied = Result == EDebugEditResult::Applied;
	return FinishDebugEdit(bApplied, bApplied ? FString::Printf(TEXT("el Warden dice \"%s\""), *Key) : Soulbound::Presentation::DescribeDebugResult(Result));
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
	// S-022: un evento con el prólogo en marcha lo acaba antes de lanzarse, para que corran sus plazos.
	EndPrologue(TEXT("SbEvent"));
	State.ClockSeconds = MatchSeconds();
	// S-016: el sorteo del Warden es el del estado; un evento lanzado a mano no gasta usos ni cuenta para la garantía.
	FRandomStream WardenRandom = Soulbound::Rules::WardenDraw(State);
	const EWardenEventStart Result = Soulbound::Debug::LaunchEvent(State, EventId, State.ClockSeconds, WardenRandom);
	const bool bApplied = Result == EWardenEventStart::Started;
	// S-014: con la partida detenida, el reloj vuelve a correr para que corran los plazos del evento; el turno siguiente
	// sigue esperando su paso (ShouldHold vuelve a detener al terminar el evento). S-017: también con una consulta del Oráculo pendiente.
	if (bApplied && (State.CurrentEvent.IsActive() || State.Oracle.IsBusy()) && StepClock.bHeld)
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
	// S-022: como SbEvent, acaba el prólogo antes de golpear.
	EndPrologue(TEXT("SbStrike"));
	State.ClockSeconds = MatchSeconds();
	const bool bTurnInProgress = State.TurnPhase != ETurnPhase::BetweenTurns;
	FRandomStream WardenRandom = Soulbound::Rules::WardenDraw(State);
	const EWardenEventStart Result = Soulbound::Debug::StrikeEvent(State, EventId, State.ClockSeconds, MatchRng, WardenRandom);
	const bool bApplied = Result == EWardenEventStart::Started;
	if (bApplied && (State.CurrentEvent.IsActive() || State.Oracle.IsBusy()) && StepClock.bHeld)
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
