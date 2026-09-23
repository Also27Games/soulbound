// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/A27OnlineSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Settings/A27OnlineSettings.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

void UA27OnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Login();
}

void UA27OnlineSubsystem::Login()
{
	if (LoginStatus == EA27LoginStatus::LoggingIn || LoginStatus == EA27LoginStatus::LoggedIn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: World NOT FOUND"));
		return;
	}

	IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(World, FName("RedpointEOS"));

	if (!EOSSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: EOS Subsystem NOT FOUND"));
		return;
	}

	IOnlineIdentityPtr OnlineIdentity = EOSSubsystem->GetIdentityInterface();

	if (!OnlineIdentity)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: Online Identity NOT FOUND"));
		return;
	}

	LoginStatus = EA27LoginStatus::LoggingIn;

	OnlineIdentity->OnLoginCompleteDelegates[0].AddUObject(this, &UA27OnlineSubsystem::OnLoginCompleted);

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("persistentauth");

	if (!OnlineIdentity->Login(0, Credentials))
	{
		OnLoginStatusChanged.Broadcast(false, TEXT("Fallo al iniciar el proceso de Login"));
	}
}

void UA27OnlineSubsystem::CreateGameSession(int32 MaxPlayers, FString MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: World NOT FOUND"));
		return;
	}

	IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(World, FName("RedpointEOS"));

	if (!EOSSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: EOS Subsystem NOT FOUND"));
		return;
	}

	IOnlineSessionPtr SessionInterface = EOSSubsystem->GetSessionInterface();

	if (!SessionInterface)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: Session Interface NOT FOUND"));
		return;
	}

	PendingMaxPlayers = MaxPlayers;
	PendingMapName = MapName;
	CurrentSessionName = MainSessionName;
	PendingAction = ESessionNextAction::Create;

	if (FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(CurrentSessionName))
	{
		SessionCleanup(SessionInterface);
		return;
	}

	InternalCreateSession();
}

void UA27OnlineSubsystem::JoinGameSession(FString SessionCode)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: World NOT FOUND"));
		return;
	}

	IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(World, FName("RedpointEOS"));

	if (!EOSSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: EOS Subsystem NOT FOUND"));
		return;
	}

	IOnlineSessionPtr SessionInterface = EOSSubsystem->GetSessionInterface();

	if (!SessionInterface)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: Session Interface NOT FOUND"));
		return;
	}

	PendingJoinCode = SessionCode.ToUpper();
	CurrentSessionName = MainSessionName;
	PendingAction = ESessionNextAction::Join;

	if (FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(CurrentSessionName))
	{
		SessionCleanup(SessionInterface);
		return;
	}

	InternalFindSessions();
}

void UA27OnlineSubsystem::LeaveSession()
{
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: World NOT FOUND"));
		return;
	}

	IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(World, FName("RedpointEOS"));

	if (!EOSSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: EOS Subsystem NOT FOUND"));
		return;
	}

	IOnlineSessionPtr SessionInterface = EOSSubsystem->GetSessionInterface();

	if (!SessionInterface)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: Session Interface NOT FOUND"));
		return;
	}

	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UA27OnlineSubsystem::HandleLeaveSessionComplete);
	
	if (!SessionInterface->DestroySession(CurrentSessionName))
	{	
		SessionInterface->ClearOnDestroySessionCompleteDelegates(this);
		OnLeaveSessionComplete.Broadcast(false);
	}
	
}

void UA27OnlineSubsystem::InternalCreateSession()
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), FName("RedpointEOS"));
	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();

	FString SessionCode = GenerateSessionCode();

	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.NumPublicConnections = PendingMaxPlayers;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bAllowJoinViaPresence = false;
	SessionSettings.bUsesPresence = false;
	SessionSettings.bUseLobbiesIfAvailable = false;

	SessionSettings.Set(SettingSessionCode, SessionCode, EOnlineDataAdvertisementType::ViaOnlineService);

	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UA27OnlineSubsystem::OnCreateSessionComplete);

	if (!SessionInterface->CreateSession(0, CurrentSessionName, SessionSettings))
	{
		OnSessionCreated.Broadcast(false);
	}
}

