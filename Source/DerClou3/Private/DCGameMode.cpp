#include "DCGameMode.h"

#include "DCCameraPawn.h"
#include "DCGameFlow.h"
#include "DCMenuController.h"
#include "DCPlayerController.h"

ADCGameMode::ADCGameMode()
{
	DefaultPawnClass = ADCCameraPawn::StaticClass();
	PlayerControllerClass = ADCPlayerController::StaticClass();
}

void ADCGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Decided before anyone logs in: the start menu gets its own controller (no planning UI, a slow camera).
	UDCGameFlowSubsystem* Flow = UDCGameFlowSubsystem::Get(this);
	PlayerControllerClass = Flow && Flow->DecideMenu(GetWorld(), Options) ? ADCMenuPlayerController::StaticClass() : ADCPlayerController::StaticClass();
}
