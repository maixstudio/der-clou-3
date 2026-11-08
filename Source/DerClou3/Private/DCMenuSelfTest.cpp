// -DCSelfTest=Menu: start menu, heist choice, loading screen, autosave, game menu, own save, back to the
// menu, "Fortsetzen", load list, options, loading a save. Uses its own save slots (DCSelfTest_*), which it
// deletes again; the player's saves and settings stay untouched.

#include "DCCrewCharacter.h"
#include "DCGameFlow.h"
#include "DCMenuController.h"
#include "DCMenuWidget.h"
#include "DCPlan.h"
#include "DCPlayerController.h"
#include "DerClou3.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void UDCGameFlowSubsystem::StartMenuSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Menu started"));
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [](float Seconds)
	{
		return [Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			End = End < 0.0 ? Now + Seconds : End;
			return Now >= End;
		};
	};
	const auto Shot = [](const TCHAR* Name)
	{
		return [Name]()
		{
			const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name);
			FScreenshotRequest::RequestScreenshot(File, true, false);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), *File);
			return true;
		};
	};
	const auto World = [this]() { return GetGameInstance()->GetWorld(); };
	const auto MapOf = [World]()
	{
		const UWorld* Current = World();
		return Current ? FPaths::GetBaseFilename(UWorld::RemovePIEPrefix(Current->GetOutermost()->GetName())) : FString();
	};
	const auto Menu = [World]() -> UDCMenuWidget*
	{
		const ADCMenuPlayerController* Controller = World() ? Cast<ADCMenuPlayerController>(World()->GetFirstPlayerController()) : nullptr;
		return Controller ? Controller->GetMenu() : nullptr;
	};
	const auto Game = [World]() { return World() ? Cast<ADCPlayerController>(World()->GetFirstPlayerController()) : nullptr; };
	const auto Vic = [World]() -> ADCCrewCharacter*
	{
		for (TActorIterator<ADCCrewCharacter> It(World()); It; ++It)
		{
			if (It->GetCrewName().ToString() == TEXT("Vic"))
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto Plans = [this]() { return GetGameInstance()->GetSubsystem<UDCPlanSubsystem>(); };
	const auto StepCount = [Plans]()
	{
		int32 Steps = 0;
		for (const FDCCrewPlan& Plan : Plans()->GetPlans())
		{
			Steps += Plan.Tasks.Num();
		}
		return Steps;
	};
	// Waits until Map is loaded, its world plays and the loading screen has gone.
	const auto Arrive = [this, World, MapOf, Expect](const TCHAR* Map)
	{
		return [this, World, MapOf, Expect, Map, Started = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			Started = Started < 0.0 ? Now : Started;
			const bool bThere = MapOf() == Map && World() && World()->HasBegunPlay() && !IsLoading();
			if (bThere || Now - Started > 120.0)
			{
				Expect(bThere, FString::Printf(TEXT("arrived at %s after %.1f s, loading screen gone"), Map, Now - Started));
				return true;
			}
			return false;
		};
	};
	const auto AddStep = [Plans, Vic]()
	{
		if (ADCCrewCharacter* Member = Vic())
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Wait;
			Task.Seconds = 2.f;
			Plans()->AddTask(Member->GetFName(), Task);
		}
	};

	// Leftovers of an earlier run.
	SelfTestSteps.Add([this]()
	{
		for (const FDCSaveInfo& Save : ListSaves())
		{
			DeleteSave(Save.Slot);
		}
		return true;
	});

	// Start: the loading screen first (the first map leads on to the villa), then the menu over the villa.
	SelfTestSteps.Add([this, World, MapOf, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		return (MapOf() == TEXT("L_Villa01") && IsLoading() && Phase == EPhase::Preparing && World()->HasBegunPlay()) || Now - Started > 60.0;
	});
	SelfTestSteps.Add(Shot(TEXT("menu_loading.png")));
	SelfTestSteps.Add(Arrive(TEXT("L_Villa01")));
	SelfTestSteps.Add([this, Expect, Menu]()
	{
		Expect(IsMenu() && Menu() != nullptr, TEXT("the start menu shows over the villa"));
		Expect(Menu() && Menu()->GetSaves().IsEmpty(), TEXT("no saves yet: „Fortsetzen“ and „Laden“ are greyed"));
		return true;
	});
	SelfTestSteps.Add(Wait(2.f));
	SelfTestSteps.Add(Shot(TEXT("menu_main.png")));
	SelfTestSteps.Add([Menu]() { if (Menu()) { Menu()->ShowPage(EDCMenuPage::Heists); } return true; });
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("menu_heists.png")));

	// "Neues Spiel" -> Testhaus.
	SelfTestSteps.Add([this, Expect]()
	{
		StartHeist(TEXT("house"));
		Expect(IsLoading(), TEXT("starting a heist puts up the loading screen at once"));
		return true;
	});
	SelfTestSteps.Add(Arrive(TEXT("L_House01")));
	SelfTestSteps.Add([this, Expect, Game, StepCount, Plans, AddStep]()
	{
		Expect(!IsMenu() && GetCurrentHeist() == TEXT("house") && Game() != nullptr, TEXT("planning the Testhaus with the planning controller"));
		Expect(StepCount() == 0 && Plans()->GetMode() == EDCPlanMode::Planning, TEXT("a new heist starts with an empty plan"));
		AddStep();
		return true;
	});
	SelfTestSteps.Add(Wait(1.6f));
	SelfTestSteps.Add([this, Expect, Game]()
	{
		const TOptional<FDCSaveInfo> Auto = FindAutosave(TEXT("house"));
		Expect(Auto.IsSet() && Auto->Steps == 1, FString::Printf(TEXT("the plan was saved automatically (%d steps)"), Auto.IsSet() ? Auto->Steps : -1));
		if (ADCPlayerController* Controller = Game())
		{
			Controller->OpenPauseMenu();
			Expect(Controller->IsPauseMenuOpen(), TEXT("Esc / „Menü“ opens the game menu"));
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("pause_menu.png")));
	SelfTestSteps.Add([Game]()
	{
		if (UDCMenuWidget* Pause = Game() ? Game()->GetPauseMenu() : nullptr)
		{
			Pause->ShowPage(EDCMenuPage::Save);
			Pause->SetSaveName(TEXT("Test-Stand"));
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("pause_save.png")));
	SelfTestSteps.Add([this, Expect, Game, AddStep]()
	{
		UDCMenuWidget* Pause = Game() ? Game()->GetPauseMenu() : nullptr;
		Expect(Pause && Pause->SaveNow(), TEXT("„Plan speichern“ saves under the name"));
		const bool bListed = ListSaves().ContainsByPredicate([](const FDCSaveInfo& Save) { return !Save.bAuto && Save.Name == TEXT("Test-Stand") && Save.Steps == 1; });
		Expect(bListed, TEXT("... and the save is listed with its step"));
		Game()->ClosePauseMenu();
		// One more step, then off to the main menu: the autosave takes it along.
		AddStep();
		OpenMainMenu();
		return true;
	});
	SelfTestSteps.Add(Arrive(TEXT("L_Villa01")));
	SelfTestSteps.Add([this, Expect, Menu, StepCount]()
	{
		const TOptional<FDCSaveInfo> Latest = FindLatestSave();
		Expect(Latest.IsSet() && Latest->bAuto && Latest->Steps == 2, TEXT("back in the menu: „Fortsetzen“ is the plan as left (2 steps)"));
		Expect(StepCount() == 0, TEXT("the menu scene has no plan in it"));
		Expect(Menu() && Menu()->GetSaves().Num() == 2, TEXT("two saves: automatic and „Test-Stand“"));
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("menu_continue.png")));
	SelfTestSteps.Add([Menu]() { if (Menu()) { Menu()->ShowPage(EDCMenuPage::Load); } return true; });
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("menu_load.png")));
	SelfTestSteps.Add([Menu, Expect]()
	{
		if (UDCMenuWidget* Start = Menu())
		{
			Start->ShowPage(EDCMenuPage::Options);
			// Changed but not taken over: the self-test keeps the player's settings as they are.
			Start->EditOptions().CameraSpeed = (Start->EditOptions().CameraSpeed + 1) % 3;
			Expect(Start->EditOptions() != DCOptions::GetCurrent(Start->GetOwningPlayer()), TEXT("options: a change waits for „Übernehmen“"));
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("menu_options.png")));
	SelfTestSteps.Add([Menu]()
	{
		if (UDCMenuWidget* Start = Menu())
		{
			Start->Back();
			Start->ShowPage(EDCMenuPage::Heists);
			Start->SelectHeist(TEXT("house"));
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("menu_heists_saved.png")));

	// "Laden": the named save, with its one step.
	SelfTestSteps.Add([this, Expect]()
	{
		const FDCSaveInfo* Named = nullptr;
		const TArray<FDCSaveInfo> Saves = ListSaves();
		Named = Saves.FindByPredicate([](const FDCSaveInfo& Save) { return Save.Name == TEXT("Test-Stand"); });
		Expect(Named && LoadSave(Named->Slot), TEXT("loading „Test-Stand“"));
		return true;
	});
	SelfTestSteps.Add(Arrive(TEXT("L_House01")));
	SelfTestSteps.Add([this, Expect, StepCount, Plans, Game]()
	{
		Expect(StepCount() == 1 && !Plans()->CanUndo(), FString::Printf(TEXT("the loaded plan has its step (%d) and no history"), StepCount()));
		// "Zurück zur Planung" reloads the room behind the loading screen, the plan stays.
		if (ADCPlayerController* Controller = Game())
		{
			Controller->ReturnToPlanning();
		}
		Expect(IsLoading(), TEXT("reloading the room shows the loading screen"));
		return true;
	});
	SelfTestSteps.Add(Arrive(TEXT("L_House01")));
	SelfTestSteps.Add([this, Expect, StepCount, bPassed]()
	{
		Expect(StepCount() == 1, TEXT("after the reload the plan is still there"));
		for (const FDCSaveInfo& Save : ListSaves())
		{
			DeleteSave(Save.Slot);
		}
		Expect(ListSaves().IsEmpty(), TEXT("the self-test's saves are gone again"));
		UE_LOG(LogDerClou, Display, TEXT("Self-test %s"), *bPassed ? TEXT("PASSED") : TEXT("FAILED"));
		FPlatformMisc::RequestExit(false, TEXT("DCSelfTest"));
		return true;
	});
}
