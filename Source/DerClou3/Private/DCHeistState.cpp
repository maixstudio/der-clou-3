#include "DCHeistState.h"

#include "Algo/Reverse.h"

#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCPlanMarker.h"
#include "DCPlanRunner.h"
#include "DCRoomVolume.h"
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
	constexpr float SimLootStepSeconds = 0.8f;
	constexpr double SimItemReach = 40.0;
	/** Waiting at a locked door: this far beside the spot in front of it (whoever unlocks it stands there). */
	constexpr double SimDoorWaitAside = 110.0;
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
		/** The pending step has not started walking yet: closed doors (WaitDoors) are in the way. */
		bool bWalkPending = false;
		TArray<FName> WaitDoors;
		/** The door it waits at is locked: unlocking frees it. Otherwise (hands full) only opening does. */
		bool bWaitForLock = false;
		float BlockedSince = 0.f;
		FDCScheduledTask Pending;
	};

	FText ItemName(FName DefId)
	{
		const FDCItemDef* Def = DCItems::Find(DefId);
		return Def ? Def->Name : FText::FromName(DefId);
	}

	/**
	 * A way from From to To in the world as it is now that may pass closed doors: the nav mesh
	 * leads up to a door, through it, and on. Doors are the only connections between rooms, so
	 * this is a small shortest-path search over the door sides (legs computed on demand).
	 */
	bool WayThroughDoors(UWorld* World, const FVector& From, const FVector& To, TArray<FVector>& OutPoints)
	{
		struct FNode
		{
			FVector Location;
			int32 Door = INDEX_NONE;
		};
		TArray<FNode> Nodes = { { From }, { To } };
		int32 DoorIndex = 0;
		for (TActorIterator<ADCDoor> It(World); It; ++It, ++DoorIndex)
		{
			// Open doors too: right after opening, the nav mesh may not have caught up yet.
			const FVector Normal = It->GetActorForwardVector().GetSafeNormal2D();
			for (const double Side : { -1.0, 1.0 })
			{
				Nodes.Add({ It->GetStandPoint(It->GetCenter() + Normal * Side * 300.0, false), DoorIndex });
			}
		}
		const int32 Count = Nodes.Num();
		TMap<int32, TArray<FVector>> Legs;
		TMap<int32, double> LegLength;
		const auto Leg = [&](int32 A, int32 B) -> double
		{
			const int32 Key = FMath::Min(A, B) * 1000 + FMath::Max(A, B);
			if (const double* Known = LegLength.Find(Key))
			{
				return *Known;
			}
			double Length = -1.0;
			TArray<FVector> Points;
			if (Nodes[A].Door != INDEX_NONE && Nodes[A].Door == Nodes[B].Door)
			{
				// Through the door itself.
				Points = { Nodes[A].Location, Nodes[B].Location };
				Length = FVector::Dist2D(Nodes[A].Location, Nodes[B].Location);
			}
			else if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Nodes[A].Location, Nodes[B].Location))
			{
				if (Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() >= 2)
				{
					Points = Path->PathPoints;
					Length = Path->GetPathLength();
				}
			}
			if (A > B)
			{
				Algo::Reverse(Points);
			}
			Legs.Add(Key, Points);
			LegLength.Add(Key, Length);
			return Length;
		};
		TArray<double> Distance;
		Distance.Init(TNumericLimits<double>::Max(), Count);
		TArray<int32> Previous;
		Previous.Init(INDEX_NONE, Count);
		TArray<bool> Settled;
		Settled.Init(false, Count);
		Distance[0] = 0.0;
		for (int32 Round = 0; Round < Count; ++Round)
		{
			int32 Current = INDEX_NONE;
			for (int32 Node = 0; Node < Count; ++Node)
			{
				if (!Settled[Node] && Distance[Node] < TNumericLimits<double>::Max() && (Current == INDEX_NONE || Distance[Node] < Distance[Current]))
				{
					Current = Node;
				}
			}
			if (Current == INDEX_NONE || Current == 1)
			{
				break;
			}
			Settled[Current] = true;
			for (int32 Node = 0; Node < Count; ++Node)
			{
				if (Settled[Node] || Node == Current)
				{
					continue;
				}
				const double Length = Leg(Current, Node);
				if (Length >= 0.0 && Distance[Current] + Length < Distance[Node])
				{
					Distance[Node] = Distance[Current] + Length;
					Previous[Node] = Current;
				}
			}
		}
		if (Previous[1] == INDEX_NONE)
		{
			return false;
		}
		TArray<int32> Chain;
		for (int32 Node = 1; Node != INDEX_NONE; Node = Previous[Node])
		{
			Chain.Insert(Node, 0);
		}
		OutPoints.Reset();
		for (int32 Step = 1; Step < Chain.Num(); ++Step)
		{
			const int32 A = Chain[Step - 1];
			const int32 B = Chain[Step];
			TArray<FVector> Points = Legs.FindRef(FMath::Min(A, B) * 1000 + FMath::Max(A, B));
			if (A > B)
			{
				Algo::Reverse(Points);
			}
			for (int32 Point = OutPoints.IsEmpty() ? 0 : 1; Point < Points.Num(); ++Point)
			{
				OutPoints.Add(Points[Point]);
			}
		}
		return OutPoints.Num() >= 2;
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
		bool bFound = false;
		if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To))
		{
			if (Path->IsValid() && !Path->IsPartial())
			{
				bFound = true;
				Length = Path->GetPathLength();
				if (OutPath && Path->PathPoints.Num() >= 2)
				{
					*OutPath = Path->PathPoints;
				}
			}
		}
		// Behind a closed door: the way through the door (the plan then waits until it is open).
		TArray<FVector> Through;
		if (!bFound && WayThroughDoors(World, From, To, Through))
		{
			Length = 0.0;
			for (int32 Point = 1; Point < Through.Num(); ++Point)
			{
				Length += FVector::Dist(Through[Point - 1], Through[Point]);
			}
			if (OutPath)
			{
				*OutPath = Through;
			}
		}
		// A little extra for starting and stopping.
		return static_cast<float>(Length / FMath::Max(50.f, Speed)) + 0.3f;
	}

	/** Everything that matters about an inventory as text (slots, items, cells, states): equal text = same contents. */
	FString DescribeItemForCompare(const UDCItem* Item)
	{
		if (!Item)
		{
			return TEXT("-");
		}
		FString Text = FString::Printf(TEXT("%s@%d,%d:%d%d%d%d{"), *Item->GetDefId().ToString(), Item->GetGridPos().X, Item->GetGridPos().Y,
			Item->IsOpen() ? 1 : 0, Item->IsLocked() ? 1 : 0, Item->IsLockBroken() ? 1 : 0, Item->IsDestroyed() ? 1 : 0);
		for (const UDCItem* Child : Item->GetContents())
		{
			Text += DescribeItemForCompare(Child);
		}
		return Text + TEXT("}");
	}

	FString DescribeInventoryForCompare(const UDCInventoryComponent* Inventory)
	{
		FString Text;
		for (const EDCEquipSlot EquipSlot : { EDCEquipSlot::Head, EDCEquipSlot::Body, EDCEquipSlot::Vest, EDCEquipSlot::Back, EDCEquipSlot::LeftHand, EDCEquipSlot::RightHand })
		{
			Text += DescribeItemForCompare(Inventory ? Inventory->GetSlotItem(EquipSlot) : nullptr) + TEXT("|");
		}
		return Text;
	}

	/** Doors the way passes through, in the order they are met, with the distance walked up to each (cm). */
	TArray<TPair<FName, double>> DoorCrossings(UWorld* World, const TArray<FVector>& Path)
	{
		TArray<TPair<FName, double>> Crossings;
		double Walked = 0.0;
		for (int32 Point = 1; Point < Path.Num(); ++Point)
		{
			const double Segment = FVector::Dist2D(Path[Point - 1], Path[Point]);
			for (TActorIterator<ADCDoor> It(World); It; ++It)
			{
				const FName Name = It->GetFName();
				if (!Crossings.ContainsByPredicate([Name](const TPair<FName, double>& Entry) { return Entry.Key == Name; })
					&& It->CrossesOpening(Path[Point - 1], Path[Point]))
				{
					const double Into = FMath::Clamp(FVector::Dist2D(Path[Point - 1], It->GetCenter()), 0.0, Segment);
					Crossings.Add({ Name, Walked + Into });
				}
			}
			Walked += Segment;
		}
		return Crossings;
	}

	/** Doors the way passes through (actor names), in the order they are met. */
	TArray<FName> DoorsOnPath(UWorld* World, const TArray<FVector>& Path)
	{
		TArray<FName> Doors;
		for (int32 Point = 1; Point < Path.Num(); ++Point)
		{
			for (TActorIterator<ADCDoor> It(World); It; ++It)
			{
				if (!Doors.Contains(It->GetFName()) && It->CrossesOpening(Path[Point - 1], Path[Point]))
				{
					Doors.Add(It->GetFName());
				}
			}
		}
		return Doors;
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
					Item->AddContent(Built, Pickup.Items[Child].GridPos);
				}
			}
		}
		return Item;
	}

	/** All stretches joined into one line (for drawing the route). */
	TArray<FVector> JoinLegs(const TArray<FDCWalkLeg>& Legs)
	{
		TArray<FVector> Points;
		for (const FDCWalkLeg& Leg : Legs)
		{
			for (int32 Point = 0; Point < Leg.Path.Num(); ++Point)
			{
				if (Points.IsEmpty() || FVector::DistSquared(Points.Last(), Leg.Path[Point]) > 1.0)
				{
					Points.Add(Leg.Path[Point]);
				}
			}
		}
		return Points;
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

TArray<FDCPlanProblem> FDCSchedule::FindProblems() const
{
	TArray<FDCPlanProblem> Problems;
	for (const FDCCrewSchedule& Crew : Crews)
	{
		const FDCScheduledTask* First = Crew.Tasks.FindByPredicate([](const FDCScheduledTask& Task) { return Task.bBlocked && !Task.bAfterBlocked; });
		if (!First)
		{
			continue;
		}
		FDCPlanProblem& Problem = Problems.AddDefaulted_GetRef();
		Problem.Crew = Crew.Crew;
		Problem.CrewName = Crew.Name;
		Problem.Index = First->Index;
		Problem.Label = First->Label;
		Problem.ShortReason = First->ShortReason.IsEmpty() ? First->WaitReason : First->ShortReason;
		Problem.Reason = First->WaitReason;
		Problem.Time = First->Ready;
		Problem.Where = First->EndLocation.IsNearlyZero() ? Crew.StartLocation : First->EndLocation;
	}
	Problems.Sort([](const FDCPlanProblem& A, const FDCPlanProblem& B) { return A.Time < B.Time; });
	return Problems;
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
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&InWorld))
	{
		Navigation->OnNavigationGenerationFinishedDelegate.AddDynamic(this, &UDCHeistStateSubsystem::OnNavigationRebuilt);
	}
}

