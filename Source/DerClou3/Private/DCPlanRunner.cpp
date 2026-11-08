#include "DCPlanRunner.h"

#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCHeistState.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCPlayerController.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "DCPlanRunner"

namespace
{
	/** One handful while looting (open something or take something). */
	constexpr float RunnerLootStepSeconds = 0.8f;
	/** Waiting for a door: stand this far beside the spot in front of it, so whoever opens it has room. */
	constexpr double WaitAside = 110.0;
	/** All doors on the way open, yet no way: the nav mesh catches up within this long. */
	constexpr float RunnerNavSettleSeconds = 1.5f;
	/** Opening the same door on the way more often than this: something is in the way. */
	constexpr int32 RunnerMaxPassTries = 3;

	void Collect(UDCItem* Item, TArray<UDCItem*>& Out)
	{
		Out.Add(Item);
		for (UDCItem* Child : Item->GetContents())
		{
			Collect(Child, Out);
		}
	}
}

UDCPlanRunnerComponent::UDCPlanRunnerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

ADCCrewCharacter* UDCPlanRunnerComponent::GetCrew() const
{
	return Cast<ADCCrewCharacter>(GetOwner());
}

void UDCPlanRunnerComponent::Start(const TArray<FDCPlanTask>& InTasks)
{
	Tasks = InTasks;
	Statuses.Init(EDCTaskStatus::Planned, Tasks.Num());
	Index = 0;
	Step = EStep::Approach;
	bStepStarted = false;
	StepTime = 0.f;
	Tries = 0;
	WaitText = FText::GetEmpty();
	LootSkip.Reset();
	PassingDoor.Reset();
	LastPassDoor.Reset();
	PassTries = 0;
	DoorSettleTime = 0.f;
	bRunning = !Tasks.IsEmpty();
}

void UDCPlanRunnerComponent::Stop()
{
	bRunning = false;
	WaitText = FText::GetEmpty();
}

EDCTaskStatus UDCPlanRunnerComponent::GetStatus(int32 TaskIndex) const
{
	return Statuses.IsValidIndex(TaskIndex) ? Statuses[TaskIndex] : EDCTaskStatus::Planned;
}

int32 UDCPlanRunnerComponent::FindTask(const FGuid& Id) const
{
	return Tasks.IndexOfByPredicate([&Id](const FDCPlanTask& Task) { return Task.Id == Id; });
}

ADCItemPickup* UDCPlanRunnerComponent::GetPickup() const
{
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	return State ? State->FindPickup(Current().Target) : nullptr;
}

UDCItem* UDCPlanRunnerComponent::GetItem() const
{
	return UDCHeistStateSubsystem::ResolveItem(GetPickup(), Current().Type == EDCPlanTaskType::Item ? Current().ItemPath : FString());
}

void UDCPlanRunnerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ADCCrewCharacter* Crew = GetCrew();
	if (!bRunning || !Crew)
	{
		return;
	}
	if (!Tasks.IsValidIndex(Index))
	{
		Stop();
		return;
	}
	StepTime += DeltaTime;

	switch (Step)
	{
	case EStep::Approach:
		if (!bStepStarted)
		{
			// Going through a door on the way first: wait until that is done, then look again.
			if (PassingDoor.IsValid())
			{
				if (Crew->IsBusy())
				{
					return;
				}
				PassingDoor.Reset();
			}
			// A closed door on the way is opened in passing; a locked one means waiting here until
			// someone unlocks it (as the plan expects).
			FText DoorWait;
			ADCDoor* OpenOnWay = nullptr;
			ADCDoor* WaitAtDoor = nullptr;
			bool bSettling = false;
			bool bDoorWait = DoorOnWay(DoorWait, OpenOnWay, WaitAtDoor, bSettling);
			if (bDoorWait && bSettling)
			{
				// Every door on the way is open: give the nav mesh a moment, then walk anyway.
				DoorSettleTime += DeltaTime;
				if (DoorSettleTime < RunnerNavSettleSeconds)
				{
					WaitText = DoorWait;
					Statuses[Index] = EDCTaskStatus::Waiting;
					return;
				}
				bDoorWait = false;
			}
			else
			{
				DoorSettleTime = 0.f;
			}
			if (!bDoorWait && OpenOnWay)
			{
				PassTries = LastPassDoor.Get() == OpenOnWay ? PassTries + 1 : 1;
				LastPassDoor = OpenOnWay;
				if (PassTries > RunnerMaxPassTries)
				{
					UE_LOG(LogDerClou, Log, TEXT("%s does not get through %s"), *Crew->GetCrewName().ToString(), *OpenOnWay->GetInteractableName().ToString());
					Crew->Say(LOCTEXT("CannotPass", "kommt nicht durch"));
					FinishTask(EDCTaskStatus::Failed);
					return;
				}
				bWentToDoorWait = false;
				PassingDoor = OpenOnWay;
				WaitText = FText::GetEmpty();
				Statuses[Index] = EDCTaskStatus::Running;
				UE_LOG(LogDerClou, Log, TEXT("%s opens %s on the way"), *Crew->GetCrewName().ToString(), *OpenOnWay->GetInteractableName().ToString());
				Crew->InteractWithDoor(OpenOnWay, EDCDoorAction::GoThrough);
				return;
			}
			if (bDoorWait)
			{
				// Trying things out directly: nobody else will open it, so say so at once.
				const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
				if (Plans && Plans->GetMode() == EDCPlanMode::Direct)
				{
					Crew->Say(WaitAtDoor && WaitAtDoor->IsLocked() ? FText::Format(LOCTEXT("DoorIsLocked", "{0} ist verschlossen"), WaitAtDoor->GetInteractableName()) : DoorWait);
					FinishTask(EDCTaskStatus::Failed);
					return;
				}
				// Waiting in vain: whoever should unlock it gave up, or nobody else is at work any more.
				if (WaitAtDoor && WaitAtDoor->IsLocked() && IsWaitInVain())
				{
					const FText Stuck = FText::Format(LOCTEXT("StuckAtDoor", "{0} kommt nicht weiter: {1} bleibt verschlossen"), Crew->GetCrewName(), WaitAtDoor->GetInteractableName());
					UE_LOG(LogDerClou, Log, TEXT("%s"), *Stuck.ToString());
					Crew->Say(LOCTEXT("SayStuck", "kommt nicht weiter"));
					if (ADCPlayerController* Player = Cast<ADCPlayerController>(GetWorld()->GetFirstPlayerController()))
					{
						Player->ShowToast(Stuck, true);
					}
					FinishTask(EDCTaskStatus::Failed);
					Stop();
					return;
				}
				// Walk up to the door and wait beside it (whoever unlocks it needs the spot in front).
				if (WaitAtDoor && !bWentToDoorWait && !Crew->IsBusy())
				{
					bWentToDoorWait = true;
					const FVector Near = WaitAtDoor->GetStandPoint(Crew->GetActorLocation(), false) + WaitAtDoor->GetActorRightVector().GetSafeNormal2D() * WaitAside;
					if (FVector::Dist2D(Crew->GetActorLocation(), Near) > 60.0)
					{
						Crew->MoveToLocation(Near);
					}
				}
				if (!DoorWait.EqualTo(WaitText))
				{
					UE_LOG(LogDerClou, Log, TEXT("%s %s"), *Crew->GetCrewName().ToString(), *DoorWait.ToString());
				}
				WaitText = DoorWait;
				Statuses[Index] = EDCTaskStatus::Waiting;
				return;
			}
			WaitText = FText::GetEmpty();
			bWentToDoorWait = false;
			bStepStarted = true;
			Statuses[Index] = EDCTaskStatus::Running;
			StartApproach();
			return;
		}
		if (!Crew->IsBusy())
		{
			Step = EStep::Wait;
			bStepStarted = false;
		}
		return;

	case EStep::Wait:
	{
		if (AlreadyDone())
		{
			FinishTask(EDCTaskStatus::Done);
			return;
		}
		FText Wait;
		if (!ConditionMet(Wait))
		{
			if (!Wait.EqualTo(WaitText))
			{
				UE_LOG(LogDerClou, Log, TEXT("%s %s"), *Crew->GetCrewName().ToString(), *Wait.ToString());
			}
			WaitText = Wait;
			Statuses[Index] = EDCTaskStatus::Waiting;
			return;
		}
		WaitText = FText::GetEmpty();
		Step = EStep::Execute;
		bStepStarted = false;
		return;
	}

	case EStep::Execute:
		if (!bStepStarted)
		{
			bStepStarted = true;
			StepTime = 0.f;
			Statuses[Index] = EDCTaskStatus::Running;
			StartExecute();
			return;
		}
		switch (Current().Type)
		{
		case EDCPlanTaskType::Wait:
			if (StepTime >= Current().Seconds)
			{
				FinishTask(EDCTaskStatus::Done);
			}
			return;
		case EDCPlanTaskType::Loot:
			if (Crew->IsBusy())
			{
				return;
			}
			switch (LootPhase)
			{
			case ELootPhase::Opening:
				if (StepTime < RunnerLootStepSeconds)
				{
					return;
				}
				StepTime = 0.f;
				if (!LootOpenStep())
				{
					LootPhase = ELootPhase::Searching;
					Crew->Say(LOCTEXT("Searching", "durchsucht"));
				}
				return;
			case ELootPhase::Searching:
			{
				ADCItemPickup* Pickup = GetPickup();
				if (StepTime < DCItems::SearchSeconds(CountSearchCells(Pickup ? Pickup->GetItem() : nullptr)))
				{
					return;
				}
				const TArray<UDCItem*> Finds = CollectFinds();
				if (Finds.IsEmpty() || !Pickup)
				{
					Crew->Say(LOCTEXT("NothingFound", "nichts gefunden"));
					UE_LOG(LogDerClou, Display, TEXT("%s searched %s: nothing found"), *Crew->GetCrewName().ToString(), *Current().Target.ToString());
					FinishTask(EDCTaskStatus::Done);
					return;
				}
				// The player decides; the game stands still meanwhile.
				LootPhase = ELootPhase::Deciding;
				ADCPlayerController* Player = Cast<ADCPlayerController>(GetWorld()->GetFirstPlayerController());
				if (Player)
				{
					Player->RequestFindDecision(Crew, Pickup);
				}
				else
				{
					ResolveFind(0.f);
				}
				return;
			}
			case ELootPhase::Deciding:
				return;
			case ELootPhase::Handling:
				if (StepTime >= HoldSeconds)
				{
					FinishTask(EDCTaskStatus::Done);
				}
				return;
			}
			return;
		default:
			if (!Crew->IsBusy())
			{
				EvaluateResult();
			}
			return;
		}
	}
}

