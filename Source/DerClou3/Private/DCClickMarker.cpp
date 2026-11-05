#include "DCClickMarker.h"

#include "Components/StaticMeshComponent.h"
#include "DCVisuals.h"

namespace
{
	constexpr float Lifetime = 0.8f;
	const FLinearColor ReachableColor(0.f, 0.85f, 1.f);
	const FLinearColor BlockedColor(1.f, 0.15f, 0.1f);
}

ADCClickMarker::ADCClickMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	RootComponent = Ring;
	Ring->SetStaticMesh(DCVisuals::Cylinder());
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCanEverAffectNavigation(false);
	Ring->SetCastShadow(false);
	Ring->SetHiddenInGame(true);
}

void ADCClickMarker::BeginPlay()
{
	Super::BeginPlay();
	DCVisuals::Paint(Ring, ReachableColor);
}

void ADCClickMarker::Show(const FVector& Location, bool bReachable)
{
	SetActorLocation(Location + FVector(0.0, 0.0, 2.0));
	if (bValid != bReachable)
	{
		DCVisuals::Paint(Ring, bReachable ? ReachableColor : BlockedColor);
		bValid = bReachable;
	}
	Age = 0.f;
	Ring->SetHiddenInGame(false);
}

void ADCClickMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Age >= Lifetime)
	{
		return;
	}
	Age += DeltaSeconds;
	// Shrinks from 60 cm to nothing.
	const float Size = 0.6f * FMath::Max(0.f, 1.f - Age / Lifetime);
	Ring->SetRelativeScale3D(FVector(Size, Size, 0.01f));
	Ring->SetHiddenInGame(Age >= Lifetime);
}
