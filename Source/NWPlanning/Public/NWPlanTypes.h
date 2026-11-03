#pragma once

#include "CoreMinimal.h"

#include "NWPlanTypes.generated.h"

/** Mirrors nw::sim::ActionType. Keep the order in sync. */
UENUM(BlueprintType)
enum class ENWPlanActionType : uint8
{
	MoveTo,
	Wait,
	Signal,
	WaitForSignal,
	PickLock,
	DisableAlarm,
	CrackSafe
};

/** One block on a crew member's planner track. */
USTRUCT(BlueprintType)
struct NWPLANNING_API FNWPlanAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plan")
	ENWPlanActionType Type = ENWPlanActionType::Wait;

	/** Node, door, alarm, safe or signal index, depending on Type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plan")
	int32 Target = INDEX_NONE;

	/** Only used by Wait. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plan", meta = (ClampMin = "0"))
	float DurationSeconds = 0.f;
};

/** All actions of one crew member, in order. */
USTRUCT(BlueprintType)
struct NWPLANNING_API FNWCrewPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plan")
	TArray<FNWPlanAction> Actions;
};