void UDCPlanRunnerComponent::StartApproach()
{
	ADCCrewCharacter* Crew = GetCrew();
	const FDCPlanTask& Task = Current();
	switch (Task.Type)
	{
	case EDCPlanTaskType::Door:
	{
		const ADCDoor* Door = UDCHeistStateSubsystem::Get(this)->FindDoor(Task.Target);
		if (!Door)
		{
			FinishTask(EDCTaskStatus::Failed);
			return;
		}
		FVector Spot = Door->GetStandPoint(Crew->GetActorLocation(), false);
		FText Ignored;
		if (!ConditionMet(Ignored))
		{
			// Will have to wait: stand aside so the one who opens the door has room.
			Spot += Door->GetActorRightVector().GetSafeNormal2D() * WaitAside;
		}
		Crew->MoveToLocation(Spot);
		return;
	}
	case EDCPlanTaskType::Item:
	case EDCPlanTaskType::Loot:
		if (ADCItemPickup* Pickup = GetPickup())
		{
			Crew->Search(Pickup);
		}
		else
		{
			FinishTask(EDCTaskStatus::Failed);
		}
		return;
	default:
		// Move and pick up walk by themselves; waiting needs no walk.
		return;
	}
}

bool UDCPlanRunnerComponent::ConditionMet(FText& OutWait) const
{
	const FDCPlanTask& Task = Current();
	if (Task.Type == EDCPlanTaskType::WaitFor)
	{
		// Asks the other crew member's runner: is that step (or signal) over, or their whole plan?
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		const ADCCrewCharacter* Other = Heist ? Heist->FindCrew(Task.Target) : nullptr;
		const UDCPlanRunnerComponent* Runner = Other ? Other->GetPlanRunner() : nullptr;
		bool bDone = true;
		if (Runner && Task.Ref.IsValid())
		{
			const int32 RefStep = Runner->FindTask(Task.Ref);
			const EDCTaskStatus Status = Runner->GetStatus(RefStep);
			bDone = RefStep == INDEX_NONE || Status == EDCTaskStatus::Done || Status == EDCTaskStatus::Failed;
		}
		else if (Runner)
		{
			bDone = Runner->IsFinished();
		}
		if (!bDone)
		{
			OutWait = Heist->DescribeWaitFor(Task, true);
		}
		return bDone;
	}
	if (Task.Type == EDCPlanTaskType::Door)
	{
		const EDCDoorAction Action = static_cast<EDCDoorAction>(Task.Action);
		const ADCDoor* Door = UDCHeistStateSubsystem::Get(this)->FindDoor(Task.Target);
		if (Door && Door->IsLocked() && (Action == EDCDoorAction::Open || Action == EDCDoorAction::GoThrough))
		{
			OutWait = FText::Format(LOCTEXT("WaitDoor", "wartet: {0} verschlossen"), Door->GetInteractableName());
			return false;
		}
		return true;
	}
	if (Task.Type == EDCPlanTaskType::Item || Task.Type == EDCPlanTaskType::Loot)
	{
		const UDCItem* Item = GetItem();
		if (!Item)
		{
			return true;
		}
		if (Item->GetParent() && !Item->GetParent()->IsReachable())
		{
			OutWait = FText::Format(LOCTEXT("WaitInside", "wartet: {0}"), Item->CheckCanTakeOut());
			return false;
		}
		const EDCItemAction Action = static_cast<EDCItemAction>(Task.Action);
		const bool bNeedsUnlocked = Task.Type == EDCPlanTaskType::Loot || Action == EDCItemAction::Open || Action == EDCItemAction::Take;
		if (bNeedsUnlocked && Item->IsLocked())
		{
			OutWait = FText::Format(LOCTEXT("WaitLocked", "wartet: {0} versperrt"), Item->GetDisplayName());
			return false;
		}
	}
	return true;
}

