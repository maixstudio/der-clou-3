#include "DCHeistState.h"

#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCPlanMarker.h"
#include "DCPlanRunner.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

#define LOCTEXT_NAMESPACE "DCHeistState"

namespace
{
	/** Rough times for the small things (the crew's own code uses the same values). */
	constexpr float HandSeconds = 0.5f;
	constexpr float SwingSeconds = 0.6f;
	constexpr float OpenSeconds = 0.4f;
	constexpr float TakeSeconds = 0.6f;
	constexpr float LootStepSeconds = 0.8f;
	constexpr double ItemReach = 40.0;
	/** Without a nav path (e.g. behind a closed door) the way is guessed as this much longer than straight. */
	constexpr double DetourFactor = 1.4;

	struct FSimEvent
	{
		float Time = 0.f;
		TFunction<void(FDCWorldSnapshot&)> Apply;
		/** Whose step brings it about (to tell the waiting ones whom they wait for). */
		int32 CrewIndex = INDEX_NONE;
		int32 TaskIndex = INDEX_NONE;
	};

	struct FCursor
	{
		ADCCrewCharacter* Crew = nullptr;
		const TArray<FDCPlanTask>* Tasks = nullptr;
		int32 CrewIndex = 0;
		int32 Next = 0;
		float Time = 0.f;
		FVector Location = FVector::ZeroVector;
		float Speed = 250.f;
		float BaseSpeed = 250.f;
		/** What the crew member carries at this point of the plan (a throwaway copy). */
		UDCInventoryComponent* Inventory = nullptr;
		bool bFinished = false;
		bool bBlocked = false;
		bool bHasPending = false;
		float BlockedSince = 0.f;
		FDCScheduledTask Pending;
	};

	FText ItemName(FName DefId)
	{
		const FDCItemDef* Def = DCItems::Find(DefId);
		return Def ? Def->Name : FText::FromName(DefId);
	}

	float WalkSeconds(UWorld* World, const FVector& From, const FVector& To, float Speed, TArray<FVector>* OutPath = nullptr)
	{
		const double Straight = FVector::Dist2D(From, To);
		if (OutPath)
		{
			*OutPath = { From, To };
		}
		if (Straight < 20.0)
		{
			return 0.f;
		}
		double Length = Straight * DetourFactor;
		if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To))
		{
			if (Path->IsValid() && !Path->IsPartial())
			{
				Length = Path->GetPathLength();
				if (OutPath && Path->PathPoints.Num() >= 2)
				{
					*OutPath = Path->PathPoints;
				}
			}
		}
		// A little extra for starting and stopping.
		return static_cast<float>(Length / FMath::Max(50.f, Speed)) + 0.3f;
	}

	/** A real item built from the simulation's state of it (with what is still inside), to carry in a planned inventory. */
	UDCItem* BuildSimItem(const FDCSimPickup& Pickup, int32 Index)
	{
		const FDCSimItem& Sim = Pickup.Items[Index];
		UDCItem* Item = UDCItem::CreateBare(Sim.DefId, Sim.bOpen, Sim.bLocked, Sim.bDestroyed, Sim.bAttached);
		if (!Item)
		{
			return nullptr;
		}
		for (const int32 Child : Sim.Children)
		{
			if (!Pickup.Items[Child].bTaken)
			{
				if (UDCItem* Built = BuildSimItem(Pickup, Child))
				{
					Item->AddContent(Built);
				}
			}
		}
		return Item;
	}

	/** A point along a polyline, Fraction 0..1 of its length. */
	FVector AlongPath(const TArray<FVector>& Path, float Fraction)
	{
		if (Path.Num() < 2)
		{
			return Path.IsEmpty() ? FVector::ZeroVector : Path[0];
		}
		double Total = 0.0;
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			Total += FVector::Dist2D(Path[Index - 1], Path[Index]);
		}
		double Left = Total * FMath::Clamp(Fraction, 0.f, 1.f);
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			const double Segment = FVector::Dist2D(Path[Index - 1], Path[Index]);
			if (Left <= Segment && Segment > 0.0)
			{
				return FMath::Lerp(Path[Index - 1], Path[Index], Left / Segment);
			}
			Left -= Segment;
		}
		return Path.Last();
	}

	/** "10,2" */
	FText TenthsText(float Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), Seconds).Replace(TEXT("."), TEXT(",")));
	}

	/** "Ring, Goldkette" */
	FText JoinNames(const TArray<FText>& Names)
	{
		TArray<FString> Parts;
		for (const FText& Name : Names)
		{
			Parts.Add(Name.ToString());
		}
		return FText::FromString(FString::Join(Parts, TEXT(", ")));
	}

	/** Marks an item and everything inside it as taken. */
	void TakeSubtree(FDCSimPickup& Pickup, int32 Index)
	{
		Pickup.Items[Index].bTaken = true;
		for (const int32 Child : Pickup.Items[Index].Children)
		{
			TakeSubtree(Pickup, Child);
		}
		if (Index == 0)
		{
			Pickup.bGone = true;
		}
	}

	/** The outermost closed (or taken) container around the item, or INDEX_NONE. */
	int32 FindBlocker(const FDCSimPickup& Pickup, int32 Index)
	{
		int32 Blocker = INDEX_NONE;
		for (int32 Current = Pickup.Items[Index].Parent; Current != INDEX_NONE; Current = Pickup.Items[Current].Parent)
		{
			if (!Pickup.Items[Current].IsOpen() || Pickup.Items[Current].bTaken)
			{
				Blocker = Current;
			}
		}
		return Blocker;
	}
}

//~ Snapshot -------------------------------------------------------------------------------------

FDCSimDoor* FDCWorldSnapshot::FindDoor(FName Actor)
{
	return Doors.FindByPredicate([Actor](const FDCSimDoor& Door) { return Door.Actor == Actor; });
}

FDCSimPickup* FDCWorldSnapshot::FindPickup(FName Actor)
{
	return Pickups.FindByPredicate([Actor](const FDCSimPickup& Pickup) { return Pickup.Actor == Actor; });
}

int32 FDCWorldSnapshot::FindItem(const FDCSimPickup& Pickup, const FString& Path)
{
	if (Pickup.Items.IsEmpty())
	{
		return INDEX_NONE;
	}
	int32 Current = 0;
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("/"), true);
	for (const FString& Segment : Segments)
	{
		int32 Found = INDEX_NONE;
		for (const int32 Child : Pickup.Items[Current].Children)
		{
			if (Pickup.Items[Child].DefId == FName(*Segment))
			{
				Found = Child;
				break;
			}
		}
		if (Found == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		Current = Found;
	}
	return Current;
}

