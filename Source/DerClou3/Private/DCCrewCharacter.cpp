#include "DCCrewCharacter.h"

#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCCrewAnim.h"
#include "DCHeistState.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCNoise.h"
#include "DCPlanRunner.h"
#include "DCVisuals.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
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
	constexpr float NoticeSeconds = 2.5f;
	/** A move that ends further than this from its goal was stopped by something in the way. */
	constexpr double ArrivedTolerance = 60.0;
	/** Time it takes to pick something up. */
	constexpr float PickUpSeconds = 0.6f;
	/** Gap between the crew member and an item they walk up to. */
	constexpr double ItemReach = 40.0;

	/** The walkable spot near Location on its own storey (see DCNav::ProjectOnStorey - never the floor below). */
	bool CrewProjectOnStorey(const UNavigationSystemV1* Navigation, const FVector& Location, double Radius, FNavLocation& Out)
	{
		FVector Found;
		if (!Navigation || !DCNav::ProjectOnStorey(Navigation->GetWorld(), Location, Radius, Found))
		{
			return false;
		}
		Out = FNavLocation(Found);
		return true;
	}
	/** One footstep noise per this many seconds while walking; lock-picking clicks likewise. */
	constexpr float StepSeconds = 0.55f;
	constexpr float ClickSeconds = 1.2f;

	UStaticMeshComponent* MakeCarried(ACharacter* Owner, const TCHAR* Name)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Owner->GetCapsuleComponent());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetVisibility(false);
		return Part;
	}
	/** Name above the head (relative to the capsule centre); the activity line sits below it along the camera's up axis. */
	constexpr double LabelHeight = 120.0;
	constexpr double ActivityOffset = 30.0;

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
	// Vic: a locksmith with a strong back; safes are not his thing.
	Skills.Locks = 6;
	Skills.Safes = 1;
	Skills.Electronics = 1;
	Skills.Explosives = 0;
	Skills.Driving = 5;
	Skills.Fighting = 4;
	Skills.Lookout = 3;
	Skills.Strength = 6;

	// AI controller walks the character along nav mesh paths.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0, 720.0, 0.0);
	Movement->MaxWalkSpeed = BaseWalkSpeed;

	Inventory = CreateDefaultSubobject<UDCInventoryComponent>(TEXT("Inventory"));
	PlanRunner = CreateDefaultSubobject<UDCPlanRunnerComponent>(TEXT("PlanRunner"));

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

	CarriedFront = MakeCarried(this, TEXT("CarriedFront"));
	CarriedRight = MakeCarried(this, TEXT("CarriedRight"));
	CarriedLeft = MakeCarried(this, TEXT("CarriedLeft"));
	CarriedBack = MakeCarried(this, TEXT("CarriedBack"));
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
	ApplyModel();
}

void ADCCrewCharacter::ApplyModel()
{
	FDCCrewModel Model;
	bHasModel = DCVisuals::CrewModel(CrewName.ToString(), Model);
	USkeletalMeshComponent* Figure = GetMesh();
	if (bHasModel && Figure)
	{
		// Origin between the feet, front along +Y: down to the capsule's bottom, turned to face the actor's +X.
		const double HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		Figure->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -HalfHeight), FRotator(0.0, -90.0, 0.0));
		Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Figure->SetCanEverAffectNavigation(false);
		if (Figure->GetSkeletalMeshAsset() != Model.Mesh)
		{
			Figure->SetSkeletalMeshAsset(Model.Mesh);
		}
		if (Figure->GetAnimClass() != UDCCrewAnimInstance::StaticClass())
		{
			Figure->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Figure->SetAnimInstanceClass(UDCCrewAnimInstance::StaticClass());
		}
#if WITH_EDITOR
		Figure->SetUpdateAnimationInEditor(true);
#endif
	}
	Body->SetVisibility(!bHasModel);
	Head->SetVisibility(!bHasModel);
	Nose->SetVisibility(!bHasModel);
}

EDCCrewAnimPose ADCCrewCharacter::GetAnimPose() const
{
	if (bTestAnimPose)
	{
		return TestAnimPose;
	}
	switch (Phase)
	{
	case ETaskPhase::ItemWork:
		return EDCCrewAnimPose::Work;
	case ETaskPhase::AtDoor:
		return TaskAction == EDCDoorAction::PickLock ? EDCCrewAnimPose::Work : EDCCrewAnimPose::Default;
	case ETaskPhase::PickingUp:
		return EDCCrewAnimPose::PickUp;
	default:
		break;
	}
	// Searching a container while the plan plays: opening, rummaging, taking things out.
	return PlanRunner && PlanRunner->IsWorkingAtContainer() ? EDCCrewAnimPose::Work : EDCCrewAnimPose::Default;
}

TArray<FName> ADCCrewCharacter::GetItemIdOptions() const
{
	TArray<FName> Ids;
	for (const FDCItemDef& Def : DCItems::GetAll())
	{
		Ids.Add(Def.Id);
	}
	return Ids;
}

void ADCCrewCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Levels saved before the model existed: the construction script does not run again on load.
	ApplyModel();
	Inventory->SetStrength(Skills.Strength);
	Inventory->OnChanged.AddUObject(this, &ADCCrewCharacter::OnInventoryChanged);
	for (const FName Id : StartItems)
	{
		FText Reason;
		UDCItem* Item = UDCItem::Create(Inventory, Id);
		if (Item && !Inventory->AddAnywhere(Item, Reason))
		{
			UE_LOG(LogDerClou, Warning, TEXT("%s: start item %s does not fit: %s"), *GetName(), *Id.ToString(), *Reason.ToString());
		}
	}
	OnInventoryChanged();
	if (bDebugWalkThroughDoorOnStart)
	{
		// Give the AI controller and the nav mesh a moment to come up.
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &ADCCrewCharacter::StartDebugWalkThrough, 1.5f, false);
	}

	// Command-line self-test: -DCSelfTest=CloseFromSide (walk through, step behind the open
	// panel, close the door from there, then quit).
	// Only the first crew member runs it.
	FString SelfTest;
	TActorIterator<ADCCrewCharacter> First(GetWorld());
	FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), SelfTest);
	if (First && *First == this && SelfTest == TEXT("CloseFromSide"))
	{
		GetWorldTimerManager().SetTimer(SelfTestTimer, this, &ADCCrewCharacter::TickCloseFromSideTest, 0.25f, true, 1.5f);
	}
	// -DCSelfTest=CrewLook: close-ups of Rosa's model and animations.
	if (SelfTest == TEXT("CrewLook") && CrewName.ToString() == TEXT("Rosa"))
	{
		DCCrewLook::StartSelfTest(this);
	}
}

ADCDoor* ADCCrewCharacter::FindNearestDoor() const
{
	ADCDoor* Nearest = nullptr;
	for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
	{
		if (!Nearest || FVector::DistSquared(It->GetCenter(), GetActorLocation()) < FVector::DistSquared(Nearest->GetCenter(), GetActorLocation()))
		{
			Nearest = *It;
		}
	}
	return Nearest;
}

void ADCCrewCharacter::TickCloseFromSideTest()
{
	ADCDoor* Door = FindNearestDoor();
	SelfTestTime += 0.25f;
	const auto Finish = [this, Door](bool bPassed)
	{
		UE_LOG(LogDerClou, Display, TEXT("%s: self-test CloseFromSide %s (door open: %d, at %s)"), *GetName(),
			bPassed ? TEXT("PASSED") : TEXT("FAILED"), Door && Door->IsOpen() ? 1 : 0, *GetActorLocation().ToCompactString());
		GetWorldTimerManager().ClearTimer(SelfTestTimer);
		FPlatformMisc::RequestExit(false, TEXT("DCSelfTest"));
	};
	if (!Door || SelfTestTime > 40.f)
	{
		Finish(false);
		return;
	}
	if (Phase != ETaskPhase::Idle)
	{
		return;
	}
	const double Side = FVector::DotProduct(GetActorLocation() - Door->GetCenter(), Door->GetActorForwardVector()) >= 0.0 ? 1.0 : -1.0;
	switch (SelfTestStep++)
	{
	case 0:
		SelfTestStartSide = Side;
		StartDebugWalkThrough();
		return;
	case 1:
	{
		if (Side == SelfTestStartSide)
		{
			UE_LOG(LogDerClou, Warning, TEXT("%s: self-test - did not get through the door"), *GetName());
			Finish(false);
			return;
		}
		// Now on the side the panel swung to: stand on the hinge side, behind the open panel.
		const FVector Behind = Door->GetActorTransform().TransformPosition(FVector(Side * 60.0, -60.0, 0.0));
		UE_LOG(LogDerClou, Log, TEXT("%s: self-test - step behind the open panel to %s"), *GetName(), *Behind.ToCompactString());
		MoveToLocation(Behind);
		return;
	}
	case 2:
		InteractWithDoor(Door, EDCDoorAction::Close);
		return;
	default:
		Finish(!Door->IsOpen());
		return;
	}
}

void ADCCrewCharacter::StartDebugWalkThrough()
{
	ADCDoor* Nearest = FindNearestDoor();
	if (!Nearest)
	{
		return;
	}
	UE_LOG(LogDerClou, Log, TEXT("%s: self-test - walk through %s (locked: %d)"), *GetName(), *Nearest->GetName(), Nearest->IsLocked() ? 1 : 0);
	if (Nearest->IsLocked())
	{
		InteractWithDoor(Nearest, EDCDoorAction::PickLock);
		QueueDoorAction(Nearest, EDCDoorAction::GoThrough);
	}
	else
	{
		InteractWithDoor(Nearest, EDCDoorAction::GoThrough);
	}
}

void ADCCrewCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTask(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
	UpdateLabels(DeltaSeconds);
}

void ADCCrewCharacter::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	SelectionRing->SetVisibility(bSelected);
}

bool ADCCrewCharacter::StartMove(const FVector& Location, FVector* OutGoal)
{
	AAIController* AI = Cast<AAIController>(GetController());
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!AI || !Navigation)
	{
		return false;
	}
	FNavLocation Projected;
	if (!CrewProjectOnStorey(Navigation, Location, 60.0, Projected))
	{
		return false;
	}
	// No partial paths: a target behind a closed door is "not reachable", not "as close as possible".
	const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Projected.Location, 10.f, true, true, false, true, nullptr, false);
	if (OutGoal)
	{
		*OutGoal = Projected.Location;
	}
	return Result != EPathFollowingRequestResult::Failed;
}

bool ADCCrewCharacter::HasArrived() const
{
	const AAIController* AI = Cast<AAIController>(GetController());
	return !AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle;
}

void ADCCrewCharacter::SetPhase(ETaskPhase InPhase)
{
	const bool bDoorTaskEnded = InPhase == ETaskPhase::Idle && Phase != ETaskPhase::Idle && Phase != ETaskPhase::Walking;
	if (InPhase != Phase)
	{
		UE_LOG(LogDerClou, Log, TEXT("%s: phase %d -> %d at %s"), *GetName(), static_cast<int32>(Phase), static_cast<int32>(InPhase), *GetActorLocation().ToCompactString());
	}
	Phase = InPhase;
	PhaseTime = 0.f;

	// Continue with the next queued door action.
	if (bDoorTaskEnded && !DoorQueue.IsEmpty())
	{
		const FQueuedDoorAction Next = DoorQueue[0];
		DoorQueue.RemoveAt(0);
		if (Next.Door.IsValid())
		{
			StartDoorTask(Next.Door.Get(), Next.Action);
		}
	}
}

