#include "SpikeOnlinePlayerController.h"

#include "../Match/MatchHostSubsystem.h"
#include "../Presentation/BoardTileActor.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
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
