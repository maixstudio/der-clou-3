#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCPlanTrail.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * The planned ways on the floor while planning: footprints in each crew member's colour (left, right, in the
 * direction of walking) and a ring where they stand to do something. One actor for the whole crew; every
 * footprint is an instance with its own colour and opacity (per-instance custom data of M_PlanMark), so
 * hundreds of them cost next to nothing. Marks on storeys above the shown one are hidden.
 *
 * Only something to look at: no collision, no shadows, never part of the walkable area.
 */
UCLASS(NotPlaceable)
class DERCLOU3_API ADCPlanTrail : public AActor
{
	GENERATED_BODY()

public:
	ADCPlanTrail();

	/** Never part of the walkable area (adding marks must not rebuild the nav mesh). */
	virtual bool IsComponentRelevantForNavigation(UActorComponent* Component) const override { return false; }
	virtual void Tick(float DeltaSeconds) override;

	/** Removes all marks (call Finish after adding the new ones). */
	void Clear();
	/** One footprint: Location on or near the floor, Yaw = walking direction, bLeft = mirrored. */
	void AddFootprint(const FVector& Location, double Yaw, bool bLeft, const FLinearColor& Color, float Opacity, float Scale);
	/** A ring on the floor where a crew member stands to act (Radius in cm). */
	void AddRing(const FVector& Location, const FLinearColor& Color, float Opacity, float Radius);
	/** Shows what was added since Clear. */
	void Finish();

	/** How many footprints the trail shows (self-tests). */
	int32 GetFootprintCount() const { return Footprints.Num(); }

	/** The floor right under a point of a way (a few cm below the nav mesh); the point itself if there is none. */
	FVector OnFloor(const FVector& Location) const;

private:
	struct FMark
	{
		FTransform Transform;
		FLinearColor Color;
		float Opacity = 1.f;
		int32 Storey = 0;
	};
	/** The instances of one kind on one storey (made when needed). */
	UInstancedStaticMeshComponent* GetLayer(bool bRing, int32 Storey);
	void Fill(bool bRing, const TArray<FMark>& Marks);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> MarkLayers;
	/** Per layer: ring or footprint, and its storey. */
	TArray<TPair<bool, int32>> MarkLayerKeys;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> FootprintMesh;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RingMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MarkMaterial;

	TArray<FMark> Footprints;
	TArray<FMark> Rings;
	int32 ShownStorey = TNumericLimits<int32>::Max();
};
