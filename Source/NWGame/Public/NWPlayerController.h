#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "NWPlayerController.generated.h"

class ANWRoomDirector;
class ANWSimulationDirector;
class UNWPlannerScreen;
class UNWRoomScreen;

/**
 * Mouse-first controller. In a level with an ANWRoomDirector it drives the room prototype
 * (select, walk, radial menu); otherwise it opens the timeline planner of the jeweler graybox.
 */
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
	// Jeweler planner (timeline prototype).
	void OpenPlanner(ANWSimulationDirector& InDirector);
	void HandlePlannerClick();

	// Room prototype.
	void OpenRoom(ANWRoomDirector& InDirector);
	void HandleRoomInput();
	void OpenOpeningMenu(int32 Opening);
	void OpenAgentMenu(int32 Agent);

	UPROPERTY(Transient)
	TObjectPtr<ANWSimulationDirector> Director;

	UPROPERTY(Transient)
	TObjectPtr<UNWPlannerScreen> PlannerScreen;

	UPROPERTY(Transient)
	TObjectPtr<ANWRoomDirector> RoomDirector;

	UPROPERTY(Transient)
	TObjectPtr<UNWRoomScreen> RoomScreen;
};