bool ADCCrewCharacter::MoveToLocation(const FVector& Location)
{
	TaskDoor.Reset();
	TaskPickup.Reset();
	TaskItem.Reset();
	DoorQueue.Reset();
	const bool bStarted = StartMove(Location);
	SetPhase(bStarted ? ETaskPhase::Walking : ETaskPhase::Idle);
	return bStarted;
}

void ADCCrewCharacter::InteractWithDoor(ADCDoor* Door, EDCDoorAction Action)
{
	DoorQueue.Reset();
	TaskPickup.Reset();
	TaskItem.Reset();
	StartDoorTask(Door, Action);
}

void ADCCrewCharacter::QueueDoorAction(ADCDoor* Door, EDCDoorAction Action)
{
	if (Phase == ETaskPhase::Idle || Phase == ETaskPhase::Walking)
	{
		StartDoorTask(Door, Action);
		return;
	}
	DoorQueue.Add({ Door, Action });
}

void ADCCrewCharacter::StartDoorTask(ADCDoor* Door, EDCDoorAction Action)
{
	if (!Door)
	{
		return;
	}
	const FDCTaskCheck Check = CheckDoorAction(Door, Action);
	if (!Check.bPossible)
	{
		ShowNotice(Check.Reason);
		SetPhase(ETaskPhase::Idle);
		return;
	}
	TaskWorkSeconds = Action == EDCDoorAction::PickLock ? Check.Seconds : HandOnDoorSeconds;
	TaskFailChance = Check.FailChance;
	TaskNoiseFactor = Check.NoiseFactor;
	TaskDoor = Door;
	TaskAction = Action;
	const FVector Here = GetActorLocation();
	PassTarget = Door->GetStandPoint(Here, true);
	const bool bClosing = Action == EDCDoorAction::Close && Door->IsOpen();
	const FVector StandPoint = bClosing ? Door->GetCloseStandPoint(Here) : Door->GetStandPoint(Here, false);
	// Closing: if there is no room beside the panel, reach through from the other side.
	if (StartMove(StandPoint, &DoorGoal) || (bClosing && StartMove(Door->GetClearSideStandPoint(), &DoorGoal)))
	{
		SetPhase(ETaskPhase::ToDoor);
		return;
	}
	ShowNotice(LOCTEXT("CannotReachDoor", "kommt nicht zur Tür"));
	SetPhase(ETaskPhase::Idle);
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
			if (FVector::Dist2D(GetActorLocation(), DoorGoal) > ArrivedTolerance)
			{
				UE_LOG(LogDerClou, Warning, TEXT("%s: stopped short of the door at %s (goal %s)"), *GetName(), *GetActorLocation().ToCompactString(), *DoorGoal.ToCompactString());
				ShowNotice(LOCTEXT("CannotReachDoor", "kommt nicht zur Tür"));
				SetPhase(ETaskPhase::Idle);
				return;
			}
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
		if (PhaseTime < TaskWorkSeconds)
		{
			// Picking a lock clicks quietly now and then.
			if (TaskAction == EDCDoorAction::PickLock && (ClickTimer += DeltaSeconds) >= ClickSeconds)
			{
				ClickTimer = 0.f;
				EmitNoise(DCItems::Noise::PickClick * TaskNoiseFactor, LOCTEXT("NoiseClick", "Klicken am Schloss"), false);
			}
			return;
		}
		switch (TaskAction)
		{
		case EDCDoorAction::Open:
			if (!Door->OpenAwayFrom(GetActorLocation()))
			{
				ShowNotice(LOCTEXT("Locked", "Tür ist verschlossen"));
			}
			else
			{
				UDCNoiseSubsystem::Get(this)->Emit(Door->GetCenter(), DCItems::Noise::DoorOpen, LOCTEXT("NoiseDoorOpen", "Tür knarrt"), this);
			}
			SetPhase(ETaskPhase::Idle);
			return;
		case EDCDoorAction::Close:
			if (Door->IsOpen())
			{
				UDCNoiseSubsystem::Get(this)->Emit(Door->GetCenter(), DCItems::Noise::DoorClose, LOCTEXT("NoiseDoorClose", "Tür fällt zu"), this);
			}
			Door->Close();
			SetPhase(ETaskPhase::Idle);
			return;
		case EDCDoorAction::GoThrough:
			if (!Door->IsOpen() && !Door->IsLocked())
			{
				UDCNoiseSubsystem::Get(this)->Emit(Door->GetCenter(), DCItems::Noise::DoorOpen, LOCTEXT("NoiseDoorOpen", "Tür knarrt"), this);
			}
			if (!Door->OpenAwayFrom(GetActorLocation()))
			{
				ShowNotice(LOCTEXT("Locked", "Tür ist verschlossen"));
				SetPhase(ETaskPhase::Idle);
				return;
			}
			SetPhase(ETaskPhase::WaitingForDoor);
			return;
		case EDCDoorAction::PickLock:
			if (!RollSuccess())
			{
				ShowNotice(LOCTEXT("PickFailed", "abgerutscht – Schloss bleibt zu"));
				DoorQueue.Reset();
				SetPhase(ETaskPhase::Idle);
				return;
			}
			Door->Unlock();
			ShowNotice(LOCTEXT("Unlocked", "Schloss ist offen"));
			SetPhase(ETaskPhase::Idle);
			return;
		}
		return;

	case ETaskPhase::WaitingForDoor:
		if (!Door || PhaseTime > GiveUpSeconds)
		{
			FVector OnNav;
			const bool bOnNav = DCNav::ProjectOnStorey(GetWorld(), PassTarget, 60.0, OnNav);
			UE_LOG(LogDerClou, Warning, TEXT("%s: no path through the door to %s (%s, door open amount %.2f)"), *GetName(), *PassTarget.ToString(),
				bOnNav ? *FString::Printf(TEXT("nav mesh at %s"), *OnNav.ToCompactString()) : TEXT("no nav mesh there"), Door ? Door->GetOpenAmount() : -1.f);
			ShowNotice(LOCTEXT("NoWay", "kommt nicht durch"));
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

	case ETaskPhase::ToItem:
		if (!TaskPickup.IsValid())
		{
			SetPhase(ETaskPhase::Idle);
			return;
		}
		if (HasArrived())
		{
			if (FVector::Dist2D(GetActorLocation(), DoorGoal) > ArrivedTolerance)
			{
				ShowNotice(LOCTEXT("CannotReachItem", "kommt nicht hin"));
				SetPhase(ETaskPhase::Idle);
				return;
			}
			const FVector ToItem = TaskPickup->GetActorLocation() - GetActorLocation();
			SetActorRotation(FRotator(0.0, ToItem.Rotation().Yaw, 0.0));
			if (bSearchTask)
			{
				ADCItemPickup* Container = TaskPickup.Get();
				TaskPickup.Reset();
				SetPhase(ETaskPhase::Idle);
				OnContainerReached.Broadcast(Container);
				return;
			}
			SetPhase(ETaskPhase::PickingUp);
		}
		return;

	case ETaskPhase::ItemWork:
		if (!TaskItem.IsValid())
		{
			SetPhase(ETaskPhase::Idle);
			return;
		}
		if (PhaseTime >= TaskWorkSeconds)
		{
			FinishItemWork();
			return;
		}
		if (TaskItemAction == EDCItemAction::PickLock && (ClickTimer += DeltaSeconds) >= ClickSeconds)
		{
			ClickTimer = 0.f;
			EmitNoise(DCItems::Noise::PickClick * TaskNoiseFactor, LOCTEXT("NoiseClick", "Klicken am Schloss"), false);
		}
		return;

	case ETaskPhase::PickingUp:
		if (PhaseTime >= PickUpSeconds)
		{
			FinishPickUp();
		}
		return;
	}
}

