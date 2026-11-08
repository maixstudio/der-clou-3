#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCPlanMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** How strongly a step marker shows. */
enum class EDCMarkerLook : uint8
{
	/** The plan being edited: numbered disc and route. */
	Normal,
	/** The step picked in the list or the timeline. */
	Highlight,
	/** Someone else's plan: small and dark, no number. */
	Faint
};

/**
 * Where a planned step happens: a numbered disc in the crew member's colour, with the route from
 * the step before drawn on the floor. Also used as the preview pin that shows where a crew member
 * is at a chosen time of the plan.
 */
UCLASS(NotPlaceable)
class DERCLOU3_API ADCPlanMarker : public AActor
{
	GENERATED_BODY()

public:
	/** Only something to look at: never part of the walkable area (spawning it must not rebuild the nav mesh). */
	virtual bool IsComponentRelevantForNavigation(UActorComponent* Component) const override { return false; }
	ADCPlanMarker();

	/** Route: the way walked to this step (floor height), may be empty. */
	void Init(const FLinearColor& Color, int32 Number, const TArray<FVector>& Route, bool bBlocked, EDCMarkerLook Look);
	/** A pin with a label ("Rosa: wartet auf Vic") for the time preview. */
	void InitGhost(const FLinearColor& Color);
	void SetGhost(const FVector& FloorLocation, const FText& Text);

	virtual void Tick(float DeltaSeconds) override;

private:
	void AddSegment(const FVector& From, const FVector& To, const FLinearColor& Color, double Width);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pole;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;
};
