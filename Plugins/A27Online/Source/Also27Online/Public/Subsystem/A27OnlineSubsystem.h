// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineSessionSettings.h"
#include "A27OnlineSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginCompleteBP, bool, bWasSuccessful, const FString&, ErrorMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionCreatedBP, bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinSessionCompleteBP, bool, bWasSuccessful);

UENUM(BlueprintType)
enum class EA27LoginStatus : uint8
{
	NotStarted,
	LoggingIn,
	LoggedIn,
	Failed
};

/**
 * 
 */
UCLASS(Blueprintable)
class ALSO27ONLINE_API UA27OnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "A27|Events")
	FOnLoginCompleteBP OnLoginStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "A27|Events")
	FOnSessionCreatedBP OnSessionCreated;

	UPROPERTY(BlueprintAssignable, Category = "A27|Events")
	FOnJoinSessionCompleteBP OnJoinSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "Also27|Events")
	FOnSessionCreatedBP OnLeaveSessionComplete;

private:
	FString PlayerUniqueId;

	FString PlayerNickName = TEXT("");;

	FString OnlineSessioCode;

	EA27LoginStatus LoginStatus = EA27LoginStatus::NotStarted;

	FString LastErrorMessage;

	int32 PendingMaxPlayers;

	FString PendingMapName;

	FName CurrentSessionName;

	FName MainSessionName = FName(TEXT("A27Session"));

	TSharedPtr<class FOnlineSessionSearch> SessionSearch;

	FString PendingJoinCode;

	FName SettingSessionCode = FName(TEXT("SETTING_SESSIONCODE"));

	enum class ESessionNextAction : uint8 { None, Create, Join };

	ESessionNextAction PendingAction = ESessionNextAction::None;

	int32 TotalPlayersInGame = 0;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "A27|Login")
	void Login();

	UFUNCTION(BlueprintCallable, Category = "A27|Session")
	void CreateGameSession(int32 MaxPlayers, FString MapName);

	UFUNCTION(BlueprintCallable, Category = "A27|Session")
	void JoinGameSession(FString SessionCode);

	UFUNCTION(BlueprintCallable, Category = "Also27|Session")
	void LeaveSession();

	UFUNCTION(BlueprintCallable, Category = "EOS|Utils")
	FString GenerateSessionCode();

	UFUNCTION(BlueprintPure)
	FString GetPlayerUniqueId() const { return PlayerUniqueId; }

	UFUNCTION(BlueprintPure)
	FString GetPlayerNickName() const { return PlayerNickName; }

	UFUNCTION(BlueprintPure)
	FString GetOnlineSessioCode() const { return OnlineSessioCode; }

	UFUNCTION(BlueprintPure)
	EA27LoginStatus GetLoginStatus() const { return LoginStatus; }

	UFUNCTION(BlueprintPure)
	FString GetLastErrorMessage() const { return LastErrorMessage; }

	UFUNCTION(BlueprintPure)
	int32 GetTotalPlayersInGame() const { return TotalPlayersInGame; }

	UFUNCTION(BlueprintCallable)
	void SetTotalPlayersInGame(int32 NumPlayers) { TotalPlayersInGame = NumPlayers; }

private:
	void InternalCreateSession();

	void InternalFindSessions();

	void OnLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	void OnCreateSessionComplete(FName NewSessionName, bool bWasSuccessful);

	void SessionCleanup(const IOnlineSessionPtr& SessionInterface);

	void HandleDestroySessionComplete(FName ActualSessionName, bool bWasSuccessful);

	void OnFindSessionsComplete(bool bWasSuccessful);

	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	void HandleLeaveSessionComplete(FName SessionName, bool bWasSuccessful);
};
