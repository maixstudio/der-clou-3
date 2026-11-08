#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DCPlan.generated.h"

/** What one step of a crew member's plan does. */
UENUM()
enum class EDCPlanTaskType : uint8
{
	/** Walk to a spot. */
	Move,
	/** Stand still for a while (to wait for the others). */
	Wait,
	/** Something with a door: open, close, go through, pick the lock. */
	Door,
	/** Something with an item in the world: pick or force a lock, smash, open, close, take. */
	Item,
	/** Pick up an item lying in the world (the whole thing). */
	PickUp,
	/** Open what can be opened in a container and take everything of value. */
	Loot,
	/** Put down something the crew member carries (ItemPath = its item id). */
	Drop,
	/** Wait until another crew member (Target) has done one step (Ref) – or their whole plan (Ref unset). */
	WaitFor,
	/** Give a signal (Action = its number, Target = who it is for) that others wait for. */
	Signal
};

/** Planning (clicks add steps), playing the plan, or direct control (clicks act at once). */
UENUM()
enum class EDCPlanMode : uint8
{
	Planning,
	Playing,
	Direct
};

/** One planned step. Targets are named by actor name, so a plan survives reloading the level. */
USTRUCT()
struct FDCPlanTask
{
	GENERATED_BODY()

	UPROPERTY()
	EDCPlanTaskType Type = EDCPlanTaskType::Move;

	/** Stays the same when steps are moved, so others can wait for exactly this step. */
	UPROPERTY()
	FGuid Id;

	/** WaitFor: the step waited for (unset = until the crew member in Target has done everything). */
	UPROPERTY()
	FGuid Ref;

	/** Move: where to. */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Wait: how long. */
	UPROPERTY()
	float Seconds = 0.f;

	/** Door or item pickup (actor name); WaitFor: the crew member waited for. */
	UPROPERTY()
	FName Target;

	/** Item: path of item ids inside the pickup ("" = the pickup's own item, "desk_drawer", ...). Drop: the item id. */
	UPROPERTY()
	FString ItemPath;

	/** EDCDoorAction or EDCItemAction; Signal: its number. */
	UPROPERTY()
	uint8 Action = 0;
};

/** What a crew member waits for, as chosen in the wait dialog. */
struct FDCWaitChoice
{
	enum class EKind : uint8
	{
		/** A fixed number of seconds. */
		Seconds,
		/** Until Crew has done their step Step (they then give a signal). */
		AfterStep,
		/** Until Crew is done with their whole plan. */
		AllDone
	};

	EKind Kind = EKind::Seconds;
	float Seconds = 5.f;
	FName Crew;
	int32 Step = INDEX_NONE;

	bool operator==(const FDCWaitChoice& Other) const
	{
		return Kind == Other.Kind && (Kind == EKind::Seconds ? Seconds == Other.Seconds : Crew == Other.Crew && (Kind == EKind::AllDone || Step == Other.Step));
	}
};

USTRUCT()
struct FDCCrewPlan
{
	GENERATED_BODY()

	/** Crew member (actor name). */
	UPROPERTY()
	FName Crew;

	UPROPERTY()
	TArray<FDCPlanTask> Tasks;
};

/** The plans as they were before a change, and what the change was ("„Hintertür knacken“ gelöscht"). */
struct FDCPlanHistoryEntry
{
	TArray<FDCCrewPlan> Plans;
	FText What;
};

/**
 * The heist plan: a list of steps per crew member, plus the current mode. Lives in the game
 * instance, so it survives reloading the level ("back to planning" restores the start state).
 */
