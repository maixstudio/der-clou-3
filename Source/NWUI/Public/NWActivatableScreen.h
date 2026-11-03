#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"

#include "NWActivatableScreen.generated.h"

/**
 * Base class for all full-screen menus (hub, planner, debrief).
 * Create Widget Blueprints in /Game/NW/UI that derive from this class.
 */
UCLASS(Abstract, Blueprintable)
class NWUI_API UNWActivatableScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	/** Shown in the screen header and used by the back stack for debugging. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screen")
	FText ScreenTitle;
};
