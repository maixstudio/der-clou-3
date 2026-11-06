#include "DCItemPickup.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCItem.h"
#include "DCVisuals.h"
#include "DerClou3.h"
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
	ChangedHandle = UDCItem::OnAnyChanged().AddUObject(this, &ADCItemPickup::OnItemChanged);
}

void ADCItemPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDCItem::OnAnyChanged().Remove(ChangedHandle);
	Super::EndPlay(EndPlayReason);
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
		BuildItem();
	}
	return Item;
}

void ADCItemPickup::BuildItem()
{
	Item = UDCItem::Create(ItemId);
	if (!Item)
	{
		return;
	}
	TSet<UDCItem*> Configured;
	for (const FDCContentSpec& Spec : Contents)
	{
		// Walk down the path of ids to the container.
		UDCItem* Container = Item;
		TArray<FString> Path;
		Spec.Inside.ParseIntoArray(Path, TEXT("/"), true);
		for (const FString& Segment : Path)
		{
			UDCItem* Next = nullptr;
			for (UDCItem* Child : Container->GetContents())
			{
				if (Child->GetDefId() == FName(*Segment))
				{
					Next = Child;
					break;
				}
			}
			Container = Next;
			if (!Container)
			{
				break;
			}
		}
		if (!Container)
		{
			UE_LOG(LogDerClou, Warning, TEXT("%s: no '%s' inside %s for %s"), *GetName(), *Spec.Inside, *ItemId.ToString(), *Spec.ItemId.ToString());
			continue;
		}

		// A fixed part of this kind (the desk's drawer) gets configured instead of adding another one.
		UDCItem* Target = nullptr;
		for (UDCItem* Child : Container->GetContents())
		{
			if (Child->IsAttached() && Child->GetDefId() == Spec.ItemId && !Configured.Contains(Child))
			{
				Target = Child;
				break;
			}
		}
		if (!Target)
		{
			Target = UDCItem::Create(Spec.ItemId);
			if (!Target)
			{
				continue;
			}
			// The level designer decides; still say so if it would not fit by the rules.
			if (Target->GetDef().Size > Container->GetDef().MaxContentSize || !Container->IsContainer())
			{
				UE_LOG(LogDerClou, Warning, TEXT("%s: %s does not really fit into %s"), *GetName(), *Spec.ItemId.ToString(), *Container->GetDefId().ToString());
			}
			Container->AddContent(Target);
		}
		Configured.Add(Target);
		Target->SetOpen(Spec.bOpen);
		Target->SetLocked(Spec.bLocked);
	}
	Item->SetOpen(bOpen);
	Item->SetLocked(bLocked);
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

void ADCItemPickup::OnItemChanged()
{
	if (Item)
	{
		const FText State = Item->DescribeState();
		Label->SetText(State.IsEmpty() ? Item->GetDisplayName() : FText::Format(LOCTEXT("LabelWithState", "{0} ({1})"), Item->GetDisplayName(), State));
	}
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

	// Furniture and other big things are in the way; the crew walks over small ones.
	const bool bObstacle = Def->IsObstacle();
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, bObstacle ? ECR_Block : ECR_Ignore);
	Mesh->SetCanEverAffectNavigation(bObstacle);

	Label->SetText(Def->Name);
	Label->SetRelativeLocation(FVector(0.0, 0.0, Def->ShapeSize.Z + 30.0));
	OnItemChanged();
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
	}
	const FDCItemDef* Def = DCItems::Find(ItemId);
	if (Def && !Def->Description.IsEmpty())
	{
		OutLines.Add({ LOCTEXT("LabelNote", "Hinweis"), Def->Description });
	}
}

#undef LOCTEXT_NAMESPACE
