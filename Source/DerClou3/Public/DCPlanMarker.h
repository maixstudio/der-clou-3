#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCPlanMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Where a planned step happens: a numbered disc in the crew member's colour, with a line from the step before. */
UCLASS(NotPlaceable)
class DERCLOU3_API ADCPlanMarker : public AActor
{
	GENERATED_BODY()

public:
	ADCPlanMarker();

	void Init(const FLinearColor& Color, int32 Number, const FVector& From, bool bBlocked);
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Line;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;
};
