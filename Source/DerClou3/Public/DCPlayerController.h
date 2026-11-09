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
class UDCCrewBar;
class UDCFrame;
class UDCWaitDialog;
class UDCMenuWidget;
struct FDCWaitChoice;
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
	/** "Planen": back from trying things out (resets the room) – nothing to do if already planning. */
	void ReturnToPlanningIfNeeded();
	void SetDirectMode(bool bDirect);
	void ToggleInventory();
	/** Planning: the selected crew member waits. */
	void AddWait(float Seconds);
	void RemoveLastStep();
	/** The picked step of the selected crew member, else her last one (button and Entf). */
	void DeleteStep();
	/** Button text for DeleteStep ("Letzten Schritt löschen", "Schritt 3 löschen"); empty when there is nothing to delete. */
	FText DescribeDeleteStep() const;
	/**
	 * Planning, after the player switched to this crew member: the room and the timeline jump to the
	 * end of her last planned step (no plan yet: to the start).
	 */
	void ShowCrewMoment(ADCCrewCharacter* Crew);
	/** After new steps of the selected crew member: a time cursor in use moves to the end of the last new one. */
	void FollowNewSteps(int32 LastNewIndex);
	void ClearPlan();
	/** Planning: takes the last plan change back (Strg+Z, "Rückgängig" in the help bar); RedoPlan has no key any more. */
	void UndoPlan();
	void RedoPlan();
	/**
	 * "Rosa wartet hierauf": Waiter waits until Giver's step is done - placed before Waiter's first
	 * step that would start earlier (else at the end). A second call takes the wait away again.
	 */
	void ToggleWaitOn(FName Giver, int32 StepIndex, FName Waiter);
	/** Planning: where new steps of the selected crew member go - right after the picked step (INDEX_NONE: at the end). */
	int32 GetInsertIndex() const;
	/** New steps go to the end again (Esc, "ans Ende"). */
	void InsertAtEnd();
	/** Variant "nur per Knopf": "+ danach einfügen" at the selected crew member's step. */
	void InsertAfterStep(int32 Index);
	/** "Varianten …": 0 ghost as pin, 1 help as menu at the mouse, 2 insert only with the button. */
	void SetVariant(int32 Which, bool bValue);

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

	//~ Waiting for each other
	/** Planning: asks what the selected crew member should wait for (guided dialog). */
	void OpenWaitDialog();
	/**
	 * Adds the chosen wait to the selected crew member's plan. Waiting for another crew member's
	 * step puts a signal into their plan right after that step (or reuses the one already there).
	 */
	void ApplyWaitChoice(const FDCWaitChoice& Choice);
	UDCWaitDialog* GetWaitDialog() const { return WaitDialog; }

	//~ Feedback
	/** A short message above the plan bar for a few seconds (bProblem: red) – instead of text in the room. */
	void ShowToast(const FText& Text, bool bProblem = false);
	/** The message being shown (empty when there is none). */
	FText GetToast(bool& bOutProblem) const;
	/** The help bar over the plan after a click made the plan hang (empty: none). Stays until the player reacts. */
	FText GetPlanHelp() const;
	void DismissPlanHelp() { PlanHelp.Reset(); }
	/** "Lösen …": picks the first step that does not happen, shows where and opens the help again. */
	void ShowPlanProblem();

	//~ Crew cards
	/** Selects the crew member and moves the camera to them. */
	void SelectFromBar(ADCCrewCharacter* Crew);
	/** Selects the crew member and opens their profile (skills and inventory). */
	void OpenProfile(ADCCrewCharacter* Crew);
	/** 1x, 2x, 4x. */
	void CycleSpeed();

	//~ Game menu (Esc, "Menü" top right): pause, save, load, options, main menu
	void OpenPauseMenu();
	void ClosePauseMenu();
	bool IsPauseMenuOpen() const;
	UDCMenuWidget* GetPauseMenu() const { return PauseMenu; }

	//~ Finds during the heist
	/** A crew member found something while searching: pause the game and let the player decide (one find at a time). */
	void RequestFindDecision(ADCCrewCharacter* Crew, ADCItemPickup* Pickup);
	bool IsDecidingFind() const { return FindCrew.IsValid(); }

protected:
	virtual void BeginPlay() override;

