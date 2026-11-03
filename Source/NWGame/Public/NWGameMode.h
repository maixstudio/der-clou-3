#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "NWGameMode.generated.h"

/** Default game mode: top-down camera pawn and a mouse-driven player controller. */
UCLASS()
class NWGAME_API ANWGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANWGameMode();
};