void ADCCrewCharacter::PickUp(ADCItemPickup* Pickup)
{
	UDCItem* Item = Pickup ? Pickup->GetItem() : nullptr;
	if (!Item)
	{
		return;
	}
	// Do not walk all the way just to find out it does not fit.
	const FText Reason = Inventory->CheckAdd(Item);
	if (!Reason.IsEmpty())
	{
		ShowNotice(Reason);
		return;
	}
	WalkToPickup(Pickup, false);
}

void ADCCrewCharacter::Search(ADCItemPickup* Pickup)
{
	if (Pickup && Pickup->GetItem())
	{
		WalkToPickup(Pickup, true);
	}
}

void ADCCrewCharacter::WalkToPickup(ADCItemPickup* Pickup, bool bSearch)
{
	DoorQueue.Reset();
	TaskDoor.Reset();
	TaskItem.Reset();
	TaskPickup = Pickup;
	bSearchTask = bSearch;
	const FVector ItemLocation = Pickup->GetActorLocation();
	const double Reach = Pickup->GetFootprintRadius() + ItemReach;
	// In the item's room on its storey (as the planning assumes). Someone else already works there or is on the way
	// (two at the safe): a free place beside them.
	FVector Spot;
	DCNav::FindStandSpot(GetWorld(), ItemLocation, Reach, GetActorLocation(), Spot, [this](const FVector& Candidate)
	{
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			const bool bHeadingThere = It->Phase == ETaskPhase::ToItem || It->Phase == ETaskPhase::ToDoor;
			if (*It != this && (FVector::Dist2D(It->GetActorLocation(), Candidate) < 75.0 || (bHeadingThere && FVector::Dist2D(It->DoorGoal, Candidate) < 75.0)))
			{
				return true;
			}
		}
		return false;
	});
	UE_LOG(LogDerClou, Verbose, TEXT("%s: to %s, item at %s, spot %s"), *GetName(), *Pickup->GetName(), *ItemLocation.ToCompactString(), *Spot.ToCompactString());
	if (StartMove(Spot, &DoorGoal))
	{
		SetPhase(ETaskPhase::ToItem);
		return;
	}
	ShowNotice(LOCTEXT("CannotReachItem", "kommt nicht hin"));
	SetPhase(ETaskPhase::Idle);
}

void ADCCrewCharacter::FinishPickUp()
{
	ADCItemPickup* Pickup = TaskPickup.Get();
	TaskPickup.Reset();
	UDCItem* Item = Pickup ? Pickup->GetItem() : nullptr;
	FText Reason;
	if (Item && Inventory->AddAnywhere(Item, Reason))
	{
		// The item now belongs to the inventory; the pickup actor goes away.
		Pickup->Destroy();
		ShowNotice(FText::Format(LOCTEXT("PickedUp", "{0} genommen"), Item->GetDisplayName()));
		UE_LOG(LogDerClou, Log, TEXT("%s: picked up %s"), *GetName(), *Item->GetDefId().ToString());
	}
	else if (Item)
	{
		ShowNotice(Reason);
		UE_LOG(LogDerClou, Log, TEXT("%s: cannot pick up %s: %s"), *GetName(), *Item->GetDefId().ToString(), *Reason.ToString());
	}
	SetPhase(ETaskPhase::Idle);
}