bool FDCWorldSnapshot::IsReachable(const FDCSimPickup& Pickup, int32 Index)
{
	return !Pickup.bGone && FindBlocker(Pickup, Index) == INDEX_NONE && !Pickup.Items[Index].bTaken;
}

const FDCCrewSchedule* FDCSchedule::Find(FName Crew) const
{
	return Crews.FindByPredicate([Crew](const FDCCrewSchedule& Entry) { return Entry.Crew == Crew; });
}

//~ Subsystem --------------------------------------------------------------------------------------

UDCHeistStateSubsystem* UDCHeistStateSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDCHeistStateSubsystem>() : nullptr;
}

void UDCHeistStateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(&InWorld))
	{
		PlanChangedHandle = Plans->OnPlanChanged.AddUObject(this, &UDCHeistStateSubsystem::OnPlanChanged);
	}
}

void UDCHeistStateSubsystem::Deinitialize()
{
	if (UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(GetWorld()))
	{
		Plans->OnPlanChanged.Remove(PlanChangedHandle);
	}
	Super::Deinitialize();
}

void UDCHeistStateSubsystem::OnPlanChanged()
{
	// While the plan runs, the schedule from the start stays as it is.
	if (!IsPlaying())
	{
		Refresh();
	}
	else
	{
		UpdateMarkers();
	}
}

ADCDoor* UDCHeistStateSubsystem::FindDoor(FName Actor) const
{
	for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
	{
		if (It->GetFName() == Actor)
		{
			return *It;
		}
	}
	return nullptr;
}

ADCItemPickup* UDCHeistStateSubsystem::FindPickup(FName Actor) const
{
	for (TActorIterator<ADCItemPickup> It(GetWorld()); It; ++It)
	{
		if (It->GetFName() == Actor && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

ADCCrewCharacter* UDCHeistStateSubsystem::FindCrew(FName Actor) const
{
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFName() == Actor)
		{
			return *It;
		}
	}
	return nullptr;
}

UDCItem* UDCHeistStateSubsystem::ResolveItem(ADCItemPickup* Pickup, const FString& Path)
{
	UDCItem* Current = Pickup ? Pickup->GetItem() : nullptr;
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("/"), true);
	for (const FString& Segment : Segments)
	{
		UDCItem* Found = nullptr;
		for (UDCItem* Child : Current ? Current->GetContents() : TArray<TObjectPtr<UDCItem>>())
		{
			if (Child->GetDefId() == FName(*Segment))
			{
				Found = Child;
				break;
			}
		}
		Current = Found;
	}
	return Current;
}

FString UDCHeistStateSubsystem::PathOf(const UDCItem* Item)
{
	TArray<FString> Segments;
	for (const UDCItem* Current = Item; Current && Current->GetParent(); Current = Current->GetParent())
	{
		Segments.Insert(Current->GetDefId().ToString(), 0);
	}
	return FString::Join(Segments, TEXT("/"));
}

FDCWorldSnapshot UDCHeistStateSubsystem::TakeSnapshot() const
{
	FDCWorldSnapshot Snapshot;
	for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
	{
		FDCSimDoor& Door = Snapshot.Doors.AddDefaulted_GetRef();
		Door.Actor = It->GetFName();
		Door.bLocked = It->IsLocked();
		Door.bOpen = It->IsOpen();
	}
	for (TActorIterator<ADCItemPickup> It(GetWorld()); It; ++It)
	{
		UDCItem* Root = It->GetItem();
		if (!Root || It->IsActorBeingDestroyed())
		{
			continue;
		}
		FDCSimPickup& Pickup = Snapshot.Pickups.AddDefaulted_GetRef();
		Pickup.Actor = It->GetFName();
		// Flatten the item tree.
		TFunction<void(const UDCItem*, int32)> Add = [&Pickup, &Add](const UDCItem* Item, int32 Parent)
		{
			const int32 Index = Pickup.Items.Num();
			FDCSimItem& Sim = Pickup.Items.AddDefaulted_GetRef();
			const FDCItemDef& Def = Item->GetDef();
			Sim.DefId = Item->GetDefId();
			Sim.Parent = Parent;
			Sim.bOpenable = Def.bOpenable;
			Sim.bOpen = Item->IsOpen();
			Sim.bLocked = Item->IsLocked();
			Sim.bDestroyed = Item->IsDestroyed();
			Sim.bAttached = Item->IsAttached();
			Sim.bMovable = Def.bMovable;
			Sim.Value = Def.Value;
			if (Parent != INDEX_NONE)
			{
				Pickup.Items[Parent].Children.Add(Index);
			}
			for (const UDCItem* Child : Item->GetContents())
			{
				Add(Child, Index);
			}
		};
		Add(Root, INDEX_NONE);
	}
	return Snapshot;
}

