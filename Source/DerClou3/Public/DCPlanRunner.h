#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "DCPlan.h"

#include "DCPlanRunner.generated.h"

class ADCCrewCharacter;
class ADCDoor;
class ADCItemPickup;
class UDCItem;

/** How far one step of the plan has got while it is played. */
enum class EDCTaskStatus : uint8
{
	Planned,
	Waiting,
	Running,
	Done,
	Failed
};

/**
 * Plays one crew member's plan, step by step: walk to where the step happens, wait there until
 * the world allows it (the door is unlocked, the drawer is open – asked from the real objects),
 * then do it. A lock that is not picked on the first try is tried again (up to three times).
 */
UCLASS(ClassGroup = (DerClou))
class DERCLOU3_API UDCPlanRunnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCPlanRunnerComponent();

	void Start(const TArray<FDCPlanTask>& InTasks);
	void Stop();
	bool IsRunning() const { return bRunning; }

	int32 GetCurrentIndex() const { return Index; }
	EDCTaskStatus GetStatus(int32 TaskIndex) const;
	/** Index of the step with this id in the plan being played (INDEX_NONE if it is not in it). */
	int32 FindTask(const FGuid& Id) const;
	/** Through with every step (or nothing to do). */
	bool IsFinished() const { return Index >= Tasks.Num(); }
	/** "wartet: Hintertür verschlossen" while waiting, else empty. */
	const FText& GetWaitText() const { return WaitText; }

	/** Searching found something: the game is paused until the player has decided what to take. */
	bool IsAwaitingDecision() const { return LootPhase == ELootPhase::Deciding; }
	/** The player is through with the find; the crew member still stands there for the handling time. */
	void ResolveFind(float HandlingSeconds);
	/** Cells searched in a container: its own grid and those of its fixed parts (what a floor plan shows). */
	static int32 CountSearchCells(const UDCItem* Root);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static constexpr int32 MaxTries = 3;

private:
	enum class EStep : uint8
	{
		Approach,
		Wait,
		Execute
	};

	ADCCrewCharacter* GetCrew() const;
	const FDCPlanTask& Current() const { return Tasks[Index]; }
	void StartApproach();
	/** True if the step may start; otherwise OutWait says what it waits for. */
	bool ConditionMet(FText& OutWait) const;
	/**
	 * Doors on the way to where the step happens. Returns true if the crew member has to wait
	 * (a locked door, or a closed one with no hand free: OutWait says which). OutOpen is a closed,
	 * unlocked door she can open in passing (she goes through it first, then on).
	 */
	bool DoorOnWay(FText& OutWait, ADCDoor*& OutOpen, ADCDoor*& OutWaitAt) const;
	/** True if there is nothing left to do (lock already open, ...). */
	bool AlreadyDone() const;
	void StartExecute();
	/** Called once the crew member is idle again after executing; may retry. */
	void EvaluateResult();
	/** Opens one thing that opens without a key; false when nothing is left to open. */
	bool LootOpenStep();
	/** Done searching: everything movable that can be reached now, most valuable first. */
	TArray<UDCItem*> CollectFinds() const;
	void FinishTask(EDCTaskStatus Status);

	ADCItemPickup* GetPickup() const;
	UDCItem* GetItem() const;

	TArray<FDCPlanTask> Tasks;
	TArray<EDCTaskStatus> Statuses;
	int32 Index = 0;
	EStep Step = EStep::Approach;
	bool bStepStarted = false;
	bool bRunning = false;
	float StepTime = 0.f;
	int32 Tries = 0;
	FText WaitText;
	TSet<TWeakObjectPtr<UDCItem>> LootSkip;
	/** Opening a door on the way (going through it) before walking on to the step. */
	TWeakObjectPtr<ADCDoor> PassingDoor;
	/** Waiting at a locked door: already sent there (once per wait). */
	bool bWentToDoorWait = false;

	enum class ELootPhase : uint8
	{
		Opening,
		Searching,
		Deciding,
		Handling
	};
	ELootPhase LootPhase = ELootPhase::Opening;
	float HoldSeconds = 0.f;
};
