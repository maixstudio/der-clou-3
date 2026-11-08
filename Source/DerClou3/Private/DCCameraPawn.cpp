#include "DCCameraPawn.h"

#include "DCOptions.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

ADCCameraPawn::ADCCameraPawn()
{
	// Look around while the plan is paused.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->SetRelativeRotation(FRotator(-65.f, 0.f, 0.f));
	SpringArm->TargetArmLength = StartDistance;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(50.f);
}

void ADCCameraPawn::FocusOn(const FVector& Location)
{
	FocusTarget = Location;
}

void ADCCameraPawn::SetFloorHeight(double InFloorZ)
{
	FloorZ = InFloorZ;
}

void ADCCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FVector2D Pan = FVector2D::ZeroVector;
	Pan.X += PC->IsInputKeyDown(EKeys::W) ? 1.0 : 0.0;
	Pan.X -= PC->IsInputKeyDown(EKeys::S) ? 1.0 : 0.0;
	Pan.Y += PC->IsInputKeyDown(EKeys::D) ? 1.0 : 0.0;
	Pan.Y -= PC->IsInputKeyDown(EKeys::A) ? 1.0 : 0.0;
	if (!Pan.IsNearlyZero())
	{
		const FRotator Yaw(0.0, GetActorRotation().Yaw, 0.0);
		const FVector Direction = (Yaw.RotateVector(FVector::ForwardVector) * Pan.X + Yaw.RotateVector(FVector::RightVector) * Pan.Y).GetSafeNormal();
		const float Scale = SpringArm->TargetArmLength / StartDistance;
		AddActorWorldOffset(Direction * PanSpeed * DCOptions::CameraSpeedFactor() * Scale * DeltaSeconds);
		FocusTarget.Reset();
	}
	// Storeys: the pivot glides to the shown storey's floor (kept relative to the start height).
	if (!BaseZ.IsSet())
	{
		BaseZ = GetActorLocation().Z;
	}
	const double WantZ = *BaseZ + FloorZ;
	if (!FMath::IsNearlyEqual(GetActorLocation().Z, WantZ, 0.5))
	{
		FVector Location = GetActorLocation();
		Location.Z = FMath::FInterpTo(Location.Z, WantZ, DeltaSeconds, 6.f);
		SetActorLocation(Location);
	}
	if (FocusTarget.IsSet())
	{
		const FVector Target(FocusTarget->X, FocusTarget->Y, GetActorLocation().Z);
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, 6.f));
		if (FVector::Dist2D(GetActorLocation(), Target) < 5.0)
		{
			FocusTarget.Reset();
		}
	}

	const float Rotate = (PC->IsInputKeyDown(EKeys::E) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Q) ? 1.f : 0.f);
	if (Rotate != 0.f)
	{
		AddActorWorldRotation(FRotator(0.0, Rotate * RotateSpeed * DCOptions::CameraSpeedFactor() * DeltaSeconds, 0.0));
	}

	// Drag with the right or middle mouse button: left/right rotates, up/down tilts.
	float MouseX = 0.f;
	float MouseY = 0.f;
	const bool bHasMouse = PC->GetMousePosition(MouseX, MouseY);
	const FVector2D Mouse(MouseX, MouseY);
	const bool bButton = PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::MiddleMouseButton);
	if (bButton && bHasMouse)
	{
		if (bDragging)
		{
			const FVector2D Delta = Mouse - LastMouse;
			AddActorWorldRotation(FRotator(0.0, Delta.X * DragRotateSpeed, 0.0));
			FRotator Arm = SpringArm->GetRelativeRotation();
			Arm.Pitch = FMath::Clamp(Arm.Pitch - Delta.Y * DragTiltSpeed, MinPitch, MaxPitch);
			SpringArm->SetRelativeRotation(Arm);
		}
		bDragging = true;
		LastMouse = Mouse;
	}
	else
	{
		bDragging = false;
	}

	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		SpringArm->TargetArmLength -= ZoomStep;
	}
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		SpringArm->TargetArmLength += ZoomStep;
	}
	SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength, MinDistance, MaxDistance);
}
