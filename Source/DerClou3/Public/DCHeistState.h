#pragma once

#include "CoreMinimal.h"
#include "DCPlan.h"
#include "DCWorldBadges.h"
#include "Subsystems/WorldSubsystem.h"

#include "DCHeistState.generated.h"

class ADCCrewCharacter;
class ADCDoor;
class ADCItemPickup;
class ADCPlanTrail;
class ADCPlanGhost;
class ANavigationData;
class UDCInventoryComponent;
class UDCItem;

/** State of one item in the planning simulation (mirrors the real item tree). */
struct FDCSimItem
{
	FName DefId;
	int32 Parent = INDEX_NONE;
	TArray<int32> Children;
	bool bOpenable = false;
	bool bOpen = false;
	bool bLocked = false;
	bool bDestroyed = false;
	bool bTaken = false;
	bool bAttached = false;
	bool bMovable = true;
	int32 Value = 0;
	/** Cell inside the parent container (AnyCell if none). */
	FIntPoint GridPos = FIntPoint(-1, -1);

	bool IsOpen() const { return !bOpenable || bOpen || bDestroyed; }
};

/** A pickup in the simulation; Items[0] is its own item. */
struct FDCSimPickup
{
	FName Actor;
	TArray<FDCSimItem> Items;
	bool bGone = false;
};

struct FDCSimDoor
{
	FName Actor;
	bool bLocked = false;
	bool bOpen = false;
	/** Which way the panel stands once open (+1/-1, 0 = as the real door). */
	float Swing = 0.f;
};

/** A door changing in the plan: who changes it when, and how it is afterwards. */
struct FDCDoorChange
{
	float Time = 0.f;
	FName Door;
	bool bLocked = false;
	bool bOpen = false;
	float Swing = 0.f;
	/** Whose step (index into the schedule's crews, position in their plan). */
	int32 CrewIndex = INDEX_NONE;
	int32 TaskIndex = INDEX_NONE;
	/** Before the change. */
	bool bWasLocked = false;
	bool bWasOpen = false;
};

/** What the world looks like at one moment of the plan: doors and the state of every item. */
struct FDCWorldSnapshot
{
	TArray<FDCSimDoor> Doors;
	TArray<FDCSimPickup> Pickups;
	/** Steps that are over (by id) – what "wait for" and signals react to. */
	TSet<FGuid> DoneTasks;
	/** Crew members through with their whole plan. */
	TSet<FName> DoneCrews;

	FDCSimDoor* FindDoor(FName Actor);
	FDCSimPickup* FindPickup(FName Actor);
	/** Index of the item at Path in the pickup (INDEX_NONE if there is none). */
	static int32 FindItem(const FDCSimPickup& Pickup, const FString& Path);
	/** All containers around the item are open (and nothing was taken away). */
	static bool IsReachable(const FDCSimPickup& Pickup, int32 Index);
};

/** One stretch of walking in a step; between two stretches the crew member waits (at a locked door). */
struct FDCWalkLeg
{
	TArray<FVector> Path;
	float Start = 0.f;
	float End = 0.f;
};

