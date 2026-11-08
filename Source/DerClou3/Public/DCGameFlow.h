#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "DCPlan.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DCGameFlow.generated.h"

class SDCLoadingScreen;
struct FDCLoadingState;

/** One heist the player can start ("Neues Spiel"). */
struct FDCHeistInfo
{
	FName Id;
	FText Name;
	/** One or two sentences for the choice. */
	FText Description;
	/** Long package name of the map, e.g. /Game/DerClou/Maps/L_Villa01. */
	FString Map;
	/** Listed under "Übung". */
	bool bPractice = false;
};

/** A saved plan as the load list shows it. */
struct FDCSaveInfo
{
	FString Slot;
	/** The player's name for it; autosaves have none. */
	FString Name;
	FName Heist;
	FDateTime Saved;
	int32 Steps = 0;
	bool bAuto = false;
};

/** A saved plan: which heist, and every crew member's steps. A heist in progress is not saved. */
UCLASS()
class DERCLOU3_API UDCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	bool bAuto = false;

	UPROPERTY()
	FName Heist;

	UPROPERTY()
	FString Map;

	/** Local time of saving. */
	UPROPERTY()
	FDateTime Saved;

	UPROPERTY()
	TArray<FDCCrewPlan> Plans;

	UPROPERTY()
	float Speed = 1.f;
};

/**
 * The way through the game: start menu, the heists, saves and the loading screen in between.
 *
 * The start menu is a mode of the villa map (option "?menu"): the scene stays the real villa at night,
 * a different player controller shows the menu. Every change of map goes through the loading screen,
 * which stays up until the new map is really usable (nav mesh built, shaders compiled) and then fades.
 * Self-tests skip menu, loading screen and autosaves - except -DCSelfTest=Menu, which tests them.
 */
UCLASS()
class DERCLOU3_API UDCGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDCGameFlowSubsystem* Get(const UObject* WorldContext);

	/** Working title of the game; the one place to change it. */
	static FText GetGameTitle();
	static const TArray<FDCHeistInfo>& GetHeists();
	static const FDCHeistInfo* FindHeist(FName Id);
	static const FDCHeistInfo* FindHeistByMap(const FString& Map);
	/** The map behind the start menu. */
	static const TCHAR* MenuMap();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * The game mode asks before anyone logs in: does this world show the start menu? The first map
	 * of a session shows it (a map other than the villa goes there behind the loading screen).
	 */
	bool DecideMenu(UWorld* World, const FString& Options);
	/** This world shows the start menu. */
	bool IsMenu() const { return bMenu; }
	/** The first map of the session is not the villa: it only leads on to the menu. */
	bool IsLeavingForMenu() const { return bRedirectToMenu; }
	FName GetCurrentHeist() const { return CurrentHeist; }

	//~ Going places, each behind the loading screen
	void OpenMainMenu();
	/** A new, empty plan for this heist. */
	void StartHeist(FName HeistId);
	/** The saved plan and its heist; false if the save cannot be read. */
	bool LoadSave(const FString& Slot);
	/** The same map again (its start state), plan kept: "Abspielen" after a change, "Zurück zur Planung". */
	void ReloadCurrentMap(const UObject* WorldContext);
	/** Ends the game (in the editor: ends play). */
	void QuitGame(const UObject* WorldContext);

	/** The loading screen is up: the game takes no input. */
	bool IsLoading() const;
	/** Fires when the loading screen has gone. */
	FSimpleMulticastDelegate OnLoadingFinished;

	//~ Saves
	/** All saves, newest first. */
	TArray<FDCSaveInfo> ListSaves() const;
	TOptional<FDCSaveInfo> FindLatestSave() const;
	TOptional<FDCSaveInfo> FindAutosave(FName Heist) const;
	/** Saves the current plan under this name; returns the slot (empty on failure). */
	FString SavePlan(const FString& Name);
	bool DeleteSave(const FString& Slot);
	/** "Villa Lindenhof · 12 Schritte · heute 18:42". */
	static FText DescribeSave(const FDCSaveInfo& Save);
	/** How the load list names it: the player's name or "Automatisch gespeichert". */
	static FText SaveTitle(const FDCSaveInfo& Save);
	/** "Villa Lindenhof – 08.11. 18:42". */
	FString DefaultSaveName() const;
	/** Writes the autosave now if the plan changed since the last one. */
	void FlushAutosave();

private:
	bool Tick(float DeltaTime);
	void OnPlanChanged();
	void OnPreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void TravelTo(const FString& Map, const FString& Options, const FText& Title, const FText& Kind);
	void ShowOverlay(const FText& Title, const FText& Kind);
	void HideOverlay();
	/** The current plan's crew names and step ids: changes when steps come, go or move. */
	FString PlanSignature() const;
	bool WriteSave(const FString& Slot, const FString& Name, bool bAuto) const;
	FString SlotPrefix() const;
	void StartMenuSelfTest();

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle PlanChangedHandle;
	FDelegateHandle PreLoadMapHandle;

	bool bBooted = false;
	bool bMenu = false;
	bool bRedirectToMenu = false;
	FName CurrentHeist;

	//~ Loading screen
	enum class EPhase : uint8
	{
		Hidden,
		/** Shown, a few frames still to draw before the map is opened (the load blocks the screen). */
		Travelling,
		/** OpenLevel has been called: the engine loads. */
		Loading,
		/** The new map is there: wait until it is usable. */
		Preparing,
		FadingOut
	};
	EPhase Phase = EPhase::Hidden;
	TSharedPtr<FDCLoadingState> LoadingState;
	TSharedPtr<SDCLoadingScreen> LoadingWidget;
	TWeakObjectPtr<UGameViewportClient> OverlayViewport;
	FString PendingMap;
	FString PendingOptions;
	int32 FramesToWait = 0;
	int32 FramesPrepared = 0;
	double PhaseStart = 0.0;
	/** Self-tests other than "Menu": no loading screen, no menu, no autosave. */
	bool bScreens = true;
	FString SelfTest;

	//~ Autosave
	FString SavedSignature;
	double AutosaveDue = -1.0;
	/** Plans being replaced by a load or a new heist: no autosave for that. */
	bool bApplyingPlan = false;

	//~ -DCSelfTest=Menu
	TArray<TFunction<bool()>> SelfTestSteps;
};
