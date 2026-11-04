#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "NWPlayerController.generated.h"

class ANWSimulationDirector;
class UNWPlannerScreen;

/** Mouse-first controller for planning and tactical play. */
UCLASS()
class NWGAME_API ANWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANWPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void OpenPlanner(ANWSimulationDirector& InDirector);
	/** Feeds a click in the world to the planner while it waits for a target. */
	void HandlePlannerClick();

	UPROPERTY(Transient)
	TObjectPtr<ANWSimulationDirector> Director;

	UPROPERTY(Transient)
	TObjectPtr<UNWPlannerScreen> PlannerScreen;
};