/** One planned step as the simulation sees it: when it starts, waits, works and ends. */
struct FDCScheduledTask
{
	int32 Index = 0;
	EDCPlanTaskType Type = EDCPlanTaskType::Move;
	/** Starts walking / doing it. */
	float Start = 0.f;
	/** Starts walking: later than Start if a closed door on the way has to be opened first. */
	float WalkStart = 0.f;
	/** Arrived and could act. */
	float Ready = 0.f;
	/** Actually starts working (after waiting for someone else). */
	float WorkStart = 0.f;
	float End = 0.f;
	/** Will never happen (a condition is never met, or the crew member cannot do it). */
	bool bBlocked = false;
	/** Blocked before even starting to walk (a locked door on the way): stays where the walk would begin. */
	bool bWalkBlocked = false;
	/** Blocked only because an earlier step of the same crew member is ("kommt nicht dran"). */
	bool bAfterBlocked = false;
	/** Doors opened on the way (closed but not locked: the crew member opens them in passing). */
	TArray<FName> OpenedOnWay;
	/** The walk in stretches: up to a locked door, wait beside it, on (one stretch without waiting). */
	TArray<FDCWalkLeg> Legs;
	/** Seconds spent waiting on the way (before or between the stretches). */
	float WaitedOnTheWay() const
	{
		float Waited = 0.f;
		float Previous = Start;
		for (const FDCWalkLeg& Leg : Legs)
		{
			Waited += FMath::Max(0.f, Leg.Start - Previous);
			Previous = Leg.End;
		}
		return Waited;
	}
	/** True while the crew member waits on the way (before or between the stretches). */
	bool IsWaitingOnTheWay(float Time) const
	{
		float Previous = Start;
		for (const FDCWalkLeg& Leg : Legs)
		{
			if (Time >= Previous && Time < Leg.Start)
			{
				return true;
			}
			Previous = Leg.End;
		}
		return bBlocked && bWalkBlocked && Time >= Previous;
	}
	/** "öffnet unterwegs Hintertür". */
	FText WayNote;
	/** Makes a loud noise. */
	bool bLoud = false;
	float FailChance = 0.f;
	/** Duration and chance come from a skill check (worth showing "17 s · 100 %"). */
	bool bHasChance = false;
	/** "Hintertür knacken". */
	FText Label;
	/** "ca. 3 s · Erfolg 100 % · Schlösser 6 (nötig 3): geübt". */
	FText Detail;
	/** Why it waits or is blocked. */
	FText WaitReason;
	/** Why it is blocked, in a few words for status lines ("Bürotür bleibt verschlossen"). */
	FText ShortReason;
	/** Who finished what this step waited for (NAME_None: nobody else). */
	FName WaitFor;
	FText WaitForName;
	int32 WaitForIndex = INDEX_NONE;
	/** That step as the list names it ("Hintertür knacken"). */
	FText WaitForLabel;
	/** What it changes in what the crew member carries: "→ rechte Hand", "nimmt Ring, Goldkette". */
	FText Carry;
	FVector EndLocation = FVector::ZeroVector;
	bool bHasLocation = false;
	/** The way there (Start..Ready). */
	TArray<FVector> Path;
	/** Walking while working (through a door), starting WorkPathDelay seconds into the work. */
	TArray<FVector> WorkPath;
	float WorkPathDelay = 0.f;
};

struct FDCCrewSchedule
{
	FName Crew;
	FText Name;
	FLinearColor Color = FLinearColor::White;
	TArray<FDCScheduledTask> Tasks;
	float End = 0.f;
	FVector StartLocation = FVector::ZeroVector;
};

/** A step that does not happen - the first one of a crew member (the ones after it follow from it). */
struct FDCPlanProblem
{
	FName Crew;
	FText CrewName;
	/** Position in the crew member's plan. */
	int32 Index = INDEX_NONE;
	FText Label;
	/** "Bürotür bleibt verschlossen". */
	FText ShortReason;
	/** The whole reason, with who could help. */
	FText Reason;
	/** When the crew member gets stuck, and where. */
	float Time = 0.f;
	FVector Where = FVector::ZeroVector;
};

struct FDCSchedule
{
	TArray<FDCCrewSchedule> Crews;
	float Duration = 0.f;

	const FDCCrewSchedule* Find(FName Crew) const;
	/** The steps that do not happen (first one per crew member), earliest first. */
	TArray<FDCPlanProblem> FindProblems() const;

	/** The doors at the start, and every change the plan makes to them, in time order. */
	TArray<FDCSimDoor> StartDoors;
	TArray<FDCDoorChange> DoorChanges;
	/** A door at a moment of the plan; By: the last change up to then (nullptr: none). False if unknown. */
	bool GetDoorAt(FName Door, float Time, FDCSimDoor& Out, const FDCDoorChange** By = nullptr) const;
};

namespace DCNav
{
	/**
	 * A complete way on the nav mesh as it is now (false: none, or only part of the way). Use this instead of
	 * UNavigationSystemV1::FindPathToLocationSynchronously: that one keeps every path alive until the next garbage
	 * collection, and whenever a door opens the engine invalidates and re-plans each of those paths. With the
	 * thousands of queries the planning makes, that froze the game for minutes (2026-11-09). Here the path
	 * object is gone as soon as the points are copied.
	 */
	DERCLOU3_API bool FindPath(UWorld* World, const FVector& From, const FVector& To, TArray<FVector>* OutPoints = nullptr, double* OutLength = nullptr);

	/**
	 * The walkable spot nearest to Location (seen from above, within Radius) on Location's own storey: looks
	 * 2.4 m down and 0.4 m up. The engine picks the nearest spot in 2D only, so a box reaching a storey down
	 * chose the cellar floor right under the living-room sofa (2026-11-09).
	 */
	DERCLOU3_API bool ProjectOnStorey(const UWorld* World, const FVector& Location, double Radius, FVector& Out);

	/**
	 * Where to stand to reach an item at ItemLocation (Reach = footprint radius + arm), coming from From: on the item's
	 * storey and in the item's room - a painting on an outside wall is not taken from the garden. Tries round the item,
	 * starting on the side that faces From, then a little further out; IsTaken rules out spots others already use.
	 * Planning and playback both use it, so they walk to the same place. False: nothing found (Out is a plain guess).
	 */
	DERCLOU3_API bool FindStandSpot(const UWorld* World, const FVector& ItemLocation, double Reach, const FVector& From, FVector& Out,
		TFunctionRef<bool(const FVector&)> IsTaken);
}

