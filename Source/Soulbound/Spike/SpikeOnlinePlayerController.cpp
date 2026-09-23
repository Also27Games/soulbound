#include "Spike/SpikeOnlinePlayerController.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystem/A27OnlineSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundSpike, Log, All);

void ASpikeOnlinePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UA27OnlineSubsystem* Online = GetOnline())
	{
		Online->OnLoginStatusChanged.AddDynamic(this, &ASpikeOnlinePlayerController::HandleLogin);
		Online->OnSessionCreated.AddDynamic(this, &ASpikeOnlinePlayerController::HandleCreated);
		Online->OnJoinSessionComplete.AddDynamic(this, &ASpikeOnlinePlayerController::HandleJoined);
		Online->OnLeaveSessionComplete.AddDynamic(this, &ASpikeOnlinePlayerController::HandleLeft);
	}

	Say(TEXT("Spike online. Comandos: SbStatus, SbLogin, SbCreate, SbJoin <codigo>, SbLeave"));
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
