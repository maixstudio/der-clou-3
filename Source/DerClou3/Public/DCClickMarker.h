#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCClickMarker.generated.h"

class UStaticMeshComponent;

/** Short-lived ring on the floor where the player sent a crew member. */
UCLASS()
class DERCLOU3_API ADCClickMarker : public AActor
{
	GENERATED_BODY()

public:
	ADCClickMarker();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Shows the marker at the location; red means "not reachable". */
	void Show(const FVector& Location, bool bReachable);

private:
	UPROPERTY(VisibleAnywhere, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> Ring;

	float Age = 10.f;
	bool bValid = true;
};
