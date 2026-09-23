// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "A27OnlineSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "A27 Online Settings"))
class ALSO27ONLINE_API UA27OnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	
	UA27OnlineSettings();
	
	UPROPERTY(Config, EditAnywhere, Category = "Session")
	FString SessionCodeValidCharacters;

		UPROPERTY(Config, EditAnywhere, Category = "Session")
	int32 SessionCodeLength;
	
};