void ADCCrewCharacter::DropItem(UDCItem* Item)
{
	if (!Item || !Inventory->Remove(Item))
	{
		return;
	}
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FTransform Transform(FRotator(0.0, GetActorRotation().Yaw, 0.0), Feet + GetActorForwardVector() * 50.0);
	ADCItemPickup* Pickup = GetWorld()->SpawnActorDeferred<ADCItemPickup>(ADCItemPickup::StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return;
	}
	Pickup->SetItem(Item);
	Pickup->FinishSpawning(Transform);
	const float Thud = Item->GetTotalMass() >= 1.f ? FMath::Min(DCItems::Noise::DropMax, Item->GetTotalMass() * DCItems::Noise::DropPerKg) : 0.f;
	if (Thud > 0.f)
	{
		UDCNoiseSubsystem::Get(this)->Emit(Feet, Thud, FText::Format(LOCTEXT("NoiseDrop", "{0} poltert auf den Boden"), Item->GetDisplayName()), this);
	}
	ShowNotice(FText::Format(LOCTEXT("Dropped", "{0} abgelegt"), Item->GetDisplayName()));
	UE_LOG(LogDerClou, Log, TEXT("%s: dropped %s"), *GetName(), *Item->GetDefId().ToString());
}

const UDCItem* ADCCrewCharacter::GetWorkItem() const
{
	return Phase == ETaskPhase::ItemWork ? TaskItem.Get() : nullptr;
}

FText ADCCrewCharacter::PerformItemAction(UDCItem* Item, EDCItemAction Action, ADCItemPickup* Source)
{
	if (!Item)
	{
		return FText::GetEmpty();
	}
	const FDCItemDef& Def = Item->GetDef();
	// Doors, lids and locks of things inside a closed container cannot be reached.
	if (Action != EDCItemAction::Take && Action != EDCItemAction::Drop && Item->GetParent() && !Item->GetParent()->IsReachable())
	{
		return Item->CheckCanTakeOut();
	}
	const FDCTaskCheck Check = CheckItemAction(Item, Action);
	if (!Check.bPossible)
	{
		return Check.Reason;
	}
	FText Reason;
	switch (Action)
	{
	case EDCItemAction::Open:
		Item->SetOpen(true);
		return FText::GetEmpty();

	case EDCItemAction::Close:
		Item->SetOpen(false);
		return FText::GetEmpty();

	case EDCItemAction::PickLock:
	case EDCItemAction::Force:
	case EDCItemAction::Smash:
		StartItemWork(Item, Action, Check);
		return FText::GetEmpty();

	case EDCItemAction::Take:
		if (!Inventory->AddAnywhere(Item, Reason))
		{
			return Reason;
		}
		if (Source && Source->GetItem() == Item)
		{
			Source->Destroy();
		}
		ShowNotice(FText::Format(LOCTEXT("PickedUp", "{0} genommen"), Item->GetDisplayName()));
		UE_LOG(LogDerClou, Log, TEXT("%s: took %s"), *GetName(), *Item->GetDefId().ToString());
		return FText::GetEmpty();

	case EDCItemAction::Drop:
		DropItem(Item);
		return FText::GetEmpty();

	}
	return FText::GetEmpty();
}

namespace
{
	/** Putting something down for a task and picking it up again afterwards. */
	constexpr float SetAsideSeconds = 1.f;

	/** The task takes a little longer when something has to be put down first. */
	void AddSetAside(FDCTaskCheck& Check, const FText& Names, int32 Count)
	{
		if (!Check.bPossible || Count <= 0)
		{
			return;
		}
		if (Check.Seconds > 0.f)
		{
			Check.Seconds += SetAsideSeconds * Count;
		}
		const FText Note = FText::Format(LOCTEXT("SetAsideNote", "legt {0} kurz ab"), Names);
		Check.SkillNote = Check.SkillNote.IsEmpty() ? Note : FText::Format(LOCTEXT("SetAsideJoin", "{0} · {1}"), Check.SkillNote, Note);
	}
}

FText ADCCrewCharacter::CheckToolAndHands(FName Tool, int32 Hands, const FText& What, const UDCInventoryComponent* With,
	FText* OutSetAside, int32* OutSetAsideCount) const
{
	const UDCInventoryComponent* Carried = With ? With : Inventory.Get();
	if (!Tool.IsNone() && !Carried->FindReadyTool(Tool))
	{
		const FDCItemDef* ToolDef = DCItems::Find(Tool);
		const FText ToolName = ToolDef ? ToolDef->Name : FText::FromName(Tool);
		return Carried->HasTool(Tool)
			? FText::Format(LOCTEXT("ToolNotReady", "{0} steckt zu tief im Gepäck – erst in die Hand, Kleidung oder Weste"), ToolName)
			: FText::Format(LOCTEXT("ToolMissing", "Zum {0} braucht es: {1}"), What, ToolName);
	}
	const int32 Free = Carried->CountFreeHands(Tool);
	if (Free < Hands && OutSetAside && OutSetAsideCount)
	{
		// Something held in one hand can be put down for a moment (not a large thing that needs both).
		TArray<FString> Names;
		int32 Count = 0;
		for (const EDCEquipSlot Hand : { EDCEquipSlot::RightHand, EDCEquipSlot::LeftHand })
		{
			const UDCItem* Held = Carried->GetSlotItem(Hand);
			if (Free + Count < Hands && Held && Held->GetDefId() != Tool && Held->GetDef().Size != EDCItemSize::Large)
			{
				Names.Add(Held->GetDisplayName().ToString());
				++Count;
			}
		}
		if (Free + Count >= Hands)
		{
			*OutSetAside = FText::FromString(FString::Join(Names, TEXT(" und ")));
			*OutSetAsideCount = Count;
			return FText::GetEmpty();
		}
	}
	if (Free < Hands)
	{
		return Hands >= 2
			? FText::Format(LOCTEXT("NeedBothHandsFree", "Zum {0} braucht es beide Hände frei"), What)
			: FText::Format(LOCTEXT("NeedHandFree", "Zum {0} braucht es eine freie Hand"), What);
	}
	return FText::GetEmpty();
}

