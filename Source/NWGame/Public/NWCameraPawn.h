#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "NWCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Top-down tactical camera: WASD pan, Q/E rotate, mouse wheel zoom.
 *
 * Graybox placeholder: keys are polled directly. Replace with Enhanced Input actions
 * (IA_Pan, IA_Rotate, IA_Zoom in /Game/NW/Input) once the input assets exist.
 */
UCLASS()
class NWGAME_API ANWCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ANWCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Pan speed in cm/s at the default zoom level. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float PanSpeed = 1500.f;

	/** Degrees per second. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float RotateSpeed = 90.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomStep = 250.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinArmLength = 600.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxArmLength = 4000.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float DefaultArmLength = 2500.f;
};
