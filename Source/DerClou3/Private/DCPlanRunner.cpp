#include "DCPlanRunner.h"

#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCHeistState.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DerClou3.h"

#define LOCTEXT_NAMESPACE "DCPlanRunner"

namespace
{
	/** One handful while looting (open something or take something). */
	constexpr float LootStepSeconds = 0.8f;
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
			if (Crew->IsBusy() || StepTime < LootStepSeconds)
			{
				return;
			}
			StepTime = 0.f;
			if (!LootStep())
			{
				FinishTask(EDCTaskStatus::Done);
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
		StepTime = LootStepSeconds;
		LootSkip.Reset();
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

bool UDCPlanRunnerComponent::LootStep()
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
	const auto CanReach = [Root](const UDCItem* Item) { return Item == Root || (Item->GetParent() && Item->GetParent()->IsReachable()); };

	// Open whatever can be opened without a key first: it may hide more.
	for (UDCItem* Item : All)
	{
		if (!LootSkip.Contains(Item) && Item->GetDef().bOpenable && !Item->IsOpen() && !Item->IsLocked() && CanReach(Item))
		{
			const FText Reason = Crew->PerformItemAction(Item, EDCItemAction::Open, Item == Root ? Pickup : nullptr);
			if (!Reason.IsEmpty())
			{
				LootSkip.Add(Item);
			}
			return true;
		}
	}
	// Then take the most valuable thing within reach.
	UDCItem* Best = nullptr;
	for (UDCItem* Item : All)
	{
		const FDCItemDef& Def = Item->GetDef();
		if (Item != Root && !Item->IsAttached() && Def.bMovable && Def.Value > 0 && CanReach(Item) && !LootSkip.Contains(Item)
			&& (!Best || Def.Value > Best->GetDef().Value))
		{
			Best = Item;
		}
	}
	if (!Best)
	{
		return false;
	}
	const FText Reason = Crew->PerformItemAction(Best, EDCItemAction::Take);
	if (!Reason.IsEmpty())
	{
		LootSkip.Add(Best);
		Crew->Say(Reason);
	}
	return true;
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
