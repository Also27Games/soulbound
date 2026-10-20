#include "SpikeOnlinePlayerController.h"

#include "../Match/MatchHostSubsystem.h"
#include "../Presentation/BoardTileActor.h"
#include "../Presentation/BoardViewActor.h"
#include "../Warden/WardenHostSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Subsystem/A27OnlineSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundSpike, Log, All);

void ASpikeOnlinePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	ShowMouseCursorAndInputGame();

	if (UA27OnlineSubsystem* Online = GetOnline())
	{
		Online->OnLoginStatusChanged.AddDynamic(this, &ASpikeOnlinePlayerController::HandleLogin);
		Online->OnSessionCreated.AddDynamic(this, &ASpikeOnlinePlayerController::HandleCreated);
		Online->OnJoinSessionComplete.AddDynamic(this, &ASpikeOnlinePlayerController::HandleJoined);
		Online->OnLeaveSessionComplete.AddDynamic(this, &ASpikeOnlinePlayerController::HandleLeft);
	}

	Say(TEXT("Spike online. Comandos: SbStatus, SbLogin, SbCreate, SbJoin <codigo>, SbLeave"));
}

void ASpikeOnlinePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		// Enlace directo a la tecla: no crea ningún asset de entrada (S-005 no toca assets).
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASpikeOnlinePlayerController::OnPrimaryClick);
		// Paso a paso (S-007): también sin assets de entrada.
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASpikeOnlinePlayerController::OnStepKey);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpikeOnlinePlayerController::OnStepKey);
		// Elecciones (S-009): teclas numéricas y R, también sin assets de entrada. Cada número lleva su valor al manejador.
		const FKey NumberKeys[] = {EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
		for (int32 Number = 0; Number < UE_ARRAY_COUNT(NumberKeys); ++Number)
		{
			FInputKeyBinding Binding(FInputChord(NumberKeys[Number]), IE_Pressed);
			Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ASpikeOnlinePlayerController::OnChoiceKey, Number);
			InputComponent->KeyBindings.Emplace(MoveTemp(Binding));
		}
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASpikeOnlinePlayerController::OnRerollKey);
	}
}

void ASpikeOnlinePlayerController::ShowMouseCursorAndInputGame()
{
	// Vista de depuración (S-005): cursor visible para hacer clic en las casillas. Juego y UI a la vez, sin capturar
	// ni ocultar el ratón, así el teclado y el mando siguen llegando a UGT (la pausa del Blueprint hijo incluida).
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASpikeOnlinePlayerController::OnPrimaryClick()
{
	UWorld* World = GetWorld();
	UMatchHostSubsystem* Match = World ? World->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (!Match || !Match->HasMatch())
	{
		return;
	}
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		if (const ABoardTileActor* Tile = Cast<ABoardTileActor>(Hit.GetActor()))
		{
			// Con un salto planar pendiente, el clic en una casilla es la respuesta a la elección (S-009).
			if (Match->GetState().PendingChoice.Kind == EChoiceKind::PlanarJump)
			{
				Match->SubmitHumanChoice(EChoiceKind::PlanarJump, Tile->GetTileId());
			}
			else
			{
				Match->SubmitHumanIntent(Tile->GetTileId());
			}
		}
	}
}

void ASpikeOnlinePlayerController::OnChoiceKey(int32 Number)
{
	UWorld* World = GetWorld();
	UMatchHostSubsystem* Match = World ? World->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (IsPaused() || !Match || !Match->HasMatch())
	{
		return;
	}
	// El número es la posición de la opción en la línea de petición de la vista; 0 es "no" en las elecciones que lo admiten
	// y la décima opción en las que no (la esfera del trilero, S-010). Una posición que no existe se ignora: el humano no ha
	// elegido ninguna opción. El tipo de elección es el pendiente; si cambia antes de llegar, el motor rechaza la respuesta
	// por fase, y si no es suya, por asiento.
	const FPendingChoice& Choice = Match->GetState().PendingChoice;
	const int32 Position = Number == 0 ? (Choice.CanDecline() ? INDEX_NONE : 9) : Number - 1;
	if (Position == INDEX_NONE)
	{
		Match->SubmitHumanChoice(Choice.Kind, INDEX_NONE);
	}
	else if (Position < Choice.AnswerCount())
	{
		Match->SubmitHumanChoice(Choice.Kind, Choice.AnswerAt(Position));
	}
}

