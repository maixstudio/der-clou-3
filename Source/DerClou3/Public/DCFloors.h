#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "DCFloors.generated.h"

class APlayerController;

/**
 * Storeys of a building: the camera shows one storey and hides everything above it.
 *
 * Level geometry tells its storey with an actor tag (DCFloor_KG, DCFloor_EG, DCFloor_OG; set by
 * Scripts/Levels/build_villa.py). Untagged actors (garden, sky, outbuildings) always stay visible.
 * The shown storey follows the selected crew member. Page Up / Page Down or the arrows in the top bar
 * pick another storey, which stays until the selection changes or she changes storey herself.
 * Crew members inside the house on a hidden storey are hidden too. Mouse traces look through hidden
 * actors (HitUnderCursor); their collision stays, so the walkable area does not change.
 */
UCLASS()
class DERCLOU3_API UDCFloorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDCFloorSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** -1 cellar, 0 ground floor, 1 first floor (by height of a point, e.g. a crew member's middle). */
	static int32 FloorOfHeight(double Z);
	/** Height of the finished floor of a storey in cm. */
	static double HeightOfFloor(int32 Floor);
	static FText FloorName(int32 Floor);

	bool HasSeveralFloors() const { return MaxFloor > MinFloor; }
	int32 GetShownFloor() const { return ShownFloor; }
	bool CanStep(int32 Delta) const;

	/** Shows this storey (clamped to the storeys the level has). bManual = chosen by the player. */
	void ShowFloor(int32 Floor, bool bManual);
	void StepFloor(int32 Delta) { ShowFloor(ShownFloor + Delta, true); }

	/** Hidden because its storey lies above the shown one. */
	bool IsHiddenByFloor(const AActor* Actor) const;

	/** Like GetHitResultUnderCursor(ECC_Visibility), but looks through everything on hidden storeys. */
	static bool HitUnderCursor(APlayerController* Controller, FHitResult& OutHit);
	/** The same for a point on the screen (viewport pixels), e.g. a self-test's click. */
	static bool HitUnderScreenPosition(APlayerController* Controller, const FVector2D& ScreenPosition, FHitResult& OutHit);

private:
	void CollectFloorActors();
	void Apply();
	void UpdateCrew();

	TArray<TPair<TWeakObjectPtr<AActor>, int32>> FloorActors;
	TArray<TWeakObjectPtr<AActor>> HiddenActors;
	TArray<TWeakObjectPtr<AActor>> HiddenCrew;
	TWeakObjectPtr<AActor> Followed;
	int32 FollowedFloor = 0;
	int32 ShownFloor = 0;
	int32 MinFloor = 0;
	int32 MaxFloor = 0;
	bool bManual = false;
	bool bCollected = false;
	bool bApplied = false;
};
