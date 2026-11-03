#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "NWPlayerController.generated.h"

/** Mouse-first controller for planning and tactical play. */
UCLASS()
class NWGAME_API ANWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANWPlayerController();

protected:
	virtual void BeginPlay() override;
};
