#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "DCCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

/** Top-down camera: WASD pan, Q/E rotate, mouse wheel zoom. Spawns at the PlayerStart. */
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
};