/**
 * The state manager of the heist. Knows every door, container and crew member by name, and
 * plays the whole plan through in advance: each step walks, may have to wait for a state
 * ("door unlocked") that another crew member's step brings about, then works for its time and
 * changes the state. This gives the timeline (who does what when, who waits for whom) and the
 * planned state of the world, which the planning menus use. During playback the crew checks
 * the real state the same way before every step.
 */
UCLASS()
class DERCLOU3_API UDCHeistStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDCHeistStateSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	//~ Lookups by actor name
	ADCDoor* FindDoor(FName Actor) const;
	ADCItemPickup* FindPickup(FName Actor) const;
	ADCCrewCharacter* FindCrew(FName Actor) const;
	/** The item at Path inside the pickup ("" = its own item). */
	static UDCItem* ResolveItem(ADCItemPickup* Pickup, const FString& Path);
	/** Path of an item inside its pickup. */
	static FString PathOf(const UDCItem* Item);

	//~ Planning
	/** The world as it is right now. */
	FDCWorldSnapshot TakeSnapshot() const;
	/**
	 * Plays all plans through (does not change the world). OutFinalState: the world after the
	 * plan; OutInventories: what each crew member carries then (throwaway copies).
	 */
	FDCSchedule Simulate(FDCWorldSnapshot* OutFinalState = nullptr, TMap<FName, UDCInventoryComponent*>* OutInventories = nullptr) const;
	/** Recomputes the schedule and the planned end state (and the markers in the world). */
	void Refresh(bool bMarkers = true);
	const FDCSchedule& GetSchedule() const { return Schedule; }
	/** Goes up whenever the schedule comes out different. */
	int32 GetScheduleVersion() const { return ScheduleVersion; }
	/** The world as it will be once the whole plan has run. */
	const FDCWorldSnapshot& GetPlannedState() const { return PlannedState; }
	/** What the crew member will carry once their whole plan has run (nullptr if unknown). */
	const UDCInventoryComponent* GetPlannedInventory(FName Crew) const;
	/**
	 * The doors on the way from From to To, in the order they are met (the way the crew would take
	 * once they are open). Empty when the nav mesh lets one through as it is.
	 */
	TArray<ADCDoor*> FindDoorsOnWay(const FVector& From, const FVector& To) const;
	/** The nav mesh is built (after loading a large level it takes a few seconds). */
	static bool IsNavigationReady(const UObject* WorldContext);
	/** True if From reaches To on foot - directly or through doors (open or not). */
	bool CanReach(const FVector& From, const FVector& To) const;
	/** Short name of a step: "Hintertür knacken", "Wandtresor ausräumen". */
	FText DescribeTask(const FDCPlanTask& Task) const;
	/** A wait-for step: "warten, bis Rosa „Wandtresor knacken“ erledigt hat" – or, bWaiting, "wartet, bis …". */
	FText DescribeWaitFor(const FDCPlanTask& Task, bool bWaiting) const;
	/** A door as the plan leaves it (while planning) or as it is: "Hintertür · aufgesperrt (Vic, 0:04)", "… · zu", "… · offen". */
	FText DescribePlannedDoor(const ADCDoor* Door) const;
	/** The moment the room shows while planning: the preview time, else the end of the picked step, else the end of the plan. */
	float GetViewTime() const;
	/** "Raum zeigt: nach dem Plan (0:41)", "Raum zeigt: nach Rosas Schritt 2 (0:12)", "Raum zeigt: 0:20 (Zeitleiste)". */
	FText DescribeViewMoment() const;
	/** Doors show the plan at the view time while planning, their real state otherwise. */
	void UpdateWorldLook();
	/** The ghost of the selected crew member (nullptr while none is shown). */
	ADCPlanGhost* GetFigureGhost() const { return FigureGhost.Get(); }
	/**
	 * Where the crew member's plan has got to, as the room shows it while planning: at the preview time, else where
	 * their ghost stands (after the picked step, at the end of their plan, or where they get stuck). False: no plan known.
	 */
	bool GetPlanSpot(FName Crew, FVector& Out) const;
	/** The wait before a step: "wartet 4 s auf Vic (1. Hintertür knacken)" – or "wartet 4 s – {reason}". */
	static FText DescribeWaited(const FDCScheduledTask& Task, float Waited);

	/** Where the crew member is at this time of the plan. */
	static FVector GetPositionAt(const FDCCrewSchedule& Crew, float Time);
	/** What the crew member does then: "Schritt 2: Hintertür knacken", "wartet auf Vic", "fertig". */
	static FText DescribeActivityAt(const FDCCrewSchedule& Crew, float Time);
	/** The step that runs at this time (INDEX_NONE before the first or after the last). */
	static int32 GetStepAt(const FDCCrewSchedule& Crew, float Time);

	/** Whose plan is being edited (bright, numbered markers) and which step is picked. */
	void SetFocus(FName Crew, int32 Step = INDEX_NONE);
	FName GetFocusCrew() const { return FocusCrew; }
	int32 GetFocusStep() const { return FocusStep; }
	/** Planning preview: signs show where everyone is at this time of the plan (< 0 = off). */
	void SetPreviewTime(float Time);
	/** Signs over the room while planning (UDCWorldBadges draws them): steps per place, doors the plan changes, the preview moment. */
	const TArray<FDCWorldBadge>& GetStepBadges() const { return StepBadges; }
	const TArray<FDCWorldBadge>& GetDoorBadges() const { return DoorBadges; }
	const TArray<FDCWorldBadge>& GetMomentBadges() const { return MomentBadges; }
	float GetPreviewTime() const { return PreviewTime; }

	//~ Playback
	void StartPlayback();
	bool IsPlaying() const { return PlaybackStart >= 0.0; }
	/** Seconds since playback started (game time, so speed and pause count). */
	float GetPlaybackTime() const;
	/** All crew members are through with their plans. */
	bool IsPlaybackFinished() const;
	/** The world was changed (played or controlled directly) since the level was loaded. */
	bool IsWorldUsed() const { return bWorldUsed; }
	void MarkWorldUsed() { bWorldUsed = true; }