UCLASS()
class DERCLOU3_API UDCPlanSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDCPlanSubsystem* Get(const UObject* WorldContext);

	EDCPlanMode GetMode() const { return Mode; }
	void SetMode(EDCPlanMode InMode);

	const TArray<FDCCrewPlan>& GetPlans() const { return Plans; }
	/** Steps of one crew member (empty if none). */
	const TArray<FDCPlanTask>& GetTasks(FName Crew) const;

	/** Appends a step; returns its id. */
	FGuid AddTask(FName Crew, const FDCPlanTask& Task);
	/** Puts a step at Index (clamped) in the crew member's plan; returns its id. */
	FGuid InsertTask(FName Crew, int32 Index, const FDCPlanTask& Task);
	void RemoveLastTask(FName Crew);
	/**
	 * Removes a step. Waits for it go along (nobody can wait for a step that is not there);
	 * removing the last wait for a signal also removes the signal. Returns how many waits went along.
	 */
	int32 RemoveTask(FName Crew, int32 Index);
	/** The step with this id, in whose plan and where (nullptr if it is gone). */
	const FDCPlanTask* FindTask(const FGuid& Id, FName* OutCrew = nullptr, int32* OutIndex = nullptr) const;
	/** The number the next signal gets (1, 2, 3, ...). */
	int32 GetNextSignalNumber() const;
	/** Moves a step earlier (Delta < 0) or later in the crew member's plan. */
	void MoveTask(FName Crew, int32 Index, int32 Delta);
	/** Puts the step at From to place To in the same plan. */
	void MoveTaskTo(FName Crew, int32 From, int32 To);
	/** Inserts several steps at Index in one go (one undo step); returns their ids. */
	TArray<FGuid> InsertTasks(FName Crew, int32 Index, const TArray<FDCPlanTask>& Tasks);
	void ClearAll();

	/**
	 * Groups plan changes: everything between BeginEdit and its EndEdit is one undo step and one
	 * OnPlanChanged. Only the outermost pair counts; its What names the change. FDCPlanEditScope
	 * does both. Mode, speed and the self-test fields are not part of the history.
	 */
	void BeginEdit(const FText& What);
	void EndEdit();
	bool CanUndo() const { return !UndoStack.IsEmpty(); }
	bool CanRedo() const { return !RedoStack.IsEmpty(); }
	/** What "Rückgängig" would take back / "Wiederholen" would bring back. */
	FText GetUndoText() const { return UndoStack.IsEmpty() ? FText::GetEmpty() : UndoStack.Last().What; }
	FText GetRedoText() const { return RedoStack.IsEmpty() ? FText::GetEmpty() : RedoStack.Last().What; }
	/** Takes the last change back (false if there is none). */
	bool Undo();
	/** Brings back what Undo took back (false if there is nothing). */
	bool Redo();

	/** Playback speed (1, 2, 4). */
	float GetSpeed() const { return Speed; }
	void SetSpeed(float InSpeed) { Speed = InSpeed; }

	/** Set before reloading the level: start playing as soon as it is loaded. */
	bool bPlayAfterLoad = false;

	/** Self-tests: a find during the heist takes everything of value and goes on at once (no player). */
	bool bAutoDecideFinds = false;

	/** Variant to compare: the ghost of the selected crew member as a pin instead of a figure. */
	bool bGhostAsPin = false;

	/** Self-tests that reload the level remember how far they got. */
	int32 SelfTestStage = 0;
	bool bSelfTestPassed = true;

	/** Fires when steps are added or removed, or the mode changes. */
	FSimpleMulticastDelegate OnPlanChanged;

private:
	UPROPERTY()
	TArray<FDCCrewPlan> Plans;

	EDCPlanMode Mode = EDCPlanMode::Planning;
	float Speed = 1.f;

	/** Something changed inside the open edit. */
	void MarkChanged() { bEditChanged = true; }
	int32 EditDepth = 0;
	bool bEditChanged = false;
	FDCPlanHistoryEntry OpenEdit;
	TArray<FDCPlanHistoryEntry> UndoStack;
	TArray<FDCPlanHistoryEntry> RedoStack;
	static constexpr int32 MaxHistory = 50;
};

/** One undo step for everything done while it lives (see UDCPlanSubsystem::BeginEdit). */
struct FDCPlanEditScope
{
	FDCPlanEditScope(UDCPlanSubsystem* InPlans, const FText& What)
		: Plans(InPlans)
	{
		if (Plans)
		{
			Plans->BeginEdit(What);
		}
	}
	~FDCPlanEditScope()
	{
		if (Plans)
		{
			Plans->EndEdit();
		}
	}
	FDCPlanEditScope(const FDCPlanEditScope&) = delete;
	FDCPlanEditScope& operator=(const FDCPlanEditScope&) = delete;

private:
	UDCPlanSubsystem* Plans;
};
