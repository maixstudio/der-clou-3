#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "DCInteractable.generated.h"

/** One line of the info panel: "Zustand: verschlossen". */
struct FDCInfoLine
{
	FText Label;
	FText Value;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDCInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something the player can click and interact with (doors now; windows, safes, … later).
 * Provides what the info panel shows.
 */
class DERCLOU3_API IDCInteractable
{
	GENERATED_BODY()

public:
	/** Name of this particular object, e.g. "Hintertür". */
	virtual FText GetInteractableName() const = 0;
	/** What kind of object it is, e.g. "Tür". */
	virtual FText GetInteractableKind() const = 0;
	/** Facts for the info panel; called every frame while the panel is open. */
	virtual void GetInfoLines(TArray<FDCInfoLine>& OutLines) const = 0;
};