private:
	void OnPlanChanged();
	/** The nav mesh was rebuilt (a door opened, the level loaded): ways may have changed. */
	UFUNCTION()
	void OnNavigationRebuilt(ANavigationData* NavData);
	/** The nav mesh finished building at least once in this world (a level that rebuilds on load has no usable one before). */
	bool bNavigationGenerated = false;
	/** World time of the last "nav mesh finished" (a rebuild after loading comes in many batches). */
	double LastNavigationGenerated = -1.0;
	/**
	 * Play in the editor copies the editor's nav mesh instead of building its own. Pressed while the editor was
	 * still building it (the villa takes many seconds), the game kept a half-built or empty one for good: nobody
	 * could walk ("Dort kann niemand stehen"). So the PIE world always builds its own at start.
	 */
	bool bRebuildingAtStart = false;
	/** Footprints, rings and step signs of all plans (planning only). */
	void UpdateMarkers();
	/** The signs of the preview moment: where everyone is at the preview time. */
	void UpdateGhosts();
	/** Footprints along the crew member's planned ways; those after FadeAfter are paler. */
	void AddTrailFor(ADCPlanTrail& Out, const FDCCrewSchedule& Crew, bool bFocus, float FadeAfter) const;
	/** One sign per place with the steps done there, and a ring under it. */
	void AddStepBadgesFor(ADCPlanTrail& Out, const FDCCrewSchedule& Crew, const FDCCrewPlan* Plan, bool bFocus);
	/** The icon of a step on its sign (a safe's lock shows the safe). */
	FName BadgeIconFor(const FDCPlanTask& Task) const;
	/** Planning without a preview: one ghost of the selected crew member where their next step begins. */
	void UpdateFigureGhost();
	/** Where the ghost of this crew member stands (see UpdateFigureGhost); text and stuck for its sign. */
	FVector FindGhostSpot(const FDCCrewSchedule& Crew, FText* OutText = nullptr, bool* bOutStuck = nullptr) const;

	FDCSchedule Schedule;
	FString ScheduleSignature;
	/** What each planned inventory held at the last refresh (kept as long as this stays the same). */
	TMap<FName, FString> PlannedInventorySignatures;
	int32 ScheduleVersion = 0;
	FDCWorldSnapshot PlannedState;
	double PlaybackStart = -1.0;
	bool bWorldUsed = false;
	FDelegateHandle PlanChangedHandle;
	TWeakObjectPtr<ADCPlanTrail> Trail;
	TArray<FDCWorldBadge> StepBadges;
	TArray<FDCWorldBadge> DoorBadges;
	TArray<FDCWorldBadge> MomentBadges;
	TWeakObjectPtr<ADCPlanGhost> FigureGhost;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDCInventoryComponent>> PlannedInventories;

	FName FocusCrew;
	int32 FocusStep = INDEX_NONE;
	float PreviewTime = -1.f;
};
