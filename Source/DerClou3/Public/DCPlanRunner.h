#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "DCPlan.h"

#include "DCPlanRunner.generated.h"

class ADCCrewCharacter;
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
	/** "wartet: Hintertür verschlossen" while waiting, else empty. */
	const FText& GetWaitText() const { return WaitText; }

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
	/** True if there is nothing left to do (lock already open, ...). */
	bool AlreadyDone() const;
	void StartExecute();
	/** Called once the crew member is idle again after executing; may retry. */
	void EvaluateResult();
	/** One handful while looting; false when nothing is left. */
	bool LootStep();
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
};