FText UDCHeistStateSubsystem::DescribeTask(const FDCPlanTask& Task) const
{
	switch (Task.Type)
	{
	case EDCPlanTaskType::Move:
		return LOCTEXT("TaskMove", "gehen");
	case EDCPlanTaskType::Wait:
		return FText::Format(LOCTEXT("TaskWait", "warten {0} s"), FText::AsNumber(FMath::RoundToInt(Task.Seconds)));
	case EDCPlanTaskType::Door:
	{
		const ADCDoor* Door = FindDoor(Task.Target);
		const FText Name = Door ? Door->GetInteractableName() : FText::FromName(Task.Target);
		switch (static_cast<EDCDoorAction>(Task.Action))
		{
		case EDCDoorAction::PickLock: return FText::Format(LOCTEXT("DoorPick", "{0} knacken"), Name);
		case EDCDoorAction::Open: return FText::Format(LOCTEXT("DoorOpen", "{0} öffnen"), Name);
		case EDCDoorAction::Close: return FText::Format(LOCTEXT("DoorClose", "{0} schließen"), Name);
		case EDCDoorAction::GoThrough: return FText::Format(LOCTEXT("DoorThrough", "durch {0}"), Name);
		}
		return Name;
	}
	case EDCPlanTaskType::Drop:
		return FText::Format(LOCTEXT("TaskDrop", "{0} ablegen"), ItemName(FName(*Task.ItemPath)));
	case EDCPlanTaskType::Signal:
		return FText::Format(LOCTEXT("TaskSignal", "Signal {0} geben"), FText::AsNumber(Task.Action));
	case EDCPlanTaskType::WaitFor:
		return DescribeWaitFor(Task, false);
	case EDCPlanTaskType::Item:
	case EDCPlanTaskType::PickUp:
	case EDCPlanTaskType::Loot:
	{
		ADCItemPickup* Pickup = FindPickup(Task.Target);
		const UDCItem* Item = ResolveItem(Pickup, Task.Type == EDCPlanTaskType::Item ? Task.ItemPath : FString());
		FText Name = Item ? Item->GetDisplayName() : FText::FromName(Task.Target);
		if (!Item && !Task.ItemPath.IsEmpty())
		{
			FString Last;
			Task.ItemPath.Split(TEXT("/"), nullptr, &Last, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			Name = ItemName(FName(Last.IsEmpty() ? Task.ItemPath : Last));
		}
		if (Task.Type == EDCPlanTaskType::PickUp)
		{
			return FText::Format(LOCTEXT("PickUp", "{0} aufheben"), Name);
		}
		if (Task.Type == EDCPlanTaskType::Loot)
		{
			return FText::Format(LOCTEXT("Loot", "{0} ausräumen"), Name);
		}
		switch (static_cast<EDCItemAction>(Task.Action))
		{
		case EDCItemAction::PickLock: return FText::Format(LOCTEXT("ItemPick", "{0} knacken"), Name);
		case EDCItemAction::Force: return FText::Format(LOCTEXT("ItemForce", "{0} aufbrechen"), Name);
		case EDCItemAction::Smash: return FText::Format(LOCTEXT("ItemSmash", "{0} einschlagen"), Name);
		case EDCItemAction::Open: return FText::Format(LOCTEXT("ItemOpen", "{0} öffnen"), Name);
		case EDCItemAction::Close: return FText::Format(LOCTEXT("ItemClose", "{0} schließen"), Name);
		case EDCItemAction::Take: return FText::Format(LOCTEXT("ItemTake", "{0} nehmen"), Name);
		case EDCItemAction::Drop: return FText::Format(LOCTEXT("ItemDrop", "{0} ablegen"), Name);
		}
		return Name;
	}
	}
	return FText::GetEmpty();
}

FText UDCHeistStateSubsystem::DescribeWaitFor(const FDCPlanTask& Task, bool bWaiting) const
{
	const ADCCrewCharacter* Other = FindCrew(Task.Target);
	const FText OtherName = Other ? Other->GetCrewName() : FText::FromName(Task.Target);
	if (!Task.Ref.IsValid())
	{
		return FText::Format(bWaiting ? LOCTEXT("WaitingCrew", "wartet, bis {0} ganz fertig ist") : LOCTEXT("TaskWaitCrew", "warten, bis {0} ganz fertig ist"), OtherName);
	}
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	int32 RefIndex = INDEX_NONE;
	const FDCPlanTask* Ref = Plans ? Plans->FindTask(Task.Ref, nullptr, &RefIndex) : nullptr;
	if (!Ref)
	{
		return bWaiting ? LOCTEXT("WaitingGone", "wartet auf einen gelöschten Schritt") : LOCTEXT("TaskWaitGone", "warten auf einen gelöschten Schritt");
	}
	if (Ref->Type == EDCPlanTaskType::Signal)
	{
		return FText::Format(bWaiting ? LOCTEXT("WaitingSignal", "wartet auf Signal {0} von {1}") : LOCTEXT("TaskWaitSignal", "auf Signal {0} von {1} warten"),
			FText::AsNumber(Ref->Action), OtherName);
	}
	// A wait waiting for a wait: no nested names (they could go round in a circle).
	const FText RefLabel = Ref->Type == EDCPlanTaskType::WaitFor ? LOCTEXT("TaskWaitWait", "sein Warten") : DescribeTask(*Ref);
	// The waiting form is shown over the figure, whose font has no German quotes.
	return FText::Format(bWaiting ? LOCTEXT("WaitingStep", "wartet auf {0} (Schritt {2}: {1})")
		: LOCTEXT("TaskWaitStep", "warten, bis {0} „{1}“ erledigt hat (Schritt {2})"), OtherName, RefLabel, FText::AsNumber(RefIndex + 1));
}

FDCSchedule UDCHeistStateSubsystem::Simulate(FDCWorldSnapshot* OutFinalState, TMap<FName, UDCInventoryComponent*>* OutInventories) const
{
	UWorld* World = GetWorld();
	// The planned inventories are throwaway items: the windows need not hear about them.
	UDCItem::FMuteScope Mute;
	FDCSchedule Result;
	FDCWorldSnapshot State = TakeSnapshot();
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(World);

	TArray<FCursor> Cursors;
	for (TActorIterator<ADCCrewCharacter> It(World); It; ++It)
	{
		FDCCrewSchedule& Out = Result.Crews.AddDefaulted_GetRef();
		Out.Crew = It->GetFName();
		Out.Name = It->GetCrewName();
		Out.Color = It->GetBodyColor();
		Out.StartLocation = It->GetActorLocation();

		FCursor& Cursor = Cursors.AddDefaulted_GetRef();
		Cursor.Crew = *It;
		Cursor.CrewIndex = Result.Crews.Num() - 1;
		Cursor.Tasks = Plans ? &Plans->GetTasks(It->GetFName()) : nullptr;
		Cursor.Location = It->GetActorLocation();
		Cursor.Inventory = NewObject<UDCInventoryComponent>(GetTransientPackage());
		Cursor.Inventory->CopyFrom(*It->GetInventory());
		Cursor.BaseSpeed = It->GetBaseWalkSpeed();
		Cursor.Speed = Cursor.BaseSpeed * Cursor.Inventory->GetSpeedFactor();
		Cursor.bFinished = !Cursor.Tasks || Cursor.Tasks->IsEmpty();
		if (Cursor.bFinished)
		{
			// Nothing planned: "until done" is true from the start.
			State.DoneCrews.Add(It->GetFName());
		}
	}

	TArray<FSimEvent> Events;
	const auto AddEvent = [&Events](const FCursor& By, float Time, TFunction<void(FDCWorldSnapshot&)> Apply)
	{
		Events.Add({ Time, MoveTemp(Apply), By.CrewIndex, By.Next });
	};
	// Heavier load, slower walking.
	const auto UpdateSpeed = [](FCursor& Cursor)
	{
		Cursor.Speed = Cursor.BaseSpeed * Cursor.Inventory->GetSpeedFactor();
	};

	// Is the condition for the pending step met in the current state? If not, why not.
	const auto ConditionHolds = [this, &State](const FDCPlanTask& Task, FText& OutWait)
	{
		if (Task.Type == EDCPlanTaskType::WaitFor)
		{
			// Waiting for someone else's step (or signal), or for them to be done altogether.
			const bool bDone = Task.Ref.IsValid() ? State.DoneTasks.Contains(Task.Ref) : State.DoneCrews.Contains(Task.Target);
			if (!bDone)
			{
				OutWait = DescribeWaitFor(Task, true);
			}
			return bDone;
		}
		if (Task.Type == EDCPlanTaskType::Door)
		{
			const EDCDoorAction Action = static_cast<EDCDoorAction>(Task.Action);
			const FDCSimDoor* Door = State.FindDoor(Task.Target);
			if (Door && Door->bLocked && (Action == EDCDoorAction::Open || Action == EDCDoorAction::GoThrough))
			{
				const ADCDoor* Actor = FindDoor(Task.Target);
				OutWait = FText::Format(LOCTEXT("WaitDoorLocked", "wartet: {0} verschlossen"), Actor ? Actor->GetInteractableName() : FText::FromName(Task.Target));
				return false;
			}
			return true;
		}
		if (Task.Type == EDCPlanTaskType::Item || Task.Type == EDCPlanTaskType::Loot)
		{
			const FDCSimPickup* Pickup = State.FindPickup(Task.Target);
			if (!Pickup || Pickup->bGone)
			{
				return true;
			}
			const int32 Index = FDCWorldSnapshot::FindItem(*Pickup, Task.Type == EDCPlanTaskType::Item ? Task.ItemPath : FString());
			if (Index == INDEX_NONE)
			{
				return true;
			}
			const int32 Blocker = FindBlocker(*Pickup, Index);
			if (Blocker != INDEX_NONE)
			{
				const FDCSimItem& Block = Pickup->Items[Blocker];
				OutWait = FText::Format(Block.bLocked ? LOCTEXT("WaitLocked", "wartet: {0} versperrt") : LOCTEXT("WaitClosed", "wartet: {0} zu"), ItemName(Block.DefId));
				return false;
			}
			const FDCSimItem& Item = Pickup->Items[Index];
			const EDCItemAction Action = static_cast<EDCItemAction>(Task.Action);
			const bool bNeedsUnlocked = Task.Type == EDCPlanTaskType::Loot || Action == EDCItemAction::Open || Action == EDCItemAction::Take;
			if (bNeedsUnlocked && Item.bLocked)
			{
				OutWait = FText::Format(LOCTEXT("WaitLocked", "wartet: {0} versperrt"), ItemName(Item.DefId));
				return false;
			}
		}
		return true;
	};

	const auto Finish = [&Result, &AddEvent](FCursor& Cursor, FDCScheduledTask& Task)
	{
		// Others may wait for exactly this step being over.
		if (!Task.bBlocked)
		{
			const FGuid Id = (*Cursor.Tasks)[Cursor.Next].Id;
			AddEvent(Cursor, Task.End, [Id](FDCWorldSnapshot& S) { S.DoneTasks.Add(Id); });
		}
		Result.Crews[Cursor.CrewIndex].Tasks.Add(Task);
		Cursor.bHasPending = false;
		++Cursor.Next;
	};

	// The step can be done now: work out how long it takes and what it changes.
	const auto StartWork = [&](FCursor& Cursor)
	{
		FDCScheduledTask& Out = Cursor.Pending;
		const FDCPlanTask& Task = (*Cursor.Tasks)[Cursor.Next];
		const float T = Cursor.Time;
		Out.WorkStart = T;
		float Duration = 0.f;
		const auto Block = [&](const FText& Reason)
		{
			Out.bBlocked = true;
			Out.WaitReason = Reason;
			Out.End = T;
			Finish(Cursor, Out);
			// Nothing after a step that cannot be done.
			for (; Cursor.Next < Cursor.Tasks->Num(); ++Cursor.Next)
			{
				FDCScheduledTask Rest;
				Rest.Index = Cursor.Next;
				Rest.Start = Rest.Ready = Rest.WorkStart = Rest.End = T;
				Rest.bBlocked = true;
				Rest.Label = DescribeTask((*Cursor.Tasks)[Cursor.Next]);
				Rest.WaitReason = LOCTEXT("AfterBlocked", "kommt nicht dran (Schritt davor geht nicht)");
				Result.Crews[Cursor.CrewIndex].Tasks.Add(Rest);
			}
			Cursor.bFinished = true;
		};
		const auto UseCheck = [&](const FDCTaskCheck& Check)
		{
			Duration = Check.Seconds;
			Out.FailChance = Check.FailChance;
			Out.Detail = Check.Describe();
		};

		if (Task.Type == EDCPlanTaskType::Door)
		{
			ADCDoor* Door = FindDoor(Task.Target);
			FDCSimDoor* SimDoor = State.FindDoor(Task.Target);
			if (!Door || !SimDoor)
			{
				Block(LOCTEXT("NoDoor", "Tür nicht gefunden"));
				return;
			}
			const FName DoorName = Task.Target;
			const EDCDoorAction DoorAction = static_cast<EDCDoorAction>(Task.Action);
			if (DoorAction != EDCDoorAction::PickLock)
			{
				// A free hand for the handle.
				const FDCTaskCheck Check = Cursor.Crew->PlanCheckDoor(Door, DoorAction, Cursor.Inventory);
				if (!Check.bPossible)
				{
					Block(Check.Reason);
					return;
				}
			}
			switch (DoorAction)
			{
			case EDCDoorAction::PickLock:
				if (!SimDoor->bLocked)
				{
					Out.Detail = LOCTEXT("AlreadyOpen", "ist dann schon offen");
					break;
				}
				{
					const FDCTaskCheck Check = Cursor.Crew->PlanCheckDoor(Door, EDCDoorAction::PickLock, Cursor.Inventory);
					if (!Check.bPossible)
					{
						Block(Check.Reason);
						return;
					}
					UseCheck(Check);
				}
				AddEvent(Cursor, T + Duration, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bLocked = false; } });
				break;
			case EDCDoorAction::Open:
				Duration = HandSeconds;
				AddEvent(Cursor, T + Duration, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = true; } });
				break;
			case EDCDoorAction::Close:
				Duration = HandSeconds;
				AddEvent(Cursor, T + Duration, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = false; } });
				break;
			case EDCDoorAction::GoThrough:
			{
				const FVector Pass = Door->GetStandPoint(Cursor.Location, true);
				Duration = HandSeconds + SwingSeconds + WalkSeconds(World, Cursor.Location, Pass, Cursor.Speed, &Out.WorkPath);
				Out.WorkPathDelay = HandSeconds + SwingSeconds;
				AddEvent(Cursor, T + HandSeconds, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = true; } });
				Cursor.Location = Pass;
				Out.EndLocation = Pass;
				break;
			}
			}
		}
		else if (Task.Type == EDCPlanTaskType::Item || Task.Type == EDCPlanTaskType::PickUp || Task.Type == EDCPlanTaskType::Loot)
		{
			FDCSimPickup* SimPickup = State.FindPickup(Task.Target);
			const int32 Index = SimPickup ? FDCWorldSnapshot::FindItem(*SimPickup, Task.Type == EDCPlanTaskType::Item ? Task.ItemPath : FString()) : INDEX_NONE;
			if (!SimPickup || SimPickup->bGone || Index == INDEX_NONE || SimPickup->Items[Index].bTaken)
			{
				Block(LOCTEXT("Gone", "ist dann nicht mehr da"));
				return;
			}
			const FName PickupName = Task.Target;
			FDCSimItem& Item = SimPickup->Items[Index];
			const FDCItemDef* Def = DCItems::Find(Item.DefId);

			if (Task.Type == EDCPlanTaskType::PickUp || static_cast<EDCItemAction>(Task.Action) == EDCItemAction::Take)
			{
				// Into the planned inventory: later steps can use it (a crowbar picked up here).
				FText Reason;
				UDCItem* Built = BuildSimItem(*SimPickup, Index);
				if (!Built || !Cursor.Inventory->AddAnywhere(Built, Reason))
				{
					Block(Reason);
					return;
				}
				UpdateSpeed(Cursor);
				Out.Carry = FText::Format(LOCTEXT("CarryTo", "→ {0}"), Cursor.Inventory->DescribeWhere(Built));
				Duration = TakeSeconds;
				AddEvent(Cursor, T + Duration, [PickupName, Index](FDCWorldSnapshot& S) { if (FDCSimPickup* P = S.FindPickup(PickupName)) { TakeSubtree(*P, Index); } });
			}
			else if (Task.Type == EDCPlanTaskType::Loot)
			{
				// Open what can be opened, take what is worth something – on a scratch copy.
				FDCSimPickup Scratch = *SimPickup;
				TArray<int32> Opened;
				TArray<int32> Taken;
				TArray<int32> NoRoom;
				TArray<FText> TakenNames;
				TArray<FText> NoRoomNames;
				for (int32 Guard = 0; Guard < 100; ++Guard)
				{
					int32 ToOpen = INDEX_NONE;
					int32 ToTake = INDEX_NONE;
					for (int32 Candidate = 0; Candidate < Scratch.Items.Num(); ++Candidate)
					{
						const FDCSimItem& Sim = Scratch.Items[Candidate];
						if (Sim.bTaken || NoRoom.Contains(Candidate) || FindBlocker(Scratch, Candidate) != INDEX_NONE)
						{
							continue;
						}
						if (Sim.bOpenable && !Sim.IsOpen() && !Sim.bLocked)
						{
							ToOpen = Candidate;
							break;
						}
						if (Candidate != 0 && Sim.Value > 0 && !Sim.bAttached && Sim.bMovable
							&& (ToTake == INDEX_NONE || Sim.Value > Scratch.Items[ToTake].Value))
						{
							ToTake = Candidate;
						}
					}
					if (ToOpen != INDEX_NONE)
					{
						Scratch.Items[ToOpen].bOpen = true;
						Opened.Add(ToOpen);
					}
					else if (ToTake != INDEX_NONE)
					{
						// Only what still fits (the same rule as when it is played).
						FText Reason;
						UDCItem* Built = BuildSimItem(Scratch, ToTake);
						if (Built && Cursor.Inventory->AddAnywhere(Built, Reason))
						{
							TakeSubtree(Scratch, ToTake);
							Taken.Add(ToTake);
							TakenNames.Add(ItemName(Scratch.Items[ToTake].DefId));
						}
						else
						{
							NoRoom.Add(ToTake);
							NoRoomNames.Add(ItemName(Scratch.Items[ToTake].DefId));
						}
					}
					else
					{
						break;
					}
				}
				UpdateSpeed(Cursor);
				const int32 Steps = Opened.Num() + Taken.Num() + NoRoom.Num();
				Duration = 0.3f + LootStepSeconds * Steps;
				Out.Detail = Steps == 0 ? LOCTEXT("LootNothing", "nichts Erreichbares zu holen")
					: FText::Format(LOCTEXT("LootSteps", "ca. {0} s · {1} Handgriffe"), FText::AsNumber(FMath::RoundToInt(Duration)), FText::AsNumber(Steps));
				if (!TakenNames.IsEmpty())
				{
					Out.Carry = FText::Format(LOCTEXT("CarryLoot", "nimmt {0}"), JoinNames(TakenNames));
				}
				if (!NoRoomNames.IsEmpty())
				{
					Out.Detail = FText::Format(LOCTEXT("LootNoRoom", "{0} · kein Platz mehr für: {1}"), Out.Detail, JoinNames(NoRoomNames));
				}
				AddEvent(Cursor, T + Duration, [PickupName, Opened, Taken](FDCWorldSnapshot& S)
				{
					if (FDCSimPickup* P = S.FindPickup(PickupName))
					{
						for (const int32 Open : Opened)
						{
							P->Items[Open].bOpen = true;
						}
						for (const int32 Take : Taken)
						{
							TakeSubtree(*P, Take);
						}
					}
				});
			}
			else
			{
				const EDCItemAction Action = static_cast<EDCItemAction>(Task.Action);
				const bool bAlreadyDone = ((Action == EDCItemAction::PickLock || Action == EDCItemAction::Force) && !Item.bLocked)
					|| (Action == EDCItemAction::Smash && Item.bDestroyed) || (Action == EDCItemAction::Open && Item.IsOpen());
				if (bAlreadyDone)
				{
					Out.Detail = LOCTEXT("AlreadyDone", "ist dann schon erledigt");
				}
				else if (Action == EDCItemAction::Open || Action == EDCItemAction::Close)
				{
					if (Def)
					{
						const FDCTaskCheck Check = Cursor.Crew->PlanCheckItem(*Def, Action, Cursor.Inventory);
						if (!Check.bPossible)
						{
							Block(Check.Reason);
							return;
						}
					}
					Duration = OpenSeconds;
					const bool bOpen = Action == EDCItemAction::Open;
					AddEvent(Cursor, T + Duration, [PickupName, Index, bOpen](FDCWorldSnapshot& S) { if (FDCSimPickup* P = S.FindPickup(PickupName)) { P->Items[Index].bOpen = bOpen; } });
				}
				else if (Def)
				{
					const FDCTaskCheck Check = Cursor.Crew->PlanCheckItem(*Def, Action, Cursor.Inventory);
					if (!Check.bPossible)
					{
						Block(Check.Reason);
						return;
					}
					UseCheck(Check);
					Out.bLoud = Action == EDCItemAction::Force || Action == EDCItemAction::Smash;
					AddEvent(Cursor, T + Duration, [PickupName, Index, Action](FDCWorldSnapshot& S)
					{
						if (FDCSimPickup* P = S.FindPickup(PickupName))
						{
							FDCSimItem& Changed = P->Items[Index];
							Changed.bLocked = false;
							Changed.bOpen = Changed.bOpen || Action != EDCItemAction::PickLock;
							Changed.bDestroyed = Changed.bDestroyed || Action == EDCItemAction::Smash;
						}
					});
				}
			}
		}
		else if (Task.Type == EDCPlanTaskType::Wait)
		{
			Duration = Task.Seconds;
		}
		else if (Task.Type == EDCPlanTaskType::WaitFor)
		{
			// Whom it waits for, even when there was nothing to wait (shown at the other crew member's step).
			if (Out.WaitFor.IsNone())
			{
				const FDCCrewSchedule* Other = Result.Find(Task.Target);
				int32 RefIndex = INDEX_NONE;
				if (Task.Ref.IsValid())
				{
					Plans->FindTask(Task.Ref, nullptr, &RefIndex);
				}
				else
				{
					RefIndex = Plans->GetTasks(Task.Target).Num() - 1;
				}
				Out.WaitFor = Task.Target;
				Out.WaitForName = Other ? Other->Name : FText::FromName(Task.Target);
				Out.WaitForIndex = RefIndex;
			}
			const float Waited = T - Out.Ready;
			const int32 GoOn = FMath::RoundToInt(T);
			Out.Detail = Waited > 0.05f
				? FText::Format(LOCTEXT("WaitedFor", "wartet {0} s · geht um {1} weiter"), TenthsText(Waited), FText::FromString(FString::Printf(TEXT("%d:%02d"), GoOn / 60, GoOn % 60)))
				: LOCTEXT("NoWaitNeeded", "ist dann schon erledigt – kein Warten nötig");
		}
		else if (Task.Type == EDCPlanTaskType::Signal)
		{
			Duration = HandSeconds;
			Out.Detail = LOCTEXT("SignalDetail", "kurzes Zeichen über Funk – wer darauf wartet, legt los");
		}
		else if (Task.Type == EDCPlanTaskType::Drop)
		{
			const FName DropId(*Task.ItemPath);
			UDCItem* Carried = Cursor.Inventory->FindItem(DropId);
			if (!Carried)
			{
				Block(FText::Format(LOCTEXT("NothingToDrop", "hat dann kein {0} dabei"), ItemName(DropId)));
				return;
			}
			Cursor.Inventory->Remove(Carried);
			UpdateSpeed(Cursor);
			Duration = HandSeconds;
			Out.Carry = FText::Format(LOCTEXT("CarryDrop", "legt {0} auf den Boden"), ItemName(DropId));
		}

		Out.End = T + Duration;
		if (Out.bHasLocation && Out.EndLocation.IsZero())
		{
			Out.EndLocation = Cursor.Location;
		}
		Cursor.Time = Out.End;
		Finish(Cursor, Out);
	};

	// Starts the next step: walk to where it happens; the check comes when the walk is over.
	const auto BeginTask = [&](FCursor& Cursor)
	{
		if (Cursor.Next >= Cursor.Tasks->Num())
		{
			Cursor.bFinished = true;
			// Through with everything: whoever waits for this crew member being done may go on.
			const FName CrewName = Result.Crews[Cursor.CrewIndex].Crew;
			Events.Add({ Cursor.Time, [CrewName](FDCWorldSnapshot& S) { S.DoneCrews.Add(CrewName); }, Cursor.CrewIndex, Cursor.Next - 1 });
			return;
		}
		const FDCPlanTask& Task = (*Cursor.Tasks)[Cursor.Next];
		FDCScheduledTask& Out = Cursor.Pending;
		Out = FDCScheduledTask();
		Out.Index = Cursor.Next;
		Out.Type = Task.Type;
		Out.Start = Cursor.Time;
		Out.Label = DescribeTask(Task);
		Cursor.bHasPending = true;

		FVector Spot = Cursor.Location;
		switch (Task.Type)
		{
		case EDCPlanTaskType::Move:
			Spot = Task.Location;
			Out.bHasLocation = true;
			break;
		case EDCPlanTaskType::Wait:
		case EDCPlanTaskType::Drop:
		case EDCPlanTaskType::WaitFor:
		case EDCPlanTaskType::Signal:
			break;
		case EDCPlanTaskType::Door:
			if (const ADCDoor* Door = FindDoor(Task.Target))
			{
				Spot = Door->GetStandPoint(Cursor.Location, false);
				Out.bHasLocation = true;
			}
			break;
		default:
			if (const ADCItemPickup* Pickup = FindPickup(Task.Target))
			{
				FVector Away = (Cursor.Location - Pickup->GetActorLocation()).GetSafeNormal2D();
				Away = Away.IsNearlyZero() ? FVector::ForwardVector : Away;
				Spot = Pickup->GetActorLocation() + Away * (Pickup->GetFootprintRadius() + ItemReach);
				Out.bHasLocation = true;
			}
			break;
		}
		Out.Ready = Cursor.Time + WalkSeconds(World, Cursor.Location, Spot, Cursor.Speed, &Out.Path);
		Cursor.Location = Spot;
		Out.EndLocation = Spot;
		Cursor.Time = Out.Ready;
	};

	// Applies an event; crew members waiting for it may continue.
	const auto ApplyEvent = [&](const FSimEvent& Event)
	{
		Event.Apply(State);
		for (FCursor& Cursor : Cursors)
		{
			FText Ignored;
			if (Cursor.bBlocked && ConditionHolds((*Cursor.Tasks)[Cursor.Next], Ignored))
			{
				Cursor.bBlocked = false;
				Cursor.Time = FMath::Max(Cursor.BlockedSince, Event.Time);
				if (Result.Crews.IsValidIndex(Event.CrewIndex) && Event.CrewIndex != Cursor.CrewIndex)
				{
					Cursor.Pending.WaitFor = Result.Crews[Event.CrewIndex].Crew;
					Cursor.Pending.WaitForName = Result.Crews[Event.CrewIndex].Name;
					Cursor.Pending.WaitForIndex = Event.TaskIndex;
				}
			}
		}
	};
	const auto ApplyEventsUpTo = [&](float Time)
	{
		Events.Sort([](const FSimEvent& A, const FSimEvent& B) { return A.Time < B.Time; });
		while (!Events.IsEmpty() && Events[0].Time <= Time)
		{
			const FSimEvent Event = Events[0];
			Events.RemoveAt(0);
			ApplyEvent(Event);
		}
	};

	for (int32 Guard = 0; Guard < 10000; ++Guard)
	{
		FCursor* Next = nullptr;
		for (FCursor& Cursor : Cursors)
		{
			if (!Cursor.bFinished && !Cursor.bBlocked && (!Next || Cursor.Time < Next->Time))
			{
				Next = &Cursor;
			}
		}
		if (!Next)
		{
			// Everyone left is waiting: let time run to the next change.
			if (Events.IsEmpty())
			{
				break;
			}
			Events.Sort([](const FSimEvent& A, const FSimEvent& B) { return A.Time < B.Time; });
			const FSimEvent Event = Events[0];
			Events.RemoveAt(0);
			ApplyEvent(Event);
			continue;
		}
		ApplyEventsUpTo(Next->Time);
		if (Next->bBlocked)
		{
			continue;
		}
		if (!Next->bHasPending)
		{
			BeginTask(*Next);
			continue;
		}
		FText Wait;
		if (!ConditionHolds((*Next->Tasks)[Next->Next], Wait))
		{
			Next->bBlocked = true;
			Next->BlockedSince = Next->Time;
			Next->Pending.WaitReason = Wait;
			continue;
		}
		StartWork(*Next);
	}
	ApplyEventsUpTo(TNumericLimits<float>::Max());

	// Whoever still waits, waits forever.
	for (FCursor& Cursor : Cursors)
	{
		if (Cursor.bBlocked)
		{
			Cursor.Pending.bBlocked = true;
			Cursor.Pending.WorkStart = Cursor.Pending.End = Cursor.BlockedSince;
			const FText Reason = Cursor.Pending.WaitReason;
			Cursor.Pending.WaitReason = FText::Format(LOCTEXT("NeverHappens", "{0} – passiert im Plan nie"), Reason);
			Cursor.Time = Cursor.BlockedSince;
			Result.Crews[Cursor.CrewIndex].Tasks.Add(Cursor.Pending);
			for (++Cursor.Next; Cursor.Next < Cursor.Tasks->Num(); ++Cursor.Next)
			{
				FDCScheduledTask Rest;
				Rest.Index = Cursor.Next;
				Rest.Start = Rest.Ready = Rest.WorkStart = Rest.End = Cursor.BlockedSince;
				Rest.bBlocked = true;
				Rest.Label = DescribeTask((*Cursor.Tasks)[Cursor.Next]);
				Rest.WaitReason = LOCTEXT("AfterBlocked", "kommt nicht dran (Schritt davor geht nicht)");
				Result.Crews[Cursor.CrewIndex].Tasks.Add(Rest);
			}
		}
		Result.Crews[Cursor.CrewIndex].End = Cursor.Time;
		Result.Duration = FMath::Max(Result.Duration, Cursor.Time);
	}
	if (OutFinalState)
	{
		*OutFinalState = State;
	}
	if (OutInventories)
	{
		for (const FCursor& Cursor : Cursors)
		{
			OutInventories->Add(Result.Crews[Cursor.CrewIndex].Crew, Cursor.Inventory);
		}
	}
	return Result;
}

