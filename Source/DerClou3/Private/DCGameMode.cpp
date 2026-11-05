#include "DCGameMode.h"

#include "DCCameraPawn.h"
#include "DCPlayerController.h"

ADCGameMode::ADCGameMode()
{
	DefaultPawnClass = ADCCameraPawn::StaticClass();
	PlayerControllerClass = ADCPlayerController::StaticClass();
}
