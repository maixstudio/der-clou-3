#include "DCPlanMarker.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	UStaticMeshComponent* MakeFlat(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Owner->GetRootComponent());
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		return Part;
	}
}

ADCPlanMarker::ADCPlanMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Disc = MakeFlat(this, TEXT("Disc"), DCVisuals::Cylinder());
	Disc->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
	Disc->SetRelativeScale3D(FVector(0.34, 0.34, 0.02));
	Line = MakeFlat(this, TEXT("Line"), DCVisuals::Cube());
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(30.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADCPlanMarker::Init(const FLinearColor& Color, int32 Number, const FVector& From, bool bBlocked)
{
	const FLinearColor Shown = bBlocked ? FLinearColor(1.f, 0.1f, 0.05f) : Color;
	DCVisuals::Paint(Disc, Shown);
	DCVisuals::Paint(Line, Shown * 0.8f);
	Label->SetText(FText::AsNumber(Number));
	Label->SetTextRenderColor(Shown.ToFColor(true));

	// Thin line on the floor from the previous step to this one.
	const FVector Delta = GetActorLocation() - From;
	const double Length = Delta.Size2D();
	Line->SetVisibility(Length > 10.0);
	Line->SetWorldLocationAndRotation(From + Delta * 0.5 + FVector(0.0, 0.0, 1.5), FRotator(0.0, Delta.Rotation().Yaw, 0.0));
	Line->SetWorldScale3D(FVector(Length / 100.0, 0.04, 0.01));
}

void ADCPlanMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager)
	{
		const FQuat CameraRotation = Player->PlayerCameraManager->GetCameraRotation().Quaternion();
		Label->SetWorldRotation(FRotationMatrix::MakeFromXZ(-CameraRotation.GetForwardVector(), CameraRotation.GetUpVector()).Rotator());
	}
}