void UDCHeistStateSubsystem::Refresh(bool bMarkers)
{
	TMap<FName, UDCInventoryComponent*> Inventories;
	Schedule = Simulate(&PlannedState, &Inventories);
	PlannedInventories.Reset();
	for (const TPair<FName, UDCInventoryComponent*>& Entry : Inventories)
	{
		PlannedInventories.Add(Entry.Key, Entry.Value);
	}
	FString Signature;
	for (const FDCCrewSchedule& Crew : Schedule.Crews)
	{
		for (const FDCScheduledTask& Task : Crew.Tasks)
		{
			Signature += FString::Printf(TEXT("%.1f/%.1f/%.1f/%.1f/%d/%s/%s/%s;"), Task.Start, Task.Ready, Task.WorkStart, Task.End, Task.bBlocked ? 1 : 0,
				*Task.Detail.ToString(), *Task.WaitReason.ToString(), *Task.Carry.ToString());
		}
		Signature += TEXT("|");
	}
	const bool bChanged = Signature != ScheduleSignature;
	if (bChanged)
	{
		ScheduleSignature = Signature;
		++ScheduleVersion;
	}
	// Also when the plan came back after reloading the room, or walking times changed.
	if (bMarkers || bChanged)
	{
		UpdateMarkers();
	}
	else
	{
		UpdateGhosts();
	}
}