bool UDCPlanRunnerComponent::IsWaitInVain() const
{
	const ADCCrewCharacter* Crew = GetCrew();
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Crew || !Heist)
	{
		return false;
	}
	// The plan says whose step unlocks it: did that step fail for good?
	if (const FDCCrewSchedule* Mine = Heist->GetSchedule().Find(Crew->GetFName()))
	{
		const FDCScheduledTask* Planned = Mine->Tasks.FindByPredicate([this](const FDCScheduledTask& Entry) { return Entry.Index == Index; });
		const ADCCrewCharacter* Other = Planned && !Planned->WaitFor.IsNone() ? Heist->FindCrew(Planned->WaitFor) : nullptr;
		if (Other && Other->GetPlanRunner()->GetStatus(Planned->WaitForIndex) == EDCTaskStatus::Failed)
		{
			return true;
		}
	}
	// Nobody else is still at work: nobody will come.
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != Crew && It->GetPlanRunner()->IsRunning())
		{
			return false;
		}
	}
	return true;
}

bool UDCPlanRunnerComponent::DoorOnWay(FText& OutWait, ADCDoor*& OutOpen, ADCDoor*& OutWaitAt, bool& bOutSettling) const
{
	OutOpen = nullptr;
	OutWaitAt = nullptr;
	bOutSettling = false;
	OutWait = FText::GetEmpty();
	const ADCCrewCharacter* Crew = GetCrew();
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Crew || !Heist)
	{
		return false;
	}
	const FDCPlanTask& Task = Current();
	FVector Destination;
	switch (Task.Type)
	{
	case EDCPlanTaskType::Move:
		Destination = Task.Location;
		break;
	case EDCPlanTaskType::Door:
	{
		const ADCDoor* Door = Heist->FindDoor(Task.Target);
		if (!Door)
		{
			return false;
		}
		Destination = Door->GetStandPoint(Crew->GetActorLocation(), false);
		break;
	}
	case EDCPlanTaskType::Item:
	case EDCPlanTaskType::Loot:
	case EDCPlanTaskType::PickUp:
	{
		const ADCItemPickup* Pickup = GetPickup();
		if (!Pickup)
		{
			return false;
		}
		Destination = Pickup->GetActorLocation();
		break;
	}
	default:
		return false;
	}
	// The doors on the way, in the order they are met: the first one that is not open decides.
	const TArray<ADCDoor*> InWay = Heist->FindDoorsOnWay(Crew->GetActorLocation(), Destination);
	if (InWay.IsEmpty())
	{
		return false;
	}
	ADCDoor* const* FirstShut = InWay.FindByPredicate([](const ADCDoor* Door) { return !Door->IsOpen(); });
	if (!FirstShut)
	{
		// All open (one may still be swinging): the nav mesh lets one through in a moment.
		bOutSettling = true;
		OutWait = FText::Format(LOCTEXT("WaitDoorOpening", "wartet, bis {0} offen ist"), InWay.Last()->GetInteractableName());
		return true;
	}
	ADCDoor* First = *FirstShut;
	if (First->IsLocked())
	{
		// The plan knows who unlocks it: "wartet auf Vic (Hintertür knacken)".
		const FDCCrewSchedule* Mine = Heist->GetSchedule().Find(Crew->GetFName());
		const FDCScheduledTask* Planned = Mine ? Mine->Tasks.FindByPredicate([this](const FDCScheduledTask& Entry) { return Entry.Index == Index; }) : nullptr;
		OutWait = Planned && !Planned->WaitFor.IsNone() && !Planned->WaitForLabel.IsEmpty()
			? FText::Format(LOCTEXT("WaitDoorForStep", "wartet auf {0} ({1})"), Planned->WaitForName, Planned->WaitForLabel)
			: FText::Format(LOCTEXT("WaitAtLockedDoor", "wartet an der {0}, bis sie aufgesperrt ist"), First->GetInteractableName());
		OutWaitAt = First;
		return true;
	}
	if (!Crew->CheckDoorAction(First, EDCDoorAction::Open).bPossible)
	{
		OutWait = FText::Format(LOCTEXT("WaitAtShutDoor", "wartet an der {0}, bis sie offen ist (keine Hand frei)"), First->GetInteractableName());
		OutWaitAt = First;
		return true;
	}
	OutOpen = First;
	return false;
}

