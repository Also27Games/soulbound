#include "MatchHostSubsystem.h"

#include "../Presentation/BoardViewActor.h"
#include "../Presentation/EventText.h"
#include "../Presentation/MatchRecord.h"
#include "Framework/Application/SlateApplication.h"
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
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
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
	// S-030: cada aviso de cambio del estado vuelca al acta los eventos nuevos. Se engancha antes que la vista: el acta no depende de ella.
	OnMatchStateChanged.AddUObject(this, &UMatchHostSubsystem::FlushRecordEvents);

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
	// S-030 (R-ACT-6): el acta llega hasta el último evento y se cierra, abandonada si la partida estaba a medias. Una partida ya terminada
	// ya la cerró: el cierre se escribe una sola vez.
	if (bRecordOpen)
	{
		FlushRecordEvents();
		WriteRecordClosing();
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
	// S-030: el acta se abre ya, con la cabecera, para que lo que sigue (telón, prólogo) quede en ella.
	RecordEventsWritten = 0;
	OpenRecord();

	// S-022 (R-PRO-1, decisión del autor del 29/10/2026): el prólogo. La partida no empieza hasta que el Warden termina la apertura; los
	// segundos de línea son los de la vista, que es quien la enseña. Sin frases no hay apertura ni prólogo. Desde S-041 (R-VOZ-6), cada frase dura
	// según su longitud, con el prefijo con que la enseña la vista.
	const ABoardViewActor* ViewDefaults = GetDefault<ABoardViewActor>();
	PrologueSeconds = Soulbound::Presentation::OpeningSeconds(State, HumanSeat, ViewDefaults->WardenLineSeconds, ViewDefaults->WardenLineSecondsPerChar);
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
		Note(ELogVerbosity::Log, FString::Printf(TEXT("Telón bajado: el prólogo empieza cuando la presentación lo suba o, como tarde, a los %.0f s."), CurtainTimeoutSeconds));
	}

	Note(ELogVerbosity::Log, FString::Printf(TEXT("Partida iniciada: %d casillas, semilla %d, humano en el asiento %d."), State.Board.Tiles.Num(), UsedSeed, HumanSeat));
	return true;
}

void UMatchHostSubsystem::Note(ELogVerbosity::Type Verbosity, const FString& Text, const FString& RecordText)
{
	// El texto del log es el de siempre, letra por letra; la verbosidad se decide aquí porque UE_LOG la quiere en tiempo de compilación.
	switch (Verbosity)
	{
	case ELogVerbosity::Warning:
		UE_LOG(LogSoulboundMatch, Warning, TEXT("%s"), *Text);
		break;
	case ELogVerbosity::Error:
		UE_LOG(LogSoulboundMatch, Error, TEXT("%s"), *Text);
		break;
	default:
		UE_LOG(LogSoulboundMatch, Log, TEXT("%s"), *Text);
		break;
	}
	RecordLine(TEXT("host"), RecordText.IsEmpty() ? Text : RecordText);
}

void UMatchHostSubsystem::OpenRecord()
{
	bRecordOpen = false;
	if (!bWriteMatchRecord)
	{
		return;
	}
	const FDateTime StartedAt = FDateTime::Now();
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), MatchRecordDirectory));
	RecordPath = FPaths::Combine(Directory, Soulbound::Presentation::MatchRecordFileName(StartedAt, UsedSeed));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString BuildText = FString::Printf(TEXT("%s %s"), FApp::GetBuildVersion(), LexToString(FApp::GetBuildConfiguration()));
	bRecordOpen = true;
	AppendToRecord(FString::Join(Soulbound::Presentation::MatchRecordHeader(State, UsedSeed, HumanSeat, BuildText, BoardRelativePath, StartedAt), LINE_TERMINATOR));
	if (bRecordOpen)
	{
		UE_LOG(LogSoulboundMatch, Log, TEXT("Acta de partida: %s"), *RecordPath);
	}
}

