#include "DCCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

ADCCameraPawn::ADCCameraPawn()
{
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
		AddActorWorldOffset(Direction * PanSpeed * Scale * DeltaSeconds);
	}

	const float Rotate = (PC->IsInputKeyDown(EKeys::E) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Q) ? 1.f : 0.f);
	if (Rotate != 0.f)
	{
		AddActorWorldRotation(FRotator(0.0, Rotate * RotateSpeed * DeltaSeconds, 0.0));
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