void UDCHeistStateSubsystem::OnNavigationRebuilt(ANavigationData* NavData)
{
	// Only while planning: during playback the schedule stays as it was when it started.
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(GetWorld());
	if (Plans && Plans->GetMode() == EDCPlanMode::Planning)
	{
		Refresh();
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
			Sim.GridPos = Item->GetGridPos();
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
		// Named after the room it ends in: "gehen (Flur)".
		if (const ADCRoomVolume* Room = ADCRoomVolume::FindAt(this, Task.Location))
		{
			return FText::Format(LOCTEXT("TaskMoveRoom", "gehen ({0})"), Room->GetRoomName());
		}
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
	{
		const ADCCrewCharacter* For = Task.Target.IsNone() ? nullptr : FindCrew(Task.Target);
		return For ? FText::Format(LOCTEXT("TaskSignalFor", "{0} das Zeichen geben"), For->GetCrewName())
			: FText::Format(LOCTEXT("TaskSignal", "Zeichen {0} geben"), FText::AsNumber(Task.Action));
	}
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
			return FText::Format(LOCTEXT("Loot", "{0} durchsuchen"), Name);
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
		// Name the step the signal comes after: that is what the player chose.
		const TArray<FDCPlanTask>& Theirs = Plans->GetTasks(Task.Target);
		const FDCPlanTask* Before = Theirs.IsValidIndex(RefIndex - 1) && Theirs[RefIndex - 1].Type != EDCPlanTaskType::WaitFor ? &Theirs[RefIndex - 1] : nullptr;
		if (bWaiting)
		{
			return FText::Format(LOCTEXT("WaitingSignal", "wartet auf das Zeichen von {0}"), OtherName);
		}
		return Before
			? FText::Format(LOCTEXT("TaskWaitSignalAfter", "auf das Zeichen von {0} warten (kommt nach „{1}“)"), OtherName, DescribeTask(*Before))
			: FText::Format(LOCTEXT("TaskWaitSignal", "auf das Zeichen von {0} warten"), OtherName);
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
			Out.ShortReason = Reason;
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
				Rest.bAfterBlocked = true;
				Rest.WaitReason = LOCTEXT("AfterBlocked", "kommt nicht dran (Schritt davor geht nicht)");
				Result.Crews[Cursor.CrewIndex].Tasks.Add(Rest);
			}
			Cursor.bFinished = true;
		};
		const auto UseCheck = [&](const FDCTaskCheck& Check)
		{
			Duration = Check.Seconds;
			Out.FailChance = Check.FailChance;
			Out.bHasChance = true;
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
				// Open what can be opened (on a scratch copy). What lies inside is not known while planning:
				// the player decides during the heist, so nothing goes into the planned inventory here.
				FDCSimPickup Scratch = *SimPickup;
				TArray<int32> Opened;
				for (int32 Guard = 0; Guard < 100; ++Guard)
				{
					int32 ToOpen = INDEX_NONE;
					for (int32 Candidate = 0; Candidate < Scratch.Items.Num() && ToOpen == INDEX_NONE; ++Candidate)
					{
						const FDCSimItem& Sim = Scratch.Items[Candidate];
						if (!Sim.bTaken && FindBlocker(Scratch, Candidate) == INDEX_NONE && Sim.bOpenable && !Sim.IsOpen() && !Sim.bLocked)
						{
							ToOpen = Candidate;
						}
					}
					if (ToOpen == INDEX_NONE)
					{
						break;
					}
					Scratch.Items[ToOpen].bOpen = true;
					Opened.Add(ToOpen);
				}
				// Searching takes longer the bigger the grids (the container and its drawers: a floor plan shows those).
				int32 Cells = 0;
				for (int32 Part = 0; Part < SimPickup->Items.Num(); ++Part)
				{
					const FDCSimItem& Sim = SimPickup->Items[Part];
					if (Part == 0 || (Sim.Parent == 0 && Sim.bAttached))
					{
						const FDCItemDef* PartDef = DCItems::Find(Sim.DefId);
						Cells += PartDef ? PartDef->GetCellCount() : 0;
					}
				}
				const TArray<int32> Taken;
				Duration = 0.3f + SimLootStepSeconds * Opened.Num() + DCItems::SearchSeconds(Cells);
				Out.Detail = FText::Format(LOCTEXT("SearchEstimate", "ca. {0} s · Inhalt unbekannt"), FText::AsNumber(FMath::RoundToInt(Duration)));
				Out.Carry = LOCTEXT("SearchDecide", "was gefunden wird, entscheidest du im Einbruch");
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
				const TArray<FDCPlanTask>& Theirs = Plans->GetTasks(Task.Target);
				Out.WaitForLabel = Theirs.IsValidIndex(RefIndex) ? DescribeTask(Theirs[RefIndex]) : FText::GetEmpty();
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

		if (!Out.WayNote.IsEmpty())
		{
			Out.Detail = Out.Detail.IsEmpty() ? Out.WayNote : FText::Format(LOCTEXT("DetailWithWay", "{0} · {1}"), Out.Detail, Out.WayNote);
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
		// Coming back after waiting for a door: keep what is known about the step and the wait.
		if (!Cursor.bWalkPending)
		{
			Out = FDCScheduledTask();
			Out.Index = Cursor.Next;
			Out.Type = Task.Type;
			Out.Start = Cursor.Time;
			Out.Label = DescribeTask(Task);
		}
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
				Spot = Pickup->GetActorLocation() + Away * (Pickup->GetFootprintRadius() + SimItemReach);
				Out.bHasLocation = true;
			}
			break;
		}
		TArray<FVector> Way;
		const float Walk = WalkSeconds(World, Cursor.Location, Spot, Cursor.Speed, &Way);
		// Doors on the way, in the order they are met: a closed one she opens in passing (a hand free
		// is enough). At a locked one - or a closed one with both hands full - she walks up to it and
		// waits beside it until someone else changes it (or forever: "geht nicht"), then goes on.
		const TArray<TPair<FName, double>> Crossings = DoorCrossings(World, Way);
		int32 Blocking = INDEX_NONE;
		for (int32 Crossing = 0; Crossing < Crossings.Num() && Blocking == INDEX_NONE; ++Crossing)
		{
			const FDCSimDoor* Door = State.FindDoor(Crossings[Crossing].Key);
			const ADCDoor* Actor = FindDoor(Crossings[Crossing].Key);
			if (!Door || Door->bOpen || !Actor)
			{
				continue;
			}
			const bool bCanOpen = !Door->bLocked && Cursor.Crew && Cursor.Crew->PlanCheckDoor(Actor, EDCDoorAction::Open, Cursor.Inventory).bPossible;
			if (!bCanOpen)
			{
				Blocking = Crossing;
			}
		}
		// Opening a door in passing: hand on the handle and the swing; the door is open from then on.
		const auto OpenOnTheWay = [&](const TArray<FVector>& Path, float From) -> float
		{
			float Extra = 0.f;
			for (const TPair<FName, double>& Crossing : DoorCrossings(World, Path))
			{
				const FDCSimDoor* Door = State.FindDoor(Crossing.Key);
				if (!Door || Door->bOpen || Door->bLocked)
				{
					continue;
				}
				const float Reached = From + static_cast<float>(Crossing.Value / FMath::Max(50.f, Cursor.Speed)) + Extra;
				Extra += HandSeconds + SwingSeconds;
				const FName DoorName = Crossing.Key;
				AddEvent(Cursor, Reached + HandSeconds, [DoorName](FDCWorldSnapshot& S) { if (FDCSimDoor* D = S.FindDoor(DoorName)) { D->bOpen = true; } });
				Out.OpenedOnWay.AddUnique(DoorName);
			}
			return Extra;
		};
		if (Blocking != INDEX_NONE)
		{
			const FName DoorName = Crossings[Blocking].Key;
			const ADCDoor* Actor = FindDoor(DoorName);
			const FDCSimDoor* Door = State.FindDoor(DoorName);
			// Up to the door, beside it: whoever unlocks it needs the spot in front of the handle.
			double Total = 0.0;
			for (int32 Point = 1; Point < Way.Num(); ++Point)
			{
				Total += FVector::Dist2D(Way[Point - 1], Way[Point]);
			}
			const FVector Before = AlongPath(Way, Total > 0.0 ? static_cast<float>(FMath::Max(0.0, Crossings[Blocking].Value - 60.0) / Total) : 0.f);
			const FVector Near = Actor->GetStandPoint(Before, false) + Actor->GetActorRightVector().GetSafeNormal2D() * SimDoorWaitAside;
			if (FVector::Dist2D(Cursor.Location, Near) > 40.0)
			{
				FDCWalkLeg Leg;
				const float LegWalk = WalkSeconds(World, Cursor.Location, Near, Cursor.Speed, &Leg.Path);
				const float Extra = OpenOnTheWay(Leg.Path, Cursor.Time);
				Leg.Start = Cursor.Time;
				Leg.End = Cursor.Time + LegWalk + Extra;
				Out.Legs.Add(Leg);
				Cursor.Time = Leg.End;
				Cursor.Location = Near;
			}
			Cursor.bWalkPending = true;
			Cursor.WaitDoors = { DoorName };
			Cursor.bWaitForLock = Door && Door->bLocked;
			Cursor.bBlocked = true;
			Cursor.BlockedSince = Cursor.Time;
			Out.bWalkBlocked = true;
			Out.WalkStart = Out.Legs.IsEmpty() ? Cursor.Time : Out.Legs[0].Start;
			Out.Ready = Cursor.Time;
			Out.Path = Out.Legs.IsEmpty() ? TArray<FVector>{ Cursor.Location } : JoinLegs(Out.Legs);
			// The marker stands where she waits (not at the world origin).
			Out.EndLocation = Cursor.Location;
			const FText Name = Actor->GetInteractableName();
			Out.WaitReason = Door && Door->bLocked
				? FText::Format(LOCTEXT("WaitAtLockedDoor", "wartet an der {0}, bis sie aufgesperrt ist"), Name)
				: FText::Format(LOCTEXT("WaitAtShutDoor", "wartet an der {0}, bis sie offen ist (keine Hand frei)"), Name);
			return;
		}
		Cursor.bWalkPending = false;
		Cursor.WaitDoors.Reset();
		Out.bWalkBlocked = false;
		FDCWalkLeg Leg;
		Leg.Path = Way;
		Leg.Start = Cursor.Time;
		const float Extra = OpenOnTheWay(Way, Cursor.Time);
		Leg.End = Cursor.Time + Walk + Extra;
		Out.Legs.Add(Leg);
		Out.Path = JoinLegs(Out.Legs);
		Out.WalkStart = Out.Legs[0].Start;
		TArray<FText> OpenedNames;
		for (const FName DoorName : Out.OpenedOnWay)
		{
			const ADCDoor* Actor = FindDoor(DoorName);
			OpenedNames.Add(Actor ? Actor->GetInteractableName() : FText::FromName(DoorName));
		}
		if (!OpenedNames.IsEmpty())
		{
			Out.WayNote = FText::Format(LOCTEXT("OpensOnWay", "\u00f6ffnet unterwegs {0}"), JoinNames(OpenedNames));
		}
		Out.Ready = Leg.End;
		Cursor.Location = Spot;
		Out.EndLocation = Spot;
		Cursor.Time = Out.Ready;
	};

	// Applies an event; crew members waiting for it may continue.
	const auto ApplyEvent = [&](const FSimEvent& Event)
	{
		// The doors that waiting walks stand at, as they were before: only the right change frees them.
		TMap<FName, TPair<bool, bool>> DoorsBefore;
		for (const FCursor& Cursor : Cursors)
		{
			if (Cursor.bBlocked && Cursor.bWalkPending)
			{
				for (const FName DoorName : Cursor.WaitDoors)
				{
					if (const FDCSimDoor* Door = State.FindDoor(DoorName))
					{
						DoorsBefore.Add(DoorName, TPair<bool, bool>(Door->bLocked, Door->bOpen));
					}
				}
			}
		}
		Event.Apply(State);
		for (FCursor& Cursor : Cursors)
		{
			if (!Cursor.bBlocked)
			{
				continue;
			}
			bool bFree = false;
			if (Cursor.bWalkPending)
			{
				// A waiting walk tries again (BeginTask looks again) once its door got unlocked or opened -
				// with both hands full only once it got opened.
				const bool bForLock = Cursor.bWaitForLock;
				bFree = Cursor.WaitDoors.ContainsByPredicate([&State, &DoorsBefore, bForLock](FName DoorName)
				{
					const FDCSimDoor* Door = State.FindDoor(DoorName);
					const TPair<bool, bool>* Before = DoorsBefore.Find(DoorName);
					if (!Door || !Before)
					{
						return false;
					}
					const bool bOpened = !Before->Value && Door->bOpen;
					const bool bUnlocked = Before->Key && !Door->bLocked;
					return bOpened || (bForLock && bUnlocked);
				});
			}
			else
			{
				FText Ignored;
				bFree = ConditionHolds((*Cursor.Tasks)[Cursor.Next], Ignored);
			}
			if (bFree)
			{
				Cursor.bBlocked = false;
				Cursor.Time = FMath::Max(Cursor.BlockedSince, Event.Time);
				if (Result.Crews.IsValidIndex(Event.CrewIndex) && Event.CrewIndex != Cursor.CrewIndex)
				{
					Cursor.Pending.WaitFor = Result.Crews[Event.CrewIndex].Crew;
					Cursor.Pending.WaitForName = Result.Crews[Event.CrewIndex].Name;
					Cursor.Pending.WaitForIndex = Event.TaskIndex;
					const FCursor& By = Cursors[Event.CrewIndex];
					Cursor.Pending.WaitForLabel = By.Tasks && By.Tasks->IsValidIndex(Event.TaskIndex) ? DescribeTask((*By.Tasks)[Event.TaskIndex]) : FText::GetEmpty();
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
		if (!Next->bHasPending || Next->bWalkPending)
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

	// A door that never opens in the plan: say what would help ("Vic könnte sie knacken (ca. 2 s)").
	const auto DescribeDoorNeverOpens = [&](const FCursor& Stuck) -> FText
	{
		const ADCDoor* Actor = FindDoor(Stuck.WaitDoors[0]);
		const FText Name = Actor ? Actor->GetInteractableName() : FText::FromName(Stuck.WaitDoors[0]);
		if (!Stuck.bWaitForLock)
		{
			return FText::Format(LOCTEXT("NeverHandsFree", "{0}: keine Hand frei – erst ablegen oder jemand öffnet sie"), Name);
		}
		const FCursor* Helper = nullptr;
		float Fastest = TNumericLimits<float>::Max();
		for (const FCursor& Other : Cursors)
		{
			if (!Actor || !Other.Crew)
			{
				continue;
			}
			const FDCTaskCheck Check = Other.Crew->PlanCheckDoor(Actor, EDCDoorAction::PickLock, Other.Inventory);
			// The stuck one wins a tie: then nobody has to wait for anybody.
			const float Seconds = Check.Seconds - (&Other == &Stuck ? 0.01f : 0.f);
			if (Check.bPossible && Seconds < Fastest)
			{
				Fastest = Seconds;
				Helper = &Other;
			}
		}
		if (!Helper)
		{
			return FText::Format(LOCTEXT("NeverPickNobody", "{0} bleibt verschlossen – niemand hier kann sie knacken"), Name);
		}
		const FText About = FText::AsNumber(FMath::Max(1, FMath::RoundToInt(Fastest)));
		return Helper == &Stuck
			? FText::Format(LOCTEXT("NeverPickSelf", "{0} bleibt verschlossen – {1} könnte sie selbst knacken (ca. {2} s)"), Name, Stuck.Crew->GetCrewName(), About)
			: FText::Format(LOCTEXT("NeverPickOther", "{0} bleibt verschlossen – {1} könnte sie knacken (ca. {2} s)"), Name, Helper->Crew->GetCrewName(), About);
	};

	// Whoever still waits, waits forever.
	for (FCursor& Cursor : Cursors)
	{
		if (Cursor.bBlocked)
		{
			Cursor.Pending.bBlocked = true;
			Cursor.Pending.WorkStart = Cursor.Pending.End = Cursor.BlockedSince;
			const FText Reason = Cursor.Pending.WaitReason;
			Cursor.Pending.WaitReason = Cursor.bWalkPending && !Cursor.WaitDoors.IsEmpty()
				? DescribeDoorNeverOpens(Cursor)
				: FText::Format(LOCTEXT("NeverHappens", "{0} – passiert im Plan nie"), Reason);
			Cursor.Pending.ShortReason = Reason;
			if (Cursor.bWalkPending && !Cursor.WaitDoors.IsEmpty())
			{
				const ADCDoor* Stuck = FindDoor(Cursor.WaitDoors[0]);
				const FText DoorName = Stuck ? Stuck->GetInteractableName() : FText::FromName(Cursor.WaitDoors[0]);
				Cursor.Pending.ShortReason = Cursor.bWaitForLock
					? FText::Format(LOCTEXT("ShortDoorLocked", "{0} bleibt verschlossen"), DoorName)
					: FText::Format(LOCTEXT("ShortHandsFull", "{0}: keine Hand frei"), DoorName);
			}
			Cursor.Time = Cursor.BlockedSince;
			Result.Crews[Cursor.CrewIndex].Tasks.Add(Cursor.Pending);
			for (++Cursor.Next; Cursor.Next < Cursor.Tasks->Num(); ++Cursor.Next)
			{
				FDCScheduledTask Rest;
				Rest.Index = Cursor.Next;
				Rest.Start = Rest.Ready = Rest.WorkStart = Rest.End = Cursor.BlockedSince;
				Rest.bBlocked = true;
				Rest.Label = DescribeTask((*Cursor.Tasks)[Cursor.Next]);
				Rest.bAfterBlocked = true;
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
	// Keep the planned inventories while their contents stay the same: windows that show them
	// rebuild when the object changes, and the planning refreshes twice a second (no flicker).
	for (const TPair<FName, UDCInventoryComponent*>& Entry : Inventories)
	{
		const FString Contents = DescribeInventoryForCompare(Entry.Value);
		const FString* Known = PlannedInventorySignatures.Find(Entry.Key);
		if (!Known || *Known != Contents || !PlannedInventories.Contains(Entry.Key))
		{
			PlannedInventories.Add(Entry.Key, Entry.Value);
			PlannedInventorySignatures.Add(Entry.Key, Contents);
		}
	}
	for (auto It = PlannedInventories.CreateIterator(); It; ++It)
	{
		if (!Inventories.Contains(It.Key()))
		{
			PlannedInventorySignatures.Remove(It.Key());
			It.RemoveCurrent();
		}
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
		if (!Task.Legs.IsEmpty())
		{
			// Stretch by stretch; in between (and before the first) she waits where the stretch begins.
			for (const FDCWalkLeg& Leg : Task.Legs)
			{
				if (Time < Leg.Start)
				{
					return Leg.Path.IsEmpty() ? Location : Leg.Path[0];
				}
				if (Time < Leg.End && Leg.Path.Num() >= 2)
				{
					return AlongPath(Leg.Path, (Time - Leg.Start) / FMath::Max(0.01f, Leg.End - Leg.Start));
				}
				if (!Leg.Path.IsEmpty())
				{
					Location = Leg.Path.Last();
				}
			}
		}
		else if (Time < Task.Ready && Task.Path.Num() >= 2)
		{
			return AlongPath(Task.Path, (Time - Task.Start) / FMath::Max(0.01f, Task.Ready - Task.Start));
		}
		else if (!Task.Path.IsEmpty())
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
	if (Task->IsWaitingOnTheWay(Time))
	{
		return Task->WaitFor.IsNone()
			? FText::Format(LOCTEXT("ActivityWaitDoor", "{0}. {1}"), Number, Task->WaitReason)
			: (Task->WaitForLabel.IsEmpty()
				? FText::Format(LOCTEXT("ActivityWaitFor", "{0}. wartet auf {1}"), Number, Task->WaitForName)
				: FText::Format(LOCTEXT("ActivityWaitForStep", "{0}. wartet auf {1} ({2})"), Number, Task->WaitForName, Task->WaitForLabel));
	}
	if (Time < Task->Ready)
	{
		return FText::Format(LOCTEXT("ActivityWalk", "{0}. geht hin: {1}"), Number, Task->Label);
	}
	if (Time < Task->WorkStart)
	{
		return Task->WaitFor.IsNone()
			? FText::Format(LOCTEXT("ActivityWait", "{0}. {1}"), Number, Task->WaitReason)
			: (Task->WaitForLabel.IsEmpty()
				? FText::Format(LOCTEXT("ActivityWaitFor", "{0}. wartet auf {1}"), Number, Task->WaitForName)
				: FText::Format(LOCTEXT("ActivityWaitForStep", "{0}. wartet auf {1} ({2})"), Number, Task->WaitForName, Task->WaitForLabel));
	}
	return FText::Format(LOCTEXT("ActivityWork", "{0}. {1}"), Number, Task->Label);
}

FText UDCHeistStateSubsystem::DescribePlannedDoor(const ADCDoor* Door) const
{
	if (!Door)
	{
		return FText::GetEmpty();
	}
	bool bLocked = Door->IsLocked();
	bool bOpen = Door->IsOpen();
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(GetWorld());
	if (Plans && Plans->GetMode() == EDCPlanMode::Planning)
	{
		if (const FDCSimDoor* Planned = const_cast<FDCWorldSnapshot&>(PlannedState).FindDoor(Door->GetFName()))
		{
			bLocked = Planned->bLocked;
			bOpen = Planned->bOpen;
		}
	}
	const FText State = bOpen ? LOCTEXT("DoorStateOpen", "offen") : bLocked ? LOCTEXT("DoorStateLocked", "verschlossen") : LOCTEXT("DoorStateShut", "zu");
	return FText::Format(LOCTEXT("PlannedDoor", "{0} · {1}"), Door->GetInteractableName(), State);
}

FText UDCHeistStateSubsystem::DescribeWaited(const FDCScheduledTask& Task, float Waited)
{
	const FText Seconds = FText::AsNumber(FMath::Max(0, FMath::RoundToInt(Waited)));
	if (Task.WaitFor.IsNone())
	{
		return FText::Format(LOCTEXT("WaitedPlain", "wartet {0} s – {1}"), Seconds, Task.WaitReason);
	}
	if (Task.WaitForLabel.IsEmpty())
	{
		return FText::Format(LOCTEXT("WaitedForStep", "wartet {0} s, bis {1} Schritt {2} erledigt hat"), Seconds, Task.WaitForName, FText::AsNumber(Task.WaitForIndex + 1));
	}
	return FText::Format(LOCTEXT("WaitedForLabel", "wartet {0} s auf {1} ({2}. {3})"), Seconds, Task.WaitForName, FText::AsNumber(Task.WaitForIndex + 1), Task.WaitForLabel);
}

bool UDCHeistStateSubsystem::CanReach(const FVector& From, const FVector& To) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		return true;
	}
	FNavLocation Goal;
	if (!Navigation->ProjectPointToNavigation(To, Goal, FVector(200.0, 200.0, 300.0)))
	{
		return false;
	}
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, Goal.Location);
	if (Path && Path->IsValid() && !Path->IsPartial())
	{
		return true;
	}
	TArray<FVector> Through;
	return WayThroughDoors(World, From, Goal.Location, Through);
}

TArray<ADCDoor*> UDCHeistStateSubsystem::FindDoorsOnWay(const FVector& From, const FVector& To) const
{
	TArray<ADCDoor*> Closed;
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		return Closed;
	}
	// Furniture stands off the nav mesh: aim at the nearest walkable spot.
	FNavLocation Goal;
	if (!Navigation->ProjectPointToNavigation(To, Goal, FVector(200.0, 200.0, 300.0)))
	{
		UE_LOG(LogDerClou, Log, TEXT("No nav mesh near %s"), *To.ToCompactString());
		return Closed;
	}
	// The way as it is now is free: nothing in the way.
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, Goal.Location);
	if (Path && Path->IsValid() && !Path->IsPartial())
	{
		return Closed;
	}
	TArray<FVector> Through;
	if (!WayThroughDoors(World, From, Goal.Location, Through))
	{
		UE_LOG(LogDerClou, Log, TEXT("No way from %s to %s, not even through doors"), *From.ToCompactString(), *Goal.Location.ToCompactString());
		return Closed;
	}
	// The nav mesh does not let one through yet: every door on the way still counts as shut
	// (even one that is swinging open right now – its way is free a moment later).
	for (const FName DoorName : DoorsOnPath(World, Through))
	{
		if (ADCDoor* Door = FindDoor(DoorName))
		{
			Closed.Add(Door);
		}
	}
	return Closed;
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