bool UDCPlanRunnerComponent::AlreadyDone() const
{
	const FDCPlanTask& Task = Current();
	const ADCCrewCharacter* Crew = GetCrew();
	if (Task.Type == EDCPlanTaskType::Door)
	{
		const ADCDoor* Door = UDCHeistStateSubsystem::Get(this)->FindDoor(Task.Target);
		switch (static_cast<EDCDoorAction>(Task.Action))
		{
		case EDCDoorAction::PickLock: return Door && !Door->IsLocked();
		case EDCDoorAction::Open: return Door && Door->IsOpen();
		case EDCDoorAction::Close: return Door && !Door->IsOpen();
		case EDCDoorAction::GoThrough: return false;
		}
		return false;
	}
	if (Task.Type == EDCPlanTaskType::Item)
	{
		const UDCItem* Item = GetItem();
		if (!Item)
		{
			return false;
		}
		switch (static_cast<EDCItemAction>(Task.Action))
		{
		case EDCItemAction::PickLock:
		case EDCItemAction::Force: return !Item->IsLocked();
		case EDCItemAction::Smash: return Item->IsDestroyed();
		case EDCItemAction::Open: return Item->IsOpen();
		case EDCItemAction::Close: return !Item->GetDef().bOpenable || !Item->IsOpen();
		case EDCItemAction::Take: return Crew->GetInventory()->Owns(Item);
		case EDCItemAction::Drop: return false;
		}
	}
	return false;
}