void UDCHeistStateSubsystem::UpdateMarkers()
{
	for (const TWeakObjectPtr<ADCPlanMarker>& Marker : Markers)
	{
		if (Marker.IsValid())
		{
			Marker->Destroy();
		}
	}
	Markers.Reset();
	UpdateGhosts();
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Plans || Plans->GetMode() != EDCPlanMode::Planning)
	{
		return;
	}
	// One marker per step with a place, with the way there on the floor, in the crew member's
	// colour. Only the plan being edited is numbered; the others stay small and dark.
	for (const FDCCrewSchedule& Crew : Schedule.Crews)
	{
		const ADCCrewCharacter* Member = FindCrew(Crew.Crew);
		if (!Member)
		{
			continue;
		}
		const bool bFocus = FocusCrew.IsNone() || FocusCrew == Crew.Crew;
		const double FloorZ = Member->GetActorLocation().Z - 90.0;
		for (const FDCScheduledTask& Task : Crew.Tasks)
		{
			if (!Task.bHasLocation)
			{
				continue;
			}
			TArray<FVector> Route = Task.Path;
			Route.Append(Task.WorkPath);
			for (FVector& Point : Route)
			{
				Point.Z = FloorZ;
			}
			FVector Spot = Task.EndLocation;
			Spot.Z = FloorZ;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADCPlanMarker* Marker = GetWorld()->SpawnActor<ADCPlanMarker>(Spot, FRotator::ZeroRotator, Params))
			{
				const EDCMarkerLook Look = !bFocus ? EDCMarkerLook::Faint : Task.Index == FocusStep ? EDCMarkerLook::Highlight : EDCMarkerLook::Normal;
				Marker->Init(Crew.Color, Task.Index + 1, Route, Task.bBlocked, Look);
				Markers.Add(Marker);
			}
		}
	}
}