void UA27OnlineSubsystem::InternalFindSessions()
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), FName("RedpointEOS"));
	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->bIsLanQuery = false;
	
	SessionSearch->QuerySettings.Set(SettingSessionCode, PendingJoinCode, EOnlineComparisonOp::Equals);

	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UA27OnlineSubsystem::OnFindSessionsComplete);

	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UA27OnlineSubsystem::OnLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
                                           const FString& Error)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: World NOT FOUND"));
		return;
	}

	IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(World, FName("RedpointEOS"));

	if (!EOSSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: EOS Subsystem NOT FOUND"));
		return;
	}

	IOnlineIdentityPtr OnlineIdentity = EOSSubsystem->GetIdentityInterface();
	OnlineIdentity->ClearOnLoginCompleteDelegates(0, this);

	if (bWasSuccessful)
	{
		LoginStatus = EA27LoginStatus::LoggedIn;

		PlayerNickName = OnlineIdentity->GetPlayerNickname(0);
		PlayerUniqueId = UserId.ToString();

		OnLoginStatusChanged.Broadcast(true, TEXT(""));
	}
	else
	{
		LoginStatus = EA27LoginStatus::Failed;
		OnLoginStatusChanged.Broadcast(false, Error);
	}
}

void UA27OnlineSubsystem::OnCreateSessionComplete(FName NewSessionName, bool bWasSuccessful)
{
	
	UE_LOG(LogTemp, Log, TEXT("Sesion creada. Nombre: %s, Resultado: %d"), *NewSessionName.ToString(), bWasSuccessful);
	
	if (IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(GetWorld(), FName("RedpointEOS")))
	{
		EOSSubsystem->GetSessionInterface()->ClearOnCreateSessionCompleteDelegates(this);
	}

	CurrentSessionName = NewSessionName;

	if (bWasSuccessful)
	{
		FString TravelURL = PendingMapName + TEXT("?listen");
		GetWorld()->ServerTravel(TravelURL);
	}

	OnSessionCreated.Broadcast(bWasSuccessful);
}

void UA27OnlineSubsystem::SessionCleanup(const IOnlineSessionPtr& SessionInterface)
{
	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
		this, &UA27OnlineSubsystem::HandleDestroySessionComplete);
	SessionInterface->DestroySession(CurrentSessionName);
}

void UA27OnlineSubsystem::HandleDestroySessionComplete(FName ActualSessionName, bool bWasSuccessful)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: World NOT FOUND"));
		return;
	}

	IOnlineSubsystem* EOSSubsystem = Online::GetSubsystem(World, FName("RedpointEOS"));

	if (!EOSSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("A27OnlineSubsystem:: EOS Subsystem NOT FOUND"));
		return;
	}

	EOSSubsystem->GetSessionInterface()->ClearOnDestroySessionCompleteDelegates(this);

	if (PendingAction == ESessionNextAction::Create)
	{
		InternalCreateSession();
	}
	else if (PendingAction == ESessionNextAction::Join)
	{
		InternalFindSessions();
	}

	PendingAction = ESessionNextAction::None;
}


void UA27OnlineSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
		
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), FName("RedpointEOS"));
	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
	SessionInterface->ClearOnFindSessionsCompleteDelegates(this);

	if (bWasSuccessful && SessionSearch->SearchResults.Num() > 0)
	{
		SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
			this, &UA27OnlineSubsystem::HandleJoinSessionComplete);

		if (!SessionInterface->JoinSession(0, CurrentSessionName, SessionSearch->SearchResults[0]))
		{
			OnJoinSessionComplete.Broadcast(false);
		}
	}
	else
	{
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UA27OnlineSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), FName("RedpointEOS"));
	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
	SessionInterface->ClearOnJoinSessionCompleteDelegates(this);

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		
		OnlineSessioCode = PendingJoinCode;
		
		FString ConnectString;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
		{
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC)
			{
				PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
				OnJoinSessionComplete.Broadcast(true);
				return;
			}
		}
	}

	OnJoinSessionComplete.Broadcast(false);
}

void UA27OnlineSubsystem::HandleLeaveSessionComplete(FName SessionName, bool bWasSuccessful)
{
		
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), FName("RedpointEOS")))
	{
		Subsystem->GetSessionInterface()->ClearOnDestroySessionCompleteDelegates(this);
	}

	if (bWasSuccessful)
	{		
		OnlineSessioCode = TEXT("");
		CurrentSessionName = FName();
	}

	OnLeaveSessionComplete.Broadcast(bWasSuccessful);
	
}


FString UA27OnlineSubsystem::GenerateSessionCode()
{
	const UA27OnlineSettings* Settings = GetDefault<UA27OnlineSettings>();

	if (!Settings)
	{
		return FString();
	}

	const FString& ValidCharacters = Settings->SessionCodeValidCharacters;
	const int32 CodeLength = Settings->SessionCodeLength;

	FString GeneratedCode = "";


	for (int32 i = 0; i < CodeLength; ++i)
	{
		int32 RandomIndex = FMath::RandRange(0, ValidCharacters.Len() - 1);
		GeneratedCode.AppendChar(ValidCharacters[RandomIndex]);
	}

	OnlineSessioCode = GeneratedCode;

	return GeneratedCode;
}
