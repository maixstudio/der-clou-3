#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "DCGameMode.generated.h"

/** Top-down camera pawn and mouse controller - or, on the start menu, the menu controller. */
UCLASS()
class DERCLOU3_API ADCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADCGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