void ASpikeOnlinePlayerController::OnRerollKey()
{
	UWorld* World = GetWorld();
	UMatchHostSubsystem* Match = World ? World->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (!IsPaused() && Match && Match->HasMatch())
	{
		Match->SubmitHumanChoice(EChoiceKind::Reroll, 1);
	}
}

void ASpikeOnlinePlayerController::OnStepKey()
{
	// Con el juego en pausa (el menú de UGT abierto) la tecla no suelta un turno. Los comandos de consola no se tocan.
	if (IsPaused())
	{
		return;
	}
	SbStep();
}

void ASpikeOnlinePlayerController::SbStepMode(int32 Enabled)
{
	UWorld* World = GetWorld();
	UMatchHostSubsystem* Match = World ? World->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (!Match || !Match->HasMatch())
	{
		Say(TEXT("Sin partida: no hay modo paso a paso."));
		return;
	}
	Match->SetStepMode(Enabled != 0);
	Say(Enabled != 0 ? TEXT("Paso a paso ACTIVADO: espacio o SbStep para el siguiente turno.") : TEXT("Paso a paso desactivado."));
}

void ASpikeOnlinePlayerController::SbStep()
{
	UWorld* World = GetWorld();
	UMatchHostSubsystem* Match = World ? World->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (Match && Match->HasMatch())
	{
		Match->StepOnce();
	}
}

UA27OnlineSubsystem* ASpikeOnlinePlayerController::GetOnline() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UA27OnlineSubsystem>() : nullptr;
}

void ASpikeOnlinePlayerController::Say(const FString& Text) const
{
	UE_LOG(LogSoulboundSpike, Log, TEXT("%s"), *Text);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Cyan, Text);
	}
}

void ASpikeOnlinePlayerController::SbStatus()
{
	UA27OnlineSubsystem* Online = GetOnline();
	if (!Online)
	{
		Say(TEXT("Sin subsistema A27Online (¿plugin activado?)"));
		return;
	}

	const UEnum* StatusEnum = StaticEnum<EA27LoginStatus>();
	const FString Status = StatusEnum ? StatusEnum->GetNameStringByValue(static_cast<int64>(Online->GetLoginStatus())) : TEXT("?");
	Say(FString::Printf(TEXT("Login: %s | Apodo: %s | Codigo: %s | Ultimo error: %s | Mapa: %s | Rol: %s"),
		*Status,
		*Online->GetPlayerNickName(),
		*Online->GetOnlineSessioCode(),
		*Online->GetLastErrorMessage(),
		*GetWorld()->GetMapName(),
		GetWorld()->GetNetMode() == NM_ListenServer ? TEXT("servidor") : (GetWorld()->GetNetMode() == NM_Client ? TEXT("cliente") : TEXT("solo"))));
}

void ASpikeOnlinePlayerController::SbLogin()
{
	if (UA27OnlineSubsystem* Online = GetOnline())
	{
		Say(TEXT("Login..."));
		Online->Login();
	}
}

void ASpikeOnlinePlayerController::SbCreate()
{
	UA27OnlineSubsystem* Online = GetOnline();
	if (!Online)
	{
		return;
	}

	// Nombre del mapa actual sin el prefijo de PIE, para que ServerTravel vuelva a cargarlo como servidor de escucha.
	const FString MapName = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	Say(FString::Printf(TEXT("Creando sesion de 4 en %s..."), *MapName));
	Online->CreateGameSession(4, MapName);
}

void ASpikeOnlinePlayerController::SbJoin(FString Code)
{
	UA27OnlineSubsystem* Online = GetOnline();
	if (!Online)
	{
		return;
	}

	Say(FString::Printf(TEXT("Buscando sesion %s..."), *Code.ToUpper()));
	Online->JoinGameSession(Code);
}

