#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DCPlayerController.generated.h"

class ADCClickMarker;
class ADCCrewCharacter;
class ADCDoor;
class ADCItemPickup;
class UDCContainerPanel;
class UDCInfoPanel;
class UDCInventoryPanel;
class UDCNoiseHud;
class UDCTimelinePanel;
struct FDCPlanTask;
struct FDCRadialOption;
struct FDCTaskCheck;
enum class EDCPlanMode : uint8;
class UDCRadialMenu;

/**
 * Mouse control: click a crew member to select it, click the floor to send the selected
 * crew member there, click a door or an item to choose what to do with it (radial menu).
 * I opens the inventory of the selected crew member.
 */
UCLASS()
class DERCLOU3_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADCPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	ADCCrewCharacter* GetSelectedCrew() const { return Selected; }

	//~ Timeline buttons
	/** Plays the plan (reloads the level first if it was already played or changed). */
	void StartPlayback();
	void TogglePause();
	bool IsPlaybackPaused() const;
	/** Back to the start state of the level, plan kept. */
	void ReturnToPlanning();
	void SetDirectMode(bool bDirect);
	void ToggleInventory();
	/** Planning: the selected crew member waits. */
	void AddWait(float Seconds);
	void RemoveLastStep();
	void ClearPlan();

	//~ Plan list
	/** Selects the crew member and picks one of their steps (INDEX_NONE = none). */
	void FocusStep(FName Crew, int32 Index);
	void RemoveStep(int32 Index);
	/** Moves a step of the selected crew member earlier (-1) or later (+1). */
	void MoveStep(int32 Index, int32 Delta);
	/** Planning: the selected crew member puts this down at the end of their plan so far. */
	void PlanDrop(FName ItemId);
	/** Pins in the world show where everyone is at this time of the plan (< 0 = off). */
	void SetPreviewTime(float Time);
	/** 1x, 2x, 4x. */
	void CycleSpeed();

protected:
	virtual void BeginPlay() override;

private:
	void HandleClick();
	void Select(ADCCrewCharacter* Crew);
	void OpenDoorMenu(ADCDoor* Door);
	void OpenPickupMenu(ADCItemPickup* Pickup);
	EDCPlanMode GetMode() const;
	/** Planning: add to the selected crew member's plan. Direct: do it now. */
	void IssueTask(const FDCPlanTask& Task);
	/** A menu entry for a step, with duration and chance; not possible = greyed, says why when clicked. */
	FDCRadialOption MakeTaskOption(const FText& Label, const FDCTaskCheck& Check, const FDCPlanTask& Task);
	void ReloadLevel();
	void UpdateHoverCursor();
	ADCItemPickup* FindPickup(FName ItemId) const;

	/** Command-line self-test -DCSelfTest=Inventory: pick up the test items, check the rules, screenshot, quit. */
	void StartInventorySelfTest();
	/** -DCSelfTest=Containers: search furniture, open, pick and force locks, take and hide things. */
	void StartContainerSelfTest();
	/** -DCSelfTest=Noise: walk, break a drawer, smash the display case; the neighbour must notice. */
	void StartNoiseSelfTest();
	/** -DCSelfTest=Timeline: Vic picks the back door while Rosa waits at it, then both work through their plans. */
	void StartTimelineSelfTest();
	/** -DCSelfTest=Reload: play, back to planning (level reset, plan kept), play again via reload. */
	void StartReloadSelfTest();
	/** -DCSelfTest=PlanInventory: a crowbar picked up in the plan can be used later in the plan. */
	void StartPlanInventorySelfTest();
	void FinishSelfTest(bool bPassed);
	void OnContainerReached(ADCItemPickup* Pickup);
	/** Each step returns true when it is done. */
	TArray<TFunction<bool()>> SelfTestSteps;

	UPROPERTY(Transient)
	TObjectPtr<ADCCrewCharacter> Selected;

	UPROPERTY(Transient)
	TObjectPtr<ADCClickMarker> Marker;

	UPROPERTY(Transient)
	TObjectPtr<UDCRadialMenu> RadialMenu;

	UPROPERTY(Transient)
	TObjectPtr<UDCInfoPanel> InfoPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDCInventoryPanel> InventoryPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDCContainerPanel> ContainerPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDCNoiseHud> NoiseHud;

	UPROPERTY(Transient)
	TObjectPtr<UDCTimelinePanel> TimelinePanel;

	float RefreshTimer = 0.f;

	FDelegateHandle ContainerReachedHandle;

	TWeakObjectPtr<ADCItemPickup> HoveredPickup;
};
