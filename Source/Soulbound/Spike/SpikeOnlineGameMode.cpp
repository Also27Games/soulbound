#include "Spike/SpikeOnlineGameMode.h"

#include "Spike/SpikeOnlinePlayerController.h"

ASpikeOnlineGameMode::ASpikeOnlineGameMode()
{
	PlayerControllerClass = ASpikeOnlinePlayerController::StaticClass();
}
