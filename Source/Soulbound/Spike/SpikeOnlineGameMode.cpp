#include "SpikeOnlineGameMode.h"

#include "SpikeOnlinePlayerController.h"

ASpikeOnlineGameMode::ASpikeOnlineGameMode()
{
	PlayerControllerClass = ASpikeOnlinePlayerController::StaticClass();
}