private:
	void HandleClick();
	/** A left click at this point of the screen (viewport pixels). */
	void HandleClickAt(const FVector2D& ScreenPosition);
	/** Self-tests: a real click (trace from the camera, menus, ground check) on this world point; false if it is off screen. */
	bool ClickWorldPoint(const FVector& Location);
	void Select(ADCCrewCharacter* Crew);
	void OpenDoorMenu(ADCDoor* Door);
	void OpenPickupMenu(ADCItemPickup* Pickup);
	EDCPlanMode GetMode() const;
	/** Planning: add to the selected crew member's plan. Direct: do it now. */
	void IssueTask(const FDCPlanTask& Task);
	/** Several steps at once ("knacken und durchsuchen"). */
	void IssueTasks(const TArray<FDCPlanTask>& Tasks);
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
	/** -DCSelfTest=Signals: Rosa cracks the safe and gives a signal, Vic waits for it and loots the safe. */
	void StartSignalSelfTest();
	/** -DCSelfTest=Grid: footprints in the grids (crowbar not in the pockets but in the backpack), moving, nesting, copying. */
	void StartGridSelfTest();
	void FinishSelfTest(bool bPassed);
	void OnContainerReached(ADCItemPickup* Pickup);
	void StartNextFind();
	void FinishFind();
	/** -DCSelfTest=Find: Rosa cracks and searches the safe during the heist; the game stops, she takes one thing, leaves one, drops her torch there. */
	void StartFindSelfTest();
	/** -DCSelfTest=PlanView: search the sofa, pick up the statue; the profile shows the state after the plan, a second pick-up says where it already is. */
	void StartPlanViewSelfTest();
	/** -DCSelfTest=DoorWay: Rosa walks out through the back door Vic opens: the plan waits for the door, playback too. */
	void StartDoorWaySelfTest();
	/** -DCSelfTest=House (map L_House01): rooms, doors, crew in the garden; Vic plans through two locked doors into the office and gets there. */
	void StartHouseSelfTest();
	/** -DCSelfTest=AutoDoor (map L_House01): the user's plan - Vic only picks the back door, Rosa waits for it, walks in, opens it herself and searches the dresser. */
	void StartAutoDoorSelfTest();
	/** -DCSelfTest=Undo: every plan change can be taken back and brought back, a click's pair and "Plan leeren" too. */
	void StartUndoSelfTest();
	/** -DCSelfTest=PlanHelp (map L_House01): a click that makes the plan hang says so at once; "Passt jetzt" once it works. */
	void StartPlanHelpSelfTest();
	/** -DCSelfTest=PlanLook (map L_House01): doors show the plan at the moment the room shows (padlock, panel, sign). */
	void StartPlanLookSelfTest();
	/** -DCSelfTest=Insert (map L_House01): new steps after the picked one, "Rosa wartet hierauf", deleting takes waits along. */
	void StartInsertSelfTest();
	/** -DCSelfTest=Variants (map L_House01): the three switches in "Varianten …" (ghost, help, insert). */
	void StartVariantsSelfTest();
	/** -DCSelfTest=Villa (map L_Villa01): rooms, doors, stairs walkable, the camera shows one storey. */
	void StartVillaSelfTest();
	void StartReplaySelfTest();
	/** Trying things out: walk there - through a door as a one-step plan, so it opens on the way (or she says it is locked). */
	void TryMoveTo(const FVector& Location);
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

	UPROPERTY(Transient)
	TObjectPtr<UDCCrewBar> CrewBar;

	UPROPERTY(Transient)
	TObjectPtr<UDCFrame> Frame;

	UPROPERTY(Transient)
	TObjectPtr<UDCWaitDialog> WaitDialog;

	UPROPERTY(Transient)
	TObjectPtr<UDCMenuWidget> PauseMenu;
	/** The menu stopped a running plan (and lets it go on when it closes). */
	bool bPausedByMenu = false;
	/** "Abspielen" after a reload: starts once the loading screen is gone. */
	void StartPlaybackWhenLoaded();

	float RefreshTimer = 0.f;
	FText ToastText;
	/** Steps that do not happen right now, as crew + step id (ids survive moving steps). */
	TArray<TPair<FName, FGuid>> CollectProblems() const;
	/**
	 * After a plan change: a step that newly does not happen opens the help bar (returns true);
	 * nothing left that hangs: "Passt jetzt". NewIndex: the step just added for Crew.
	 */
	bool CheckPlanAfterEdit(const TArray<TPair<FName, FGuid>>& Before, FName Crew = NAME_None, int32 NewIndex = INDEX_NONE);
	struct FPlanHelp
	{
		FName Crew;
		FGuid Step;
		FText Text;
	};
	TOptional<FPlanHelp> PlanHelp;
	/** The last planned click made the plan hang (the click ring turns red). */
	bool bLastEditProblem = false;
	/** Variant "nur per Knopf": new steps go after this step (invalid: at the end). */
	FGuid InsertAfter;
	/** A step was put in at NewIndex: it is the picked one now, so the next click follows it. */
	void AfterInsert(FName Crew, int32 NewIndex);
	double ToastTime = -100.0;
	bool bToastProblem = false;

	FDelegateHandle ContainerReachedHandle;

	TWeakObjectPtr<ADCItemPickup> HoveredPickup;

	struct FFindRequest
	{
		TWeakObjectPtr<ADCCrewCharacter> Crew;
		TWeakObjectPtr<ADCItemPickup> Pickup;
	};
	TArray<FFindRequest> FindQueue;
	TWeakObjectPtr<ADCCrewCharacter> FindCrew;
};