void UMatchHostSubsystem::AppendToRecord(const FString& Text)
{
	if (!bRecordOpen)
	{
		return;
	}
	// Cada escritura abre, añade y cierra el archivo: el cierre lo vuelca al disco, y si el jugador cierra la ventana a mitad, el acta llega
	// hasta la última línea escrita.
	if (!FFileHelper::SaveStringToFile(Text + LINE_TERMINATOR, *RecordPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		UE_LOG(LogSoulboundMatch, Warning, TEXT("Acta de partida: no se puede escribir en %s; la partida sigue sin acta."), *RecordPath);
		bRecordOpen = false;
	}
}

void UMatchHostSubsystem::FlushRecordEvents()
{
	if (!bRecordOpen || !bHasMatch)
	{
		return;
	}
	if (RecordEventsWritten < State.EventLog.Num())
	{
		TArray<FString> Lines;
		const int32 RingSeat = bHumanPlaysSkillTrials ? HumanSeat : INDEX_NONE;
		for (int32 Index = RecordEventsWritten; Index < State.EventLog.Num(); ++Index)
		{
			Lines.Add(Soulbound::Presentation::MatchRecordLine(State, Index, HumanSeat, RingSeat));
		}
		RecordEventsWritten = State.EventLog.Num();
		AppendToRecord(FString::Join(Lines, LINE_TERMINATOR));
	}
	if (State.IsMatchOver())
	{
		WriteRecordClosing();
	}
}

void UMatchHostSubsystem::RecordLine(const TCHAR* Prefix, const FString& Text)
{
	if (!bRecordOpen)
	{
		return;
	}
	// Primero lo que el registro ya tiene, y luego esta línea: el archivo se lee siempre en orden cronológico.
	FlushRecordEvents();
	if (bRecordOpen)
	{
		AppendToRecord(Soulbound::Presentation::MatchRecordHostLine(MatchSeconds(), Prefix, Text));
	}
}

void UMatchHostSubsystem::RecordWardenNote(const FString& Text)
{
	RecordLine(TEXT("warden"), Text);
}

void UMatchHostSubsystem::WriteRecordClosing()
{
	if (!bRecordOpen)
	{
		return;
	}
	TArray<FString> Lines;
	Lines.Add(TEXT("# --- cierre ---"));
	Lines.Append(Soulbound::Presentation::MatchRecordSummary(State));
	AppendToRecord(FString::Join(Lines, LINE_TERMINATOR));
	bRecordOpen = false;
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
		Note(ELogVerbosity::Log, FString::Printf(TEXT("Telón subido (%s)."), Why));
	}
	// Sin margen: las herramientas de depuración no esperan, y la vista no debe seguir con el telón bajado tras acabar el prólogo.
	Curtain = FCurtain();
	bInPrologue = false;
	StepClock.Resume(GetWorld()->GetTimeSeconds());
	Note(ELogVerbosity::Log, FString::Printf(TEXT("Prólogo terminado (%s): empieza la partida."), Why));
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
	Note(ELogVerbosity::Log, FString::Printf(TEXT("Telón subido (%s)."), Why));
	Note(ELogVerbosity::Log, FString::Printf(TEXT("Prólogo: la partida empieza dentro de %.1f s (margen %.1f s y apertura), cuando el Warden termine la apertura."), Curtain.GraceSeconds + PrologueSeconds, Curtain.GraceSeconds));
}