void ASpikeOnlinePlayerController::SbLeave()
{
	if (UA27OnlineSubsystem* Online = GetOnline())
	{
		Say(TEXT("Saliendo de la sesion..."));
		Online->LeaveSession();
	}
}

void ASpikeOnlinePlayerController::HandleLogin(bool bWasSuccessful, const FString& ErrorMessage)
{
	Say(bWasSuccessful ? TEXT("Login OK") : FString::Printf(TEXT("Login FALLIDO: %s"), *ErrorMessage));
}

void ASpikeOnlinePlayerController::HandleCreated(bool bWasSuccessful)
{
	const UA27OnlineSubsystem* Online = GetOnline();
	Say(bWasSuccessful && Online
		? FString::Printf(TEXT("Sesion creada. CODIGO: %s"), *Online->GetOnlineSessioCode())
		: TEXT("Crear sesion FALLIDO"));
}

void ASpikeOnlinePlayerController::HandleJoined(bool bWasSuccessful)
{
	Say(bWasSuccessful ? TEXT("Unido a la sesion; viajando al anfitrion...") : TEXT("Unirse FALLIDO (codigo incorrecto o sesion no encontrada)"));
}

void ASpikeOnlinePlayerController::HandleLeft(bool bWasSuccessful)
{
	Say(bWasSuccessful ? TEXT("Sesion abandonada") : TEXT("Salir FALLIDO"));
}

// Herramientas de depuración (S-011). En la build de entrega, cada comando es un cuerpo vacío.

#if !UE_BUILD_SHIPPING
UMatchHostSubsystem* ASpikeOnlinePlayerController::GetDebugHost() const
{
	if (!HasAuthority())
	{
		Say(TEXT("[Depuración] Solo con autoridad: este proceso no es el anfitrión de la partida."));
		return nullptr;
	}
	UWorld* World = GetWorld();
	UMatchHostSubsystem* Match = World ? World->GetSubsystem<UMatchHostSubsystem>() : nullptr;
	if (!Match || !Match->HasMatch())
	{
		Say(TEXT("[Depuración] Sin partida iniciada."));
		return nullptr;
	}
	return Match;
}

bool ASpikeOnlinePlayerController::ParseDebugInt(const FString& Text, const TCHAR* Name, const TCHAR* Usage, int32& OutValue) const
{
	if (!LexTryParseString(OutValue, *Text))
	{
		Say(FString::Printf(TEXT("[Depuración] %s debe ser un número entero, no \"%s\". Uso: %s"), Name, *Text, Usage));
		return false;
	}
	return true;
}

void ASpikeOnlinePlayerController::ReportDebugEdit(const FDebugEditOutcome& Outcome) const
{
	Say(FString::Printf(TEXT("[Depuración] %s%s"), Outcome.bApplied ? TEXT("") : TEXT("RECHAZADO: "), *Outcome.Message));
}

ABoardViewActor* ASpikeOnlinePlayerController::FindBoardView() const
{
	// La vista del tablero vive en el mundo del anfitrión; es presentación, así que no hace falta editar la partida.
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ABoardViewActor> It(World);
		return It ? *It : nullptr;
	}
	return nullptr;
}
#endif

