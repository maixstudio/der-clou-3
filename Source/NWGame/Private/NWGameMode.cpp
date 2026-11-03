#include "NWGameMode.h"

#include "NWCameraPawn.h"
#include "NWPlayerController.h"

ANWGameMode::ANWGameMode()
{
	DefaultPawnClass = ANWCameraPawn::StaticClass();
	PlayerControllerClass = ANWPlayerController::StaticClass();
}