void UDCHeistStateSubsystem::UpdateGhosts()
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	const bool bShow = PreviewTime >= 0.f && Plans && Plans->GetMode() == EDCPlanMode::Planning;
	if (!bShow)
	{
		for (const TWeakObjectPtr<ADCPlanMarker>& Ghost : Ghosts)
		{
			if (Ghost.IsValid())
			{
				Ghost->Destroy();
			}
		}
		Ghosts.Reset();
		return;
	}
	Ghosts.RemoveAll([](const TWeakObjectPtr<ADCPlanMarker>& Ghost) { return !Ghost.IsValid(); });
	for (int32 Index = 0; Index < Schedule.Crews.Num(); ++Index)
	{
		const FDCCrewSchedule& Crew = Schedule.Crews[Index];
		if (!Ghosts.IsValidIndex(Index))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ADCPlanMarker* Ghost = GetWorld()->SpawnActor<ADCPlanMarker>(Crew.StartLocation, FRotator::ZeroRotator, Params);
			if (!Ghost)
			{
				return;
			}
			Ghost->InitGhost(Crew.Color);
			Ghosts.Add(Ghost);
		}
		FVector Floor = GetPositionAt(Crew, PreviewTime);
		Floor.Z = Crew.StartLocation.Z - 90.0;
		Ghosts[Index]->SetGhost(Floor, FText::Format(LOCTEXT("GhostLabel", "{0}: {1}"), Crew.Name, DescribeActivityAt(Crew, PreviewTime)));
	}
}

