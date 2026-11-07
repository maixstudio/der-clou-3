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
	Loot
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

	/** Move: where to. */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Wait: how long. */
	UPROPERTY()
	float Seconds = 0.f;

	/** Door or item pickup (actor name). */
	UPROPERTY()
	FName Target;

	/** Item: path of item ids inside the pickup ("" = the pickup's own item, "desk_drawer", ...). */
	UPROPERTY()
	FString ItemPath;

	/** EDCDoorAction or EDCItemAction. */
	UPROPERTY()
	uint8 Action = 0;
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

	void AddTask(FName Crew, const FDCPlanTask& Task);
	void RemoveLastTask(FName Crew);
	void ClearAll();

	/** Playback speed (1, 2, 4). */
	float GetSpeed() const { return Speed; }
	void SetSpeed(float InSpeed) { Speed = InSpeed; }

	/** Set before reloading the level: start playing as soon as it is loaded. */
	bool bPlayAfterLoad = false;

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
};
