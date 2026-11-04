#pragma once

#include "CoreMinimal.h"
#include "NWPlanTypes.h"
#include "Sim/SimDefinition.h"
#include "Sim/SimTimeline.h"
#include "UObject/Object.h"

#include "NWPlanEditor.generated.h"

/** Where a pickable target sits on the object (centimetres, simulation space). */
struct FNWPickCandidate
{
	FIntPoint PositionCm = FIntPoint::ZeroValue;
	int32 Target = INDEX_NONE;
};

/** Which signal releases a "wait for signal" action in the ghost run. */
struct FNWSignalLink
{
	int32 WaitCrew = INDEX_NONE;
	int32 WaitAction = INDEX_NONE;
	int32 Signal = INDEX_NONE;

	/** The Signal action that released the wait (or the first planned one if it never did). */
	int32 SignalCrew = INDEX_NONE;
	int32 SignalAction = INDEX_NONE;

	nw::sim::Tick SentAt = nw::sim::NoTick;
	nw::sim::Tick ReleasedAt = nw::sim::NoTick;

	/** The wait ended because the signal arrived. */
	bool bReleased = false;
	/** Some crew member's plan contains a matching "send signal" action. */
	bool bSignalInPlan = false;
};

/** Live state of a running execution, pushed by the game so the planner UI can show it. */
struct FNWExecutionState
{
	bool bActive = false;
	bool bFinished = false;
	float SimulatedSeconds = 0.f;
	float PlaybackSpeed = 1.f;
	nw::sim::SimResult Result;
};

DECLARE_MULTICAST_DELEGATE(FNWOnPlanEditorChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FNWOnPlaybackSpeedRequested, float /*Speed*/);

/**
 * Editing model behind the planner: holds the working plan, undo/redo history, the action
 * the player is about to place, and the ghost-run timeline of the current plan.
 *
 * UI-agnostic. The game feeds it world clicks (PickAt) and listens to the request delegates
 * (execute, stop, playback speed).
 */
UCLASS()
class NWPLANNING_API UNWPlanEditor : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(nw::sim::ObjectDefinition InObject, std::vector<nw::sim::CrewDef> InCrew, TArray<FNWCrewPlan> InPlan, uint64 InSeed);

	// --- Data -----------------------------------------------------------------------------

	const nw::sim::ObjectDefinition& GetObjectDefinition() const { return Object; }
	int32 GetCrewCount() const { return static_cast<int32>(Crew.size()); }
	FText GetCrewName(int32 CrewIndex) const;
	FText GetObjectName() const;
	const TArray<FNWCrewPlan>& GetPlan() const { return Plan; }

	/** Ghost run of the current plan; refreshed after every change. */
	const nw::sim::PlanTimeline& GetTimeline() const { return Timeline; }

	/** One entry per "wait for signal" action in the plan, resolved against the ghost run. */
	const TArray<FNWSignalLink>& GetSignalLinks() const { return SignalLinks; }
	/** The link of a "wait for signal" action, or nullptr. */
	const FNWSignalLink* FindSignalLink(int32 CrewIndex, int32 ActionIndex) const;

	// --- Editing --------------------------------------------------------------------------

	int32 GetSelectedCrew() const { return SelectedCrew; }
	void SelectCrew(int32 CrewIndex);

	void AddAction(int32 CrewIndex, const FNWPlanAction& Action);
	void RemoveAction(int32 CrewIndex, int32 ActionIndex);
	void ClearCrew(int32 CrewIndex);

	bool CanUndo() const { return UndoStack.Num() > 0; }
	bool CanRedo() const { return RedoStack.Num() > 0; }
	bool Undo();
	bool Redo();

	// --- Placing actions in the world -----------------------------------------------------

	/** Starts placing an action that needs a target in the world (MoveTo, PickLock, DisableAlarm, CrackSafe). */
	void BeginPick(ENWPlanActionType Type);
	void CancelPick();
	bool IsPicking() const { return bPicking; }
	ENWPlanActionType GetPendingType() const { return PendingType; }

	/** Targets the pending action could use, for highlighting in the world. */
	TArray<FNWPickCandidate> GetPickCandidates() const;

	/**
	 * Resolves the target nearest to the given position (simulation space) and appends the pending
	 * action to the selected crew member's track. Returns false if nothing is within reach.
	 */
	bool PickAt(const FIntPoint& PositionCm);

	/** Maximum distance between a click and a target, in centimetres. */
	static constexpr int32 PickRadiusCm = 250;

	// --- Labels ---------------------------------------------------------------------------

	FText DescribeAction(const FNWPlanAction& Action, bool bShort = false) const;
	static FText DescribeOutcome(nw::sim::Outcome Outcome);
	FText DescribeEvent(const nw::sim::SimEvent& Event) const;
	/** "Wartet auf Signal 1 von Otto (gesendet 4:58)" or why the wait never ends. */
	FText DescribeLink(const FNWSignalLink& Link) const;

	// --- Execution (handled by the game) --------------------------------------------------

	void RequestExecute() { OnExecuteRequested.Broadcast(); }
	void RequestStop() { OnStopRequested.Broadcast(); }
	void RequestPlaybackSpeed(float Speed) { OnPlaybackSpeedRequested.Broadcast(Speed); }

	void ReportExecution(const FNWExecutionState& State) { Execution = State; }
	const FNWExecutionState& GetExecution() const { return Execution; }

	FNWOnPlanEditorChanged OnChanged;
	FSimpleMulticastDelegate OnExecuteRequested;
	FSimpleMulticastDelegate OnStopRequested;
	FNWOnPlaybackSpeedRequested OnPlaybackSpeedRequested;

private:
	void Commit(TArray<FNWCrewPlan> NewPlan);
	void Refresh();
	void ResolveSignalLinks();
	bool IsValidCrew(int32 CrewIndex) const { return CrewIndex >= 0 && CrewIndex < GetCrewCount(); }
	int32 CountTargeted(ENWPlanActionType Type, int32 Target) const;

	nw::sim::ObjectDefinition Object;
	std::vector<nw::sim::CrewDef> Crew;
	uint64 Seed = 0;

	TArray<FNWCrewPlan> Plan;
	TArray<TArray<FNWCrewPlan>> UndoStack;
	TArray<TArray<FNWCrewPlan>> RedoStack;

	nw::sim::PlanTimeline Timeline;
	TArray<FNWSignalLink> SignalLinks;
	FNWExecutionState Execution;

	int32 SelectedCrew = 0;
	bool bPicking = false;
	ENWPlanActionType PendingType = ENWPlanActionType::MoveTo;
};
