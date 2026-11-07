#include "DCPlanMarker.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	UStaticMeshComponent* MakeMarkerPart(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh)
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
	Disc = MakeMarkerPart(this, TEXT("Disc"), DCVisuals::Cylinder());
	Disc->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
	Disc->SetRelativeScale3D(FVector(0.34, 0.34, 0.02));
	Pole = MakeMarkerPart(this, TEXT("Pole"), DCVisuals::Cylinder());
	Pole->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	Pole->SetRelativeScale3D(FVector(0.06, 0.06, 1.4));
	Pole->SetVisibility(false);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(30.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADCPlanMarker::AddSegment(const FVector& From, const FVector& To, const FLinearColor& Color, double Width)
{
	const FVector Delta = To - From;
	const double Length = Delta.Size2D();
	if (Length < 5.0)
	{
		return;
	}
	UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
	Segment->SetStaticMesh(DCVisuals::Cube());
	Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segment->SetCanEverAffectNavigation(false);
	Segment->SetCastShadow(false);
	Segment->SetupAttachment(RootComponent);
	Segment->RegisterComponent();
	Segment->SetWorldLocationAndRotation(From + Delta * 0.5 + FVector(0.0, 0.0, 1.5), FRotator(0.0, Delta.Rotation().Yaw, 0.0));
	Segment->SetWorldScale3D(FVector(Length / 100.0, Width / 100.0, 0.01));
	DCVisuals::Paint(Segment, Color);
	Segments.Add(Segment);
}

void ADCPlanMarker::Init(const FLinearColor& Color, int32 Number, const TArray<FVector>& Route, bool bBlocked, EDCMarkerLook Look)
{
	const FLinearColor Base = bBlocked ? FLinearColor(1.f, 0.1f, 0.05f) : Color;
	const bool bFaint = Look == EDCMarkerLook::Faint;
	const FLinearColor Shown = bFaint ? Base * 0.45f : Base;
	DCVisuals::Paint(Disc, Shown);
	const double DiscScale = Look == EDCMarkerLook::Highlight ? 0.5 : bFaint ? 0.14 : 0.34;
	Disc->SetRelativeScale3D(FVector(DiscScale, DiscScale, 0.02));

	Label->SetVisibility(!bFaint);
	Label->SetText(FText::AsNumber(Number));
	Label->SetTextRenderColor(Shown.ToFColor(true));
	Label->SetWorldSize(Look == EDCMarkerLook::Highlight ? 44.f : 30.f);

	// The way there, on the floor, following the walked path.
	const double Width = bFaint ? 2.0 : Look == EDCMarkerLook::Highlight ? 7.0 : 4.0;
	for (int32 Index = 1; Index < Route.Num(); ++Index)
	{
		AddSegment(Route[Index - 1], Route[Index], Shown * 0.8f, Width);
	}
}

void ADCPlanMarker::InitGhost(const FLinearColor& Color)
{
	DCVisuals::Paint(Disc, Color);
	DCVisuals::Paint(Pole, Color);
	Disc->SetRelativeScale3D(FVector(0.8, 0.8, 0.02));
	Pole->SetVisibility(true);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 165.0));
	Label->SetWorldSize(26.f);
	// Light text reads on the floor and on furniture; the pin carries the crew colour.
	Label->SetTextRenderColor(FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.75f).ToFColor(true));
}

void ADCPlanMarker::SetGhost(const FVector& FloorLocation, const FText& Text)
{
	SetActorLocation(FloorLocation);
	Label->SetText(DCVisuals::PlainText(Text));
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
