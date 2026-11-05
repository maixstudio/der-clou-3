#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "DCCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Top-down camera. WASD pan, Q/E or right/middle mouse drag rotate (drag up/down tilts),
 * mouse wheel zoom. Spawns at the PlayerStart.
 */
UCLASS()
class DERCLOU3_API ADCCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ADCCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

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
	float MaxDistance = 3500.f;

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
	FVector2D LastMouse = FVector2D::ZeroVector;
};