void UDCPlanRunnerComponent::StartExecute()
{
	ADCCrewCharacter* Crew = GetCrew();
	const FDCPlanTask& Task = Current();
	switch (Task.Type)
	{
	case EDCPlanTaskType::Move:
		if (!Crew->MoveToLocation(Task.Location))
		{
			Crew->Say(LOCTEXT("CannotReach", "kommt nicht hin"));
			FinishTask(EDCTaskStatus::Failed);
		}
		return;
	case EDCPlanTaskType::Wait:
		return;
	case EDCPlanTaskType::Door:
	{
		ADCDoor* Door = UDCHeistStateSubsystem::Get(this)->FindDoor(Task.Target);
		const EDCDoorAction Action = static_cast<EDCDoorAction>(Task.Action);
		const FDCTaskCheck Check = Crew->CheckDoorAction(Door, Action);
		if (!Door || !Check.bPossible)
		{
			Crew->Say(Check.Reason);
			FinishTask(EDCTaskStatus::Failed);
			return;
		}
		Crew->InteractWithDoor(Door, Action);
		return;
	}
	case EDCPlanTaskType::Item:
	{
		ADCItemPickup* Pickup = GetPickup();
		UDCItem* Item = GetItem();
		if (!Item)
		{
			Crew->Say(LOCTEXT("Gone", "ist nicht mehr da"));
			FinishTask(EDCTaskStatus::Failed);
			return;
		}
		const FText Reason = Crew->PerformItemAction(Item, static_cast<EDCItemAction>(Task.Action), Pickup && Pickup->GetItem() == Item ? Pickup : nullptr);
		if (!Reason.IsEmpty())
		{
			Crew->Say(Reason);
			FinishTask(EDCTaskStatus::Failed);
		}
		return;
	}
	case EDCPlanTaskType::PickUp:
		if (ADCItemPickup* Pickup = GetPickup())
		{
			Crew->PickUp(Pickup);
		}
		return;
	case EDCPlanTaskType::Loot:
		// The first handful right away.
		StepTime = RunnerLootStepSeconds;
		LootSkip.Reset();
		LootPhase = ELootPhase::Opening;
		HoldSeconds = 0.f;
		return;
	case EDCPlanTaskType::WaitFor:
		// The waiting is over once the condition holds; nothing else to do.
		return;
	case EDCPlanTaskType::Signal:
		Crew->Say(FText::Format(LOCTEXT("GivesSignal", "gibt Signal {0}"), FText::AsNumber(Task.Action)));
		UE_LOG(LogDerClou, Log, TEXT("%s gives signal %d"), *Crew->GetCrewName().ToString(), Task.Action);
		return;
	case EDCPlanTaskType::Drop:
		if (UDCItem* Carried = Crew->GetInventory()->FindItem(FName(*Task.ItemPath)))
		{
			Crew->DropItem(Carried);
		}
		else
		{
			Crew->Say(LOCTEXT("NothingToDrop", "hat das nicht dabei"));
			FinishTask(EDCTaskStatus::Failed);
		}
		return;
	}
}

void UDCPlanRunnerComponent::EvaluateResult()
{
	const FDCPlanTask& Task = Current();
	bool bSucceeded = true;
	if (Task.Type == EDCPlanTaskType::Door && static_cast<EDCDoorAction>(Task.Action) == EDCDoorAction::PickLock)
	{
		const ADCDoor* Door = UDCHeistStateSubsystem::Get(this)->FindDoor(Task.Target);
		bSucceeded = Door && !Door->IsLocked();
	}
	else if (Task.Type == EDCPlanTaskType::Item)
	{
		const EDCItemAction Action = static_cast<EDCItemAction>(Task.Action);
		const UDCItem* Item = GetItem();
		if (Action == EDCItemAction::PickLock || Action == EDCItemAction::Force)
		{
			bSucceeded = Item && !Item->IsLocked();
		}
		else if (Action == EDCItemAction::Smash)
		{
			bSucceeded = Item && Item->IsDestroyed();
		}
	}
	else if (Task.Type == EDCPlanTaskType::PickUp)
	{
		bSucceeded = GetPickup() == nullptr;
	}

	if (bSucceeded)
	{
		FinishTask(EDCTaskStatus::Done);
		return;
	}
	// Locks: try again (the failure already cost time and maybe noise).
	const bool bRetry = (Task.Type == EDCPlanTaskType::Door || Task.Type == EDCPlanTaskType::Item) && ++Tries < MaxTries;
	if (bRetry)
	{
		UE_LOG(LogDerClou, Log, TEXT("%s tries again (%d)"), *GetCrew()->GetCrewName().ToString(), Tries + 1);
		bStepStarted = false;
		return;
	}
	GetCrew()->Say(LOCTEXT("GivesUp", "gibt auf"));
	FinishTask(EDCTaskStatus::Failed);
}

