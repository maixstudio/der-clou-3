#include "DCItemPickup.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCItem.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "DCItemPickup"

ADCItemPickup::ADCItemPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Clickable with the mouse.
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(22.f);
	Label->SetTextRenderColor(FColor(255, 215, 60));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetHiddenInGame(true);
}

void ADCItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void ADCItemPickup::BeginPlay()
{
	Super::BeginPlay();
	GetItem();
	ApplyLook();
}

TArray<FName> ADCItemPickup::GetItemIdOptions() const
{
	TArray<FName> Ids;
	for (const FDCItemDef& Def : DCItems::GetAll())
	{
		Ids.Add(Def.Id);
	}
	return Ids;
}

UDCItem* ADCItemPickup::GetItem()
{
	if (!Item)
	{
		Item = UDCItem::Create(this, ItemId);
	}
	return Item;
}

void ADCItemPickup::SetItem(UDCItem* InItem)
{
	Item = InItem;
	if (Item)
	{
		ItemId = Item->GetDefId();
	}
	ApplyLook();
}

void ADCItemPickup::ApplyLook()
{
	const FDCItemDef* Def = DCItems::Find(ItemId);
	if (!Def)
	{
		return;
	}
	switch (Def->Shape)
	{
	case EDCItemShape::Box: Mesh->SetStaticMesh(DCVisuals::Cube()); break;
	case EDCItemShape::Cylinder: Mesh->SetStaticMesh(DCVisuals::Cylinder()); break;
	case EDCItemShape::Sphere: Mesh->SetStaticMesh(DCVisuals::Sphere()); break;
	}
	// Basic shapes are 100 cm with the pivot in the centre; the actor origin is on the floor.
	Mesh->SetRelativeScale3D(Def->ShapeSize / 100.0);
	Mesh->SetRelativeLocation(FVector(0.0, 0.0, Def->ShapeSize.Z * 0.5));
	DCVisuals::Paint(Mesh, Def->Color);

	// Only bulky things are in the way; the crew walks over small ones.
	const bool bBulky = Def->Size == EDCItemSize::Huge;
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, bBulky ? ECR_Block : ECR_Ignore);
	Mesh->SetCanEverAffectNavigation(bBulky);

	Label->SetText(Def->Name);
	Label->SetRelativeLocation(FVector(0.0, 0.0, Def->ShapeSize.Z + 30.0));
}

float ADCItemPickup::GetFootprintRadius() const
{
	const FDCItemDef* Def = DCItems::Find(ItemId);
	return Def ? FMath::Max(Def->ShapeSize.X, Def->ShapeSize.Y) * 0.5f : 20.f;
}

void ADCItemPickup::SetHighlighted(bool bInHighlighted)
{
	if (bHighlighted == bInHighlighted)
	{
		return;
	}
	bHighlighted = bInHighlighted;
	Label->SetHiddenInGame(!bHighlighted);
	SetActorTickEnabled(bHighlighted);
}

void ADCItemPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Label faces the camera, like the crew names.
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager)
	{
		const FQuat CameraRotation = Player->PlayerCameraManager->GetCameraRotation().Quaternion();
		Label->SetWorldRotation(FRotationMatrix::MakeFromXZ(-CameraRotation.GetForwardVector(), CameraRotation.GetUpVector()).Rotator());
	}
}

FText ADCItemPickup::GetInteractableName() const
{
	const FDCItemDef* Def = DCItems::Find(ItemId);
	return Def ? Def->Name : FText::FromName(ItemId);
}

FText ADCItemPickup::GetInteractableKind() const
{
	const FDCItemDef* Def = DCItems::Find(ItemId);
	return Def ? Def->Kind : FText::GetEmpty();
}

void ADCItemPickup::GetInfoLines(TArray<FDCInfoLine>& OutLines) const
{
	if (Item)
	{
		Item->GetInfoLines(OutLines);
		if (!Item->Contents.IsEmpty())
		{
			TArray<FString> Names;
			for (const UDCItem* Content : Item->Contents)
			{
				Names.Add(Content->GetDisplayName().ToString());
			}
			OutLines.Add({ LOCTEXT("LabelContents", "Inhalt"), FText::FromString(FString::Join(Names, TEXT(", "))) });
		}
	}
	const FDCItemDef* Def = DCItems::Find(ItemId);
	if (Def && !Def->Description.IsEmpty())
	{
		OutLines.Add({ LOCTEXT("LabelNote", "Hinweis"), Def->Description });
	}
}

#undef LOCTEXT_NAMESPACE
