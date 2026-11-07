#pragma once

#include "CoreMinimal.h"
#include "DCPlan.h"
#include "Subsystems/WorldSubsystem.h"

#include "DCHeistState.generated.h"

class ADCCrewCharacter;
class ADCDoor;
class ADCItemPickup;
class ADCPlanMarker;
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
};

/** What the world looks like at one moment of the plan: doors and the state of every item. */
struct FDCWorldSnapshot
{
	TArray<FDCSimDoor> Doors;
	TArray<FDCSimPickup> Pickups;

	FDCSimDoor* FindDoor(FName Actor);
	FDCSimPickup* FindPickup(FName Actor);
	/** Index of the item at Path in the pickup (INDEX_NONE if there is none). */
	static int32 FindItem(const FDCSimPickup& Pickup, const FString& Path);
	/** All containers around the item are open (and nothing was taken away). */
	static bool IsReachable(const FDCSimPickup& Pickup, int32 Index);
};

/** One planned step as the simulation sees it: when it starts, waits, works and ends. */
struct FDCScheduledTask
{
	int32 Index = 0;
	/** Starts walking / doing it. */
	float Start = 0.f;
	/** Arrived and could act. */
	float Ready = 0.f;
	/** Actually starts working (after waiting for someone else). */
	float WorkStart = 0.f;
	float End = 0.f;
	/** Will never happen (a condition is never met, or the crew member cannot do it). */
	bool bBlocked = false;
	/** Makes a loud noise. */
	bool bLoud = false;
	float FailChance = 0.f;
	/** "Hintertür knacken". */
	FText Label;
	/** "ca. 3 s · Erfolg 100 % · Schlösser 6 (nötig 3): geübt". */
	FText Detail;
	/** Why it waits or is blocked. */
	FText WaitReason;
	FVector EndLocation = FVector::ZeroVector;
	bool bHasLocation = false;
};

struct FDCCrewSchedule
{
	FName Crew;
	FText Name;
	FLinearColor Color = FLinearColor::White;
	TArray<FDCScheduledTask> Tasks;
	float End = 0.f;
};

struct FDCSchedule
{
	TArray<FDCCrewSchedule> Crews;
	float Duration = 0.f;

	const FDCCrewSchedule* Find(FName Crew) const;
};

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
	/** Plays all plans through (does not change the world). OutFinalState: the world after the plan. */
	FDCSchedule Simulate(FDCWorldSnapshot* OutFinalState = nullptr) const;
	/** Recomputes the schedule and the planned end state (and the markers in the world). */
	void Refresh(bool bMarkers = true);
	const FDCSchedule& GetSchedule() const { return Schedule; }
	/** Goes up whenever the schedule comes out different. */
	int32 GetScheduleVersion() const { return ScheduleVersion; }
	/** The world as it will be once the whole plan has run. */
	const FDCWorldSnapshot& GetPlannedState() const { return PlannedState; }
	/** Short name of a step: "Hintertür knacken", "Wandtresor ausräumen". */
	FText DescribeTask(const FDCPlanTask& Task) const;

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
	void UpdateMarkers();

	FDCSchedule Schedule;
	FString ScheduleSignature;
	int32 ScheduleVersion = 0;
	FDCWorldSnapshot PlannedState;
	double PlaybackStart = -1.0;
	bool bWorldUsed = false;
	FDelegateHandle PlanChangedHandle;
	TArray<TWeakObjectPtr<ADCPlanMarker>> Markers;
};
