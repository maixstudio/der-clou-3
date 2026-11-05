#include "DCCrewCharacter.h"

#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"

namespace
{
	UStaticMeshComponent* MakePart(ACharacter* Owner, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Owner->GetCapsuleComponent());
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	}
}

ADCCrewCharacter::ADCCrewCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	CrewName = FText::FromString(TEXT("Vic"));

	// AI controller walks the character along nav mesh paths.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0, 720.0, 0.0);
	Movement->MaxWalkSpeed = 250.f;

	// Clicks on the figure select it.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(30.f, 90.f);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Capsule centre is 90 cm above the floor.
	Body = MakePart(this, TEXT("Body"), DCVisuals::Cylinder(), FVector(0.0, 0.0, -18.0), FVector(0.5, 0.5, 1.45));
	Head = MakePart(this, TEXT("Head"), DCVisuals::Sphere(), FVector(0.0, 0.0, 70.0), FVector(0.36));
	Nose = MakePart(this, TEXT("Nose"), DCVisuals::Cube(), FVector(20.0, 0.0, 68.0), FVector(0.16, 0.1, 0.08));
	SelectionRing = MakePart(this, TEXT("SelectionRing"), DCVisuals::Cylinder(), FVector(0.0, 0.0, -88.0), FVector(0.9, 0.9, 0.02));
	SelectionRing->SetCastShadow(false);
	SelectionRing->SetVisibility(false);

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(GetCapsuleComponent());
	NameLabel->SetRelativeLocation(FVector(0.0, 0.0, 115.0));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetVerticalAlignment(EVRTA_TextCenter);
	NameLabel->SetWorldSize(36.f);
	NameLabel->SetTextRenderColor(FColor::White);
	NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADCCrewCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DCVisuals::Paint(Body, BodyColor);
	DCVisuals::Paint(Head, FLinearColor(0.85f, 0.68f, 0.52f));
	DCVisuals::Paint(Nose, FLinearColor(0.08f, 0.08f, 0.08f));
	DCVisuals::Paint(SelectionRing, FLinearColor(1.f, 0.85f, 0.f));
	NameLabel->SetText(CrewName);
}

void ADCCrewCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Keep the name facing the player's camera.
	if (const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (Player->PlayerCameraManager)
		{
			const FVector ToCamera = Player->PlayerCameraManager->GetCameraLocation() - NameLabel->GetComponentLocation();
			NameLabel->SetWorldRotation(ToCamera.Rotation());
		}
	}
}

void ADCCrewCharacter::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	SelectionRing->SetVisibility(bSelected);
}

bool ADCCrewCharacter::MoveToLocation(const FVector& Location)
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation || !GetController())
	{
		return false;
	}
	FNavLocation Projected;
	if (!Navigation->ProjectPointToNavigation(Location, Projected, FVector(60.0, 60.0, 300.0)))
	{
		return false;
	}
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetController(), Projected.Location);
	return true;
}
