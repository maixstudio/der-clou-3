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

	float WalkSeconds(UWorld* World, const FVector& From, const FVector& To, float Speed)
	{
		const double Straight = FVector::Dist2D(From, To);
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
			}
		}
		// A little extra for starting and stopping.
		return static_cast<float>(Length / FMath::Max(50.f, Speed)) + 0.3f;
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

FDCSchedule UDCHeistStateSubsystem::Simulate(FDCWorldSnapshot* OutFinalState) const
{
	UWorld* World = GetWorld();
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

		FCursor& Cursor = Cursors.AddDefaulted_GetRef();
		Cursor.Crew = *It;
		Cursor.CrewIndex = Result.Crews.Num() - 1;
		Cursor.Tasks = Plans ? &Plans->GetTasks(It->GetFName()) : nullptr;
		Cursor.Location = It->GetActorLocation();
		Cursor.Speed = It->GetCharacterMovement()->MaxWalkSpeed;
		Cursor.bFinished = !Cursor.Tasks || Cursor.Tasks->IsEmpty();
	}

	TArray<FSimEvent> Events;
	const auto AddEvent = [&Events](float Time, TFunction<void(FDCWorldSnapshot&)> Apply)
	{
		Events.Add({ Time, MoveTemp(Apply) });
	};

	// Is the condition for the pending step met in the current state? If not, why not.
	const auto ConditionHolds = [this, &State](const FDCPlanTask& Task, FText& OutWait)
	{
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

	const auto Finish = [&Result](FCursor& Cursor, FDCScheduledTask& Task)
	{
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
			switch (static_cast<EDCDoorAction>(Task.Action))
			{
			case EDCDoorAction::PickLock:
				if (!SimDoor->bLocked)
				{
					Out.Detail = LOCTEXT("AlreadyOpen", "ist dann schon offen");
					break;
				}
				{
					const FDCTaskCheck Check = Cursor.Crew->PlanCheckDoor(Door, EDCDoorAction::PickLock);
					if (!Check.bPossible)
					{
						Block(Check.Reason);
						return;
					}
					UseCheck(Check);
				}
				AddEvent(T + Duration, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bLocked = false; } });
				break;
			case EDCDoorAction::Open:
				Duration = HandSeconds;
				AddEvent(T + Duration, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = true; } });
				break;
			case EDCDoorAction::Close:
				Duration = HandSeconds;
				AddEvent(T + Duration, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = false; } });
				break;
			case EDCDoorAction::GoThrough:
			{
				const FVector Pass = Door->GetStandPoint(Cursor.Location, true);
				Duration = HandSeconds + SwingSeconds + WalkSeconds(World, Cursor.Location, Pass, Cursor.Speed);
				AddEvent(T + HandSeconds, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = true; } });
				Cursor.Location = Pass;
				Out.EndLocation = Pass;
				break;
			}
			}
		}
		else if (Task.Type == EDCPlanTaskType::Item || Task.Type == EDCPlanTaskType::PickUp || Task.Type == EDCPlanTaskType::Loot)
		{
			FDCSimPickup* SimPickup = State.FindPickup(Task.Target);
			ADCItemPickup* Pickup = FindPickup(Task.Target);
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
				// Whether it fits is judged with what the crew member carries now.
				const FText Reason = Cursor.Crew->GetInventory()->CheckAdd(ResolveItem(Pickup, Task.Type == EDCPlanTaskType::Item ? Task.ItemPath : FString()));
				if (!Reason.IsEmpty())
				{
					Block(Reason);
					return;
				}
				Duration = TakeSeconds;
				AddEvent(T + Duration, [PickupName, Index](FDCWorldSnapshot& S) { if (FDCSimPickup* P = S.FindPickup(PickupName)) { TakeSubtree(*P, Index); } });
			}
			else if (Task.Type == EDCPlanTaskType::Loot)
			{
				// Open what can be opened, take what is worth something – on a scratch copy.
				FDCSimPickup Scratch = *SimPickup;
				TArray<int32> Opened;
				TArray<int32> Taken;
				for (int32 Guard = 0; Guard < 100; ++Guard)
				{
					int32 ToOpen = INDEX_NONE;
					int32 ToTake = INDEX_NONE;
					for (int32 Candidate = 0; Candidate < Scratch.Items.Num(); ++Candidate)
					{
						const FDCSimItem& Sim = Scratch.Items[Candidate];
						if (Sim.bTaken || FindBlocker(Scratch, Candidate) != INDEX_NONE)
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
						TakeSubtree(Scratch, ToTake);
						Taken.Add(ToTake);
					}
					else
					{
						break;
					}
				}
				const int32 Steps = Opened.Num() + Taken.Num();
				Duration = 0.3f + LootStepSeconds * Steps;
				Out.Detail = Steps == 0 ? LOCTEXT("LootNothing", "nichts Erreichbares zu holen")
					: FText::Format(LOCTEXT("LootSteps", "ca. {0} s · {1} Handgriffe"), FText::AsNumber(FMath::RoundToInt(Duration)), FText::AsNumber(Steps));
				AddEvent(T + Duration, [PickupName, Opened, Taken](FDCWorldSnapshot& S)
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
					Duration = OpenSeconds;
					const bool bOpen = Action == EDCItemAction::Open;
					AddEvent(T + Duration, [PickupName, Index, bOpen](FDCWorldSnapshot& S) { if (FDCSimPickup* P = S.FindPickup(PickupName)) { P->Items[Index].bOpen = bOpen; } });
				}
				else if (Def)
				{
					const FDCTaskCheck Check = Cursor.Crew->PlanCheckItem(*Def, Action);
					if (!Check.bPossible)
					{
						Block(Check.Reason);
						return;
					}
					UseCheck(Check);
					Out.bLoud = Action == EDCItemAction::Force || Action == EDCItemAction::Smash;
					AddEvent(T + Duration, [PickupName, Index, Action](FDCWorldSnapshot& S)
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
			return;
		}
		const FDCPlanTask& Task = (*Cursor.Tasks)[Cursor.Next];
		FDCScheduledTask& Out = Cursor.Pending;
		Out = FDCScheduledTask();
		Out.Index = Cursor.Next;
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
		Out.Ready = Cursor.Time + WalkSeconds(World, Cursor.Location, Spot, Cursor.Speed);
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
	return Result;
}

void UDCHeistStateSubsystem::Refresh(bool bMarkers)
{
	Schedule = Simulate(&PlannedState);
	FString Signature;
	for (const FDCCrewSchedule& Crew : Schedule.Crews)
	{
		for (const FDCScheduledTask& Task : Crew.Tasks)
		{
			Signature += FString::Printf(TEXT("%.1f/%.1f/%.1f/%.1f/%d/%s;"), Task.Start, Task.Ready, Task.WorkStart, Task.End, Task.bBlocked ? 1 : 0, *Task.Detail.ToString());
		}
		Signature += TEXT("|");
	}
	if (Signature != ScheduleSignature)
	{
		ScheduleSignature = Signature;
		++ScheduleVersion;
	}
	if (bMarkers)
	{
		UpdateMarkers();
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
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Plans || Plans->GetMode() != EDCPlanMode::Planning)
	{
		return;
	}
	// One numbered marker per step with a place, joined by a line, in the crew member's colour.
	for (const FDCCrewSchedule& Crew : Schedule.Crews)
	{
		const ADCCrewCharacter* Member = FindCrew(Crew.Crew);
		if (!Member)
		{
			continue;
		}
		const double FloorZ = Member->GetActorLocation().Z - 90.0;
		FVector Previous = Member->GetActorLocation();
		Previous.Z = FloorZ;
		for (const FDCScheduledTask& Task : Crew.Tasks)
		{
			if (!Task.bHasLocation)
			{
				continue;
			}
			FVector Spot = Task.EndLocation;
			Spot.Z = FloorZ;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADCPlanMarker* Marker = GetWorld()->SpawnActor<ADCPlanMarker>(Spot, FRotator::ZeroRotator, Params))
			{
				Marker->Init(Crew.Color, Task.Index + 1, Previous, Task.bBlocked);
				Markers.Add(Marker);
			}
			Previous = Spot;
		}
	}
}

void UDCHeistStateSubsystem::StartPlayback()
{
	UWorld* World = GetWorld();
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(World);
	if (!Plans || IsPlaying())
	{
		return;
	}
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
