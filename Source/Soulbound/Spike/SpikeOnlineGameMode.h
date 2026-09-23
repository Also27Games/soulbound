#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpikeOnlineGameMode.generated.h"

/** Spike, paso 0.5: modo de juego mínimo que usa el controlador con los comandos de consola de sesión. Desechable. */
UCLASS()
class SOULBOUND_API ASpikeOnlineGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpikeOnlineGameMode();
};