FDCTaskCheck ADCCrewCharacter::CheckItemAction(const UDCItem* Item, EDCItemAction Action) const
{
	FDCTaskCheck Check;
	const auto Refuse = [&Check](const FText& Reason)
	{
		Check.bPossible = false;
		Check.Reason = Reason;
		return Check;
	};
	if (!Item)
	{
		return Refuse(FText::GetEmpty());
	}
	const FDCItemDef& Def = Item->GetDef();
	// First what the item's state allows, then what this crew member can do.
	switch (Action)
	{
	case EDCItemAction::Open:
		if (Item->IsLocked())
		{
			return Refuse(FText::Format(LOCTEXT("IsLocked", "{0} ist versperrt"), Item->GetDisplayName()));
		}
		break;
	case EDCItemAction::PickLock:
	case EDCItemAction::Force:
		if (!Item->IsLocked())
		{
			return Refuse(FText::Format(LOCTEXT("NotLocked", "{0} ist nicht versperrt"), Item->GetDisplayName()));
		}
		break;
	case EDCItemAction::Smash:
		if (Item->IsDestroyed())
		{
			return Refuse(FText::Format(LOCTEXT("AlreadySmashed", "{0} ist schon eingeschlagen"), Item->GetDisplayName()));
		}
		break;
	case EDCItemAction::Take:
	{
		const FText Reason = Inventory->CheckAdd(Item);
		return Reason.IsEmpty() ? Check : Refuse(Reason);
	}
	default:
		break;
	}
	return PlanCheckItem(Def, Action);
}

FDCTaskCheck ADCCrewCharacter::PlanCheckItem(const FDCItemDef& Def, EDCItemAction Action, const UDCInventoryComponent* With) const
{
	FDCTaskCheck Check;
	const auto Refuse = [&Check](const FText& Reason)
	{
		Check.bPossible = false;
		Check.Reason = Reason;
		return Check;
	};
	FText Reason;
	FText SetAside;
	int32 SetAsideCount = 0;
	FDCTaskCheck Result;
	switch (Action)
	{
	case EDCItemAction::Open:
		Reason = CheckToolAndHands(NAME_None, 1, LOCTEXT("WhatOpen", "Öffnen"), With, &SetAside, &SetAsideCount);
		return Reason.IsEmpty() ? Check : Refuse(Reason);

	case EDCItemAction::Close:
		Reason = CheckToolAndHands(NAME_None, 1, LOCTEXT("WhatClose", "Schließen"), With, &SetAside, &SetAsideCount);
		return Reason.IsEmpty() ? Check : Refuse(Reason);

	case EDCItemAction::PickLock:
		if (!Def.HasLock())
		{
			return Refuse(FText::Format(LOCTEXT("NoLock", "{0} hat kein Schloss"), Def.Name));
		}
		Reason = CheckToolAndHands(Def.LockTool, 2, LOCTEXT("WhatPick", "Knacken"), With, &SetAside, &SetAsideCount);
		if (!Reason.IsEmpty())
		{
			return Refuse(Reason);
		}
		Result = DCSkills::Evaluate(Def.LockSkill, Skills.Get(Def.LockSkill), Def.LockDifficulty, Def.LockPickSeconds, CrewName);
		AddSetAside(Result, SetAside, SetAsideCount);
		return Result;

	case EDCItemAction::Force:
		if (!Def.bForceable)
		{
			return Refuse(FText::Format(LOCTEXT("NotForceable", "{0} lässt sich nicht aufbrechen – nur knacken"), Def.Name));
		}
		Reason = CheckToolAndHands(DCItems::CrowbarId, 2, LOCTEXT("WhatForce", "Aufbrechen"), With, &SetAside, &SetAsideCount);
		if (!Reason.IsEmpty())
		{
			return Refuse(Reason);
		}
		Result = DCSkills::Evaluate(EDCSkill::Strength, Skills.Strength, Def.ForceDifficulty, DCItems::ForceSeconds, CrewName);
		AddSetAside(Result, SetAside, SetAsideCount);
		return Result;

	case EDCItemAction::Smash:
		if (!Def.bBreakable)
		{
			return Refuse(FText::Format(LOCTEXT("NotBreakable", "{0} lässt sich nicht einschlagen"), Def.Name));
		}
		Reason = CheckToolAndHands(DCItems::CrowbarId, 1, LOCTEXT("WhatSmash", "Einschlagen"), With, &SetAside, &SetAsideCount);
		if (!Reason.IsEmpty())
		{
			return Refuse(Reason);
		}
		Result = DCSkills::Evaluate(EDCSkill::Strength, Skills.Strength, DCItems::SmashDifficulty, DCItems::SmashSeconds, CrewName);
		AddSetAside(Result, SetAside, SetAsideCount);
		return Result;

	default:
		return Check;
	}
}