void UDCHeistStateSubsystem::SetFocus(FName Crew, int32 Step)
{
	if (FocusCrew != Crew || FocusStep != Step)
	{
		FocusCrew = Crew;
		FocusStep = Step;
		UpdateMarkers();
	}
}

void UDCHeistStateSubsystem::SetPreviewTime(float Time)
{
	PreviewTime = Time < 0.f ? -1.f : FMath::Min(Time, FMath::Max(Schedule.Duration, 0.f));
	UpdateGhosts();
}

const UDCInventoryComponent* UDCHeistStateSubsystem::GetPlannedInventory(FName Crew) const
{
	const TObjectPtr<UDCInventoryComponent>* Found = PlannedInventories.Find(Crew);
	return Found ? Found->Get() : nullptr;
}

FVector UDCHeistStateSubsystem::GetPositionAt(const FDCCrewSchedule& Crew, float Time)
{
	FVector Location = Crew.StartLocation;
	for (const FDCScheduledTask& Task : Crew.Tasks)
	{
		if (Time < Task.Start)
		{
			return Location;
		}
		if (Time < Task.Ready && Task.Path.Num() >= 2)
		{
			return AlongPath(Task.Path, (Time - Task.Start) / FMath::Max(0.01f, Task.Ready - Task.Start));
		}
		if (!Task.Path.IsEmpty())
		{
			Location = Task.Path.Last();
		}
		if (Task.bBlocked)
		{
			// Nothing happens after a step that cannot be done.
			return Location;
		}
		if (Time < Task.WorkStart)
		{
			return Location;
		}
		if (Task.WorkPath.Num() >= 2)
		{
			const float WalkStart = Task.WorkStart + Task.WorkPathDelay;
			if (Time < Task.End)
			{
				return Time < WalkStart ? Location : AlongPath(Task.WorkPath, (Time - WalkStart) / FMath::Max(0.01f, Task.End - WalkStart));
			}
			Location = Task.WorkPath.Last();
		}
		else if (Time < Task.End)
		{
			return Location;
		}
	}
	return Location;
}