bool UMatchHostSubsystem::NoteWardenLines()
{
	// S-041 (R-VOZ-6): la suma de las duraciones de lo que detiene la mesa, las mismas frases y duraciones que enseña la vista (WardenLineItems).
	const ABoardViewActor* ViewDefaults = GetDefault<ABoardViewActor>();
	const double Extra = Soulbound::Presentation::WardenWaitSeconds(State, WardenLinesScanned, HumanSeat, ViewDefaults->WardenLineSeconds, ViewDefaults->WardenLineSecondsPerChar);
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
		Note(ELogVerbosity::Log, FString::Printf(TEXT("El Warden habla: la mesa espera %.1f s (t=%.1f s; el reloj de partida sigue)."), Extra, MatchSeconds()));
	}
	else
	{
		// Decisión del autor del 29/10/2026: una frase dicha más tarde sustituye en pantalla a las que quedaban; la espera acaba con ella.
		WardenWaitEndsWorldSeconds = FMath::Max(WardenWaitEndsWorldSeconds, WorldNow + Extra);
		Note(ELogVerbosity::Log, FString::Printf(TEXT("El Warden sigue hablando: la mesa espera hasta dentro de %.1f s."), WardenWaitEndsWorldSeconds - WorldNow));
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
	Note(ELogVerbosity::Log, FString::Printf(TEXT("El Warden ha terminado: la mesa ha esperado %.1f s; %d plazos desplazados (t=%.1f s)."), Waited, Shifted, MatchSeconds()));
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
	// S-032 (R-ANF-2): el bucle tiene tope. Una llamada normal da menos de diez vueltas; si se pasa, algo no avanza y se deja constancia.
	// TODO(R-ANF-1): el bucle entero sigue aquí, en el anfitrión, y solo se prueba a mano con el acta; sacarlo al motor para probarlo sin mundo
	// (S-032 solo extrae la decisión de repetir y el tope) es una refactorización grande que se deja para otra story.
	FAdvanceGuard Guard;
	while (true)
	{
		if (!Guard.Allow())
		{
			int32 Trips = 0;
			if (AdvanceTripNotice.NoteTrip(GetWorld()->GetTimeSeconds(), Trips))
			{
				Note(ELogVerbosity::Error, FString::Printf(TEXT("Advance: el bucle ha superado las %d vueltas y se corta hasta el siguiente tick (%d %s desde el aviso anterior). Fase del turno %d, asiento activo %d, elección %s, votación %s, Oráculo %s, reloj %.1f s."),
					Guard.Limit, Trips, Trips == 1 ? TEXT("vez") : TEXT("veces"), static_cast<int32>(State.TurnPhase), State.ActiveSeat,
					State.PendingChoice.IsOpen() ? TEXT("abierta") : TEXT("no"), State.Vote.IsOpen() ? TEXT("abierta") : TEXT("no"),
					State.Oracle.IsBusy() ? TEXT("abierto") : TEXT("no"), Now));
			}
			bChanged = true;
			break;
		}
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
			Note(ELogVerbosity::Log, FString::Printf(TEXT("Paso a paso: partida detenida en t=%.1f s, esperando el siguiente turno."), StepClock.FrozenMatchSeconds));
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
					bVoted |= Soulbound::Match::AfterAnswer(Soulbound::Rules::SubmitVote(State, Voter, PendingVotes[Voter]->Value, Now)) == Soulbound::Match::EAfterAnswer::Repeat;
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
				const EIntentResult Result = Soulbound::Rules::SubmitChoiceIntent(State, ChoiceSeat, Pending.ChoiceKind, Pending.Option, Now, MatchRng);
				bChanged |= Result == EIntentResult::Accepted;
				Pending.bPending = false;
				// S-032 (R-ANF-1): solo se repite la vuelta si el motor aceptó; si no, sigue hasta AdvanceTurn, que caduca la elección vencida.
				if (Soulbound::Match::AfterAnswer(Result) == Soulbound::Match::EAfterAnswer::Repeat)
				{
					continue;
				}
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
				if (Soulbound::Match::AfterAnswer(Result) == Soulbound::Match::EAfterAnswer::Repeat)
				{
					continue; // S-032 (R-ANF-1): si no la aceptó, sigue hasta AdvanceTurn
				}
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
					Note(ELogVerbosity::Log, FString::Printf(TEXT("Anillo: no se abre; SbTrial fija la prueba como %s."), Pending.bPassedTrial ? TEXT("superada") : TEXT("fallada")));
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
				const EIntentResult Result = Soulbound::Rules::SubmitSkillTrialResult(State, Seat, Pending.bPassedTrial, Now);
				bChanged |= Result == EIntentResult::Accepted;
				Pending.bPending = false;
				// Tras el minijuego puede quedar abierta una elección (S-009, R-INT-1): se atiende en la vuelta siguiente. S-032 (R-ANF-1): solo si
				// el motor aceptó; si no, la vuelta sigue hasta AdvanceTurn.
				if (Soulbound::Match::AfterAnswer(Result) == Soulbound::Match::EAfterAnswer::Repeat)
				{
					continue;
				}
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
				const EIntentResult Result = Soulbound::Rules::SubmitSkillTrialResult(State, Seat, Pending.bPassedTrial, Now);
				bChanged |= Result == EIntentResult::Accepted;
				Pending.bPending = false;
				// Tras el minijuego puede quedar abierta una elección (S-009, R-INT-1): se atiende en la vuelta siguiente. S-032 (R-ANF-1): solo si
				// el motor aceptó; si no, la vuelta sigue hasta AdvanceTurn.
				if (Soulbound::Match::AfterAnswer(Result) == Soulbound::Match::EAfterAnswer::Repeat)
				{
					continue;
				}
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
				// Contra el Warden no hay entrega del retador: cuenta como no aceptada.
				EIntentResult ChallengerResult = EIntentResult::WrongSeat;
				if (!State.Duel.bAgainstWarden)
				{
					ChallengerResult = Soulbound::Rules::SubmitDuelPerformance(State, Challenger, Pending.ChallengerPerformance, Now);
				}
				const EIntentResult ChallengedResult = Soulbound::Rules::SubmitDuelPerformance(State, Challenged, Pending.ChallengedPerformance, Now);
				bChanged |= ChallengerResult == EIntentResult::Accepted || ChallengedResult == EIntentResult::Accepted;
				Pending.bPending = false;
				// Tras el duelo puede abrirse el trilero del botín, con sus elecciones: se atienden en la vuelta siguiente. S-032 (R-ANF-1): se repite
				// si alguna de las dos entregas fue aceptada (decisión del autor); si ninguna, la vuelta sigue hasta AdvanceTurn.
				if (Soulbound::Match::AfterAnswer(ChallengerResult) == Soulbound::Match::EAfterAnswer::Repeat
					|| Soulbound::Match::AfterAnswer(ChallengedResult) == Soulbound::Match::EAfterAnswer::Repeat)
				{
					continue;
				}
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
	// S-030: lo que no avisó de un cambio también queda en el acta antes del siguiente tick.
	FlushRecordEvents();
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
		Note(ELogVerbosity::Log, TEXT("Paso a paso desactivado: el prólogo sigue hasta que el Warden termine la apertura."));
		return;
	}
	if (!bEnabled)
	{
		StepClock.Disable(GetWorld()->GetTimeSeconds());
		Note(ELogVerbosity::Log, TEXT("Paso a paso desactivado: la partida sigue sola."));
		OnMatchStateChanged.Broadcast();
	}
	else
	{
		Note(ELogVerbosity::Log, TEXT("Paso a paso activado: la partida se detendra al acabar el turno en curso."));
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
	Note(ELogVerbosity::Log, TEXT("Paso a paso: se juega un turno."));
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
	// S-030 (R-ACT-8): cada entrada del humano deja su línea, con lo que devolvió el motor o con que la mesa esperaba al Warden.
	const FString What = FString::Printf(TEXT("destino: casilla %d (%s)"), TileId, *Soulbound::Presentation::TileShortName(State.Board, TileId));
	if (RejectWhileWardenSpeaks(TEXT("intención de destino")))
	{
		RecordHuman(What + TEXT(" -> la mesa espera al Warden"));
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitDestinationIntent(State, HumanSeat, TileId, Now);
	if (Result == EIntentResult::Unreachable)
	{
		// S-039 (R-DEC-16, R-ACT-8): el motivo, que la vista enseña y el acta escribe; el motor lo calcula sin cambiar el estado.
		++LastHumanRejection.Serial;
		LastHumanRejection.Reason = Soulbound::Rules::UnreachableReason(State, HumanSeat, TileId);
		LastHumanRejection.Roll = State.Roll;
		// El punto del clic se lee ahora, en la misma llamada que el clic: el controlador llama a SubmitHumanIntent al recibirlo, con el cursor donde se
		// hizo. Leerlo luego, en el refresco del HUD, daría el punto de ese momento.
		LastHumanRejection.ScreenPosition = FSlateApplication::IsInitialized() ? FVector2f(FSlateApplication::Get().GetCursorPos()) : FVector2f::ZeroVector;
		RecordHuman(FString::Printf(TEXT("%s -> %s (%s)"), *What, *Soulbound::Presentation::IntentResultText(Result),
			*Soulbound::Presentation::UnreachableReasonText(LastHumanRejection.Reason)));
	}
	else
	{
		RecordHuman(FString::Printf(TEXT("%s -> %s"), *What, *Soulbound::Presentation::IntentResultText(Result)));
	}
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
	if (RejectWhileWardenSpeaks(TEXT("avance de ficha")))
	{
		// El avance es de la vista, no una entrada del humano (R-ACT-8): no tiene línea "humano". El rechazo sí deja la suya, para que el acta
		// no pierda un avance que no llegó.
		RecordLine(TEXT("host"), FString::Printf(TEXT("Rechazado (avance de ficha, asiento %d, casilla %d): la mesa espera a que el Warden termine de hablar."), Seat, TileId));
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
	const FString What = FString::Printf(TEXT("elección %s: opción %s"), *StaticEnum<EChoiceKind>()->GetNameStringByValue(static_cast<int64>(Kind)),
		Option == INDEX_NONE ? TEXT("no") : *FString::FromInt(Option));
	if (RejectWhileWardenSpeaks(TEXT("elección")))
	{
		RecordHuman(What + TEXT(" -> la mesa espera al Warden"));
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitChoiceIntent(State, HumanSeat, Kind, Option, Now, MatchRng);
	RecordHuman(FString::Printf(TEXT("%s -> %s"), *What, *Soulbound::Presentation::IntentResultText(Result)));
	if (Result == EIntentResult::Accepted)
	{
		OnMatchStateChanged.Broadcast();
	}
	return Result;
}

EIntentResult UMatchHostSubsystem::SubmitHumanOracleConsult()
{
	if (!bHasMatch)
	{
		return EIntentResult::WrongPhase;
	}
	if (RejectWhileWardenSpeaks(TEXT("consulta del Oráculo")))
	{
		RecordHuman(TEXT("consulta del Oráculo -> la mesa espera al Warden"));
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitOracleConsult(State, HumanSeat, Now);
	RecordHuman(FString::Printf(TEXT("consulta del Oráculo -> %s"), *Soulbound::Presentation::IntentResultText(Result)));
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
	if (!bHasMatch)
	{
		return EIntentResult::WrongPhase;
	}
	const FString What = FString::Printf(TEXT("voto por el asiento %d"), Voted);
	if (RejectWhileWardenSpeaks(TEXT("voto")))
	{
		RecordHuman(What + TEXT(" -> la mesa espera al Warden"));
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitVote(State, HumanSeat, Voted, Now);
	RecordHuman(FString::Printf(TEXT("%s -> %s"), *What, *Soulbound::Presentation::IntentResultText(Result)));
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
	if (!bHasMatch)
	{
		return EIntentResult::WrongPhase;
	}
	if (RejectWhileWardenSpeaks(TEXT("rechazo de la sentencia")))
	{
		RecordHuman(TEXT("rechazo de la sentencia del Oráculo -> la mesa espera al Warden"));
		return EIntentResult::WrongPhase;
	}
	const double Now = MatchSeconds();
	State.ClockSeconds = Now;
	const EIntentResult Result = Soulbound::Rules::SubmitOracleRefuse(State, HumanSeat, Now);
	RecordHuman(FString::Printf(TEXT("rechazo de la sentencia del Oráculo -> %s"), *Soulbound::Presentation::IntentResultText(Result)));
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
	// S-030 (R-ACT-10): se avanza (los pulsos vencidos antes de la pulsación se resuelven y dejan su línea), se consulta qué sería esta
	// pulsación (cuente o no, deja su línea en el acta) y luego se pulsa. Press repite lo que decidió la consulta.
	HumanRing->Run.Advance(PressSeconds);
	LogRingPulses(false);
	RecordHuman(Soulbound::Presentation::RingPressLine(HumanRing->Run.QueryPress(PressSeconds)));
	// Si la pulsación cuenta, el último pulso resuelto es el suyo.
	const bool bCounted = HumanRing->Run.Press(PressSeconds);
	LogRingPulses(bCounted);
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
	// S-030 (R-ACT-10): los fotogramas del motor y los segundos reales desde ahora, para medir los fotogramas por segundo al cerrarse.
	Ring.OpenFrame = GFrameCounter;
	Ring.OpenRealSeconds = FPlatformTime::Seconds();
	Ring.Run.Begin(Soulbound::Rules::MakeRingSequence(State.Params.Ring, Params, RingSeed), FPlatformTime::Seconds() + FMath::Max(RingAnnounceSeconds, 0.f));
	HumanRing = MoveTemp(Ring);
	Note(ELogVerbosity::Log, FString::Printf(TEXT("Anillo abierto (%s): %d pulsos, cierre %.2f s, zona %.2f, semilla %d."),
		bDuel ? TEXT("duelo") : *UEnum::GetValueAsString(Trial), Params.Pulses, Params.CloseSeconds, Params.SweetZone, RingSeed));
	OnRingOpened.Broadcast();
}

void UMatchHostSubsystem::CloseRing(const TOptional<FRingOutcome>& Outcome, const TCHAR* Why)
{
	if (!HumanRing.IsSet())
	{
		return;
	}
	// S-030 (R-ACT-10): el acta añade los fotogramas por segundo durante la prueba; el texto del log sigue siendo el de siempre.
	const uint64 Frames = GFrameCounter - HumanRing->OpenFrame;
	const double RealSeconds = FPlatformTime::Seconds() - HumanRing->OpenRealSeconds;
	HumanRing.Reset();
	Note(ELogVerbosity::Log, FString::Printf(TEXT("Anillo cerrado: %s."), Why),
		Soulbound::Presentation::RingClosedRecordText(Why, static_cast<int64>(Frames), RealSeconds));
	OnRingClosed.Broadcast(Outcome);
}

void UMatchHostSubsystem::LogRingPulses(bool bLastPressed)
{
	if (!HumanRing.IsSet())
	{
		return;
	}
	const TArray<double>& Precisions = HumanRing->Run.Precisions();
	for (int32 Pulse = HumanRing->LoggedPulses; Pulse < Precisions.Num(); ++Pulse)
	{
		const bool bPressed = bLastPressed && Pulse == Precisions.Num() - 1;
		Note(ELogVerbosity::Log, FString::Printf(TEXT("Anillo: pulso %d de %d, precisión %.2f%s."), Pulse + 1, HumanRing->Run.NumPulses(), Precisions[Pulse],
			bPressed ? TEXT("") : TEXT(" (vencido sin pulsar)")));
	}
	HumanRing->LoggedPulses = Precisions.Num();
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
	LogRingPulses(false);
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
	Note(ELogVerbosity::Log, FString::Printf(TEXT("Depuración: %s%s"), bApplied ? TEXT("") : TEXT("RECHAZADA, "), *Message));
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

FDebugEditOutcome UMatchHostSubsystem::DebugStrikeEvent(const FString& EventId, bool bViaOracle)
{
	if (!bHasMatch)
	{
		return FinishDebugEdit(false, TEXT("sin partida"));
	}
	// S-022: como SbEvent, acaba el prólogo antes de golpear.
	EndPrologue(TEXT("SbStrike"));
	State.ClockSeconds = MatchSeconds();
	const bool bTurnInProgress = State.TurnPhase != ETurnPhase::BetweenTurns;
	if (bViaOracle)
	{
		return DebugStrikeViaOracle(EventId, bTurnInProgress);
	}
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

FDebugEditOutcome UMatchHostSubsystem::DebugStrikeViaOracle(const FString& EventId, bool bTurnInProgress)
{
	// S-041 (herramienta de depuración pedida en el plan, 16/11/2026): el golpe por el Oráculo (EOracleOrigin::Strike) como lo decide el cerebro, con
	// ApplyWardenDecision y sin tocar el núcleo: corta el turno en curso y abre la consulta del cortado con ese evento de sentencia, sin frase.
	const int32 EventIndex = State.WardenEvents.IndexOf(EventId);
	if (EventIndex == INDEX_NONE)
	{
		return FinishDebugEdit(false, FString::Printf(TEXT("no hay ningún evento \"%s\" (SbEvents lista los eventos)"), *EventId));
	}
	if (!State.WardenParams.HasWarden())
	{
		return FinishDebugEdit(false, TEXT("la partida no tiene Warden: sin él no hay Oráculo"));
	}
	// Una consulta o un evento en curso se pisarían: el cerebro tampoco golpea entonces (aplaza, R-WAR-7).
	if (State.Oracle.IsActive() || State.CurrentEvent.IsActive())
	{
		return FinishDebugEdit(false, TEXT("hay una consulta del Oráculo o un evento del Warden en curso: prueba cuando termine"));
	}
	if (bTurnInProgress && !Soulbound::Rules::CanInterruptTurn(State))
	{
		return FinishDebugEdit(false, TEXT("el turno está en un duelo, un trilero o un minijuego, que no se cortan: prueba cuando termine"));
	}
	FWardenDecision Decision;
	Decision.Kind = EWardenDecisionKind::Strike;
	Decision.EventIndex = EventIndex;
	Decision.bViaOracle = true;
	Soulbound::Rules::ApplyWardenDecision(State, Decision, State.ClockSeconds, MatchRng);
	const bool bApplied = State.Oracle.IsWaiting() && State.Oracle.Origin == EOracleOrigin::Strike;
	if (bApplied && StepClock.bHeld)
	{
		StepClock.Resume(GetWorld()->GetTimeSeconds());
	}
	const FDebugEditOutcome Outcome = FinishDebugEdit(bApplied, bApplied
		? FString::Printf(TEXT("golpe \"%s\" por el Oráculo %s: consulta del asiento %d"), *EventId, bTurnInProgress ? TEXT("corta el turno") : TEXT("entre turnos"), State.Oracle.Consultant)
		: FString(TEXT("el Oráculo no se abrió")));
	if (bApplied)
	{
		Advance();
	}
	return Outcome;
}
#endif
