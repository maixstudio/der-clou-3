#include "DCRoomVolume.h"

#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/** Room names on the floor: large, quiet, readable from the planning camera. */
	constexpr float RoomLabelSize = 46.f;
	const FColor RoomLabelColor(196, 188, 160);
}

ADCRoomVolume::ADCRoomVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	RootComponent = Area;
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetCanEverAffectNavigation(false);
	Area->SetHiddenInGame(true);
	Area->ShapeColor = FColor(120, 200, 255);

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(Area);
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetVerticalAlignment(EVRTA_TextCenter);
	NameLabel->SetWorldSize(RoomLabelSize);
	NameLabel->SetTextRenderColor(RoomLabelColor);
	NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameLabel->SetCanEverAffectNavigation(false);
	// Flat on the floor, readable with the room's +X pointing up on screen.
	NameLabel->SetRelativeRotation(FRotator(90.0, 180.0, 0.0));
}

void ADCRoomVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Area->SetBoxExtent(FVector(HalfSize.X, HalfSize.Y, 120.0));
	// The box stands on the floor (actor 120 cm up); the name lies just above the floor, towards the far side.
	NameLabel->SetRelativeLocation(FVector(HalfSize.X * 0.55, 0.0, -118.0));
	NameLabel->SetText(DCVisuals::PlainText(RoomName));
}

bool ADCRoomVolume::ContainsPoint(const FVector& Location) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	return FMath::Abs(Local.X) <= HalfSize.X && FMath::Abs(Local.Y) <= HalfSize.Y;
}

ADCRoomVolume* ADCRoomVolume::FindAt(const UObject* WorldContext, const FVector& Location)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCRoomVolume> It(World); It; ++It)
	{
		if (It->ContainsPoint(Location))
		{
			return *It;
		}
	}
	return nullptr;
}