int32 UDCHeistStateSubsystem::GetStepAt(const FDCCrewSchedule& Crew, float Time)
{
	for (const FDCScheduledTask& Task : Crew.Tasks)
	{
		if (Task.bBlocked && Time >= Task.Start)
		{
			return Task.Index;
		}
		if (Time >= Task.Start && Time < FMath::Max(Task.End, Task.Start + 0.05f))
		{
			return Task.Index;
		}
	}
	return INDEX_NONE;
}

FText UDCHeistStateSubsystem::DescribeActivityAt(const FDCCrewSchedule& Crew, float Time)
{
	const int32 Step = GetStepAt(Crew, Time);
	const FDCScheduledTask* Task = Crew.Tasks.FindByPredicate([Step](const FDCScheduledTask& Entry) { return Entry.Index == Step; });
	if (!Task)
	{
		return Crew.Tasks.IsEmpty() || Time < Crew.Tasks[0].Start ? LOCTEXT("ActivityIdle", "steht") : LOCTEXT("ActivityDone", "fertig");
	}
	const FText Number = FText::AsNumber(Task->Index + 1);
	if (Task->bBlocked && Time >= Task->Ready)
	{
		return FText::Format(LOCTEXT("ActivityBlocked", "{0}. {1}: geht nicht"), Number, Task->Label);
	}
	if (Time < Task->Ready)
	{
		return FText::Format(LOCTEXT("ActivityWalk", "{0}. geht hin: {1}"), Number, Task->Label);
	}
	if (Time < Task->WorkStart)
	{
		return Task->WaitFor.IsNone()
			? FText::Format(LOCTEXT("ActivityWait", "{0}. {1}"), Number, Task->WaitReason)
			: FText::Format(LOCTEXT("ActivityWaitFor", "{0}. wartet auf {1}"), Number, Task->WaitForName);
	}
	return FText::Format(LOCTEXT("ActivityWork", "{0}. {1}"), Number, Task->Label);
}

void UDCHeistStateSubsystem::StartPlayback()
{
	UWorld* World = GetWorld();
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(World);
	if (!Plans || IsPlaying())
	{
		return;
	}
	PreviewTime = -1.f;
	Refresh();
	bWorldUsed = true;
	PlaybackStart = World->GetTimeSeconds();
	Plans->SetMode(EDCPlanMode::Playing);
	UGameplayStatics::SetGlobalTimeDilation(World, Plans->GetSpeed());
	for (TActorIterator<ADCCrewCharacter> It(World); It; ++It)
	{
		It->GetPlanRunner()->Start(Plans->GetTasks(It->GetFName()));
	}
	UE_LOG(LogDerClou, Log, TEXT("Playback started (planned duration %.1f s)"), Schedule.Duration);
}

float UDCHeistStateSubsystem::GetPlaybackTime() const
{
	return IsPlaying() ? static_cast<float>(GetWorld()->GetTimeSeconds() - PlaybackStart) : 0.f;
}

bool UDCHeistStateSubsystem::IsPlaybackFinished() const
{
	if (!IsPlaying())
	{
		return false;
	}
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetPlanRunner()->IsRunning())
		{
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
