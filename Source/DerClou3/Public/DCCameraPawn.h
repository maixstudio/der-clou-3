#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "DCCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Top-down camera. WASD pan, Q/E or right/middle mouse drag rotate (drag up/down tilts),
 * mouse wheel zoom. Spawns at the PlayerStart.
 *
 * Follows the selected crew member (the controller hands over where they are every frame). While
 * planning it stays still as long as that point is in the calm middle of the view and only then glides
 * over; while the plan runs it stays on the figure. Panning with WASD lets go until ResumeFollow.
 */
UCLASS()
class DERCLOU3_API ADCCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ADCCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** Glides over to look at this place (stops as soon as the player pans). Stops following. */
	void FocusOn(const FVector& Location);

	/** Where the followed crew member is (planning: where their plan has got to); bCalmMiddle: only move once it leaves the middle. */
	void SetFollowPoint(const FVector& Location, bool bCalmMiddle);
	/** Follow the selected crew member again: glides over to them at once. */
	void ResumeFollow();
	/** False after the player panned away (WASD) or FocusOn sent the camera somewhere else. */
	bool IsFollowing() const { return bFollow; }

	/** Glides up or down so the camera looks at this storey (finished floor height in cm). */
	void SetFloorHeight(double FloorZ);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float PanSpeed = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float RotateSpeed = 90.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomStep = 200.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinDistance = 600.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxDistance = 5500.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float StartDistance = 1600.f;

	/** Degrees per pixel of mouse drag. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float DragRotateSpeed = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float DragTiltSpeed = 0.2f;

	/** Camera pitch range (negative = looking down). */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinPitch = -85.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxPitch = -35.f;

private:
	bool bDragging = false;
	TOptional<FVector> FocusTarget;
	/** Follow the selected crew member (FollowPoint, set by the controller every frame). */
	bool bFollow = true;
	/** Glide over even if the point is in the calm middle (after selecting someone or ResumeFollow). */
	bool bGlideToFollow = true;
	bool bFollowCalm = true;
	TOptional<FVector> FollowPoint;
	/** Pivot height at the start (ground floor) and the storey to look at. */
	TOptional<double> BaseZ;
	double FloorZ = 0.0;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
