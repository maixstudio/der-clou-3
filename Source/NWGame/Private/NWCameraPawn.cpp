#include "NWCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

ANWCameraPawn::ANWCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->TargetArmLength = DefaultArmLength;
	SpringArm->SetRelativeRotation(FRotator(-72.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraLagSpeed = 8.f;
	SpringArm->CameraRotationLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(45.f);
}

void ANWCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Pan relative to the current camera yaw, faster when zoomed out.
	FVector2D Pan = FVector2D::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::W)) { Pan.X += 1.0; }
	if (PC->IsInputKeyDown(EKeys::S)) { Pan.X -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::D)) { Pan.Y += 1.0; }
	if (PC->IsInputKeyDown(EKeys::A)) { Pan.Y -= 1.0; }

	if (!Pan.IsNearlyZero())
	{
		const FRotator YawOnly(0.0, GetActorRotation().Yaw, 0.0);
		const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
		const float ZoomFactor = SpringArm->TargetArmLength / DefaultArmLength;
		const FVector Direction = (Forward * Pan.X + Right * Pan.Y).GetSafeNormal();
		AddActorWorldOffset(Direction * PanSpeed * ZoomFactor * DeltaSeconds);
	}

	float Rotate = 0.f;
	if (PC->IsInputKeyDown(EKeys::E)) { Rotate += 1.f; }
	if (PC->IsInputKeyDown(EKeys::Q)) { Rotate -= 1.f; }
	if (Rotate != 0.f)
	{
		AddActorWorldRotation(FRotator(0.f, Rotate * RotateSpeed * DeltaSeconds, 0.f));
	}

	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		SpringArm->TargetArmLength -= ZoomStep;
	}
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		SpringArm->TargetArmLength += ZoomStep;
	}
	SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength, MinArmLength, MaxArmLength);
}