int32 UDCPlanRunnerComponent::CountSearchCells(const UDCItem* Root)
{
	if (!Root)
	{
		return 0;
	}
	int32 Cells = Root->GetDef().GetCellCount();
	for (const UDCItem* Part : Root->GetContents())
	{
		Cells += Part->IsAttached() ? Part->GetDef().GetCellCount() : 0;
	}
	return Cells;
}

bool UDCPlanRunnerComponent::LootOpenStep()
{
	ADCCrewCharacter* Crew = GetCrew();
	ADCItemPickup* Pickup = GetPickup();
	UDCItem* Root = Pickup ? Pickup->GetItem() : nullptr;
	if (!Root)
	{
		return false;
	}
	TArray<UDCItem*> All;
	Collect(Root, All);
	// Open whatever can be opened without a key: it may hide more.
	for (UDCItem* Item : All)
	{
		const bool bReachable = Item == Root || (Item->GetParent() && Item->GetParent()->IsReachable());
		if (!LootSkip.Contains(Item) && Item->GetDef().bOpenable && !Item->IsOpen() && !Item->IsLocked() && bReachable)
		{
			const FText Reason = Crew->PerformItemAction(Item, EDCItemAction::Open, Item == Root ? Pickup : nullptr);
			if (!Reason.IsEmpty())
			{
				LootSkip.Add(Item);
			}
			return true;
		}
	}
	return false;
}

TArray<UDCItem*> UDCPlanRunnerComponent::CollectFinds() const
{
	TArray<UDCItem*> Finds;
	ADCItemPickup* Pickup = GetPickup();
	UDCItem* Root = Pickup ? Pickup->GetItem() : nullptr;
	if (!Root)
	{
		return Finds;
	}
	TArray<UDCItem*> All;
	Collect(Root, All);
	for (UDCItem* Item : All)
	{
		if (Item != Root && !Item->IsAttached() && Item->GetDef().bMovable && Item->GetParent() && Item->GetParent()->IsReachable())
		{
			Finds.Add(Item);
		}
	}
	Finds.Sort([](const UDCItem& A, const UDCItem& B) { return A.GetDef().Value > B.GetDef().Value; });
	return Finds;
}

void UDCPlanRunnerComponent::ResolveFind(float HandlingSeconds)
{
	if (LootPhase != ELootPhase::Deciding)
	{
		return;
	}
	LootPhase = ELootPhase::Handling;
	HoldSeconds = HandlingSeconds;
	StepTime = 0.f;
}

void UDCPlanRunnerComponent::FinishTask(EDCTaskStatus Status)
{
	// Picked a door lock and stands still now: step aside, the next one needs the handle.
	ADCCrewCharacter* Crew = GetCrew();
	if (Status == EDCTaskStatus::Done && Crew && Tasks.IsValidIndex(Index) && Tasks[Index].Type == EDCPlanTaskType::Door
		&& static_cast<EDCDoorAction>(Tasks[Index].Action) == EDCDoorAction::PickLock)
	{
		const bool bStays = !Tasks.IsValidIndex(Index + 1) || Tasks[Index + 1].Type == EDCPlanTaskType::Wait || Tasks[Index + 1].Type == EDCPlanTaskType::WaitFor;
		const ADCDoor* Door = UDCHeistStateSubsystem::Get(this)->FindDoor(Tasks[Index].Target);
		if (bStays && Door)
		{
			Crew->MoveToLocation(Door->GetStandPoint(Crew->GetActorLocation(), false) - Door->GetActorRightVector().GetSafeNormal2D() * WaitAside);
		}
	}
	LastPassDoor.Reset();
	PassTries = 0;
	DoorSettleTime = 0.f;
	if (Statuses.IsValidIndex(Index))
	{
		Statuses[Index] = Status;
		UE_LOG(LogDerClou, Log, TEXT("%s: step %d %s"), *GetCrew()->GetCrewName().ToString(), Index + 1, Status == EDCTaskStatus::Done ? TEXT("done") : TEXT("failed"));
	}
	++Index;
	Step = EStep::Approach;
	bStepStarted = false;
	StepTime = 0.f;
	Tries = 0;
	WaitText = FText::GetEmpty();
	if (!Tasks.IsValidIndex(Index))
	{
		bRunning = false;
	}
}

#undef LOCTEXT_NAMESPACE
