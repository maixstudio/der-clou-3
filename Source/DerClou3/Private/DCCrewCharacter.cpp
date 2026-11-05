#include "DCCrewCharacter.h"

#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

#define LOCTEXT_NAMESPACE "DCCrewCharacter"

namespace
{
	/** Time with the hand on the door before it opens or closes. */
	constexpr float HandOnDoorSeconds = 0.5f;
	/** Give the nav mesh a moment to notice the opened door. */
	constexpr float NavSettleSeconds = 0.25f;
	constexpr float GiveUpSeconds = 3.f;

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

	UTextRenderComponent* MakeLabel(ACharacter* Owner, const TCHAR* Name, float Height, float Size, const FColor& Color)
	{
		UTextRenderComponent* Label = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
		Label->SetupAttachment(Owner->GetCapsuleComponent());
		Label->SetRelativeLocation(FVector(0.0, 0.0, Height));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(Size);
		Label->SetTextRenderColor(Color);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Label;
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

	NameLabel = MakeLabel(this, TEXT("NameLabel"), 118.f, 36.f, FColor::White);
	ActivityLabel = MakeLabel(this, TEXT("ActivityLabel"), 96.f, 24.f, FColor(255, 215, 60));
}

void ADCCrewCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DCVisuals::Paint(Body, BodyColor);
	DCVisuals::Paint(Head, FLinearColor(0.85f, 0.68f, 0.52f));
	DCVisuals::Paint(Nose, FLinearColor(0.08f, 0.08f, 0.08f));
	DCVisuals::Paint(SelectionRing, FLinearColor(1.f, 0.85f, 0.f));
	NameLabel->SetText(CrewName);
	ActivityLabel->SetText(FText::GetEmpty());
}

void ADCCrewCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTask(DeltaSeconds);
	UpdateLabels();
}

void ADCCrewCharacter::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	SelectionRing->SetVisibility(bSelected);
}

bool ADCCrewCharacter::StartMove(const FVector& Location)
{
	AAIController* AI = Cast<AAIController>(GetController());
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!AI || !Navigation)
	{
		return false;
	}
	FNavLocation Projected;
	if (!Navigation->ProjectPointToNavigation(Location, Projected, FVector(60.0, 60.0, 300.0)))
	{
		return false;
	}
	// No partial paths: a target behind a closed door is "not reachable", not "as close as possible".
	const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Projected.Location, 10.f, true, true, false, true, nullptr, false);
	return Result != EPathFollowingRequestResult::Failed;
}

bool ADCCrewCharacter::HasArrived() const
{
	const AAIController* AI = Cast<AAIController>(GetController());
	return !AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle;
}

void ADCCrewCharacter::SetPhase(ETaskPhase InPhase)
{
	Phase = InPhase;
	PhaseTime = 0.f;
}

bool ADCCrewCharacter::MoveToLocation(const FVector& Location)
{
	TaskDoor.Reset();
	const bool bStarted = StartMove(Location);
	SetPhase(bStarted ? ETaskPhase::Walking : ETaskPhase::Idle);
	return bStarted;
}

void ADCCrewCharacter::InteractWithDoor(ADCDoor* Door, EDCDoorAction Action)
{
	if (!Door)
	{
		return;
	}
	TaskDoor = Door;
	TaskAction = Action;
	const FVector Here = GetActorLocation();
	PassTarget = Door->GetStandPoint(Here, true);
	SetPhase(StartMove(Door->GetStandPoint(Here, false)) ? ETaskPhase::ToDoor : ETaskPhase::Idle);
}

void ADCCrewCharacter::UpdateTask(float DeltaSeconds)
{
	PhaseTime += DeltaSeconds;
	ADCDoor* Door = TaskDoor.Get();

	switch (Phase)
	{
	case ETaskPhase::Idle:
		return;

	case ETaskPhase::Walking:
		if (HasArrived())
		{
			SetPhase(ETaskPhase::Idle);
		}
		return;

	case ETaskPhase::ToDoor:
		if (!Door)
		{
			SetPhase(ETaskPhase::Idle);
			return;
		}
		if (HasArrived())
		{
			// Face the door, hand on the handle.
			const FVector ToDoor = Door->GetCenter() - GetActorLocation();
			SetActorRotation(FRotator(0.0, ToDoor.Rotation().Yaw, 0.0));
			SetPhase(ETaskPhase::AtDoor);
		}
		return;

	case ETaskPhase::AtDoor:
		if (!Door)
		{
			SetPhase(ETaskPhase::Idle);
			return;
		}
		if (PhaseTime < HandOnDoorSeconds)
		{
			return;
		}
		switch (TaskAction)
		{
		case EDCDoorAction::Open:
			Door->OpenAwayFrom(GetActorLocation());
			SetPhase(ETaskPhase::Idle);
			return;
		case EDCDoorAction::Close:
			Door->Close();
			SetPhase(ETaskPhase::Idle);
			return;
		case EDCDoorAction::GoThrough:
			Door->OpenAwayFrom(GetActorLocation());
			SetPhase(ETaskPhase::WaitingForDoor);
			return;
		}
		return;

	case ETaskPhase::WaitingForDoor:
		if (!Door || PhaseTime > GiveUpSeconds)
		{
			SetPhase(ETaskPhase::Idle);
			return;
		}
		// Wait until the panel is out of the way, then keep trying until the nav mesh lets us through.
		if (Door->GetOpenAmount() >= 0.98f && PhaseTime >= NavSettleSeconds && StartMove(PassTarget))
		{
			SetPhase(ETaskPhase::ThroughDoor);
		}
		return;

	case ETaskPhase::ThroughDoor:
		if (HasArrived())
		{
			SetPhase(ETaskPhase::Idle);
		}
		return;
	}
}

FText ADCCrewCharacter::GetActivityText() const
{
	switch (Phase)
	{
	case ETaskPhase::Idle: return FText::GetEmpty();
	case ETaskPhase::Walking: return LOCTEXT("Walking", "geht");
	case ETaskPhase::ToDoor: return LOCTEXT("ToDoor", "geht zur Tür");
	case ETaskPhase::AtDoor:
		if (TaskAction == EDCDoorAction::Close)
		{
			return LOCTEXT("Closing", "schließt die Tür");
		}
		return TaskDoor.IsValid() && TaskDoor->IsOpen() ? LOCTEXT("Passing", "geht durch die Tür") : LOCTEXT("Opening", "öffnet die Tür");
	case ETaskPhase::WaitingForDoor: return LOCTEXT("OpeningWait", "öffnet die Tür");
	case ETaskPhase::ThroughDoor: return LOCTEXT("Through", "geht durch die Tür");
	}
	return FText::GetEmpty();
}

void ADCCrewCharacter::UpdateLabels()
{
	ActivityLabel->SetText(GetActivityText());

	// Keep the labels facing the player's camera.
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager)
	{
		const FVector Camera = Player->PlayerCameraManager->GetCameraLocation();
		NameLabel->SetWorldRotation((Camera - NameLabel->GetComponentLocation()).Rotation());
		ActivityLabel->SetWorldRotation((Camera - ActivityLabel->GetComponentLocation()).Rotation());
	}
}

#undef LOCTEXT_NAMESPACE
