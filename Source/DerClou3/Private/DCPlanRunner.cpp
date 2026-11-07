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

#define LOCTEXT_NAMESPACE "DCPlanRunner"

namespace
{
	/** One handful while looting (open something or take something). */
	constexpr float RunnerLootStepSeconds = 0.8f;
	/** Waiting for a door: stand this far beside the spot in front of it, so whoever opens it has room. */
	constexpr double WaitAside = 110.0;

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