FDCTaskCheck ADCCrewCharacter::CheckDoorAction(const ADCDoor* Door, EDCDoorAction Action) const
{
	return PlanCheckDoor(Door, Action, nullptr);
}

FDCTaskCheck ADCCrewCharacter::PlanCheckDoor(const ADCDoor* Door, EDCDoorAction Action, const UDCInventoryComponent* With) const
{
	FDCTaskCheck Check;
	if (!Door)
	{
		Check.bPossible = false;
		return Check;
	}
	FText SetAside;
	int32 SetAsideCount = 0;
	const FText Reason = Action == EDCDoorAction::PickLock
		? CheckToolAndHands(DCItems::LockpicksId, 2, LOCTEXT("WhatPick", "Knacken"), With, &SetAside, &SetAsideCount)
		: CheckToolAndHands(NAME_None, 1, LOCTEXT("WhatDoor", "Türöffnen"), With, &SetAside, &SetAsideCount);
	if (!Reason.IsEmpty())
	{
		Check.bPossible = false;
		Check.Reason = Reason;
		return Check;
	}
	if (Action == EDCDoorAction::PickLock)
	{
		FDCTaskCheck Result = DCSkills::Evaluate(EDCSkill::Locks, Skills.Locks, Door->GetLockDifficulty(), Door->GetPickLockSeconds(), CrewName);
		AddSetAside(Result, SetAside, SetAsideCount);
		return Result;
	}
	return Check;
}

bool ADCCrewCharacter::RollSuccess() const
{
	if (bTestFailTasks)
	{
		return false;
	}
	return TaskFailChance <= 0.f || FMath::FRand() >= TaskFailChance;
}

void ADCCrewCharacter::StartItemWork(UDCItem* Item, EDCItemAction Action, const FDCTaskCheck& Check)
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	DoorQueue.Reset();
	TaskDoor.Reset();
	TaskPickup.Reset();
	TaskItem = Item;
	TaskItemAction = Action;
	TaskWorkSeconds = Check.Seconds;
	TaskFailChance = Check.FailChance;
	TaskNoiseFactor = Check.NoiseFactor;
	ClickTimer = 0.f;
	SetPhase(ETaskPhase::ItemWork);
}

void ADCCrewCharacter::FinishItemWork()
{
	UDCItem* Item = TaskItem.Get();
	TaskItem.Reset();
	if (Item && !RollSuccess())
	{
		// Failed: the time is gone, the lock stays; breaking things is loud either way.
		ShowNotice(FText::Format(LOCTEXT("WorkFailed", "nicht geschafft – {0} bleibt zu"), Item->GetDisplayName()));
		if (TaskItemAction == EDCItemAction::Force)
		{
			EmitNoise(DCItems::Noise::Force * FMath::Max(1.f, TaskNoiseFactor), FText::Format(LOCTEXT("NoiseForceFail", "Holz kracht ({0})"), Item->GetDisplayName()));
		}
		UE_LOG(LogDerClou, Log, TEXT("%s: failed at %s"), *GetName(), *Item->GetDefId().ToString());
		SetPhase(ETaskPhase::Idle);
		return;
	}
	if (Item && TaskItemAction == EDCItemAction::Smash)
	{
		Item->Smash();
		ShowNotice(FText::Format(LOCTEXT("Smashed", "{0} eingeschlagen"), Item->GetDisplayName()));
		// Breaking glass or wood is loud however skilled one is; clumsy hands make it worse.
		EmitNoise(DCItems::Noise::Smash * FMath::Max(1.f, TaskNoiseFactor), FText::Format(LOCTEXT("NoiseSmash", "Glas splittert ({0})"), Item->GetDisplayName()));
		UE_LOG(LogDerClou, Log, TEXT("%s: smashed %s"), *GetName(), *Item->GetDefId().ToString());
	}
	else if (Item && Item->IsLocked())
	{
		if (TaskItemAction == EDCItemAction::Force)
		{
			Item->BreakLock();
			ShowNotice(FText::Format(LOCTEXT("Forced", "{0} aufgebrochen"), Item->GetDisplayName()));
			EmitNoise(DCItems::Noise::Force * FMath::Max(1.f, TaskNoiseFactor), FText::Format(LOCTEXT("NoiseForce", "Holz kracht ({0} aufgebrochen)"), Item->GetDisplayName()));
		}
		else
		{
			Item->SetLocked(false);
			ShowNotice(FText::Format(LOCTEXT("Picked", "{0}: Schloss offen"), Item->GetDisplayName()));
		}
		UE_LOG(LogDerClou, Log, TEXT("%s: %s %s"), *GetName(), TaskItemAction == EDCItemAction::Force ? TEXT("forced") : TEXT("picked"), *Item->GetDefId().ToString());
	}
	SetPhase(ETaskPhase::Idle);
}

void ADCCrewCharacter::OnInventoryChanged()
{
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * Inventory->GetSpeedFactor();
	UpdateCarriedLook();
}

void ADCCrewCharacter::EmitNoise(float Range, const FText& What, bool bLabel)
{
	if (UDCNoiseSubsystem* Noise = UDCNoiseSubsystem::Get(this))
	{
		const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Noise->Emit(Feet, Range, What, this, bLabel);
	}
}

void ADCCrewCharacter::UpdateFootsteps(float DeltaSeconds)
{
	if (GetVelocity().Size2D() < 50.f)
	{
		// The first step after standing still is heard right away.
		StepTimer = StepSeconds;
		return;
	}
	StepTimer += DeltaSeconds;
	if (StepTimer < StepSeconds)
	{
		return;
	}
	StepTimer = 0.f;
	// Heavier load, louder steps.
	const float Load = FMath::Clamp(Inventory->GetTotalMass() / Inventory->GetMaxCarryMass(), 0.f, 1.f);
	const float Range = DCItems::Noise::Footstep + DCItems::Noise::FootstepLoad * Load + (Inventory->HasBothHandsFull() ? DCItems::Noise::FootstepBothHands : 0.f);
	EmitNoise(Range, LOCTEXT("NoiseSteps", "Schritte"), false);
}

