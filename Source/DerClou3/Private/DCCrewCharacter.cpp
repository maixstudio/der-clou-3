#include "DCCrewCharacter.h"

#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
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

	// AI controller walks the character along nav mesh paths.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0, 720.0, 0.0);
	Movement->MaxWalkSpeed = BaseWalkSpeed;

	Inventory = CreateDefaultSubobject<UDCInventoryComponent>(TEXT("Inventory"));

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
	FString SelfTest;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), SelfTest) && SelfTest == TEXT("CloseFromSide"))
	{
		GetWorldTimerManager().SetTimer(SelfTestTimer, this, &ADCCrewCharacter::TickCloseFromSideTest, 0.25f, true, 1.5f);
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
	if (!Navigation->ProjectPointToNavigation(Location, Projected, FVector(60.0, 60.0, 300.0)))
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
	if (Action == EDCDoorAction::PickLock && !Inventory->HasTool(DCItems::LockpicksId))
	{
		ShowNotice(LOCTEXT("NeedLockpicks", "Ohne Dietrich-Set geht das nicht"));
		SetPhase(ETaskPhase::Idle);
		return;
	}
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
		if (PhaseTime < (TaskAction == EDCDoorAction::PickLock ? Door->GetPickLockSeconds() : HandOnDoorSeconds))
		{
			return;
		}
		switch (TaskAction)
		{
		case EDCDoorAction::Open:
			if (!Door->OpenAwayFrom(GetActorLocation()))
			{
				ShowNotice(LOCTEXT("Locked", "Tür ist verschlossen"));
			}
			SetPhase(ETaskPhase::Idle);
			return;
		case EDCDoorAction::Close:
			Door->Close();
			SetPhase(ETaskPhase::Idle);
			return;
		case EDCDoorAction::GoThrough:
			if (!Door->OpenAwayFrom(GetActorLocation()))
			{
				ShowNotice(LOCTEXT("Locked", "Tür ist verschlossen"));
				SetPhase(ETaskPhase::Idle);
				return;
			}
			SetPhase(ETaskPhase::WaitingForDoor);
			return;
		case EDCDoorAction::PickLock:
			Door->Unlock();
			ShowNotice(LOCTEXT("Unlocked", "Schloss ist offen"));
			SetPhase(ETaskPhase::Idle);
			return;
		}
		return;

	case ETaskPhase::WaitingForDoor:
		if (!Door || PhaseTime > GiveUpSeconds)
		{
			UE_LOG(LogDerClou, Warning, TEXT("%s: no path through the door to %s"), *GetName(), *PassTarget.ToString());
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
	FVector Away = (GetActorLocation() - ItemLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -GetActorForwardVector();
	}
	if (StartMove(ItemLocation + Away * (Pickup->GetFootprintRadius() + ItemReach), &DoorGoal))
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
	FText Reason;
	switch (Action)
	{
	case EDCItemAction::Open:
		if (Item->IsLocked())
		{
			return FText::Format(LOCTEXT("IsLocked", "{0} ist versperrt"), Item->GetDisplayName());
		}
		Item->SetOpen(true);
		return FText::GetEmpty();

	case EDCItemAction::Close:
		Item->SetOpen(false);
		return FText::GetEmpty();

	case EDCItemAction::PickLock:
		if (!Item->IsLocked())
		{
			return FText::Format(LOCTEXT("NotLocked", "{0} ist nicht versperrt"), Item->GetDisplayName());
		}
		if (!Inventory->HasTool(DCItems::LockpicksId))
		{
			return LOCTEXT("NeedLockpicks", "Ohne Dietrich-Set geht das nicht");
		}
		StartItemWork(Item, Action, Def.LockPickSeconds);
		return FText::GetEmpty();

	case EDCItemAction::Force:
		if (!Item->IsLocked())
		{
			return FText::Format(LOCTEXT("NotLocked", "{0} ist nicht versperrt"), Item->GetDisplayName());
		}
		if (!Def.bForceable)
		{
			return FText::Format(LOCTEXT("NotForceable", "{0} lässt sich nicht aufbrechen – nur knacken"), Item->GetDisplayName());
		}
		if (!Inventory->HasTool(DCItems::CrowbarId))
		{
			return LOCTEXT("NeedCrowbar", "Ohne Brecheisen geht das nicht");
		}
		StartItemWork(Item, Action, DCItems::ForceSeconds);
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

void ADCCrewCharacter::StartItemWork(UDCItem* Item, EDCItemAction Action, float Seconds)
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
	TaskWorkSeconds = Seconds;
	SetPhase(ETaskPhase::ItemWork);
}

void ADCCrewCharacter::FinishItemWork()
{
	UDCItem* Item = TaskItem.Get();
	TaskItem.Reset();
	if (Item && Item->IsLocked())
	{
		if (TaskItemAction == EDCItemAction::Force)
		{
			Item->BreakLock();
			ShowNotice(FText::Format(LOCTEXT("Forced", "{0} aufgebrochen"), Item->GetDisplayName()));
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
}

FText ADCCrewCharacter::GetActivityText() const
{
	switch (Phase)
	{
	case ETaskPhase::Idle: return FText::GetEmpty();
	case ETaskPhase::Walking: return LOCTEXT("Walking", "geht");
	case ETaskPhase::ToDoor: return LOCTEXT("ToDoor", "geht zur Tür");
	case ETaskPhase::AtDoor:
		if (TaskAction == EDCDoorAction::PickLock)
		{
			const float Total = TaskDoor.IsValid() ? TaskDoor->GetPickLockSeconds() : 0.f;
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
	ActivityLabel->SetText(Activity.IsEmpty() && NoticeTime > 0.f ? Notice : Activity);

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