void ASpikeOnlinePlayerController::SbGiveRune(FString Seat, FString RuneType, FString Count)
{
#if !UE_BUILD_SHIPPING
	static const TCHAR* Usage = TEXT("SbGiveRune <asiento> <tipo> [n]");
	int32 SeatId = 0;
	int32 Type = 0;
	int32 Amount = 1;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), Usage, SeatId) && ParseDebugInt(RuneType, TEXT("tipo"), Usage, Type) && (Count.IsEmpty() || ParseDebugInt(Count, TEXT("n"), Usage, Amount)))
		{
			ReportDebugEdit(Host->DebugGiveRune(SeatId, Type, Amount));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbTakeRune(FString Seat, FString RuneType, FString Count)
{
#if !UE_BUILD_SHIPPING
	static const TCHAR* Usage = TEXT("SbTakeRune <asiento> <tipo> [n]");
	int32 SeatId = 0;
	int32 Type = 0;
	int32 Amount = 1;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), Usage, SeatId) && ParseDebugInt(RuneType, TEXT("tipo"), Usage, Type) && (Count.IsEmpty() || ParseDebugInt(Count, TEXT("n"), Usage, Amount)))
		{
			ReportDebugEdit(Host->DebugTakeRune(SeatId, Type, Amount));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbMove(FString Seat, FString Tile)
{
#if !UE_BUILD_SHIPPING
	static const TCHAR* Usage = TEXT("SbMove <asiento> <casilla>");
	int32 SeatId = 0;
	int32 TileId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), Usage, SeatId) && ParseDebugInt(Tile, TEXT("casilla"), Usage, TileId))
		{
			ReportDebugEdit(Host->DebugMove(SeatId, TileId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbBond(FString Seat, FString Domain)
{
#if !UE_BUILD_SHIPPING
	static const TCHAR* Usage = TEXT("SbBond <asiento> <dominio>");
	int32 SeatId = 0;
	int32 DomainId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), Usage, SeatId) && ParseDebugInt(Domain, TEXT("dominio"), Usage, DomainId))
		{
			ReportDebugEdit(Host->DebugBond(SeatId, DomainId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbSeal(FString Tile)
{
#if !UE_BUILD_SHIPPING
	int32 TileId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Tile, TEXT("casilla"), TEXT("SbSeal <casilla>"), TileId))
		{
			ReportDebugEdit(Host->DebugSeal(TileId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbUnseal(FString Tile)
{
#if !UE_BUILD_SHIPPING
	int32 TileId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Tile, TEXT("casilla"), TEXT("SbUnseal <casilla>"), TileId))
		{
			ReportDebugEdit(Host->DebugUnseal(TileId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbEndRace()
{
#if !UE_BUILD_SHIPPING
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		ReportDebugEdit(Host->DebugEndRace());
	}
#endif
}

void ASpikeOnlinePlayerController::SbRoll(FString Value)
{
#if !UE_BUILD_SHIPPING
	int32 Roll = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Value, TEXT("n"), TEXT("SbRoll <n>"), Roll))
		{
			ReportDebugEdit(Host->DebugFixRoll(Roll));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbTrial(FString Result)
{
#if !UE_BUILD_SHIPPING
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (Result.Equals(TEXT("pass"), ESearchCase::IgnoreCase))
		{
			ReportDebugEdit(Host->DebugFixTrial(true));
		}
		else if (Result.Equals(TEXT("fail"), ESearchCase::IgnoreCase))
		{
			ReportDebugEdit(Host->DebugFixTrial(false));
		}
		else
		{
			Say(FString::Printf(TEXT("[Depuración] El resultado debe ser pass o fail, no \"%s\". Uso: SbTrial pass|fail"), *Result));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbDuel(FString Result)
{
#if !UE_BUILD_SHIPPING
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (Result.Equals(TEXT("win"), ESearchCase::IgnoreCase))
		{
			ReportDebugEdit(Host->DebugFixDuel(true));
		}
		else if (Result.Equals(TEXT("lose"), ESearchCase::IgnoreCase))
		{
			ReportDebugEdit(Host->DebugFixDuel(false));
		}
		else
		{
			Say(FString::Printf(TEXT("[Depuración] El resultado debe ser win o lose, no \"%s\". Uso: SbDuel win|lose"), *Result));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbTurn(FString Seat)
{
#if !UE_BUILD_SHIPPING
	int32 SeatId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), TEXT("SbTurn <asiento>"), SeatId))
		{
			ReportDebugEdit(Host->DebugFixNextSeat(SeatId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbBanish(FString Seat)
{
#if !UE_BUILD_SHIPPING
	int32 SeatId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), TEXT("SbBanish <asiento>"), SeatId))
		{
			ReportDebugEdit(Host->DebugBanish(SeatId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbInsolence(FString Seat, FString Level)
{
#if !UE_BUILD_SHIPPING
	int32 SeatId = 0;
	int32 LevelValue = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), TEXT("SbInsolence <asiento> <n>"), SeatId) && ParseDebugInt(Level, TEXT("insolencia"), TEXT("SbInsolence <asiento> <n>"), LevelValue))
		{
			ReportDebugEdit(Host->DebugSetInsolence(SeatId, LevelValue));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbRelease()
{
#if !UE_BUILD_SHIPPING
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		ReportDebugEdit(Host->DebugRelease());
	}
#endif
}

void ASpikeOnlinePlayerController::SbSendHome(FString Seat)
{
#if !UE_BUILD_SHIPPING
	int32 SeatId = 0;
	if (UMatchHostSubsystem* Host = GetDebugHost())
	{
		if (ParseDebugInt(Seat, TEXT("asiento"), TEXT("SbSendHome <asiento>"), SeatId))
		{
			ReportDebugEdit(Host->DebugSendHome(SeatId));
		}
	}
#endif
}

void ASpikeOnlinePlayerController::SbEvent(FString Id)
{
#if !UE_BUILD_SHIPPING
	if (!GetDebugHost())
	{
		return;
	}
	if (Id.IsEmpty())
	{
		Say(TEXT("[Depuración] Uso: SbEvent <id> (SbEvents lista los eventos)"));
		return;
	}
	// El evento lo lanza el anfitrión del Warden, con su sorteo; el anfitrión de partida lo aplica (S-014).
	UWardenHostSubsystem* Warden = GetWorld() ? GetWorld()->GetSubsystem<UWardenHostSubsystem>() : nullptr;
	if (!Warden)
	{
		Say(TEXT("[Depuración] No hay anfitrión del Warden en este mundo."));
		return;
	}
	ReportDebugEdit(Warden->DebugLaunchEvent(Id));
#endif
}

void ASpikeOnlinePlayerController::SbStrike(FString Id)
{
#if !UE_BUILD_SHIPPING
	UMatchHostSubsystem* Host = GetDebugHost();
	if (!Host)
	{
		return;
	}
	if (Id.IsEmpty())
	{
		Say(TEXT("[Depuración] Uso: SbStrike <id> (SbEvents lista los eventos)"));
		return;
	}
	// S-020: el corte y el evento los hace el motor (Soulbound::Debug::StrikeEvent), con los dos sorteos que guarda el anfitrión.
	ReportDebugEdit(Host->DebugStrikeEvent(Id));
#endif
}

void ASpikeOnlinePlayerController::SbEvents()
{
#if !UE_BUILD_SHIPPING
	if (const UMatchHostSubsystem* Host = GetDebugHost())
	{
		const TArray<FWardenEventDef>& Events = Host->GetState().WardenEvents.Events;
		const FString List = FString::JoinBy(Events, TEXT(" | "), [](const FWardenEventDef& Event)
		{
			return FString::Printf(TEXT("%s (%s)"), *Event.Id, Event.IsDilemma() ? TEXT("dilema") : TEXT("decreto"));
		});
		Say(FString::Printf(TEXT("[Depuración] %d eventos: %s"), Events.Num(), *List));
	}
#endif
}

void ASpikeOnlinePlayerController::SbWardenMode(FString Mode)
{
#if !UE_BUILD_SHIPPING
	if (!GetDebugHost())
	{
		return;
	}
	UWardenHostSubsystem* Warden = GetWorld() ? GetWorld()->GetSubsystem<UWardenHostSubsystem>() : nullptr;
	if (!Warden)
	{
		Say(TEXT("[Depuración] No hay anfitrión del Warden en este mundo."));
		return;
	}
	if (Mode != TEXT("off") && Mode != TEXT("auto"))
	{
		Say(TEXT("[Depuración] Uso: SbWardenMode off|auto"));
		return;
	}
	Warden->SetMode(Mode == TEXT("off") ? EWardenHostMode::Off : EWardenHostMode::Auto);
	Say(FString::Printf(TEXT("[Depuración] Warden %s"), Mode == TEXT("off") ? TEXT("apagado: no evalúa (SbEvent sigue lanzando eventos)") : TEXT("en marcha: evalúa solo")));
#endif
}

void ASpikeOnlinePlayerController::SbSeatedView(FString Height, FString Distance, FString Pitch, FString Yaw)
{
#if !UE_BUILD_SHIPPING
	static const TCHAR* Usage = TEXT("SbSeatedView <altura_cm> <distancia_cm> <inclinación_grados> [<ángulo_grados>: 180 abajo, menos a la derecha, más a la izquierda], "
		"o sin argumentos para ver los valores");
	ABoardViewActor* View = FindBoardView();
	if (!View)
	{
		Say(TEXT("[Depuración] No hay vista del tablero en este mundo."));
		return;
	}
	if (Height.IsEmpty() && Distance.IsEmpty() && Pitch.IsEmpty() && Yaw.IsEmpty())
	{
		Say(FString::Printf(TEXT("[Depuración] Vista sentada: %s"), *View->DescribeSeatedView()));
		return;
	}
	float HeightCm = 0.f;
	float DistanceCm = 0.f;
	float PitchDegrees = 0.f;
	if (!LexTryParseString(HeightCm, *Height) || !LexTryParseString(DistanceCm, *Distance) || !LexTryParseString(PitchDegrees, *Pitch))
	{
		Say(FString::Printf(TEXT("[Depuración] Hacen falta los tres números. Uso: %s"), Usage));
		return;
	}
	// El ángulo del asiento es opcional: sin él, la cámara sigue en el lado de la mesa en que estaba.
	TOptional<float> YawDegrees;
	if (!Yaw.IsEmpty())
	{
		float Parsed = 0.f;
		if (!LexTryParseString(Parsed, *Yaw))
		{
			Say(FString::Printf(TEXT("[Depuración] El ángulo debe ser un número, no \"%s\". Uso: %s"), *Yaw, Usage));
			return;
		}
		YawDegrees = Parsed;
	}
	// Una inclinación de 90° o más mira del revés; una distancia negativa metería la cámara dentro de la mesa.
	if (DistanceCm < 0.f || PitchDegrees <= -90.f || PitchDegrees >= 90.f)
	{
		Say(FString::Printf(TEXT("[Depuración] RECHAZADO: la distancia no puede ser negativa y la inclinación va entre -90 y 90 grados. Uso: %s"), Usage));
		return;
	}
	View->SetSeatedView(HeightCm, DistanceCm, PitchDegrees, YawDegrees);
	Say(FString::Printf(TEXT("[Depuración] Vista sentada: %s"), *View->DescribeSeatedView()));
#endif
}

void ASpikeOnlinePlayerController::SbHelp()
{
#if !UE_BUILD_SHIPPING
	Say(TEXT("[Depuración] Comandos: SbGiveRune <asiento> <tipo> [n] | SbTakeRune <asiento> <tipo> [n] | SbMove <asiento> <casilla> | "
		"SbBond <asiento> <dominio> | SbSeal <casilla> | SbUnseal <casilla> | SbEndRace | SbRoll <n> | SbTrial pass|fail | "
		"SbDuel win|lose | SbTurn <asiento> | SbBanish <asiento> | SbRelease | SbSendHome <asiento> | SbInsolence <asiento> <n> | SbEvent <id> | SbStrike <id> | SbEvents | SbWardenMode off|auto | "
		"SbSeatedView [<altura_cm> <distancia_cm> <inclinación_grados> [<ángulo_grados>]] | SbHelp. "
		"Los asientos van de 0 a 3 y el tipo de runa es el Dominio de 0 a 5. SbBanish y SbSendHome no valen con quien está jugando su turno, "
		"y SbEvent solo entre turnos: con SbStepMode 1. SbStrike corta el turno en curso, también a medio camino."));
	// S-019: las teclas de la vista, las que tenga configuradas; las de ocultar son independientes y se combinan.
	if (const ABoardViewActor* View = FindBoardView())
	{
		Say(FString::Printf(TEXT("[Depuración] Teclas de la vista: %s."), *View->DescribeViewKeys()));
	}
#endif
}