void ADCCrewCharacter::UpdateCarriedLook()
{
	// Copies of what is carried, so one can tell from above who has the painting.
	const auto Show = [](UStaticMeshComponent* Part, const UDCItem* Item, const FVector& Location, double Yaw)
	{
		if (!Item)
		{
			Part->SetVisibility(false);
			return;
		}
		const FDCItemDef& Def = Item->GetDef();
		// Location is the middle of the item; models have their origin at the bottom.
		const bool bModel = DCVisuals::SetItemLook(Part, Def);
		Part->SetRelativeLocationAndRotation(Location - FVector(0.0, 0.0, bModel ? Def.ShapeSize.Z * 0.5 : 0.0), FRotator(0.0, Yaw, 0.0));
		Part->SetVisibility(true);
	};
	const UDCItem* Right = Inventory->GetSlotItem(EDCEquipSlot::RightHand);
	const UDCItem* Left = Inventory->GetSlotItem(EDCEquipSlot::LeftHand);
	const bool bBothHands = Inventory->HasBothHandsFull();
	Show(CarriedFront, bBothHands ? Right : nullptr, FVector(45.0, 0.0, -5.0), 0.0);
	Show(CarriedRight, bBothHands ? nullptr : Right, FVector(20.0, 32.0, -25.0), 0.0);
	Show(CarriedLeft, bBothHands ? nullptr : Left, FVector(20.0, -32.0, -25.0), 0.0);
	Show(CarriedBack, Inventory->GetSlotItem(EDCEquipSlot::Back), FVector(-30.0, 0.0, 10.0), 90.0);
}

FText ADCCrewCharacter::GetActivityText() const
{
	switch (Phase)
	{
	// Between steps of the plan: what the crew member waits for.
	case ETaskPhase::Idle: return PlanRunner ? PlanRunner->GetWaitText() : FText::GetEmpty();
	case ETaskPhase::Walking: return LOCTEXT("Walking", "geht");
	case ETaskPhase::ToDoor: return LOCTEXT("ToDoor", "geht zur Tür");
	case ETaskPhase::AtDoor:
		if (TaskAction == EDCDoorAction::PickLock)
		{
			const float Total = TaskWorkSeconds;
			const int32 Remaining = FMath::Max(1, FMath::CeilToInt(Total - PhaseTime));
			return FText::Format(LOCTEXT("Picking", "knackt das Schloss … {0} s"), FText::AsNumber(Remaining));
		}
		if (TaskAction == EDCDoorAction::Close)
		{
			return LOCTEXT("Closing", "schließt die Tür");
		}
		return TaskDoor.IsValid() && TaskDoor->IsOpen() ? LOCTEXT("Passing", "geht durch die Tür") : LOCTEXT("Opening", "öffnet die Tür");
	case ETaskPhase::WaitingForDoor: return LOCTEXT("OpeningWait", "öffnet die Tür");
	case ETaskPhase::ThroughDoor: return LOCTEXT("Through", "geht durch die Tür");
	case ETaskPhase::ToItem: return LOCTEXT("ToItem", "geht hin");
	case ETaskPhase::PickingUp: return LOCTEXT("PickingUp", "hebt auf");
	case ETaskPhase::ItemWork:
	{
		const int32 Remaining = FMath::Max(1, FMath::CeilToInt(TaskWorkSeconds - PhaseTime));
		const FText Name = TaskItem.IsValid() ? TaskItem->GetDisplayName() : FText::GetEmpty();
		return TaskItemAction == EDCItemAction::Force
			? FText::Format(LOCTEXT("Forcing", "bricht {0} auf ... {1} s"), Name, FText::AsNumber(Remaining))
			: FText::Format(LOCTEXT("PickingItem", "knackt {0} ... {1} s"), Name, FText::AsNumber(Remaining));
	}
	}
	return FText::GetEmpty();
}

void ADCCrewCharacter::ShowNotice(const FText& Text)
{
	Notice = Text;
	NoticeTime = NoticeSeconds;
}

void ADCCrewCharacter::UpdateLabels(float DeltaSeconds)
{
	NoticeTime = FMath::Max(0.f, NoticeTime - DeltaSeconds);
	const FText Activity = GetActivityText();
	ActivityLabel->SetText(DCVisuals::PlainText(Activity.IsEmpty() && NoticeTime > 0.f ? Notice : Activity));

	// Face the player's camera; stack the two lines along the camera's up axis so they never
	// overlap, however steep the camera looks down.
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager)
	{
		// Billboard: parallel to the camera plane (facing back along the view, upright on screen).
		const FQuat CameraRotation = Player->PlayerCameraManager->GetCameraRotation().Quaternion();
		const FVector Up = CameraRotation.GetUpVector();
		const FRotator Facing = FRotationMatrix::MakeFromXZ(-CameraRotation.GetForwardVector(), Up).Rotator();
		const FVector NameLocation = GetActorLocation() + FVector(0.0, 0.0, LabelHeight);
		NameLabel->SetWorldLocationAndRotation(NameLocation, Facing);
		ActivityLabel->SetWorldLocationAndRotation(NameLocation - Up * ActivityOffset, Facing);
	}
}

#undef LOCTEXT_NAMESPACE
