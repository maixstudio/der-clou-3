// -DCSelfTest=Search on L_Room01: every piece of furniture can be searched, locked things ask "… öffnen mit" in a second
// ring (lock picks, crowbar, glass), and during the heist valuables are packed without stopping the game.

#include "DCContainerPanel.h"
#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "DCRadialMenu.h"
#include "DerClou3.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ADCPlayerController::StartSearchSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Search started"));
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
	const auto CrewNamed = [this](const TCHAR* Name) -> ADCCrewCharacter*
	{
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetCrewName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto Has = [this, Expect](const TCHAR* Label, bool bWantEnabled, const FString& What)
	{
		bool bEnabled = false;
		const bool bThere = RadialMenu->HasOption(Label, &bEnabled);
		Expect(bThere && bEnabled == bWantEnabled, FString::Printf(TEXT("%s („%s“ %s)"), *What, Label,
			!bThere ? TEXT("missing") : bEnabled ? TEXT("possible") : TEXT("greyed out")));
	};
	const TSharedRef<bool> bEverPaused = MakeShared<bool>(false);
	const TSharedRef<int32> ValueBefore = MakeShared<int32>(0);

	SelfTestSteps.Add(Wait(1.5f));
	// Planning with Vic: an armchair (nothing visible inside) can be searched; small loose things are only taken along.
	SelfTestSteps.Add([this, Expect, CrewNamed, Has]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->bAutoDecideFinds = false;
		Plans->bPauseOnEveryFind = false;
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCItemPickup* Armchair = FindPickup(TEXT("armchair"));
		if (!Vic || !Armchair)
		{
			Expect(false, TEXT("Vic and the armchair are there"));
			return true;
		}
		Select(Vic);
		RadialMenu->bOpenAtCentreForTests = true;
		OpenPickupMenu(Armchair);
		Has(TEXT("Durchsuchen"), true, TEXT("the armchair can be searched"));
		Expect(!RadialMenu->HasOption(TEXT("Öffnen mit")), TEXT("... and has no „Öffnen mit“ (no lock)"));
		RadialMenu->ChooseOption(TEXT("Durchsuchen"));
		const TArray<FDCPlanTask>& Steps = Plans->GetTasks(Vic->GetFName());
		Expect(Steps.Num() == 1 && Steps[0].Type == EDCPlanTaskType::Loot, TEXT("... which plans one search"));
		for (TActorIterator<ADCItemPickup> It(GetWorld()); It; ++It)
		{
			const UDCItem* Item = It->GetItem();
			if (Item && Item->GetDef().bMovable && Item->GetDef().Size == EDCItemSize::Small && !Item->GetDef().IsContainer() && Item->GetContents().IsEmpty())
			{
				OpenPickupMenu(*It);
				Expect(!RadialMenu->HasOption(TEXT("Durchsuchen")) && RadialMenu->HasOption(TEXT("Mitnehmen")),
					FString::Printf(TEXT("%s (small, loose): only „Mitnehmen“"), *Item->GetDisplayName().ToString()));
				RadialMenu->Close();
				break;
			}
		}
		Plans->ClearAll();
		return true;
	});
	SelfTestSteps.Add(Wait(0.3f));
	// The desk's drawer is locked: "Durchsuchen" asks how to open it.
	SelfTestSteps.Add([this, Expect, CrewNamed, Has]()
	{
		ADCItemPickup* Desk = FindPickup(TEXT("desk"));
		if (!Desk)
		{
			Expect(false, TEXT("the desk is there"));
			return true;
		}
		OpenPickupMenu(Desk);
		Has(TEXT("Öffnen mit"), true, TEXT("the desk offers „Öffnen mit …“ (its drawer is locked)"));
		RadialMenu->ChooseOption(TEXT("Durchsuchen"));
		Expect(RadialMenu->IsOpen() && RadialMenu->GetTitle().ToString().Contains(TEXT("Schublade")),
			FString::Printf(TEXT("„Durchsuchen“ opens the second ring „%s“"), *RadialMenu->GetTitle().ToString().Replace(TEXT("\n"), TEXT(" / "))));
		Has(TEXT("mit Dietrich-Set knacken"), true, TEXT("Vic can pick the drawer"));
		Has(TEXT("mit Brecheisen aufbrechen"), false, TEXT("... but not break it open (no crowbar)"));
		Has(TEXT("ohne Schublade durchsuchen"), true, TEXT("... or search the rest without it"));
		Has(TEXT("zurück"), true, TEXT("... and go back"));
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("search_desk_ring.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, Has]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		RadialMenu->ChooseOption(TEXT("knacken"));
		const TArray<FDCPlanTask>& Steps = Plans->GetTasks(Vic->GetFName());
		Expect(Steps.Num() == 2 && Steps[0].Type == EDCPlanTaskType::Item && Steps[0].Action == static_cast<uint8>(EDCItemAction::PickLock)
			&& Steps[0].ItemPath == TEXT("desk_drawer") && Steps[1].Type == EDCPlanTaskType::Loot, TEXT("one choice plans two steps: pick the drawer, then search the desk"));
		// The safe is too much for Vic: both ways greyed out, each saying why.
		OpenPickupMenu(FindPickup(TEXT("safe")));
		RadialMenu->ChooseOption(TEXT("Durchsuchen"));
		Has(TEXT("mit Stethoskop knacken"), false, TEXT("Vic cannot crack the safe"));
		Has(TEXT("aufbrechen"), false, TEXT("... and nobody breaks it open"));
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("search_safe_vic.png")));
	// The display case: "Öffnen mit" offers smashing the glass; "zurück" leads back to the first ring.
	SelfTestSteps.Add([this, Expect, Has]()
	{
		ADCItemPickup* Case = FindPickup(TEXT("display_case"));
		if (!Case)
		{
			Expect(false, TEXT("the display case is there"));
			return true;
		}
		OpenPickupMenu(Case);
		RadialMenu->ChooseOption(TEXT("Öffnen mit"));
		Has(TEXT("Glas einschlagen"), false, TEXT("the display case: „Glas einschlagen“ (Vic has no crowbar)"));
		Has(TEXT("mit Dietrich-Set knacken"), true, TEXT("... or pick its lock"));
		RadialMenu->ChooseOption(TEXT("zurück"));
		Expect(RadialMenu->IsOpen() && RadialMenu->HasOption(TEXT("Durchsuchen")), TEXT("„zurück“ shows the first ring again"));
		RadialMenu->Close();
		RadialMenu->bOpenAtCentreForTests = false;
		return true;
	});
	SelfTestSteps.Add(Wait(0.3f));
	// The heist: Rosa cracks the safe and searches it. Everything in it is worth something and fits: no stop.
	SelfTestSteps.Add([this, Expect, CrewNamed, ValueBefore]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->ClearAll();
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		if (!Rosa || !Safe)
		{
			Expect(false, TEXT("Rosa and the safe are there"));
			return true;
		}
		Select(Rosa);
		OpenPickupMenu(Safe);
		const bool bChosen = RadialMenu->ChooseOption(TEXT("Durchsuchen")) && RadialMenu->ChooseOption(TEXT("mit Stethoskop knacken"));
		const TArray<FDCPlanTask>& Steps = Plans->GetTasks(Rosa->GetFName());
		Expect(bChosen && Steps.Num() == 2 && Steps[1].Type == EDCPlanTaskType::Loot, TEXT("Rosa: crack the safe, then search it"));
		*ValueBefore = Rosa->GetInventory()->GetTotalValue();
		StartPlayback();
		return true;
	});
	SelfTestSteps.Add([this, bEverPaused, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		*bEverPaused |= IsDecidingFind();
		if (IsDecidingFind() && ContainerPanel)
		{
			// Should not happen; let it go on so the test ends.
			ContainerPanel->ContinueFind();
		}
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaying() && Heist->IsPlaybackFinished()) || Now - Started > 90.0;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Shot(TEXT("search_packed.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, bEverPaused, ValueBefore]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const int32 Value = Rosa ? Rosa->GetInventory()->GetTotalValue() : 0;
		Expect(!*bEverPaused, TEXT("the game did not stop: Rosa packed the valuables herself"));
		Expect(Value > *ValueBefore, FString::Printf(TEXT("Rosa carries more of value now (%d -> %d €)"), *ValueBefore, Value));
		Expect(Rosa && Rosa->GetPlanRunner()->GetStatus(1) == EDCTaskStatus::Done, TEXT("her search step is done"));
		return true;
	});
	SelfTestSteps.Add([this, bPassed]()
	{
		FinishSelfTest(*bPassed);
		return true;
	});
}
