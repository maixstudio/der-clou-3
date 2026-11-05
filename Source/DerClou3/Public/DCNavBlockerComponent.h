#pragma once

#include "Components/BoxComponent.h"
#include "CoreMinimal.h"

#include "DCNavBlockerComponent.generated.h"

/**
 * Invisible box that removes its volume from the nav mesh while it affects navigation
 * (dynamic obstacle with the null area). Toggle with SetCanEverAffectNavigation.
 * Does not collide with anything and ignores mouse traces.
 */
UCLASS(ClassGroup = (DerClou), meta = (BlueprintSpawnableComponent))
class DERCLOU3_API UDCNavBlockerComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UDCNavBlockerComponent();
};
