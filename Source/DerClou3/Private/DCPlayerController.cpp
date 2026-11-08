#include "DCPlayerController.h"

#include "DCClickMarker.h"
#include "DCHeistState.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCTimelinePanel.h"
#include "DCCameraPawn.h"
#include "DCPlanGhost.h"
#include "Misc/ScopeExit.h"
#include "DCCrewBar.h"
#include "DCWaitDialog.h"
#include "DCFrame.h"
#include "Kismet/GameplayStatics.h"
#include "DCContainerPanel.h"
#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCInfoPanel.h"
#include "DCInventoryComponent.h"
#include "DCInventoryPanel.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCItemWidgets.h"
#include "DCNoise.h"
#include "DCNoiseHud.h"
#include "DCNoiseListener.h"
#include "DCRadialMenu.h"
#include "DCRoomVolume.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "DCFloors.h"
#include "DCGameFlow.h"
#include "DCMenuWidget.h"
#include "NavigationSystem.h"

#define LOCTEXT_NAMESPACE "DCPlayerController"

ADCPlayerController::ADCPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	// Keys and buttons keep working while the plan is paused.
	bShouldPerformFullTickWhenPaused = true;
}

void ADCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (!IsLocalController())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Marker = GetWorld()->SpawnActor<ADCClickMarker>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// The frame around the scene comes first, everything else sits in or on it.
	Frame = CreateWidget<UDCFrame>(this, UDCFrame::StaticClass());
	if (Frame)
	{
		Frame->AddToViewport(0);
	}
	RadialMenu = CreateWidget<UDCRadialMenu>(this, UDCRadialMenu::StaticClass());
	if (RadialMenu)
	{
		// Covers the screen, but only its buttons take clicks.
		RadialMenu->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RadialMenu->AddToViewport(10);
	}
	InfoPanel = CreateWidget<UDCInfoPanel>(this, UDCInfoPanel::StaticClass());
	if (InfoPanel)
	{
		InfoPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		InfoPanel->AddToViewport(5);
	}
	InventoryPanel = CreateWidget<UDCInventoryPanel>(this, UDCInventoryPanel::StaticClass());
	if (InventoryPanel)
	{
		InventoryPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		InventoryPanel->AddToViewport(6);
		InventoryPanel->Show();
	}
	NoiseHud = CreateWidget<UDCNoiseHud>(this, UDCNoiseHud::StaticClass());
	if (NoiseHud)
	{
		NoiseHud->SetVisibility(ESlateVisibility::HitTestInvisible);
		NoiseHud->AddToViewport(4);
	}
	TimelinePanel = CreateWidget<UDCTimelinePanel>(this, UDCTimelinePanel::StaticClass());
	if (TimelinePanel)
	{
		TimelinePanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		TimelinePanel->AddToViewport(8);
	}
	CrewBar = CreateWidget<UDCCrewBar>(this, UDCCrewBar::StaticClass());
	if (CrewBar)
	{
		CrewBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CrewBar->AddToViewport(6);
	}
	WaitDialog = CreateWidget<UDCWaitDialog>(this, UDCWaitDialog::StaticClass());
	if (WaitDialog)
	{
		WaitDialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WaitDialog->AddToViewport(20);
	}
	ContainerPanel = CreateWidget<UDCContainerPanel>(this, UDCContainerPanel::StaticClass());
	if (ContainerPanel)
	{
		ContainerPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ContainerPanel->AddToViewport(7);
	}
	PauseMenu = CreateWidget<UDCMenuWidget>(this, UDCMenuWidget::StaticClass());
	if (PauseMenu)
	{
		PauseMenu->Setup(true);
		PauseMenu->OnCloseRequested = [WeakThis = TWeakObjectPtr<ADCPlayerController>(this)]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ClosePauseMenu();
			}
		};
		PauseMenu->SetVisibility(ESlateVisibility::Collapsed);
		PauseMenu->AddToViewport(50);
	}

	// Start with the first crew member selected.
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		Select(*It);
		break;
	}

	FString SelfTest;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), SelfTest) && SelfTest == TEXT("Inventory"))
	{
		StartInventorySelfTest();
	}
	else if (SelfTest == TEXT("Containers"))
	{
		StartContainerSelfTest();
	}
	else if (SelfTest == TEXT("Noise"))
	{
		StartNoiseSelfTest();
	}
	else if (SelfTest == TEXT("Timeline"))
	{
		StartTimelineSelfTest();
	}
	else if (SelfTest == TEXT("Reload"))
	{
		StartReloadSelfTest();
	}
	else if (SelfTest == TEXT("PlanInventory"))
	{
		StartPlanInventorySelfTest();
	}
	else if (SelfTest == TEXT("Signals"))
	{
		StartSignalSelfTest();
	}
	else if (SelfTest == TEXT("Grid"))
	{
		StartGridSelfTest();
	}
	else if (SelfTest == TEXT("Find"))
	{
		StartFindSelfTest();
	}
	else if (SelfTest == TEXT("PlanView"))
	{
		StartPlanViewSelfTest();
	}
	else if (SelfTest == TEXT("DoorWay"))
	{
		StartDoorWaySelfTest();
	}
	else if (SelfTest == TEXT("House"))
	{
		StartHouseSelfTest();
	}
	else if (SelfTest == TEXT("AutoDoor"))
	{
		StartAutoDoorSelfTest();
	}
	else if (SelfTest == TEXT("Undo"))
	{
		StartUndoSelfTest();
	}
	else if (SelfTest == TEXT("PlanHelp"))
	{
		StartPlanHelpSelfTest();
	}
	else if (SelfTest == TEXT("PlanLook"))
	{
		StartPlanLookSelfTest();
	}
	else if (SelfTest == TEXT("Insert"))
	{
		StartInsertSelfTest();
	}
	else if (SelfTest == TEXT("Variants"))
	{
		StartVariantsSelfTest();
	}
	else if (SelfTest == TEXT("Villa"))
	{
		StartVillaSelfTest();
	}

	// Coming back from "Abspielen" after a reload: play right away.
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (Plans && Plans->bPlayAfterLoad)
	{
		Plans->bPlayAfterLoad = false;
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &ADCPlayerController::StartPlaybackWhenLoaded, 0.5f, false);
	}
	else if (Plans && Plans->GetMode() == EDCPlanMode::Playing)
	{
		Plans->SetMode(EDCPlanMode::Planning);
	}
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHoverCursor();

	if (!SelfTestSteps.IsEmpty() && SelfTestSteps[0]())
	{
		SelfTestSteps.RemoveAt(0);
	}
	// While the loading screen is up the game takes no input.
	if (const UDCGameFlowSubsystem* Flow = UDCGameFlowSubsystem::Get(this); Flow && Flow->IsLoading())
	{
		return;
	}
	// The game menu has all keys to itself: Esc closes its page, then the menu.
	if (IsPauseMenuOpen())
	{
		if (WasInputKeyJustPressed(EKeys::Escape) && !PauseMenu->Back())
		{
			ClosePauseMenu();
		}
		return;
	}
	// While a find waits for a decision: Enter = "Weiter".
	if (ContainerPanel && ContainerPanel->IsFindMode())
	{
		if (WasInputKeyJustPressed(EKeys::Enter))
		{
			ContainerPanel->ContinueFind();
		}
		return;
	}
	// While the wait dialog asks, only its keys count.
	if (WaitDialog && WaitDialog->IsOpen())
	{
		if (WasInputKeyJustPressed(EKeys::Escape) || GetMode() != EDCPlanMode::Planning)
		{
			WaitDialog->Close();
		}
		else if (WasInputKeyJustPressed(EKeys::Enter))
		{
			WaitDialog->Confirm();
		}
		return;
	}
	if (InventoryPanel && WasInputKeyJustPressed(EKeys::I))
	{
		InventoryPanel->Toggle();
	}
	// 1 to 9: the crew members, in the order of the cards on the left.
	{
		static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		int32 Number = 0;
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It && Number < 9; ++It, ++Number)
		{
			if (WasInputKeyJustPressed(Digits[Number]))
			{
				SelectFromBar(*It);
				break;
			}
		}
	}
	// Planning: keep the timeline up to date (walking times change with what is carried).
	if (UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this); Heist && GetMode() == EDCPlanMode::Planning)
	{
		RefreshTimer += DeltaTime;
		if (RefreshTimer >= 0.5f)
		{
			RefreshTimer = 0.f;
			Heist->Refresh(false);
		}
	}
	if (WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		GetMode() == EDCPlanMode::Planning ? StartPlayback() : TogglePause();
	}
	if (WasInputKeyJustPressed(EKeys::BackSpace) || WasInputKeyJustPressed(EKeys::Delete))
	{
		DeleteStep();
	}
	// Strg+Z still takes the last change back (no buttons for it any more, no "Wiederholen").
	const bool bControl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bControl && WasInputKeyJustPressed(EKeys::Z))
	{
		UndoPlan();
	}

	const bool bMenuOpen = RadialMenu && RadialMenu->IsOpen();
	if (bMenuOpen && (WasInputKeyJustPressed(EKeys::RightMouseButton) || WasInputKeyJustPressed(EKeys::Escape)))
	{
		RadialMenu->Close();
		return;
	}
	if (!bMenuOpen && WasInputKeyJustPressed(EKeys::Escape))
	{
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		if (Heist && Heist->GetPreviewTime() >= 0.f)
		{
			Heist->SetPreviewTime(-1.f);
			return;
		}
	}
	if (!bMenuOpen && ContainerPanel && ContainerPanel->IsOpen() && WasInputKeyJustPressed(EKeys::Escape))
	{
		ContainerPanel->Close();
		return;
	}
	if (!bMenuOpen && InfoPanel && InfoPanel->IsShown() && WasInputKeyJustPressed(EKeys::Escape))
	{
		InfoPanel->Hide();
		return;
	}
	// Esc: no step picked any more - new steps go to the end again.
	if (!bMenuOpen && WasInputKeyJustPressed(EKeys::Escape) && GetInsertIndex() != INDEX_NONE)
	{
		InsertAtEnd();
		return;
	}
	// Esc with nothing else to close: the game menu.
	if (!bMenuOpen && WasInputKeyJustPressed(EKeys::Escape))
	{
		OpenPauseMenu();
		return;
	}
	// Clicks on the menu buttons are handled by the UI and never arrive here.
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (bMenuOpen)
		{
			// Clicking next to the menu just closes it.
			RadialMenu->Close();
			return;
		}
		HandleClick();
	}
}

void ADCPlayerController::UpdateHoverCursor()
{
	FHitResult Hit;
	const bool bHit = UDCFloorSubsystem::HitUnderCursor(this, Hit);
	ADCItemPickup* Pickup = bHit ? Cast<ADCItemPickup>(Hit.GetActor()) : nullptr;
	const bool bOverDoor = bHit && (Cast<ADCDoor>(Hit.GetActor()) || Cast<ADCNoiseListener>(Hit.GetActor()));
	CurrentMouseCursor = bOverDoor || Pickup ? EMouseCursor::Hand : EMouseCursor::Default;

	// Name label over the item under the mouse.
	if (HoveredPickup.Get() != Pickup)
	{
		if (HoveredPickup.IsValid())
		{
			HoveredPickup->SetHighlighted(false);
		}
		HoveredPickup = Pickup;
		if (Pickup)
		{
			Pickup->SetHighlighted(true);
		}
	}
}

void ADCPlayerController::OpenDoorMenu(ADCDoor* Door)
{
	if (!RadialMenu || !Door || !Selected)
	{
		return;
	}
	const bool bPlanning = GetMode() == EDCPlanMode::Planning;
	// Planning: with what the crew member will carry by then.
	const UDCInventoryComponent* Carried = bPlanning ? UDCHeistStateSubsystem::Get(this)->GetPlannedInventory(Selected->GetFName()) : nullptr;
	// Planning offers what the door will allow once the plan so far has run.
	bool bLocked = Door->IsLocked();
	bool bOpen = Door->IsOpen();
	if (bPlanning)
	{
		if (const FDCSimDoor* Planned = const_cast<FDCWorldSnapshot&>(UDCHeistStateSubsystem::Get(this)->GetPlannedState()).FindDoor(Door->GetFName()))
		{
			bLocked = Planned->bLocked;
			bOpen = Planned->bOpen;
		}
	}
	TArray<EDCDoorAction> Actions;
	if (bLocked)
	{
		Actions = { EDCDoorAction::PickLock, EDCDoorAction::GoThrough };
	}
	else
	{
		// Going through opens it on the way; "Öffnen" / "Schließen" leave it so for the others.
		Actions = { EDCDoorAction::GoThrough, bOpen ? EDCDoorAction::Close : EDCDoorAction::Open };
	}
	const auto DoorTask = [Door](EDCDoorAction Action)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Door;
		Task.Target = Door->GetFName();
		Task.Action = static_cast<uint8>(Action);
		return Task;
	};

	TArray<FDCRadialOption> Options;
	if (bLocked)
	{
		// The usual pair in one click: pick the lock, then go through.
		const FDCTaskCheck PickCheck = Selected->PlanCheckDoor(Door, EDCDoorAction::PickLock, Carried);
		FDCRadialOption Both = MakeTaskOption(LOCTEXT("PickThroughVerb", "knacken und durchgehen"), PickCheck, DoorTask(EDCDoorAction::PickLock));
		Both.Detail = PickCheck.bPossible ? FText::Format(LOCTEXT("PickThroughDetail", "{0} · dann durch"), PickCheck.DescribeShort()) : PickCheck.DescribeShort();
		Both.Tooltip = FText::Format(LOCTEXT("PickThroughTip", "Zwei Schritte: Schloss knacken, danach durchgehen.\n{0}"), PickCheck.Describe());
		TWeakObjectPtr<ADCPlayerController> WeakThis = this;
		const TArray<FDCPlanTask> Pair = { DoorTask(EDCDoorAction::PickLock), DoorTask(EDCDoorAction::GoThrough) };
		const bool bPossible = PickCheck.bPossible;
		const FText Reason = PickCheck.Reason;
		Both.OnSelected = [WeakThis, Pair, bPossible, Reason]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (!bPossible)
			{
				WeakThis->ShowToast(FText::Format(LOCTEXT("NotPossible", "Geht nicht: {0}"), Reason), true);
				return;
			}
			WeakThis->IssueTasks(Pair);
		};
		Options.Add(Both);
	}
	for (const EDCDoorAction Action : Actions)
	{
		const FDCPlanTask Task = DoorTask(Action);
		FDCTaskCheck Check = Selected->PlanCheckDoor(Door, Action, Carried);
		if (Check.bPossible && bLocked && Action == EDCDoorAction::GoThrough)
		{
			// Through a locked door: only planned, waiting for someone to unlock it.
			Check.bPossible = bPlanning;
			Check.Reason = bPlanning ? LOCTEXT("ThroughLockedPlan", "wartet, bis jemand aufsperrt") : LOCTEXT("ThroughLocked", "ist verschlossen");
			Options.Add(MakeTaskOption(ADCDoor::DescribeAction(Action), Check, Task));
			Options.Last().Detail = Check.Reason;
			Options.Last().Tooltip = Check.Reason;
			Options.Last().bEnabled = bPlanning;
			continue;
		}
		Options.Add(MakeTaskOption(ADCDoor::DescribeAction(Action), Check, Task));
	}
	// Info is always the last entry, for every interactable.
	TWeakObjectPtr<UDCInfoPanel> WeakPanel = InfoPanel.Get();
	TWeakObjectPtr<ADCDoor> WeakDoor = Door;
	FDCRadialOption& Info = Options.AddDefaulted_GetRef();
	Info.Label = LOCTEXT("Info", "Info");
	Info.Icon = TEXT("info");
	Info.OnSelected = [WeakPanel, WeakDoor]()
	{
		if (WeakPanel.IsValid() && WeakDoor.IsValid())
		{
			WeakPanel->ShowFor(WeakDoor.Get());
		}
	};
	const FText Title = FText::Format(bPlanning ? LOCTEXT("PlanTitle", "{0}\nplant: {1}") : LOCTEXT("DirectTitle", "{0}\n{1}"),
		UDCHeistStateSubsystem::Get(this)->DescribePlannedDoor(Door), Selected->GetCrewName());
	RadialMenu->Open(Title, MoveTemp(Options));
}

void ADCPlayerController::OpenPickupMenu(ADCItemPickup* Pickup)
{
	UDCItem* Root = Pickup ? Pickup->GetItem() : nullptr;
	if (!RadialMenu || !Root || !Selected)
	{
		return;
	}
	const bool bPlanning = GetMode() == EDCPlanMode::Planning;
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const FDCSimPickup* Planned = bPlanning ? const_cast<FDCWorldSnapshot&>(Heist->GetPlannedState()).FindPickup(Pickup->GetFName()) : nullptr;
	// Planning: tools and hands as they will be after the crew member's plan so far (a crowbar picked up earlier counts).
	const UDCInventoryComponent* Carried = bPlanning ? Heist->GetPlannedInventory(Selected->GetFName()) : nullptr;
	const bool bGoneInPlan = Planned && Planned->bGone;
	// Say who takes it, and in which step: otherwise "gone" looks like a mistake.
	FText GoneReason = LOCTEXT("GoneInPlan", "ist dann schon weg (im Plan mitgenommen)");
	if (bGoneInPlan)
	{
		for (const FDCCrewPlan& Plan : UDCPlanSubsystem::Get(this)->GetPlans())
		{
			for (int32 Index = 0; Index < Plan.Tasks.Num(); ++Index)
			{
				const FDCPlanTask& Task = Plan.Tasks[Index];
				const bool bTakes = Task.Type == EDCPlanTaskType::PickUp
					|| (Task.Type == EDCPlanTaskType::Item && Task.ItemPath.IsEmpty() && Task.Action == static_cast<uint8>(EDCItemAction::Take));
				if (bTakes && Task.Target == Pickup->GetFName())
				{
					const ADCCrewCharacter* Taker = Heist->FindCrew(Plan.Crew);
					GoneReason = FText::Format(LOCTEXT("GoneInPlanBy", "schon im Plan: {0} nimmt es in Schritt {1}"),
						Taker ? Taker->GetCrewName() : FText::FromName(Plan.Crew), FText::AsNumber(Index + 1));
				}
			}
		}
	}
	// Locked, smashed, ... as it will be after the plan so far (planning) or as it is now (direct).
	const auto StateOf = [Planned](const UDCItem* Item, bool& bLocked, bool& bDestroyed)
	{
		bLocked = Item->IsLocked();
		bDestroyed = Item->IsDestroyed();
		if (Planned)
		{
			const int32 Index = FDCWorldSnapshot::FindItem(*Planned, UDCHeistStateSubsystem::PathOf(Item));
			if (Index != INDEX_NONE)
			{
				bLocked = Planned->Items[Index].bLocked;
				bDestroyed = Planned->Items[Index].bDestroyed;
			}
		}
	};
	const auto Check = [this, bPlanning, Carried, bGoneInPlan, GoneReason](const UDCItem* Item, EDCItemAction Action)
	{
		if (bGoneInPlan)
		{
			FDCTaskCheck Gone;
			Gone.bPossible = false;
			Gone.Reason = GoneReason;
			return Gone;
		}
		return bPlanning ? Selected->PlanCheckItem(Item->GetDef(), Action, Carried) : Selected->CheckItemAction(Item, Action);
	};
	const auto ItemTask = [Pickup](const UDCItem* Item, EDCItemAction Action)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Item;
		Task.Target = Pickup->GetFName();
		Task.ItemPath = UDCHeistStateSubsystem::PathOf(Item);
		Task.Action = static_cast<uint8>(Action);
		return Task;
	};

	TArray<FDCRadialOption> Options;
	// Locks and glass: the thing itself and its built-in parts (the desk's drawer).
	TArray<UDCItem*> WithLocks = { Root };
	for (UDCItem* Part : Root->GetContents())
	{
		if (Part->IsAttached())
		{
			WithLocks.Add(Part);
		}
	}
	for (UDCItem* Item : WithLocks)
	{
		const FDCItemDef& Def = Item->GetDef();
		bool bLocked = false;
		bool bDestroyed = false;
		StateOf(Item, bLocked, bDestroyed);
		const auto Label = [Item, Root](const FText& Verb)
		{
			return Item == Root ? Verb : FText::Format(LOCTEXT("PartAction", "{0} {1}"), Item->GetDisplayName(), Verb);
		};
		if (bLocked && Def.HasLock())
		{
			Options.Add(MakeTaskOption(Label(LOCTEXT("PickVerb", "knacken")), Check(Item, EDCItemAction::PickLock), ItemTask(Item, EDCItemAction::PickLock)));
			// The usual pair in one click: pick the lock, then search the whole thing.
			if (Root->IsContainer() || !Root->GetContents().IsEmpty())
			{
				const FDCTaskCheck PickCheck = Check(Item, EDCItemAction::PickLock);
				FDCPlanTask Search;
				Search.Type = EDCPlanTaskType::Loot;
				Search.Target = Pickup->GetFName();
				FDCRadialOption Both = MakeTaskOption(Label(LOCTEXT("PickSearchVerb", "knacken und durchsuchen")), PickCheck, ItemTask(Item, EDCItemAction::PickLock));
				Both.Detail = PickCheck.bPossible ? FText::Format(LOCTEXT("PickSearchDetail", "{0} · dann durchsuchen"), PickCheck.DescribeShort()) : PickCheck.DescribeShort();
				Both.Tooltip = FText::Format(LOCTEXT("PickSearchTip", "Zwei Schritte: knacken, danach durchsuchen.\n{0}"), PickCheck.Describe());
				TWeakObjectPtr<ADCPlayerController> WeakThis = this;
				const TArray<FDCPlanTask> Pair = { ItemTask(Item, EDCItemAction::PickLock), Search };
				const bool bPossible = PickCheck.bPossible;
				const FText Reason = PickCheck.Reason;
				Both.OnSelected = [WeakThis, Pair, bPossible, Reason]()
				{
					if (!WeakThis.IsValid())
					{
						return;
					}
					if (!bPossible)
					{
						WeakThis->ShowToast(FText::Format(LOCTEXT("NotPossible", "Geht nicht: {0}"), Reason), true);
						return;
					}
					WeakThis->IssueTasks(Pair);
				};
				Options.Add(Both);
			}
			if (Def.bForceable)
			{
				Options.Add(MakeTaskOption(Label(LOCTEXT("ForceVerb", "aufbrechen")), Check(Item, EDCItemAction::Force), ItemTask(Item, EDCItemAction::Force)));
			}
		}
		if (Def.bBreakable && !bDestroyed)
		{
			Options.Add(MakeTaskOption(Label(LOCTEXT("SmashVerb", "einschlagen")), Check(Item, EDCItemAction::Smash), ItemTask(Item, EDCItemAction::Smash)));
		}
	}
	// Search: open what opens; what is found is decided during the heist.
	if (Root->IsContainer() || !Root->GetContents().IsEmpty())
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Loot;
		Task.Target = Pickup->GetFName();
		FDCTaskCheck LootCheck;
		LootCheck.bPossible = !bGoneInPlan;
		LootCheck.Reason = GoneReason;
		FDCRadialOption Option = MakeTaskOption(LOCTEXT("Loot", "Durchsuchen"), LootCheck, Task);
		Option.Detail = bGoneInPlan ? LootCheck.Reason : LOCTEXT("LootDetail", "Inhalt unbekannt · Funde live wählen");
		if (!bGoneInPlan)
		{
			Option.Tooltip = LOCTEXT("LootTip", "Der Inhalt ist noch unbekannt. Beim Einbruch hält das Spiel bei jedem Fund an, dann wird entschieden, was mitkommt.");
		}
		Options.Add(Option);
	}
	if (Root->GetDef().bMovable)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::PickUp;
		Task.Target = Pickup->GetFName();
		FDCTaskCheck Take;
		const FText Reason = bGoneInPlan ? GoneReason
			: (Carried ? Carried : Selected->GetInventory())->CheckAdd(Root);
		Take.bPossible = Reason.IsEmpty();
		Take.Reason = Reason;
		Options.Add(MakeTaskOption(LOCTEXT("PickUp", "Aufheben"), Take, Task));
	}
	if (!bPlanning && (Root->IsContainer() || !Root->GetContents().IsEmpty()))
	{
		TWeakObjectPtr<ADCItemPickup> WeakPickup = Pickup;
		TWeakObjectPtr<ADCCrewCharacter> WeakCrew = Selected.Get();
		FDCRadialOption& Search = Options.AddDefaulted_GetRef();
		Search.Label = LOCTEXT("Search", "Durchsuchen");
		Search.Icon = TEXT("search");
		Search.Detail = LOCTEXT("SearchDetail", "hingehen und selbst umpacken");
		Search.OnSelected = [WeakPickup, WeakCrew]()
		{
			if (WeakPickup.IsValid() && WeakCrew.IsValid())
			{
				WeakCrew->GetPlanRunner()->Stop();
				WeakCrew->Search(WeakPickup.Get());
			}
		};
	}
	// Info is always the last entry, for every interactable.
	TWeakObjectPtr<UDCInfoPanel> WeakPanel = InfoPanel.Get();
	TWeakObjectPtr<ADCItemPickup> WeakInfo = Pickup;
	FDCRadialOption& Info = Options.AddDefaulted_GetRef();
	Info.Label = LOCTEXT("Info", "Info");
	Info.Icon = TEXT("info");
	Info.OnSelected = [WeakPanel, WeakInfo]()
	{
		if (WeakPanel.IsValid() && WeakInfo.IsValid())
		{
			WeakPanel->ShowFor(WeakInfo.Get());
		}
	};
	const FText Title = FText::Format(bPlanning ? LOCTEXT("PlanTitle", "{0}\nplant: {1}") : LOCTEXT("DirectTitle", "{0}\n{1}"), Pickup->GetInteractableName(), Selected->GetCrewName());
	RadialMenu->Open(Title, MoveTemp(Options));
}

FDCRadialOption ADCPlayerController::MakeTaskOption(const FText& Label, const FDCTaskCheck& Check, const FDCPlanTask& Task)
{
	FDCRadialOption Option;
	Option.Label = Label;
	Option.Icon = DCItemWidgets::IconForTask(Task);
	Option.Detail = Check.DescribeShort();
	Option.Tooltip = Check.Describe();
	Option.bEnabled = Check.bPossible;
	if (Check.bPossible && Selected && GetMode() == EDCPlanMode::Planning)
	{
		for (const FDCPlanProblem& Problem : UDCHeistStateSubsystem::Get(this)->GetSchedule().FindProblems())
		{
			if (Problem.Crew == Selected->GetFName())
			{
				Option.Detail = FText::Format(LOCTEXT("ComesTooLate", "kommt nicht dran – {0} hängt bei Schritt {1}"), Problem.CrewName, FText::AsNumber(Problem.Index + 1));
				Option.Tooltip = FText::Format(LOCTEXT("ComesTooLateTip", "{0}\nNeue Schritte kommen ans Ende – {1} hängt vorher bei Schritt {2}: {3}"), Check.Describe(), Problem.CrewName,
					FText::AsNumber(Problem.Index + 1), Problem.ShortReason);
				Option.Tone = EDCOptionTone::Problem;
			}
		}
	}
	TWeakObjectPtr<ADCPlayerController> WeakThis = this;
	TWeakObjectPtr<ADCCrewCharacter> WeakCrew = Selected.Get();
	const bool bPossible = Check.bPossible;
	const FText Reason = Check.Reason;
	Option.OnSelected = [WeakThis, WeakCrew, Task, bPossible, Reason]()
	{
		if (!WeakThis.IsValid() || !WeakCrew.IsValid())
		{
			return;
		}
		if (!bPossible)
		{
			WeakThis->ShowToast(FText::Format(LOCTEXT("NotPossible", "Geht nicht: {0}"), Reason), true);
			UE_LOG(LogDerClou, Display, TEXT("Not possible: %s"), *Reason.ToString());
			return;
		}
		WeakThis->IssueTask(Task);
	};
	return Option;
}

void ADCPlayerController::Select(ADCCrewCharacter* Crew)
{
	if (Selected)
	{
		Selected->SetSelected(false);
		Selected->OnContainerReached.Remove(ContainerReachedHandle);
	}
	Selected = Crew;
	if (Selected)
	{
		Selected->SetSelected(true);
		ContainerReachedHandle = Selected->OnContainerReached.AddUObject(this, &ADCPlayerController::OnContainerReached);
	}
	if (ContainerPanel)
	{
		ContainerPanel->Close();
	}
	if (InventoryPanel)
	{
		InventoryPanel->SetCrew(Selected);
	}
	if (UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this))
	{
		Heist->SetFocus(Selected ? Selected->GetFName() : NAME_None);
	}
}

void ADCPlayerController::HandleClick()
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY))
	{
		HandleClickAt(FVector2D(MouseX, MouseY));
	}
}

bool ADCPlayerController::ClickWorldPoint(const FVector& Location)
{
	FVector2D Screen;
	if (!ProjectWorldLocationToScreen(Location, Screen))
	{
		return false;
	}
	HandleClickAt(Screen);
	return true;
}

void ADCPlayerController::HandleClickAt(const FVector2D& ScreenPosition)
{
	FHitResult Hit;
	if (!UDCFloorSubsystem::HitUnderScreenPosition(this, ScreenPosition, Hit))
	{
		return;
	}
	if (ADCCrewCharacter* Crew = Cast<ADCCrewCharacter>(Hit.GetActor()))
	{
		const bool bOther = Crew != Selected;
		Select(Crew);
		if (bOther)
		{
			ShowCrewMoment(Crew);
		}
		return;
	}
	// The ghost of a crew member stands for them: clicking it selects them.
	if (const ADCPlanGhost* Ghost = Cast<ADCPlanGhost>(Hit.GetActor()))
	{
		if (ADCCrewCharacter* Crew = UDCHeistStateSubsystem::Get(this)->FindCrew(Ghost->GetCrew()))
		{
			const bool bOther = Crew != Selected;
			Select(Crew);
			if (bOther)
			{
				ShowCrewMoment(Crew);
			}
		}
		return;
	}
	// People who listen: clicking them shows how suspicious they are.
	if (ADCNoiseListener* Listener = Cast<ADCNoiseListener>(Hit.GetActor()))
	{
		if (InfoPanel)
		{
			InfoPanel->ShowFor(Listener);
		}
		return;
	}
	// While the plan plays, the crew does what was planned.
	if (!Selected || GetMode() == EDCPlanMode::Playing)
	{
		return;
	}
	if (ADCDoor* Door = Cast<ADCDoor>(Hit.GetActor()))
	{
		OpenDoorMenu(Door);
		return;
	}
	if (ADCItemPickup* Pickup = Cast<ADCItemPickup>(Hit.GetActor()))
	{
		OpenPickupMenu(Pickup);
		return;
	}
	if (GetMode() == EDCPlanMode::Planning)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Move;
		Task.Location = Hit.ImpactPoint;
		// The goal is the walkable ground there (not the top of a hedge or a fountain rim).
		if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation OnGround;
			if (Navigation->ProjectPointToNavigation(Hit.ImpactPoint, OnGround, FVector(80.0, 80.0, 250.0)))
			{
				Task.Location = OnGround.Location;
			}
			else if (UDCHeistStateSubsystem::IsNavigationReady(this))
			{
				UE_LOG(LogDerClou, Display, TEXT("No ground: click at %s on %s"), *Hit.ImpactPoint.ToCompactString(), *GetNameSafe(Hit.GetActor()));
				ShowToast(LOCTEXT("NoGround", "Dort kann niemand stehen"), true);
				if (Marker)
				{
					Marker->Show(Hit.ImpactPoint, false);
				}
				return;
			}
		}
		IssueTask(Task);
		if (Marker)
		{
			// Red when this click made the plan hang.
			Marker->Show(Hit.ImpactPoint, !bLastEditProblem);
		}
		return;
	}
	TryMoveTo(Hit.ImpactPoint);
}

void ADCPlayerController::TryMoveTo(const FVector& Location)
{
	UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Selected || !Heist)
	{
		return;
	}
	const bool bReachable = Heist->CanReach(Selected->GetActorLocation(), Location);
	if (!Heist->FindDoorsOnWay(Selected->GetActorLocation(), Location).IsEmpty())
	{
		// Through a door: as a plan of one step, so the door opens on the way (or she says it is locked).
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Move;
		Task.Location = Location;
		IssueTask(Task);
	}
	else
	{
		Selected->GetPlanRunner()->Stop();
		Selected->MoveToLocation(Location);
		Heist->MarkWorldUsed();
	}
	if (Marker)
	{
		Marker->Show(Location, bReachable);
	}
	UE_LOG(LogDerClou, Verbose, TEXT("%s -> %s (%s)"), *Selected->GetCrewName().ToString(), *Location.ToString(), bReachable ? TEXT("ok") : TEXT("unreachable"));
}

EDCPlanMode ADCPlayerController::GetMode() const
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	return Plans ? Plans->GetMode() : EDCPlanMode::Direct;
}

void ADCPlayerController::IssueTasks(const TArray<FDCPlanTask>& Tasks)
{
	if (!Selected || Tasks.IsEmpty())
	{
		return;
	}
	switch (GetMode())
	{
	case EDCPlanMode::Planning:
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		const int32 At = GetInsertIndex();
		const int32 First = (At == INDEX_NONE ? Plans->GetTasks(Selected->GetFName()).Num() : At) + 1;
		TArray<FString> Names;
		for (const FDCPlanTask& Task : Tasks)
		{
			Names.Add(UDCHeistStateSubsystem::Get(this)->DescribeTask(Task).ToString());
		}
		const TArray<TPair<FName, FGuid>> Before = CollectProblems();
		{
			// One click, one undo step.
			FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoAddSeveral", "{0}: \u201e{1}\u201c angelegt"), Selected->GetCrewName(), FText::FromString(FString::Join(Names, TEXT(", ")))));
			Plans->InsertTasks(Selected->GetFName(), At == INDEX_NONE ? TNumericLimits<int32>::Max() : At, Tasks);
		}
		if (At != INDEX_NONE)
		{
			AfterInsert(Selected->GetFName(), First - 1 + Tasks.Num() - 1);
		}
		ShowToast(FText::Format(LOCTEXT("StepsAdded", "{0}, Schritte {1}–{2}: {3}"), Selected->GetCrewName(), FText::AsNumber(First),
			FText::AsNumber(First + Tasks.Num() - 1), FText::FromString(FString::Join(Names, TEXT(", dann ")))));
		bLastEditProblem = CheckPlanAfterEdit(Before, Selected->GetFName(), First - 1);
		FollowNewSteps(First - 1 + Tasks.Num() - 1);
		return;
	}
	case EDCPlanMode::Direct:
		Selected->GetPlanRunner()->Start(Tasks);
		UDCHeistStateSubsystem::Get(this)->MarkWorldUsed();
		return;
	case EDCPlanMode::Playing:
		return;
	}
}

void ADCPlayerController::IssueTask(const FDCPlanTask& Task)
{
	if (!Selected)
	{
		return;
	}
	switch (GetMode())
	{
	case EDCPlanMode::Planning:
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		const TArray<TPair<FName, FGuid>> Before = CollectProblems();
		const int32 At = GetInsertIndex();
		const FName Own = Selected->GetFName();
		const int32 NewIndex = At == INDEX_NONE ? Plans->GetTasks(Own).Num() : At;
		const FText After = At > 0 && Plans->GetTasks(Own).IsValidIndex(At - 1) ? Heist->DescribeTask(Plans->GetTasks(Own)[At - 1]) : FText::GetEmpty();
		{
			FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoAdd", "{0}: \u201e{1}\u201c angelegt"), Selected->GetCrewName(), Heist->DescribeTask(Task)));
			Plans->InsertTask(Own, NewIndex, Task);
		}
		if (At != INDEX_NONE)
		{
			AfterInsert(Own, NewIndex);
		}
		ON_SCOPE_EXIT
		{
			bLastEditProblem = CheckPlanAfterEdit(Before, Own, NewIndex);
			FollowNewSteps(NewIndex);
		};
		ShowToast(At == INDEX_NONE
			? FText::Format(LOCTEXT("StepAdded", "{0}, Schritt {1}: {2}"), Selected->GetCrewName(), FText::AsNumber(NewIndex + 1), Heist->DescribeTask(Task))
			: FText::Format(LOCTEXT("StepInserted", "{0}, neuer Schritt {1}: {2} (nach \u201e{3}\u201c)"), Selected->GetCrewName(), FText::AsNumber(NewIndex + 1), Heist->DescribeTask(Task), After));
		UE_LOG(LogDerClou, Display, TEXT("Plan %s: step %d %s"), *Selected->GetCrewName().ToString(), Plans->GetTasks(Selected->GetFName()).Num(),
			*UDCHeistStateSubsystem::Get(this)->DescribeTask(Task).ToString());
		return;
	}
	case EDCPlanMode::Direct:
		// Direct control: a plan of one step, started right away.
		Selected->GetPlanRunner()->Start({ Task });
		UDCHeistStateSubsystem::Get(this)->MarkWorldUsed();
		return;
	case EDCPlanMode::Playing:
		return;
	}
}

void ADCPlayerController::StartPlayback()
{
	UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Heist || !Plans || Heist->IsPlaying())
	{
		return;
	}
	// The plan always starts from the level's start state.
	if (Heist->IsWorldUsed())
	{
		Plans->bPlayAfterLoad = true;
		ReloadLevel();
		return;
	}
	// Right after loading, a large level still builds its ways: starting now, everyone would stand still.
	if (!UDCHeistStateSubsystem::IsNavigationReady(this))
	{
		ShowToast(LOCTEXT("WaitForWays", "Wege werden noch berechnet …"));
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, this, &ADCPlayerController::StartPlayback, 0.25f, false);
		return;
	}
	if (ContainerPanel)
	{
		ContainerPanel->Close();
	}
	Heist->StartPlayback();
}

void ADCPlayerController::RequestFindDecision(ADCCrewCharacter* Crew, ADCItemPickup* Pickup)
{
	FindQueue.Add({ Crew, Pickup });
	if (!IsDecidingFind())
	{
		StartNextFind();
	}
}

void ADCPlayerController::StartNextFind()
{
	while (!FindQueue.IsEmpty())
	{
		const FFindRequest Next = FindQueue[0];
		FindQueue.RemoveAt(0);
		ADCCrewCharacter* Crew = Next.Crew.Get();
		ADCItemPickup* Pickup = Next.Pickup.Get();
		if (!Crew || !Pickup || !ContainerPanel)
		{
			if (Crew)
			{
				Crew->GetPlanRunner()->ResolveFind(0.f);
			}
			continue;
		}
		FindCrew = Crew;
		UGameplayStatics::SetGamePaused(this, true);
		Select(Crew);
		TWeakObjectPtr<ADCPlayerController> WeakThis = this;
		ContainerPanel->OpenFind(Crew, Pickup, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->FinishFind();
			}
		});
		if (InventoryPanel)
		{
			InventoryPanel->Show();
			InventoryPanel->HandlingHook = [WeakThis](bool bWasOwned, bool bNowOwned)
			{
				if (WeakThis.IsValid() && WeakThis->ContainerPanel)
				{
					WeakThis->ContainerPanel->AddHandling(!bWasOwned && bNowOwned ? DCItems::FindTakeSeconds : DCItems::FindRepackSeconds);
				}
			};
		}
		ShowToast(FText::Format(LOCTEXT("FindToast", "{0} hat in {1} etwas gefunden – das Spiel wartet auf dich"), Crew->GetCrewName(), Pickup->GetInteractableName()));
		UE_LOG(LogDerClou, Display, TEXT("%s found in %s: %s"), *Crew->GetCrewName().ToString(), *Pickup->GetName(), *ContainerPanel->DescribeFound().ToString());
		if (UDCPlanSubsystem::Get(this)->bAutoDecideFinds)
		{
			ContainerPanel->TakeAllValuable();
			ContainerPanel->ContinueFind();
		}
		return;
	}
}

void ADCPlayerController::FinishFind()
{
	ADCCrewCharacter* Crew = FindCrew.Get();
	const float Hold = ContainerPanel ? ContainerPanel->GetHandlingSeconds() : 0.f;
	const FText Result = ContainerPanel ? ContainerPanel->DescribeFindResult() : FText::GetEmpty();
	if (ContainerPanel)
	{
		ContainerPanel->Close();
	}
	if (InventoryPanel)
	{
		InventoryPanel->HandlingHook = nullptr;
	}
	FindCrew.Reset();
	UGameplayStatics::SetGamePaused(this, false);
	if (Crew)
	{
		Crew->GetPlanRunner()->ResolveFind(Hold);
		ShowToast(FText::Format(LOCTEXT("FindDone", "{0}: {1}"), Crew->GetCrewName(), Result));
		UE_LOG(LogDerClou, Display, TEXT("%s decided (%.1f s handling): %s"), *Crew->GetCrewName().ToString(), Hold, *Result.ToString());
	}
	StartNextFind();
}

void ADCPlayerController::TogglePause()
{
	if (IsDecidingFind())
	{
		return;
	}
	if (GetMode() == EDCPlanMode::Playing)
	{
		UGameplayStatics::SetGamePaused(this, !UGameplayStatics::IsGamePaused(this));
	}
}

bool ADCPlayerController::IsPlaybackPaused() const
{
	return UGameplayStatics::IsGamePaused(this);
}

void ADCPlayerController::ReturnToPlanning()
{
	if (UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this))
	{
		Plans->bPlayAfterLoad = false;
		Plans->SetMode(EDCPlanMode::Planning);
	}
	ReloadLevel();
}

void ADCPlayerController::ReturnToPlanningIfNeeded()
{
	if (GetMode() != EDCPlanMode::Planning)
	{
		ReturnToPlanning();
	}
}

void ADCPlayerController::ReloadLevel()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (UDCGameFlowSubsystem* Flow = UDCGameFlowSubsystem::Get(this))
	{
		// Behind the loading screen: the room comes back in its start state, the plan stays.
		Flow->ReloadCurrentMap(this);
		return;
	}
	const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	UGameplayStatics::OpenLevel(this, FName(*Map));
}

void ADCPlayerController::StartPlaybackWhenLoaded()
{
	const UDCGameFlowSubsystem* Flow = UDCGameFlowSubsystem::Get(this);
	if (Flow && Flow->IsLoading())
	{
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, this, &ADCPlayerController::StartPlaybackWhenLoaded, 0.1f, false);
		return;
	}
	StartPlayback();
}

void ADCPlayerController::OpenPauseMenu()
{
	if (!PauseMenu || IsPauseMenuOpen())
	{
		return;
	}
	if (RadialMenu && RadialMenu->IsOpen())
	{
		RadialMenu->Close();
	}
	PauseMenu->Refresh();
	PauseMenu->SetVisibility(ESlateVisibility::Visible);
	// A running plan stops while the menu is open.
	bPausedByMenu = GetMode() == EDCPlanMode::Playing && !UGameplayStatics::IsGamePaused(this);
	if (bPausedByMenu)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
}

void ADCPlayerController::ClosePauseMenu()
{
	if (!IsPauseMenuOpen())
	{
		return;
	}
	PauseMenu->SetVisibility(ESlateVisibility::Collapsed);
	if (bPausedByMenu)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedByMenu = false;
	}
}

bool ADCPlayerController::IsPauseMenuOpen() const
{
	return PauseMenu && PauseMenu->GetVisibility() == ESlateVisibility::Visible;
}

void ADCPlayerController::SetDirectMode(bool bDirect)
{
	if (UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this))
	{
		Plans->SetMode(bDirect ? EDCPlanMode::Direct : EDCPlanMode::Planning);
	}
}

void ADCPlayerController::ToggleInventory()
{
	if (InventoryPanel)
	{
		InventoryPanel->Toggle();
	}
}

void ADCPlayerController::AddWait(float Seconds)
{
	if (GetMode() == EDCPlanMode::Planning)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Wait;
		Task.Seconds = Seconds;
		IssueTask(Task);
	}
}

void ADCPlayerController::RemoveLastStep()
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (Selected && Plans && GetMode() == EDCPlanMode::Planning && !Plans->GetTasks(Selected->GetFName()).IsEmpty())
	{
		const TArray<TPair<FName, FGuid>> Before = CollectProblems();
		{
			const FText Label = UDCHeistStateSubsystem::Get(this)->DescribeTask(Plans->GetTasks(Selected->GetFName()).Last());
			FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoRemove", "{0}: \u201e{1}\u201c gelöscht"), Selected->GetCrewName(), Label));
			Plans->RemoveLastTask(Selected->GetFName());
		}
		CheckPlanAfterEdit(Before);
	}
}

void ADCPlayerController::DeleteStep()
{
	const int32 Insert = GetInsertIndex();
	Insert != INDEX_NONE ? RemoveStep(Insert - 1) : RemoveLastStep();
}

FText ADCPlayerController::DescribeDeleteStep() const
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Selected || !Plans || GetMode() != EDCPlanMode::Planning)
	{
		return FText::GetEmpty();
	}
	const int32 Count = Plans->GetTasks(Selected->GetFName()).Num();
	if (Count == 0)
	{
		return FText::GetEmpty();
	}
	// A picked step in the middle goes, otherwise the last one.
	const int32 Insert = GetInsertIndex();
	return Insert != INDEX_NONE && Insert < Count
		? FText::Format(LOCTEXT("DeletePicked", "Schritt {0} löschen"), FText::AsNumber(Insert))
		: LOCTEXT("DeleteLast", "Letzten Schritt löschen");
}

void ADCPlayerController::FollowNewSteps(int32 LastNewIndex)
{
	UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Selected || !Heist || Heist->GetPreviewTime() < 0.f)
	{
		return;
	}
	// The time cursor is in use: it moves on to the end of what was just planned.
	Heist->Refresh(false);
	if (const FDCCrewSchedule* Plan = Heist->GetSchedule().Find(Selected->GetFName()))
	{
		if (const FDCScheduledTask* Task = Plan->Tasks.FindByPredicate([LastNewIndex](const FDCScheduledTask& Entry) { return Entry.Index == LastNewIndex; }))
		{
			Heist->SetPreviewTime(Task->End);
		}
	}
}

void ADCPlayerController::ShowCrewMoment(ADCCrewCharacter* Crew)
{
	UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Crew || !Heist || GetMode() != EDCPlanMode::Planning)
	{
		return;
	}
	Heist->Refresh(false);
	const FDCCrewSchedule* Plan = Heist->GetSchedule().Find(Crew->GetFName());
	Heist->SetFocus(Crew->GetFName());
	// The time cursor stands where her plan ends so far - the moment her next step starts from.
	Heist->SetPreviewTime(Plan && !Plan->Tasks.IsEmpty() ? Plan->Tasks.Last().End : 0.f);
}

void ADCPlayerController::FocusStep(FName Crew, int32 Index)
{
	UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	ADCCrewCharacter* Member = Heist ? Heist->FindCrew(Crew) : nullptr;
	if (!Member)
	{
		return;
	}
	if (Member != Selected)
	{
		Select(Member);
	}
	Heist->SetFocus(Crew, Index);
}

void ADCPlayerController::RemoveStep(int32 Index)
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (Selected && Plans && GetMode() == EDCPlanMode::Planning && Plans->GetTasks(Selected->GetFName()).IsValidIndex(Index))
	{
		const FText Label = UDCHeistStateSubsystem::Get(this)->DescribeTask(Plans->GetTasks(Selected->GetFName())[Index]);
		const TArray<TPair<FName, FGuid>> Before = CollectProblems();
		int32 Followers = 0;
		{
			FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoRemove", "{0}: \u201e{1}\u201c gelöscht"), Selected->GetCrewName(), Label));
			Followers = Plans->RemoveTask(Selected->GetFName(), Index);
		}
		UDCHeistStateSubsystem::Get(this)->SetFocus(Selected->GetFName());
		ShowToast(Followers > 0
			? FText::Format(LOCTEXT("RemovedWithWaits", "{0}: \u201e{1}\u201c gelöscht – das Warten darauf auch"), Selected->GetCrewName(), Label)
			: FText::Format(LOCTEXT("Removed", "{0}: \u201e{1}\u201c gelöscht"), Selected->GetCrewName(), Label));
		CheckPlanAfterEdit(Before);
	}
}

void ADCPlayerController::MoveStep(int32 Index, int32 Delta)
{
	if (Selected && GetMode() == EDCPlanMode::Planning)
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		const int32 Count = Plans->GetTasks(Selected->GetFName()).Num();
		const TArray<TPair<FName, FGuid>> Before = CollectProblems();
		if (Plans->GetTasks(Selected->GetFName()).IsValidIndex(Index))
		{
			const FText Label = UDCHeistStateSubsystem::Get(this)->DescribeTask(Plans->GetTasks(Selected->GetFName())[Index]);
			FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoMove", "{0}: \u201e{1}\u201c verschoben"), Selected->GetCrewName(), Label));
			Plans->MoveTask(Selected->GetFName(), Index, Delta);
		}
		CheckPlanAfterEdit(Before);
		// The moved step stays picked.
		UDCHeistStateSubsystem::Get(this)->SetFocus(Selected->GetFName(), FMath::Clamp(Index + Delta, 0, Count - 1));
	}
}

void ADCPlayerController::PlanDrop(FName ItemId)
{
	if (Selected && GetMode() == EDCPlanMode::Planning)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Drop;
		Task.ItemPath = ItemId.ToString();
		IssueTask(Task);
	}
}

void ADCPlayerController::OpenWaitDialog()
{
	if (Selected && WaitDialog && GetMode() == EDCPlanMode::Planning)
	{
		if (RadialMenu)
		{
			RadialMenu->Close();
		}
		WaitDialog->Open(Selected);
	}
}

void ADCPlayerController::ApplyWaitChoice(const FDCWaitChoice& Choice)
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Selected || !Plans || GetMode() != EDCPlanMode::Planning)
	{
		return;
	}
	const FName Own = Selected->GetFName();
	const TArray<TPair<FName, FGuid>> Before = CollectProblems();
	FDCPlanTask Wait;
	{
	// The wait and a new signal for it are one change.
	FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoWait", "{0}: Warten angelegt"), Selected->GetCrewName()));
	switch (Choice.Kind)
	{
	case FDCWaitChoice::EKind::Seconds:
		Wait.Type = EDCPlanTaskType::Wait;
		Wait.Seconds = Choice.Seconds;
		break;
	case FDCWaitChoice::EKind::AllDone:
		Wait.Type = EDCPlanTaskType::WaitFor;
		Wait.Target = Choice.Crew;
		break;
	case FDCWaitChoice::EKind::AfterStep:
	{
		const TArray<FDCPlanTask>& Theirs = Plans->GetTasks(Choice.Crew);
		if (Choice.Crew == Own || !Theirs.IsValidIndex(Choice.Step))
		{
			return;
		}
		// The other one gives a signal right after that step; a signal already there is shared.
		FGuid Signal;
		if (Theirs.IsValidIndex(Choice.Step + 1) && Theirs[Choice.Step + 1].Type == EDCPlanTaskType::Signal)
		{
			Signal = Theirs[Choice.Step + 1].Id;
		}
		else
		{
			FDCPlanTask Give;
			Give.Type = EDCPlanTaskType::Signal;
			Give.Target = Own;
			Give.Action = static_cast<uint8>(FMath::Clamp(Plans->GetNextSignalNumber(), 1, 255));
			Signal = Plans->InsertTask(Choice.Crew, Choice.Step + 1, Give);
		}
		Wait.Type = EDCPlanTaskType::WaitFor;
		Wait.Target = Choice.Crew;
		Wait.Ref = Signal;
		break;
	}
	}
	const int32 At = GetInsertIndex();
	Plans->InsertTask(Own, At == INDEX_NONE ? TNumericLimits<int32>::Max() : At, Wait);
	if (At != INDEX_NONE)
	{
		AfterInsert(Own, At);
	}
	}
	ShowToast(FText::Format(LOCTEXT("WaitAdded", "{0}, Schritt {1}: {2}"), Selected->GetCrewName(), FText::AsNumber(Plans->GetTasks(Own).Num()),
		UDCHeistStateSubsystem::Get(this)->DescribeTask(Wait)));
	CheckPlanAfterEdit(Before, Own, Plans->GetTasks(Own).Num() - 1);
}

void ADCPlayerController::ShowToast(const FText& Text, bool bProblem)
{
	ToastText = Text;
	ToastTime = FPlatformTime::Seconds();
	bToastProblem = bProblem;
}

FText ADCPlayerController::GetToast(bool& bOutProblem) const
{
	bOutProblem = bToastProblem;
	return FPlatformTime::Seconds() - ToastTime < 3.5 ? ToastText : FText::GetEmpty();
}

void ADCPlayerController::SelectFromBar(ADCCrewCharacter* Crew)
{
	if (!Crew)
	{
		return;
	}
	const bool bOther = Crew != Selected;
	Select(Crew);
	if (bOther)
	{
		ShowCrewMoment(Crew);
	}
	if (ADCCameraPawn* Camera = Cast<ADCCameraPawn>(GetPawn()))
	{
		Camera->FocusOn(Crew->GetActorLocation());
	}
}

void ADCPlayerController::OpenProfile(ADCCrewCharacter* Crew)
{
	if (!Crew)
	{
		return;
	}
	Select(Crew);
	if (InventoryPanel)
	{
		InventoryPanel->Show();
	}
}

void ADCPlayerController::SetPreviewTime(float Time)
{
	if (UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this); Heist && GetMode() == EDCPlanMode::Planning)
	{
		Heist->SetPreviewTime(Time);
	}
}

void ADCPlayerController::ClearPlan()
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (Plans && GetMode() == EDCPlanMode::Planning)
	{
		{
			FDCPlanEditScope Edit(Plans, LOCTEXT("UndoClear", "Plan geleert"));
			Plans->ClearAll();
		}
		ShowToast(LOCTEXT("PlanCleared", "Plan geleert – \u201eRückgängig\u201c holt ihn zurück"));
	}
}

TArray<TPair<FName, FGuid>> ADCPlayerController::CollectProblems() const
{
	TArray<TPair<FName, FGuid>> Out;
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Heist || !Plans)
	{
		return Out;
	}
	for (const FDCPlanProblem& Problem : Heist->GetSchedule().FindProblems())
	{
		const TArray<FDCPlanTask>& Tasks = Plans->GetTasks(Problem.Crew);
		if (Tasks.IsValidIndex(Problem.Index))
		{
			Out.Add(TPair<FName, FGuid>(Problem.Crew, Tasks[Problem.Index].Id));
		}
	}
	return Out;
}

bool ADCPlayerController::CheckPlanAfterEdit(const TArray<TPair<FName, FGuid>>& Before, FName Crew, int32 NewIndex)
{
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Heist || !Plans || GetMode() != EDCPlanMode::Planning)
	{
		return false;
	}
	const TArray<FDCPlanProblem> Problems = Heist->GetSchedule().FindProblems();
	if (Problems.IsEmpty())
	{
		PlanHelp.Reset();
		if (!Before.IsEmpty())
		{
			ShowToast(LOCTEXT("PlanFineAgain", "Passt jetzt – Plan geht auf"));
			UE_LOG(LogDerClou, Display, TEXT("Plan works again"));
		}
		return false;
	}
	for (const FDCPlanProblem& Problem : Problems)
	{
		const TArray<FDCPlanTask>& Tasks = Plans->GetTasks(Problem.Crew);
		if (!Tasks.IsValidIndex(Problem.Index))
		{
			continue;
		}
		const TPair<FName, FGuid> Key(Problem.Crew, Tasks[Problem.Index].Id);
		if (Before.Contains(Key))
		{
			// Hung before already: a step added behind it simply has to wait its turn.
			if (Problem.Crew == Crew && NewIndex > Problem.Index && Tasks.IsValidIndex(NewIndex))
			{
				ShowToast(FText::Format(LOCTEXT("AfterProblem", "{0}, Schritt {1}: {2} – kommt erst dran, wenn Schritt {3} geht"), Problem.CrewName,
					FText::AsNumber(NewIndex + 1), Heist->DescribeTask(Tasks[NewIndex]), FText::AsNumber(Problem.Index + 1)), true);
			}
			continue;
		}
		// New: say it at once and keep it on screen until the player reacts.
		PlanHelp = FPlanHelp{ Problem.Crew, Key.Value, FText::Format(LOCTEXT("PlanHelpText", "Geht so nicht: {0}, Schritt {1} \u201e{2}\u201c – {3}"),
			Problem.CrewName, FText::AsNumber(Problem.Index + 1), Problem.Label, Problem.Reason) };
		UE_LOG(LogDerClou, Display, TEXT("Plan problem: %s"), *PlanHelp->Text.ToString());
		if (Plans->bHelpAsMenu && RadialMenu)
		{
			// Variant: the same help as a menu right where the player clicked.
			TWeakObjectPtr<ADCPlayerController> WeakThis = this;
			TArray<FDCRadialOption> Options;
			FDCRadialOption& Back = Options.AddDefaulted_GetRef();
			Back.Label = LOCTEXT("HelpMenuUndo", "Rückgängig");
			Back.Icon = TEXT("undo-2");
			Back.Detail = LOCTEXT("HelpMenuUndoDetail", "den Klick zurücknehmen");
			Back.OnSelected = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->DismissPlanHelp(); WeakThis->UndoPlan(); } };
			FDCRadialOption& Later = Options.AddDefaulted_GetRef();
			Later.Label = LOCTEXT("HelpMenuLater", "Später");
			Later.Icon = TEXT("x");
			Later.Detail = LOCTEXT("HelpMenuLaterDetail", "oben steht weiter, was hängt");
			Later.OnSelected = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->DismissPlanHelp(); } };
			RadialMenu->Open(FText::Format(LOCTEXT("HelpMenuTitle", "Geht so nicht\n{0}"), Problem.ShortReason), MoveTemp(Options));
		}
		return true;
	}
	return false;
}

FText ADCPlayerController::GetPlanHelp() const
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!PlanHelp.IsSet() || GetMode() != EDCPlanMode::Planning || (Plans && Plans->bHelpAsMenu))
	{
		return FText::GetEmpty();
	}
	// Gone as soon as that step works again (or is gone).
	return CollectProblems().Contains(TPair<FName, FGuid>(PlanHelp->Crew, PlanHelp->Step)) ? PlanHelp->Text : FText::GetEmpty();
}

void ADCPlayerController::ShowPlanProblem()
{
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	const TArray<FDCPlanProblem> Problems = Heist ? Heist->GetSchedule().FindProblems() : TArray<FDCPlanProblem>();
	if (Problems.IsEmpty() || !Plans || !Plans->GetTasks(Problems[0].Crew).IsValidIndex(Problems[0].Index))
	{
		return;
	}
	const FDCPlanProblem& First = Problems[0];
	FocusStep(First.Crew, First.Index);
	if (ADCCameraPawn* Camera = Cast<ADCCameraPawn>(GetPawn()))
	{
		Camera->FocusOn(First.Where);
	}
	PlanHelp = FPlanHelp{ First.Crew, Plans->GetTasks(First.Crew)[First.Index].Id, FText::Format(LOCTEXT("PlanHelpText", "Geht so nicht: {0}, Schritt {1} \u201e{2}\u201c – {3}"),
		First.CrewName, FText::AsNumber(First.Index + 1), First.Label, First.Reason) };
}

int32 ADCPlayerController::GetInsertIndex() const
{
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Heist || !Plans || !Selected || GetMode() != EDCPlanMode::Planning || Heist->GetFocusCrew() != Selected->GetFName())
	{
		return INDEX_NONE;
	}
	const TArray<FDCPlanTask>& Tasks = Plans->GetTasks(Selected->GetFName());
	if (Plans->bInsertOnlyWithButton)
	{
		// Only after "+ danach einfügen".
		const int32 After = InsertAfter.IsValid() ? Tasks.IndexOfByPredicate([this](const FDCPlanTask& Task) { return Task.Id == InsertAfter; }) : INDEX_NONE;
		return After == INDEX_NONE ? INDEX_NONE : After + 1;
	}
	const int32 Picked = Heist->GetFocusStep();
	return Tasks.IsValidIndex(Picked) ? Picked + 1 : INDEX_NONE;
}

void ADCPlayerController::AfterInsert(FName Crew, int32 NewIndex)
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	UDCHeistStateSubsystem::Get(this)->SetFocus(Crew, NewIndex);
	if (Plans && Plans->GetTasks(Crew).IsValidIndex(NewIndex))
	{
		InsertAfter = Plans->bInsertOnlyWithButton ? Plans->GetTasks(Crew)[NewIndex].Id : FGuid();
	}
}

void ADCPlayerController::InsertAtEnd()
{
	InsertAfter.Invalidate();
	if (Selected)
	{
		UDCHeistStateSubsystem::Get(this)->SetFocus(Selected->GetFName());
	}
}

void ADCPlayerController::InsertAfterStep(int32 Index)
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (Selected && Plans && Plans->GetTasks(Selected->GetFName()).IsValidIndex(Index))
	{
		InsertAfter = Plans->GetTasks(Selected->GetFName())[Index].Id;
		UDCHeistStateSubsystem::Get(this)->SetFocus(Selected->GetFName(), Index);
	}
}

void ADCPlayerController::SetVariant(int32 Which, bool bValue)
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Plans)
	{
		return;
	}
	switch (Which)
	{
	case 0: Plans->bGhostAsPin = bValue; break;
	case 1: Plans->bHelpAsMenu = bValue; break;
	default: Plans->bInsertOnlyWithButton = bValue; InsertAfter.Invalidate(); break;
	}
	Plans->SaveVariants();
	UDCHeistStateSubsystem::Get(this)->UpdateWorldLook();
	UE_LOG(LogDerClou, Display, TEXT("Variants: ghost %s, help %s, insert %s"), Plans->bGhostAsPin ? TEXT("pin") : TEXT("figure"),
		Plans->bHelpAsMenu ? TEXT("menu") : TEXT("bar"), Plans->bInsertOnlyWithButton ? TEXT("button") : TEXT("picked step"));
}

void ADCPlayerController::ToggleWaitOn(FName Giver, int32 StepIndex, FName Waiter)
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Plans || !Heist || GetMode() != EDCPlanMode::Planning || Giver == Waiter || !Plans->GetTasks(Giver).IsValidIndex(StepIndex))
	{
		return;
	}
	const FDCPlanTask Step = Plans->GetTasks(Giver)[StepIndex];
	const ADCCrewCharacter* GiverMember = Heist->FindCrew(Giver);
	const ADCCrewCharacter* WaiterMember = Heist->FindCrew(Waiter);
	const FText GiverName = GiverMember ? GiverMember->GetCrewName() : FText::FromName(Giver);
	const FText WaiterName = WaiterMember ? WaiterMember->GetCrewName() : FText::FromName(Waiter);
	const FText StepLabel = Heist->DescribeTask(Step);
	const TArray<TPair<FName, FGuid>> Before = CollectProblems();
	const int32 Existing = Plans->GetTasks(Waiter).IndexOfByPredicate([&Step](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::WaitFor && Task.Ref == Step.Id; });
	if (Existing != INDEX_NONE)
	{
		{
			FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoUnwait", "{0} wartet nicht mehr auf \u201e{1}\u201c"), WaiterName, StepLabel));
			Plans->RemoveTask(Waiter, Existing);
		}
		ShowToast(FText::Format(LOCTEXT("NoLongerWaits", "{0} wartet nicht mehr auf \u201e{1}\u201c"), WaiterName, StepLabel));
		CheckPlanAfterEdit(Before);
		return;
	}
	// Before the waiter's first step that would start before the giver's step is done.
	float Done = 0.f;
	if (const FDCCrewSchedule* Theirs = Heist->GetSchedule().Find(Giver))
	{
		if (const FDCScheduledTask* Scheduled = Theirs->Tasks.FindByPredicate([StepIndex](const FDCScheduledTask& Task) { return Task.Index == StepIndex; }))
		{
			Done = Scheduled->End;
		}
	}
	int32 At = Plans->GetTasks(Waiter).Num();
	if (const FDCCrewSchedule* Mine = Heist->GetSchedule().Find(Waiter))
	{
		for (const FDCScheduledTask& Task : Mine->Tasks)
		{
			if (Task.Start < Done - 0.05f && Task.Type != EDCPlanTaskType::WaitFor)
			{
				At = Task.Index;
				break;
			}
		}
	}
	FDCPlanTask Wait;
	Wait.Type = EDCPlanTaskType::WaitFor;
	Wait.Target = Giver;
	Wait.Ref = Step.Id;
	const TArray<FDCPlanTask>& WaiterTasks = Plans->GetTasks(Waiter);
	const FText Where = WaiterTasks.IsValidIndex(At)
		? FText::Format(LOCTEXT("WaitWhereBefore", "vor ihrem Schritt {0} \u201e{1}\u201c"), FText::AsNumber(At + 1), Heist->DescribeTask(WaiterTasks[At]))
		: LOCTEXT("WaitWhereEnd", "am Ende ihres Plans");
	{
		FDCPlanEditScope Edit(Plans, FText::Format(LOCTEXT("UndoWaitOn", "{0} wartet auf \u201e{1}\u201c"), WaiterName, StepLabel));
		Plans->InsertTask(Waiter, At, Wait);
	}
	ShowToast(FText::Format(LOCTEXT("WaitsNow", "{0} wartet jetzt {1} auf {2} (\u201e{3}\u201c)"), WaiterName, Where, GiverName, StepLabel));
	UE_LOG(LogDerClou, Display, TEXT("Plan %s: waits for %s step %d, as step %d"), *WaiterName.ToString(), *GiverName.ToString(), StepIndex + 1, At + 1);
	CheckPlanAfterEdit(Before, Waiter, At);
}

void ADCPlayerController::UndoPlan()
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Plans || GetMode() != EDCPlanMode::Planning)
	{
		return;
	}
	if (!Plans->CanUndo())
	{
		ShowToast(LOCTEXT("NothingToUndo", "Nichts zum Zurücknehmen"));
		return;
	}
	const FText What = Plans->GetUndoText();
	const TArray<TPair<FName, FGuid>> Before = CollectProblems();
	Plans->Undo();
	ON_SCOPE_EXIT
	{
		CheckPlanAfterEdit(Before);
	};
	UDCHeistStateSubsystem::Get(this)->SetFocus(Selected ? Selected->GetFName() : NAME_None);
	ShowToast(FText::Format(LOCTEXT("Undone", "Zurückgenommen: {0}"), What));
	UE_LOG(LogDerClou, Display, TEXT("Undo: %s"), *What.ToString());
}

void ADCPlayerController::RedoPlan()
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Plans || GetMode() != EDCPlanMode::Planning)
	{
		return;
	}
	if (!Plans->CanRedo())
	{
		ShowToast(LOCTEXT("NothingToRedo", "Nichts zum Wiederholen"));
		return;
	}
	const FText What = Plans->GetRedoText();
	const TArray<TPair<FName, FGuid>> Before = CollectProblems();
	Plans->Redo();
	ON_SCOPE_EXIT
	{
		CheckPlanAfterEdit(Before);
	};
	UDCHeistStateSubsystem::Get(this)->SetFocus(Selected ? Selected->GetFName() : NAME_None);
	ShowToast(FText::Format(LOCTEXT("Redone", "Wiederholt: {0}"), What));
	UE_LOG(LogDerClou, Display, TEXT("Redo: %s"), *What.ToString());
}

void ADCPlayerController::CycleSpeed()
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Plans)
	{
		return;
	}
	const float Speed = Plans->GetSpeed() >= 4.f ? 1.f : Plans->GetSpeed() * 2.f;
	Plans->SetSpeed(Speed);
	if (GetMode() == EDCPlanMode::Playing)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, Speed);
	}
}

void ADCPlayerController::OnContainerReached(ADCItemPickup* Pickup)
{
	// The search window belongs to direct control; a played plan just walks there.
	if (GetMode() != EDCPlanMode::Direct)
	{
		return;
	}
	if (ContainerPanel && Pickup)
	{
		ContainerPanel->Open(Selected, Pickup);
	}
	// Both windows side by side so things can be dragged across.
	if (InventoryPanel)
	{
		InventoryPanel->Show();
	}
	if (InfoPanel)
	{
		InfoPanel->Hide();
	}
}

ADCItemPickup* ADCPlayerController::FindPickup(FName ItemId) const
{
	for (TActorIterator<ADCItemPickup> It(GetWorld()); It; ++It)
	{
		if (It->GetItem() && It->GetItem()->GetDefId() == ItemId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADCPlayerController::StartInventorySelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Inventory started"));
	UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Direct);
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
			return Now >= End;
		};
	};
	const auto PickUpStep = [this](FName Id)
	{
		return [this, Id, bStarted = false]() mutable
		{
			if (!Selected)
			{
				return true;
			}
			if (!bStarted)
			{
				ADCItemPickup* Pickup = FindPickup(Id);
				UE_LOG(LogDerClou, Display, TEXT("Self-test: pick up %s%s"), *Id.ToString(), Pickup ? TEXT("") : TEXT(" (not in the level)"));
				if (!Pickup)
				{
					return true;
				}
				Selected->PickUp(Pickup);
				bStarted = true;
				return false;
			}
			if (Selected->IsBusy())
			{
				return false;
			}
			UE_LOG(LogDerClou, Display, TEXT("Self-test: %s -> '%s'"), *Id.ToString(), *Selected->GetLastNotice().ToString());
			return true;
		};
	};

	SelfTestSteps.Add(Wait(1.5f));
	for (const TCHAR* Id : { TEXT("backpack"), TEXT("tool_vest"), TEXT("necklace"), TEXT("crowbar"), TEXT("laptop"), TEXT("painting"), TEXT("statue"), TEXT("piano") })
	{
		SelfTestSteps.Add(PickUpStep(Id));
	}
	// A move that must be refused, with the reason shown in the window.
	SelfTestSteps.Add([this]()
	{
		UDCInventoryComponent* Inventory = Selected ? Selected->GetInventory() : nullptr;
		UDCItem* Pockets = Inventory ? Inventory->GetSlotItem(EDCEquipSlot::Body) : nullptr;
		UDCItem* Back = Inventory ? Inventory->GetSlotItem(EDCEquipSlot::Back) : nullptr;
		UDCItem* Laptop = nullptr;
		if (Back)
		{
			for (UDCItem* Item : Back->GetContents())
			{
				Laptop = Item->GetDefId() == TEXT("laptop") ? Item : Laptop;
			}
		}
		if (InventoryPanel)
		{
			InventoryPanel->Show();
			InventoryPanel->MoveItem(Laptop, EDCEquipSlot::None, Pockets);
		}
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add([this]()
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/inventory_selftest.png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), *File);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	// Put the painting down again: it must lie on the floor afterwards.
	SelfTestSteps.Add([this]()
	{
		if (UDCInventoryComponent* Inventory = Selected ? Selected->GetInventory() : nullptr)
		{
			Selected->DropItem(Inventory->GetSlotItem(EDCEquipSlot::RightHand));
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]()
	{
		const UDCInventoryComponent* Inventory = Selected ? Selected->GetInventory() : nullptr;
		bool bPassed = Inventory != nullptr;
		const auto Expect = [&bPassed](bool bCondition, const TCHAR* What)
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), What);
			bPassed &= bCondition;
		};
		if (Inventory)
		{
			const UDCItem* Back = Inventory->GetSlotItem(EDCEquipSlot::Back);
			Expect(Back && Back->GetDefId() == TEXT("backpack"), TEXT("backpack worn"));
			Expect(Inventory->GetSlotItem(EDCEquipSlot::Vest) != nullptr, TEXT("vest worn"));
			Expect(Back && Back->GetContents().Num() == 2, TEXT("crowbar and laptop in the backpack"));
			Expect(!FindPickup(TEXT("necklace")), TEXT("necklace taken"));
			Expect(FindPickup(TEXT("statue")) != nullptr, TEXT("statue refused (hands full)"));
			Expect(FindPickup(TEXT("piano")) != nullptr, TEXT("piano refused (too heavy)"));
			Expect(FindPickup(TEXT("painting")) != nullptr && !Inventory->HasBothHandsFull(), TEXT("painting back on the floor"));
			UE_LOG(LogDerClou, Display, TEXT("Self-test: load %.1f kg, speed %.0f %%"), Inventory->GetTotalMass(), Inventory->GetSpeedFactor() * 100.f);
		}
		FinishSelfTest(bPassed);
		return true;
	});
}

void ADCPlayerController::StartContainerSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Containers started"));
	UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Direct);
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
			return Now >= End;
		};
	};
	// Walk to a container and wait until its window is open.
	const auto SearchStep = [this, Expect](FName Id)
	{
		return [this, Id, Expect, bStarted = false, Started = 0.0]() mutable
		{
			if (!bStarted)
			{
				ADCItemPickup* Pickup = FindPickup(Id);
				Expect(Pickup != nullptr, FString::Printf(TEXT("%s is in the level"), *Id.ToString()));
				if (!Pickup || !Selected)
				{
					return true;
				}
				Selected->Search(Pickup);
				bStarted = true;
				Started = GetWorld()->GetTimeSeconds();
				return false;
			}
			const bool bOpen = ContainerPanel && ContainerPanel->IsOpen() && ContainerPanel->GetPickup() && ContainerPanel->GetPickup()->GetItem()->GetDefId() == Id;
			if (bOpen || GetWorld()->GetTimeSeconds() - Started > 15.0)
			{
				Expect(bOpen, FString::Printf(TEXT("search %s opens its window"), *Id.ToString()));
				return true;
			}
			return false;
		};
	};
	const auto Inside = [this](FName Container, FName Id) -> UDCItem*
	{
		ADCItemPickup* Pickup = FindPickup(Container);
		return Pickup ? (Pickup->GetItem()->GetDefId() == Id ? Pickup->GetItem() : Pickup->GetItem()->FindInside(Id)) : nullptr;
	};
	const auto Act = [this, Expect, Inside](FName Container, FName Id, EDCItemAction Action, bool bExpectRefusal)
	{
		return [this, Expect, Inside, Container, Id, Action, bExpectRefusal]()
		{
			UDCItem* Item = Inside(Container, Id);
			ADCItemPickup* Pickup = FindPickup(Container);
			const FText Reason = Item && Selected ? Selected->PerformItemAction(Item, Action, Pickup && Pickup->GetItem() == Item ? Pickup : nullptr) : FText::FromString(TEXT("missing"));
			UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s -> '%s'"), *DCItems::DescribeAction(Action).ToString(), *Id.ToString(), *Reason.ToString());
			Expect(Reason.IsEmpty() != bExpectRefusal, FString::Printf(TEXT("%s %s %s"), *DCItems::DescribeAction(Action).ToString(), *Id.ToString(), bExpectRefusal ? TEXT("refused") : TEXT("works")));
			return true;
		};
	};
	const auto WaitIdle = [this]()
	{
		return [this]() { return !Selected || !Selected->IsBusy(); };
	};
	const auto Owns = [this](FName Id)
	{
		const UDCInventoryComponent* Inventory = Selected ? Selected->GetInventory() : nullptr;
		return Inventory && Inventory->HasTool(Id);
	};

	SelfTestSteps.Add(Wait(1.5f));
	// Desk: locked drawer > jewellery box > ring.
	SelfTestSteps.Add(SearchStep(TEXT("desk")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]()
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/containers_desk.png")), true, false);
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("ring"), EDCItemAction::Take, true));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk_drawer"), EDCItemAction::Open, true));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk_drawer"), EDCItemAction::Force, true));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk_drawer"), EDCItemAction::PickLock, false));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk_drawer"), EDCItemAction::Open, false));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk_drawer"), EDCItemAction::Take, true));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("jewelry_box"), EDCItemAction::Open, false));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("ring"), EDCItemAction::Take, false));
	SelfTestSteps.Add([Expect, Owns]() { Expect(Owns(TEXT("ring")), TEXT("ring is in the inventory")); return true; });
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk"), EDCItemAction::Take, true));

	// Crowbar on the floor, then the sofa: take the hidden cash, hide the ring instead.
	SelfTestSteps.Add([this]()
	{
		if (ADCItemPickup* Pickup = FindPickup(TEXT("crowbar")))
		{
			Selected->PickUp(Pickup);
		}
		return true;
	});
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(SearchStep(TEXT("sofa")));
	SelfTestSteps.Add(Act(TEXT("sofa"), TEXT("cash"), EDCItemAction::Take, false));
	SelfTestSteps.Add([this, Expect]()
	{
		UDCInventoryComponent* Inventory = Selected->GetInventory();
		UDCItem* Ring = nullptr;
		for (UDCItem* Container : Inventory->GetContainers())
		{
			Ring = Ring ? Ring : Container->FindInside(TEXT("ring"));
		}
		ADCItemPickup* Sofa = FindPickup(TEXT("sofa"));
		FText Reason;
		const bool bStashed = Ring && Sofa && Inventory->MoveInto(Ring, Sofa->GetItem(), Reason);
		Expect(bStashed, FString::Printf(TEXT("hide the ring in the sofa (%s)"), *Reason.ToString()));
		return true;
	});

	// Safe: Vic lacks the skill and the stethoscope; Rosa, the safe expert, takes over.
	SelfTestSteps.Add(SearchStep(TEXT("safe")));
	SelfTestSteps.Add(Act(TEXT("safe"), TEXT("safe"), EDCItemAction::PickLock, true));
	SelfTestSteps.Add([this, Expect]()
	{
		ADCCrewCharacter* Rosa = nullptr;
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			Rosa = It->GetCrewName().ToString() == TEXT("Rosa") ? *It : Rosa;
		}
		Expect(Rosa != nullptr, TEXT("Rosa is in the level"));
		if (Rosa)
		{
			Select(Rosa);
			// Without lockpicks she cannot pick the display case at all ...
			ADCItemPickup* Case = FindPickup(TEXT("display_case"));
			const FDCTaskCheck NoTool = Case ? Rosa->CheckItemAction(Case->GetItem(), EDCItemAction::PickLock) : FDCTaskCheck();
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Rosa picks the display case -> %s"), *NoTool.Describe().ToString());
			Expect(!NoTool.bPossible, TEXT("Rosa has no lockpicks"));
			// ... and with them it would be one below her lock skill: slower, may fail.
			const FDCItemDef* CaseDef = DCItems::Find(TEXT("display_case"));
			const FDCTaskCheck Skill = DCSkills::Evaluate(EDCSkill::Locks, Rosa->GetSkills().Locks, CaseDef->LockDifficulty, CaseDef->LockPickSeconds, Rosa->GetCrewName());
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Rosa's lock skill on the display case -> %s"), *Skill.Describe().ToString());
			Expect(Skill.bPossible && Skill.FailChance > 0.f && Skill.Seconds > CaseDef->LockPickSeconds, TEXT("below the difficulty: possible, slower, may fail"));
			const FDCTaskCheck TooHard = DCSkills::Evaluate(EDCSkill::Safes, 1, 6, 20.f, FText::FromString(TEXT("Vic")));
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Vic's safe skill -> %s"), *TooHard.Describe().ToString());
			Expect(!TooHard.bPossible, TEXT("far below the difficulty: impossible"));
		}
		return true;
	});
	SelfTestSteps.Add(SearchStep(TEXT("safe")));
	SelfTestSteps.Add(Act(TEXT("safe"), TEXT("safe"), EDCItemAction::Force, true));
	SelfTestSteps.Add(Act(TEXT("safe"), TEXT("safe"), EDCItemAction::PickLock, false));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(Act(TEXT("safe"), TEXT("safe"), EDCItemAction::Open, false));
	SelfTestSteps.Add(Act(TEXT("safe"), TEXT("watch"), EDCItemAction::Take, false));
	SelfTestSteps.Add(Act(TEXT("safe"), TEXT("safe"), EDCItemAction::Take, true));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]()
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/containers_selftest.png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), *File);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));

	// Wardrobe: closed, coat inside with a wallet in its pocket. Then furniture that cannot be carried.
	SelfTestSteps.Add(SearchStep(TEXT("wardrobe")));
	SelfTestSteps.Add(Act(TEXT("wardrobe"), TEXT("wallet"), EDCItemAction::Take, true));
	SelfTestSteps.Add(Act(TEXT("wardrobe"), TEXT("wardrobe"), EDCItemAction::Open, false));
	SelfTestSteps.Add(Act(TEXT("wardrobe"), TEXT("wallet"), EDCItemAction::Take, false));
	SelfTestSteps.Add([this, Expect]()
	{
		const auto Refused = [this](FName Id)
		{
			ADCItemPickup* Pickup = FindPickup(Id);
			const FText Reason = Pickup ? Selected->GetInventory()->CheckAdd(Pickup->GetItem()) : FText::GetEmpty();
			UE_LOG(LogDerClou, Display, TEXT("Self-test: carry %s -> '%s'"), *Id.ToString(), *Reason.ToString());
			return Pickup && !Reason.IsEmpty();
		};
		Expect(Refused(TEXT("dining_table")), TEXT("dining table is too bulky"));
		Expect(Refused(TEXT("wardrobe")), TEXT("wardrobe is too heavy"));
		Expect(Refused(TEXT("piano")), TEXT("piano is too heavy"));
		return true;
	});
	SelfTestSteps.Add([this, bPassed]()
	{
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartNoiseSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Noise started"));
	UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Direct);
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const TSharedRef<float> Before = MakeShared<float>(0.f);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Neighbour = [this]() -> ADCNoiseListener*
	{
		TActorIterator<ADCNoiseListener> It(GetWorld());
		return It ? *It : nullptr;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
			return Now >= End;
		};
	};
	const auto WaitIdle = [this]() { return [this]() { return !Selected || !Selected->IsBusy(); }; };
	const auto WaitFor = [this](FName Id)
	{
		return [this, Id, bStarted = false]() mutable
		{
			if (!bStarted)
			{
				bStarted = true;
				if (ADCItemPickup* Pickup = FindPickup(Id))
				{
					Selected->Search(Pickup);
				}
				return false;
			}
			return ContainerPanel && ContainerPanel->IsOpen() && !Selected->IsBusy();
		};
	};
	const auto Act = [this](FName Container, FName Id, EDCItemAction Action)
	{
		return [this, Container, Id, Action]()
		{
			ADCItemPickup* Pickup = FindPickup(Container);
			UDCItem* Item = Pickup ? (Pickup->GetItem()->GetDefId() == Id ? Pickup->GetItem() : Pickup->GetItem()->FindInside(Id)) : nullptr;
			const FText Reason = Item ? Selected->PerformItemAction(Item, Action, Pickup->GetItem() == Item ? Pickup : nullptr) : FText::FromString(TEXT("missing"));
			UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s -> '%s'"), *DCItems::DescribeAction(Action).ToString(), *Id.ToString(), *Reason.ToString());
			return true;
		};
	};

	SelfTestSteps.Add(Wait(1.5f));
	SelfTestSteps.Add([Expect, Neighbour]() { Expect(Neighbour() != nullptr, TEXT("neighbour is in the level")); return true; });
	// Footsteps are quiet: the neighbour behind the wall does not hear them.
	SelfTestSteps.Add([this]()
	{
		Selected->MoveToLocation(FVector(550.0, 450.0, 0.0));
		return true;
	});
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add([this]()
	{
		if (ADCItemPickup* Pickup = FindPickup(TEXT("crowbar")))
		{
			Selected->PickUp(Pickup);
		}
		return true;
	});
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add([this, Expect, Neighbour]()
	{
		const UDCNoiseSubsystem* Noise = UDCNoiseSubsystem::Get(this);
		Expect(Noise && Noise->GetNoiseCount() > 0, FString::Printf(TEXT("footsteps make noise (%d rings)"), Noise ? Noise->GetNoiseCount() : 0));
		Expect(Neighbour() && Neighbour()->GetSuspicion() < 1.f, TEXT("neighbour did not hear the footsteps"));
		return true;
	});
	// Breaking the drawer open is loud.
	SelfTestSteps.Add(WaitFor(TEXT("desk")));
	SelfTestSteps.Add(Act(TEXT("desk"), TEXT("desk_drawer"), EDCItemAction::Force));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add([Expect, Neighbour, Before]()
	{
		*Before = Neighbour() ? Neighbour()->GetSuspicion() : 0.f;
		Expect(*Before > 5.f, FString::Printf(TEXT("neighbour heard the drawer (%.0f %%)"), *Before));
		return true;
	});
	// Smashing the display case is louder still.
	SelfTestSteps.Add(WaitFor(TEXT("display_case")));
	SelfTestSteps.Add(Act(TEXT("display_case"), TEXT("display_case"), EDCItemAction::Smash));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(Wait(0.35f));
	SelfTestSteps.Add([this]()
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/noise_selftest.png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), *File);
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, Expect, Neighbour, Before]()
	{
		ADCItemPickup* Case = FindPickup(TEXT("display_case"));
		Expect(Case && Case->GetItem()->IsDestroyed(), TEXT("display case is smashed"));
		const float After = Neighbour() ? Neighbour()->GetSuspicion() : 0.f;
		Expect(After > *Before, FString::Printf(TEXT("neighbour heard the glass (%.0f %%)"), After));
		return true;
	});
	SelfTestSteps.Add(Act(TEXT("display_case"), TEXT("brooch"), EDCItemAction::Take));
	SelfTestSteps.Add([this, Expect]()
	{
		Expect(Selected->GetInventory()->HasTool(TEXT("brooch")), TEXT("brooch taken from the smashed case"));
		return true;
	});
	// The jewellery box far away on the other side of the room: too far for the neighbour.
	SelfTestSteps.Add(WaitFor(TEXT("jewelry_box")));
	SelfTestSteps.Add(Act(TEXT("jewelry_box"), TEXT("jewelry_box"), EDCItemAction::Force));
	SelfTestSteps.Add(WaitIdle());
	// Two more bangs right at the wall: the neighbour calls the police.
	SelfTestSteps.Add([this, Neighbour]()
	{
		UDCNoiseSubsystem* Noise = UDCNoiseSubsystem::Get(this);
		const FVector AtWall = Neighbour() ? Neighbour()->GetActorLocation() + FVector(0.0, 260.0, 0.0) : FVector::ZeroVector;
		for (int32 Bang = 0; Bang < 2 && Noise; ++Bang)
		{
			Noise->Emit(AtWall, 1700.f, FText::FromString(TEXT("Test: lauter Schlag an der Wand")), nullptr);
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.4f));
	SelfTestSteps.Add([this, Expect, Neighbour]()
	{
		const UDCNoiseSubsystem* Noise = UDCNoiseSubsystem::Get(this);
		Expect(Noise && Noise->IsAlarm(), FString::Printf(TEXT("alarm raised (neighbour %.0f %%)"), Neighbour() ? Neighbour()->GetSuspicion() : -1.f));
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/noise_selftest_2.png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, bPassed]()
	{
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartTimelineSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Timeline started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
			return Now >= End;
		};
	};
	const auto Screenshot = [this](const TCHAR* Name)
	{
		return [this, Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name), true, false);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), Name);
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

	SelfTestSteps.Add(Wait(1.f));
	// Before planning: the menus show duration and chance for the selected crew member.
	SelfTestSteps.Add([this, CrewNamed]()
	{
		UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Planning);
		Select(CrewNamed(TEXT("Rosa")));
		// So the screenshots show the menu (the cursor of a test run is anywhere).
		RadialMenu->bOpenAtCentreForTests = true;
		OpenPickupMenu(FindPickup(TEXT("safe")));
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("radial_safe_rosa.png")));
	SelfTestSteps.Add(Wait(0.3f));
	SelfTestSteps.Add([this, CrewNamed]()
	{
		RadialMenu->Close();
		Select(CrewNamed(TEXT("Vic")));
		return true;
	});
	SelfTestSteps.Add(Wait(0.3f));
	SelfTestSteps.Add([this]()
	{
		OpenPickupMenu(FindPickup(TEXT("safe")));
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("radial_safe_vic.png")));
	SelfTestSteps.Add(Wait(0.3f));
	SelfTestSteps.Add([this]()
	{
		RadialMenu->Close();
		RadialMenu->bOpenAtCentreForTests = false;
		return true;
	});
	SelfTestSteps.Add(Wait(0.3f));
	// The plan.
	SelfTestSteps.Add([this, Expect, CrewNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		TActorIterator<ADCDoor> DoorIt(GetWorld());
		ADCItemPickup* Desk = FindPickup(TEXT("desk"));
		ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		Expect(Vic && Rosa && DoorIt && Desk && Safe, TEXT("Vic, Rosa, door, desk and safe are in the level"));
		if (!Vic || !Rosa || !DoorIt || !Desk || !Safe)
		{
			return true;
		}
		const FName Door = DoorIt->GetFName();
		const auto Step = [](EDCPlanTaskType Type, FName Target, uint8 Action, const FString& Path = FString())
		{
			FDCPlanTask Task;
			Task.Type = Type;
			Task.Target = Target;
			Task.Action = Action;
			Task.ItemPath = Path;
			return Task;
		};
		FDCPlanTask Back;
		Back.Type = EDCPlanTaskType::Move;
		Back.Location = FVector(350.0, 300.0, 0.0);
		FDCPlanTask Pause;
		Pause.Type = EDCPlanTaskType::Wait;
		Pause.Seconds = 2.f;

		// Vic: drawer, loot the desk, then the back door, then out of the way.
		Plans->AddTask(Vic->GetFName(), Step(EDCPlanTaskType::Item, Desk->GetFName(), static_cast<uint8>(EDCItemAction::PickLock), TEXT("desk_drawer")));
		Plans->AddTask(Vic->GetFName(), Step(EDCPlanTaskType::Loot, Desk->GetFName(), 0));
		Plans->AddTask(Vic->GetFName(), Step(EDCPlanTaskType::Door, Door, static_cast<uint8>(EDCDoorAction::PickLock)));
		Plans->AddTask(Vic->GetFName(), Back);
		// Rosa: through the back door at once (has to wait for Vic), back in, then the safe.
		Plans->AddTask(Rosa->GetFName(), Step(EDCPlanTaskType::Door, Door, static_cast<uint8>(EDCDoorAction::GoThrough)));
		Plans->AddTask(Rosa->GetFName(), Pause);
		Plans->AddTask(Rosa->GetFName(), Step(EDCPlanTaskType::Door, Door, static_cast<uint8>(EDCDoorAction::GoThrough)));
		Plans->AddTask(Rosa->GetFName(), Step(EDCPlanTaskType::Item, Safe->GetFName(), static_cast<uint8>(EDCItemAction::PickLock)));
		Plans->AddTask(Rosa->GetFName(), Step(EDCPlanTaskType::Loot, Safe->GetFName(), 0));

		// What the state manager expects.
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		const FDCSchedule& Schedule = Heist->GetSchedule();
		for (const FDCCrewSchedule& Crew : Schedule.Crews)
		{
			for (const FDCScheduledTask& Task : Crew.Tasks)
			{
				UE_LOG(LogDerClou, Display, TEXT("Plan %s %d. %s: %.1f walk %.1f wait %.1f work %.1f%s %s %s"), *Crew.Name.ToString(), Task.Index + 1, *Task.Label.ToString(),
					Task.Start, Task.Ready, Task.WorkStart, Task.End, Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.Detail.ToString(), *Task.WaitReason.ToString());
				Expect(!Task.bBlocked, FString::Printf(TEXT("%s step %d can happen"), *Crew.Name.ToString(), Task.Index + 1));
			}
		}
		const FDCCrewSchedule* VicPlan = Schedule.Find(Vic->GetFName());
		const FDCCrewSchedule* RosaPlan = Schedule.Find(Rosa->GetFName());
		if (VicPlan && RosaPlan && VicPlan->Tasks.Num() >= 3 && !RosaPlan->Tasks.IsEmpty())
		{
			const FDCScheduledTask& Through = RosaPlan->Tasks[0];
			const float Unlocked = VicPlan->Tasks[2].End;
			Expect(Through.WorkStart - Through.Ready > 1.f, FString::Printf(TEXT("Rosa waits at the door (%.1f s): %s"), Through.WorkStart - Through.Ready, *Through.WaitReason.ToString()));
			Expect(FMath::Abs(Through.WorkStart - Unlocked) < 0.6f, FString::Printf(TEXT("... until Vic has unlocked it (%.1f vs %.1f)"), Through.WorkStart, Unlocked));
		}
		Plans->SetSpeed(2.f);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Screenshot(TEXT("timeline_plan.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]() { StartPlayback(); return true; });
	// Rosa waits at the door for real: the state manager's live state, not the plan.
	SelfTestSteps.Add([this, Expect, CrewNamed, Started = -1.0]() mutable
	{
		const double Now = GetWorld()->GetTimeSeconds();
		Started = Started < 0.0 ? Now : Started;
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const bool bWaiting = Rosa && !Rosa->GetPlanRunner()->GetWaitText().IsEmpty();
		if (bWaiting || Now - Started > 40.0)
		{
			Expect(bWaiting, FString::Printf(TEXT("Rosa waits during playback: %s"), Rosa ? *Rosa->GetPlanRunner()->GetWaitText().ToString() : TEXT("-")));
			return true;
		}
		return false;
	});
	SelfTestSteps.Add(Wait(3.f));
	SelfTestSteps.Add(Screenshot(TEXT("timeline_play.png")));
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = GetWorld()->GetTimeSeconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 150.0;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("timeline_done.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, Expect, CrewNamed, bPassed]()
	{
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		TActorIterator<ADCDoor> Door(GetWorld());
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s)"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration);
		Expect(Heist->IsPlaybackFinished(), TEXT("everyone is through with the plan"));
		Expect(Door && !Door->IsLocked(), TEXT("back door unlocked"));
		Expect(Vic && Vic->GetInventory()->HasTool(TEXT("ring")), TEXT("Vic has the ring from the drawer"));
		Expect(Rosa && Rosa->GetInventory()->HasTool(TEXT("watch")) && Rosa->GetInventory()->HasTool(TEXT("necklace")), TEXT("Rosa has watch and necklace from the safe"));
		for (ADCCrewCharacter* Member : { Vic, Rosa })
		{
			for (int32 Index = 0; Member && Index < UDCPlanSubsystem::Get(this)->GetTasks(Member->GetFName()).Num(); ++Index)
			{
				Expect(Member->GetPlanRunner()->GetStatus(Index) == EDCTaskStatus::Done, FString::Printf(TEXT("%s step %d done"), *Member->GetCrewName().ToString(), Index + 1));
			}
		}
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartReloadSelfTest()
{
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	const int32 Stage = Plans->SelfTestStage;
	UE_LOG(LogDerClou, Display, TEXT("Self-test Reload stage %d"), Stage);
	const auto Expect = [this](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		UDCPlanSubsystem::Get(this)->bSelfTestPassed &= bCondition;
	};
	const auto DoorLocked = [this]()
	{
		TActorIterator<ADCDoor> Door(GetWorld());
		return Door && Door->IsLocked();
	};
	const auto WaitPlayed = [this, Started = -1.0]() mutable
	{
		const double Now = GetWorld()->GetTimeSeconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist->IsPlaying() && Heist->IsPlaybackFinished()) || Now - Started > 60.0;
	};
	const auto WaitSeconds = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			End = End < 0.0 ? Now + Seconds : End;
			return Now >= End;
		};
	};

	SelfTestSteps.Add(WaitSeconds(1.f));
	if (Stage == 0)
	{
		// A one-step plan: Vic picks the back door. Play it.
		SelfTestSteps.Add([this, Expect, DoorLocked]()
		{
			UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
			Plans->SetMode(EDCPlanMode::Planning);
			Plans->ClearAll();
			Plans->bSelfTestPassed = true;
			TActorIterator<ADCDoor> Door(GetWorld());
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Door;
			Task.Target = Door->GetFName();
			Task.Action = static_cast<uint8>(EDCDoorAction::PickLock);
			Plans->AddTask(Selected->GetFName(), Task);
			Expect(DoorLocked(), TEXT("door locked at the start"));
			StartPlayback();
			return true;
		});
		SelfTestSteps.Add(WaitPlayed);
		SelfTestSteps.Add([this, Expect, DoorLocked]()
		{
			Expect(!DoorLocked(), TEXT("door unlocked after playing"));
			UDCPlanSubsystem::Get(this)->SelfTestStage = 1;
			ReturnToPlanning();
			return true;
		});
	}
	else if (Stage == 1)
	{
		// After "back to planning": fresh level, plan kept. Then pretend the world was used and play.
		SelfTestSteps.Add([this, Expect, DoorLocked]()
		{
			UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
			Expect(Plans->GetMode() == EDCPlanMode::Planning, TEXT("back in planning mode"));
			Expect(DoorLocked(), TEXT("door locked again (level reset)"));
			Expect(Plans->GetTasks(Selected->GetFName()).Num() == 1, TEXT("the plan is still there"));
			UDCHeistStateSubsystem::Get(this)->MarkWorldUsed();
			Plans->SelfTestStage = 2;
			StartPlayback();
			return true;
		});
	}
	else
	{
		// "Abspielen" on a used world reloaded the level and started on its own.
		SelfTestSteps.Add([this, Expect]()
		{
			Expect(UDCHeistStateSubsystem::Get(this)->IsPlaying(), TEXT("playback started by itself after the reload"));
			return true;
		});
		SelfTestSteps.Add(WaitPlayed);
		SelfTestSteps.Add([this, Expect, DoorLocked]()
		{
			Expect(!DoorLocked(), TEXT("door unlocked again"));
			UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
			Plans->SelfTestStage = 0;
			FinishSelfTest(Plans->bSelfTestPassed);
			return true;
		});
	}
}

void ADCPlayerController::StartPlanInventorySelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test PlanInventory started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			End = End < 0.0 ? Now + Seconds : End;
			return Now >= End;
		};
	};
	const auto Screenshot = [this](const TCHAR* Name)
	{
		return [this, Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name), true, false);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), Name);
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
	const auto Step = [](EDCPlanTaskType Type, FName Target, uint8 Action, const FString& Path = FString())
	{
		FDCPlanTask Task;
		Task.Type = Type;
		Task.Target = Target;
		Task.Action = Action;
		Task.ItemPath = Path;
		return Task;
	};

	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add([this, Expect, CrewNamed, Step]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCItemPickup* Desk = FindPickup(TEXT("desk"));
		ADCItemPickup* Crowbar = FindPickup(TEXT("crowbar"));
		ADCItemPickup* Case = FindPickup(TEXT("display_case"));
		TActorIterator<ADCDoor> DoorIt(GetWorld());
		const FDCItemDef* Drawer = DCItems::Find(TEXT("desk_drawer"));
		Expect(Vic && Rosa && Desk && Crowbar && Case && DoorIt && Drawer, TEXT("Vic, Rosa, desk, crowbar, display case and door are there"));
		if (!Vic || !Rosa || !Desk || !Crowbar || !Case || !DoorIt || !Drawer)
		{
			return true;
		}
		Select(Vic);
		const FName V = Vic->GetFName();
		Heist->Refresh();
		Expect(!Vic->PlanCheckItem(*Drawer, EDCItemAction::Force, Heist->GetPlannedInventory(V)).bPossible, TEXT("without a crowbar Vic cannot plan to force the drawer"));

		// Pick up the crowbar, force the drawer with it, loot the desk.
		Plans->AddTask(V, Step(EDCPlanTaskType::PickUp, Crowbar->GetFName(), 0));
		Plans->AddTask(V, Step(EDCPlanTaskType::Item, Desk->GetFName(), static_cast<uint8>(EDCItemAction::Force), TEXT("desk_drawer")));
		Plans->AddTask(V, Step(EDCPlanTaskType::Loot, Desk->GetFName(), 0));
		Heist->Refresh();
		const UDCInventoryComponent* Planned = Heist->GetPlannedInventory(V);
		Expect(Planned && Planned->FindReadyTool(DCItems::CrowbarId), TEXT("after the plan Vic has the crowbar at hand"));
		Expect(Vic->PlanCheckItem(*Drawer, EDCItemAction::Force, Planned).bPossible, TEXT("so the planning menu now offers forcing"));
		Expect(Vic->GetInventory()->FindItem(DCItems::CrowbarId) == nullptr, TEXT("the real inventory is untouched by planning"));
		const FDCCrewSchedule* VicPlan = Heist->GetSchedule().Find(V);
		Expect(VicPlan && VicPlan->Tasks.Num() == 3, TEXT("three steps planned"));
		for (int32 Index = 0; VicPlan && Index < VicPlan->Tasks.Num(); ++Index)
		{
			const FDCScheduledTask& Task = VicPlan->Tasks[Index];
			UE_LOG(LogDerClou, Display, TEXT("Plan Vic %d. %s | %s | %s | %s"), Index + 1, *Task.Label.ToString(), *Task.Detail.ToString(), *Task.Carry.ToString(), *Task.WaitReason.ToString());
			Expect(!Task.bBlocked, FString::Printf(TEXT("Vic step %d can happen"), Index + 1));
		}
		Expect(VicPlan && VicPlan->Tasks.Num() == 3 && VicPlan->Tasks[0].Carry.ToString().Contains(TEXT("Hand")), TEXT("the pickup says where the crowbar goes (a hand)"));
		Expect(VicPlan && VicPlan->Tasks.Num() == 3 && VicPlan->Tasks[2].Detail.ToString().Contains(TEXT("unbekannt")), TEXT("searching the desk: contents unknown while planning"));

		// Forcing before picking up: not possible.
		Plans->MoveTask(V, 1, -1);
		Heist->Refresh();
		VicPlan = Heist->GetSchedule().Find(V);
		Expect(VicPlan && VicPlan->Tasks[0].bBlocked, FString::Printf(TEXT("moved before the pickup, forcing is blocked: %s"), VicPlan ? *VicPlan->Tasks[0].WaitReason.ToString() : TEXT("-")));
		Plans->MoveTask(V, 0, 1);
		Heist->Refresh();
		VicPlan = Heist->GetSchedule().Find(V);
		Expect(VicPlan && !VicPlan->Tasks[0].bBlocked && !VicPlan->Tasks[1].bBlocked, TEXT("moved back, everything works again"));

		// Put the crowbar down: then the glass cannot be smashed. Remove both steps again.
		FDCPlanTask Drop;
		Drop.Type = EDCPlanTaskType::Drop;
		Drop.ItemPath = DCItems::CrowbarId.ToString();
		Plans->AddTask(V, Drop);
		Plans->AddTask(V, Step(EDCPlanTaskType::Item, Case->GetFName(), static_cast<uint8>(EDCItemAction::Smash)));
		Heist->Refresh();
		VicPlan = Heist->GetSchedule().Find(V);
		Expect(VicPlan && VicPlan->Tasks.Num() == 5 && !VicPlan->Tasks[3].bBlocked && VicPlan->Tasks[4].bBlocked,
			FString::Printf(TEXT("after putting it down, smashing is blocked: %s"), VicPlan && VicPlan->Tasks.Num() == 5 ? *VicPlan->Tasks[4].WaitReason.ToString() : TEXT("-")));
		Plans->RemoveTask(V, 4);
		Plans->RemoveTask(V, 3);
		Heist->Refresh();
		Expect(Plans->GetTasks(V).Num() == 3, TEXT("both steps deleted again"));

		// Rosa waits at the back door until Vic has picked it: the plan knows for whom and which step.
		Plans->AddTask(V, Step(EDCPlanTaskType::Door, DoorIt->GetFName(), static_cast<uint8>(EDCDoorAction::PickLock)));
		Plans->AddTask(Rosa->GetFName(), Step(EDCPlanTaskType::Door, DoorIt->GetFName(), static_cast<uint8>(EDCDoorAction::GoThrough)));
		Heist->Refresh();
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		Expect(RosaPlan && !RosaPlan->Tasks.IsEmpty() && RosaPlan->Tasks[0].WaitFor == V && RosaPlan->Tasks[0].WaitForIndex == 3,
			FString::Printf(TEXT("Rosa waits for Vic's step 4 (%s, %d)"), RosaPlan && !RosaPlan->Tasks.IsEmpty() ? *RosaPlan->Tasks[0].WaitFor.ToString() : TEXT("-"),
				RosaPlan && !RosaPlan->Tasks.IsEmpty() ? RosaPlan->Tasks[0].WaitForIndex + 1 : 0));

		// The time preview: where everyone is while Rosa waits.
		if (RosaPlan && !RosaPlan->Tasks.IsEmpty())
		{
			const float Time = RosaPlan->Tasks[0].Ready + 1.f;
			FocusStep(V, 1);
			SetPreviewTime(Time);
			UE_LOG(LogDerClou, Display, TEXT("Preview %.1f: Vic %s | Rosa %s"), Time,
				*UDCHeistStateSubsystem::DescribeActivityAt(*Heist->GetSchedule().Find(V), Time).ToString(), *UDCHeistStateSubsystem::DescribeActivityAt(*RosaPlan, Time).ToString());
			const FVector RosaAt = UDCHeistStateSubsystem::GetPositionAt(*RosaPlan, Time);
			Expect(FVector::Dist2D(RosaAt, DoorIt->GetActorLocation()) < 200.0, FString::Printf(TEXT("the preview shows Rosa at the door (%.0f cm away)"), FVector::Dist2D(RosaAt, DoorIt->GetActorLocation())));
			Expect(UDCHeistStateSubsystem::DescribeActivityAt(*RosaPlan, Time).ToString().Contains(TEXT("Vic")), TEXT("... waiting for Vic"));
		}
		Plans->SetSpeed(2.f);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Screenshot(TEXT("plan_list_vic.png")));
	SelfTestSteps.Add(Wait(0.3f));
	SelfTestSteps.Add([this, CrewNamed]()
	{
		if (ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa")))
		{
			FocusStep(Rosa->GetFName(), 0);
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("plan_list_rosa.png")));
	SelfTestSteps.Add(Wait(0.3f));
	SelfTestSteps.Add([this]()
	{
		SetPreviewTime(-1.f);
		StartPlayback();
		return true;
	});
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = GetWorld()->GetTimeSeconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 120.0;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("plan_list_done.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, bPassed]()
	{
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const UDCItem* Drawer = UDCHeistStateSubsystem::ResolveItem(FindPickup(TEXT("desk")), TEXT("desk_drawer"));
		TActorIterator<ADCDoor> Door(GetWorld());
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s)"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration);
		Expect(Heist->IsPlaybackFinished(), TEXT("everyone is through with the plan"));
		Expect(Drawer && Drawer->IsLockBroken(), TEXT("the drawer was forced with the crowbar picked up in the plan"));
		Expect(Vic && Vic->GetInventory()->FindReadyTool(DCItems::CrowbarId), TEXT("Vic holds the crowbar"));
		Expect(Vic && Vic->GetInventory()->HasTool(TEXT("ring")), TEXT("Vic has the ring"));
		Expect(Door && !Door->IsLocked(), TEXT("back door unlocked"));
		for (ADCCrewCharacter* Member : { Vic, Rosa })
		{
			for (int32 Index = 0; Member && Index < UDCPlanSubsystem::Get(this)->GetTasks(Member->GetFName()).Num(); ++Index)
			{
				Expect(Member->GetPlanRunner()->GetStatus(Index) == EDCTaskStatus::Done, FString::Printf(TEXT("%s step %d done"), *Member->GetCrewName().ToString(), Index + 1));
			}
		}
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartSignalSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Signals started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			End = End < 0.0 ? Now + Seconds : End;
			return Now >= End;
		};
	};
	const auto Screenshot = [this](const TCHAR* Name)
	{
		return [this, Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name), true, false);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), Name);
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
	const auto LogPlan = [](const FDCCrewSchedule* Crew)
	{
		for (int32 Index = 0; Crew && Index < Crew->Tasks.Num(); ++Index)
		{
			const FDCScheduledTask& Task = Crew->Tasks[Index];
			UE_LOG(LogDerClou, Display, TEXT("Plan %s %d. %s: walk %.1f-%.1f wait -%.1f work -%.1f%s | %s | waits for %s %d"), *Crew->Name.ToString(), Index + 1, *Task.Label.ToString(),
				Task.Start, Task.Ready, Task.WorkStart, Task.End, Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.Detail.ToString(), *Task.WaitFor.ToString(), Task.WaitForIndex + 1);
		}
	};

	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add([this, Expect, CrewNamed, LogPlan]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		Expect(Vic && Rosa && Safe, TEXT("Vic, Rosa and the safe are there"));
		if (!Vic || !Rosa || !Safe)
		{
			return true;
		}
		const FName V = Vic->GetFName();
		const FName R = Rosa->GetFName();
		FDCPlanTask Crack;
		Crack.Type = EDCPlanTaskType::Item;
		Crack.Target = Safe->GetFName();
		Crack.Action = static_cast<uint8>(EDCItemAction::PickLock);
		FDCPlanTask Loot;
		Loot.Type = EDCPlanTaskType::Loot;
		Loot.Target = Safe->GetFName();

		// Without anything extra: Vic's looting waits by itself until Rosa has cracked the safe.
		Plans->AddTask(R, Crack);
		Plans->AddTask(V, Loot);
		Heist->Refresh();
		const FDCCrewSchedule* VicPlan = Heist->GetSchedule().Find(V);
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(R);
		LogPlan(RosaPlan);
		LogPlan(VicPlan);
		Expect(VicPlan && VicPlan->Tasks.Num() == 1 && !VicPlan->Tasks[0].bBlocked && VicPlan->Tasks[0].WaitFor == R,
			TEXT("Rosa cracks, Vic loots: Vic waits for Rosa by himself"));

		// Now with the dialog: "Worauf soll Vic warten?" – until Rosa has cracked the safe.
		Plans->RemoveTask(V, 0);
		Select(Vic);
		const auto ChooseInDialog = [this, Expect](FDCWaitChoice::EKind Kind, FName Crew, int32 Step, float Seconds)
		{
			OpenWaitDialog();
			Expect(WaitDialog && WaitDialog->IsOpen() && !WaitDialog->HasChoice(), TEXT("the wait dialog opens with nothing chosen"));
			FDCWaitChoice Choice;
			Choice.Kind = Kind;
			Choice.Crew = Crew;
			Choice.Step = Step;
			Choice.Seconds = Seconds;
			WaitDialog->Choose(Choice);
			UE_LOG(LogDerClou, Display, TEXT("Dialog says: %s"), *WaitDialog->DescribeChoice().ToString().Replace(TEXT("\n"), TEXT(" | ")));
			const bool bConfirmed = WaitDialog->Confirm();
			Expect(bConfirmed && !WaitDialog->IsOpen(), TEXT("... confirmed and closed"));
		};
		ChooseInDialog(FDCWaitChoice::EKind::AfterStep, R, 0, 0.f);
		IssueTask(Loot);
		Heist->Refresh();
		VicPlan = Heist->GetSchedule().Find(V);
		RosaPlan = Heist->GetSchedule().Find(R);
		LogPlan(RosaPlan);
		LogPlan(VicPlan);
		const bool bShapes = VicPlan && RosaPlan && VicPlan->Tasks.Num() == 2 && RosaPlan->Tasks.Num() == 2;
		Expect(bShapes && RosaPlan->Tasks[1].Type == EDCPlanTaskType::Signal && RosaPlan->Tasks[1].Label.ToString().Contains(TEXT("Vic")),
			FString::Printf(TEXT("Rosa got a signal for Vic right after cracking: %s"), bShapes ? *RosaPlan->Tasks[1].Label.ToString() : TEXT("-")));
		Expect(bShapes && VicPlan->Tasks[0].Label.ToString().Contains(TEXT("Zeichen von Rosa")) && !VicPlan->Tasks[0].bBlocked,
			FString::Printf(TEXT("Vic's first step waits for it: %s"), bShapes ? *VicPlan->Tasks[0].Label.ToString() : TEXT("-")));
		Expect(bShapes && FMath::Abs(VicPlan->Tasks[0].WorkStart - RosaPlan->Tasks[1].End) < 0.05f,
			FString::Printf(TEXT("Vic goes on when the signal is given (%.1f / %.1f)"), bShapes ? VicPlan->Tasks[0].WorkStart : 0.f, bShapes ? RosaPlan->Tasks[1].End : 0.f));
		Expect(bShapes && VicPlan->Tasks[0].WaitFor == R && VicPlan->Tasks[0].WaitForIndex == 1, TEXT("the plan knows Vic waits for Rosa's step 2"));
		Expect(bShapes && !VicPlan->Tasks[1].bBlocked && VicPlan->Tasks[1].Start >= VicPlan->Tasks[0].End - 0.01f, TEXT("then Vic loots the safe"));

		// The same step again: the signal is shared; removing one wait keeps it.
		ChooseInDialog(FDCWaitChoice::EKind::AfterStep, R, 0, 0.f);
		Expect(Plans->GetTasks(R).Num() == 2 && Plans->GetTasks(V).Num() == 3, TEXT("a second wait on the same step shares Rosa's signal"));
		Plans->RemoveTask(V, 2);
		Expect(Plans->GetTasks(R).Num() == 2, TEXT("... and removing it keeps the signal for the first wait"));

		// Until Rosa is done altogether; a fixed time.
		ChooseInDialog(FDCWaitChoice::EKind::AllDone, R, INDEX_NONE, 0.f);
		Heist->Refresh();
		VicPlan = Heist->GetSchedule().Find(V);
		RosaPlan = Heist->GetSchedule().Find(R);
		Expect(VicPlan && RosaPlan && VicPlan->Tasks.Num() == 3 && !VicPlan->Tasks[2].bBlocked && VicPlan->Tasks[2].WorkStart >= RosaPlan->End - 0.05f,
			FString::Printf(TEXT("\"until Rosa is done\": %s"), VicPlan && VicPlan->Tasks.Num() == 3 ? *VicPlan->Tasks[2].Label.ToString() : TEXT("-")));
		Plans->RemoveTask(V, 2);
		ChooseInDialog(FDCWaitChoice::EKind::Seconds, NAME_None, INDEX_NONE, 10.f);
		Expect(Plans->GetTasks(V).Num() == 3 && Plans->GetTasks(V)[2].Type == EDCPlanTaskType::Wait && FMath::IsNearlyEqual(Plans->GetTasks(V)[2].Seconds, 10.f), TEXT("a fixed 10 s wait"));
		Plans->RemoveTask(V, 2);

		// Wait and signal belong together: deleting the signal deletes the wait, and the other way round.
		Plans->RemoveTask(R, 1);
		Expect(Plans->GetTasks(R).Num() == 1 && Plans->GetTasks(V).Num() == 1, TEXT("deleting Rosa's signal also deletes Vic's wait for it"));
		ChooseInDialog(FDCWaitChoice::EKind::AfterStep, R, 0, 0.f);
		MoveStep(1, -1);
		Plans->RemoveTask(V, 0);
		Expect(Plans->GetTasks(R).Num() == 1, TEXT("deleting the only wait also deletes the signal"));
		ChooseInDialog(FDCWaitChoice::EKind::AfterStep, R, 0, 0.f);
		MoveStep(1, -1);
		Heist->Refresh();
		VicPlan = Heist->GetSchedule().Find(V);
		Expect(VicPlan && VicPlan->Tasks.Num() == 2 && VicPlan->Tasks[0].Type == EDCPlanTaskType::WaitFor && !VicPlan->Tasks[0].bBlocked && !VicPlan->Tasks[1].bBlocked,
			TEXT("wait, then loot – ready to play"));

		FocusStep(V, 0);
		Plans->SetSpeed(2.f);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Screenshot(TEXT("signal_vic.png")));
	SelfTestSteps.Add([this, CrewNamed]()
	{
		if (ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa")))
		{
			FocusStep(Rosa->GetFName(), 1);
		}
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("signal_rosa.png")));
	// The dialog with a choice, for the screenshot (not confirmed).
	SelfTestSteps.Add([this, CrewNamed]()
	{
		SelectFromBar(CrewNamed(TEXT("Vic")));
		OpenWaitDialog();
		FDCWaitChoice Choice;
		Choice.Kind = FDCWaitChoice::EKind::AfterStep;
		Choice.Crew = CrewNamed(TEXT("Rosa"))->GetFName();
		Choice.Step = 0;
		WaitDialog->Choose(Choice);
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Screenshot(TEXT("wait_dialog.png")));
	SelfTestSteps.Add([this, CrewNamed]()
	{
		WaitDialog->Close();
		OpenProfile(CrewNamed(TEXT("Rosa")));
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Screenshot(TEXT("crew_profile.png")));
	SelfTestSteps.Add([this]()
	{
		if (InventoryPanel)
		{
			InventoryPanel->Hide();
		}
		StartPlayback();
		return true;
	});
	// During playback Vic really waits for the signal.
	SelfTestSteps.Add([this, Expect, CrewNamed, Started = -1.0]() mutable
	{
		const double Now = GetWorld()->GetTimeSeconds();
		Started = Started < 0.0 ? Now : Started;
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		const bool bWaiting = Vic && Vic->GetPlanRunner()->GetWaitText().ToString().Contains(TEXT("Zeichen"));
		if (bWaiting || Now - Started > 30.0)
		{
			Expect(bWaiting, FString::Printf(TEXT("Vic waits for the signal during playback: %s"), Vic ? *Vic->GetPlanRunner()->GetWaitText().ToString() : TEXT("-")));
			return true;
		}
		return false;
	});
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = GetWorld()->GetTimeSeconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 120.0;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Screenshot(TEXT("signal_done.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, bPassed]()
	{
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s)"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration);
		Expect(Heist->IsPlaybackFinished(), TEXT("everyone is through with the plan"));
		Expect(Safe && Safe->GetItem() && !Safe->GetItem()->IsLocked(), TEXT("the safe is cracked"));
		Expect(Vic && Vic->GetInventory()->HasTool(TEXT("watch")) && Vic->GetInventory()->HasTool(TEXT("necklace")), TEXT("Vic took watch and necklace from the safe"));
		for (ADCCrewCharacter* Member : { Vic, Rosa })
		{
			for (int32 Index = 0; Member && Index < UDCPlanSubsystem::Get(this)->GetTasks(Member->GetFName()).Num(); ++Index)
			{
				Expect(Member->GetPlanRunner()->GetStatus(Index) == EDCTaskStatus::Done, FString::Printf(TEXT("%s step %d done"), *Member->GetCrewName().ToString(), Index + 1));
			}
		}
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartGridSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Grid started"));
	UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Direct);
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = GetWorld()->GetTimeSeconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
			return Now >= End;
		};
	};
	const auto WaitIdle = [this]()
	{
		return [this]() { return !Selected || !Selected->IsBusy(); };
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
	const auto Cell = [](const UDCItem* Item) { return Item ? FString::Printf(TEXT("(%d,%d)"), Item->GetGridPos().X, Item->GetGridPos().Y) : FString(TEXT("-")); };
	const auto PickUp = [this](FName Id)
	{
		return [this, Id]()
		{
			if (ADCItemPickup* Pickup = FindPickup(Id))
			{
				Selected->PickUp(Pickup);
			}
			return true;
		};
	};
	// Walk to a container and wait until its window is open.
	const auto SearchStep = [this, Expect](FName Id)
	{
		return [this, Id, Expect, bStarted = false, Started = 0.0]() mutable
		{
			ADCItemPickup* Pickup = FindPickup(Id);
			if (!bStarted)
			{
				Expect(Pickup != nullptr, FString::Printf(TEXT("%s is in the level"), *Id.ToString()));
				if (!Pickup)
				{
					return true;
				}
				Selected->Search(Pickup);
				bStarted = true;
				Started = GetWorld()->GetTimeSeconds();
				return false;
			}
			const bool bOpen = ContainerPanel && ContainerPanel->IsOpen() && ContainerPanel->GetPickup() == Pickup;
			if (bOpen || GetWorld()->GetTimeSeconds() - Started > 15.0)
			{
				Expect(bOpen, FString::Printf(TEXT("search %s opens its window"), *Id.ToString()));
				return true;
			}
			return false;
		};
	};

	SelfTestSteps.Add(Wait(1.5f));
	// Pure rules first: a crowbar (1 x 4) fits neither the pockets (3 x 2) nor the vest (4 x 2), but the backpack (5 x 4).
	SelfTestSteps.Add([Expect]()
	{
		UDCItem* Clothes = UDCItem::Create(TEXT("work_clothes"));
		UDCItem* Vest = UDCItem::Create(TEXT("tool_vest"));
		UDCItem* Backpack = UDCItem::Create(TEXT("backpack"));
		UDCItem* Crowbar = UDCItem::Create(TEXT("crowbar"));
		UDCItem* Laptop = UDCItem::Create(TEXT("laptop"));
		UDCItem* Painting = UDCItem::Create(TEXT("painting"));
		const FText InPockets = Clothes->CheckCanHold(Crowbar);
		const FText InVest = Vest->CheckCanHold(Crowbar);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: crowbar into pockets -> '%s', into vest -> '%s'"), *InPockets.ToString(), *InVest.ToString());
		Expect(!InPockets.IsEmpty(), TEXT("crowbar does not fit the pockets"));
		Expect(!InVest.IsEmpty(), TEXT("crowbar does not fit the vest"));
		// Also by its shape alone: 4 cells high does not go into 2 rows.
		const FText Shape = Vest->CheckPlaceAt(Crowbar, FIntPoint::ZeroValue);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: crowbar footprint in the vest -> '%s'"), *Shape.ToString());
		Expect(!Shape.IsEmpty(), TEXT("crowbar footprint 1 x 4 does not fit a 4 x 2 grid"));
		Expect(Backpack->CheckCanHold(Crowbar).IsEmpty(), TEXT("crowbar fits the backpack"));
		const FText PaintingReason = Backpack->CheckCanHold(Painting);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: painting into backpack -> '%s'"), *PaintingReason.ToString());
		Expect(!PaintingReason.IsEmpty(), TEXT("painting fits no grid"));

		// Footprints in the backpack: the crowbar stands in column 0, the laptop (3 x 2) next to it.
		Backpack->AddContent(Crowbar);
		Expect(Crowbar->GetGridPos() == FIntPoint(0, 0), TEXT("crowbar on the first free cell (0,0)"));
		const FText Taken = Backpack->CheckCanHold(Laptop, FIntPoint(0, 0));
		const FText Edge = Backpack->CheckCanHold(Laptop, FIntPoint(3, 0));
		UE_LOG(LogDerClou, Display, TEXT("Self-test: laptop at (0,0) -> '%s', at (3,0) -> '%s'"), *Taken.ToString(), *Edge.ToString());
		Expect(!Taken.IsEmpty(), TEXT("laptop cannot lie on the crowbar"));
		Expect(!Edge.IsEmpty(), TEXT("laptop cannot stick out over the edge"));
		Expect(Backpack->CheckCanHold(Laptop, FIntPoint(1, 0)).IsEmpty(), TEXT("laptop fits next to the crowbar"));
		FIntPoint Free;
		Expect(Backpack->FindFreeSpot(Laptop, Free) && Free == FIntPoint(1, 0), TEXT("first free spot for the laptop is (1,0)"));
		Backpack->AddContent(Laptop, FIntPoint(2, 2));
		Expect(Laptop->GetGridPos() == FIntPoint(2, 2), TEXT("laptop placed at (2,2)"));
		Expect(!Backpack->CheckCanHold(Crowbar, FIntPoint(4, 0)).IsEmpty(), TEXT("crowbar cannot move onto the laptop"));
		Expect(Backpack->CheckCanHold(Crowbar, FIntPoint(1, 0)).IsEmpty(), TEXT("crowbar may move within the backpack"));
		Expect(Backpack->GetUsedCells() == 10, TEXT("10 of 20 cells used"));

		// Copies keep the cells (the planning simulation relies on it).
		const UDCItem* Copy = Backpack->Clone();
		bool bSameCells = Copy->GetContents().Num() == 2;
		for (int32 Index = 0; bSameCells && Index < 2; ++Index)
		{
			bSameCells = Copy->GetContents()[Index]->GetGridPos() == Backpack->GetContents()[Index]->GetGridPos();
		}
		Expect(bSameCells, TEXT("a copy keeps every item on its cell"));
		return true;
	});

	// Now with Vic: put on the backpack, pick up crowbar and laptop – both land in the backpack, not in the pockets.
	SelfTestSteps.Add(PickUp(TEXT("backpack")));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(PickUp(DCItems::CrowbarId));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(PickUp(TEXT("laptop")));
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add([this, Expect, Cell]()
	{
		UDCInventoryComponent* Inventory = Selected->GetInventory();
		UDCItem* Back = Inventory->GetSlotItem(EDCEquipSlot::Back);
		UDCItem* Crowbar = Inventory->FindItem(DCItems::CrowbarId);
		UDCItem* Laptop = Inventory->FindItem(TEXT("laptop"));
		Expect(Back && Crowbar && Crowbar->GetParent() == Back, FString::Printf(TEXT("crowbar in the backpack at %s"), *Cell(Crowbar)));
		Expect(Back && Laptop && Laptop->GetParent() == Back, FString::Printf(TEXT("laptop in the backpack at %s"), *Cell(Laptop)));
		if (const UDCItem* Pockets = Inventory->GetSlotItem(EDCEquipSlot::Body))
		{
			for (const UDCItem* Item : Pockets->GetContents())
			{
				UE_LOG(LogDerClou, Display, TEXT("Self-test: pockets hold %s at %s"), *Item->GetDisplayName().ToString(), *Cell(Item));
			}
		}
		// Moving to a cell: the panel does what a drop does.
		if (InventoryPanel && Laptop && Back)
		{
			InventoryPanel->MoveItem(Laptop, EDCEquipSlot::None, Back, FIntPoint(2, 2));
			Expect(Laptop->GetGridPos() == FIntPoint(2, 2), TEXT("drop on a cell moves the laptop to (2,2)"));
			FText Reason;
			const bool bMoved = Inventory->MoveInto(Crowbar, Back, Reason, FIntPoint(4, 0));
			Expect(!bMoved, FString::Printf(TEXT("crowbar onto the laptop refused ('%s')"), *Reason.ToString()));
			InventoryPanel->SelectItem(Laptop);
		}
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("grid_profile.png")));
	SelfTestSteps.Add(Wait(0.5f));

	// Wardrobe: its grid with the coat; the wallet selected shows the coat pocket and the wallet itself.
	SelfTestSteps.Add([this]()
	{
		if (InventoryPanel)
		{
			InventoryPanel->SelectItem(nullptr);
		}
		return true;
	});
	SelfTestSteps.Add(SearchStep(TEXT("wardrobe")));
	SelfTestSteps.Add([this, Expect, Cell]()
	{
		ADCItemPickup* Wardrobe = FindPickup(TEXT("wardrobe"));
		UDCItem* Item = Wardrobe ? Wardrobe->GetItem() : nullptr;
		if (Item && ContainerPanel)
		{
			ContainerPanel->DoAction(Item, EDCItemAction::Open);
			UDCItem* Coat = Item->FindInside(TEXT("coat"));
			Expect(Coat && Coat->HasGridPos(), FString::Printf(TEXT("coat lies on the wardrobe grid at %s"), *Cell(Coat)));
			if (UDCItem* Wallet = Coat ? Coat->FindInside(TEXT("wallet")) : nullptr)
			{
				Expect(Wallet->HasGridPos(), FString::Printf(TEXT("wallet in the coat pocket at %s"), *Cell(Wallet)));
				ContainerPanel->SelectItem(Wallet);
			}
		}
		return true;
	});
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("grid_wardrobe.png")));
	SelfTestSteps.Add(Wait(0.5f));

	// Desk: the drawer with its own grid, the jewellery box inside.
	SelfTestSteps.Add(SearchStep(TEXT("desk")));
	SelfTestSteps.Add([this]()
	{
		ADCItemPickup* Desk = FindPickup(TEXT("desk"));
		UDCItem* Drawer = Desk ? Desk->GetItem()->FindInside(TEXT("desk_drawer")) : nullptr;
		if (Drawer && ContainerPanel)
		{
			ContainerPanel->DoAction(Drawer, EDCItemAction::PickLock);
		}
		return true;
	});
	SelfTestSteps.Add(WaitIdle());
	SelfTestSteps.Add([this, Expect, Cell]()
	{
		ADCItemPickup* Desk = FindPickup(TEXT("desk"));
		UDCItem* Drawer = Desk ? Desk->GetItem()->FindInside(TEXT("desk_drawer")) : nullptr;
		if (Drawer && ContainerPanel)
		{
			ContainerPanel->DoAction(Drawer, EDCItemAction::Open);
			UDCItem* Box = Drawer->FindInside(TEXT("jewelry_box"));
			Expect(Box && Box->HasGridPos(), FString::Printf(TEXT("jewellery box on the drawer grid at %s"), *Cell(Box)));
			if (Box)
			{
				ContainerPanel->SelectItem(Box);
			}
		}
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("grid_desk.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, bPassed]()
	{
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartFindSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Find started"));
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			// Real time: the game stands still while a find waits.
			const double Now = FPlatformTime::Seconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
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
	const TSharedRef<double> PlannedSearch = MakeShared<double>(0.0);

	SelfTestSteps.Add(Wait(1.f));
	// The plan: Rosa cracks the safe and searches it (one menu entry). Nothing about the contents is known yet.
	SelfTestSteps.Add([this, Expect, CrewNamed, PlannedSearch]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->bAutoDecideFinds = false;
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		Expect(Rosa && Safe, TEXT("Rosa and the safe are in the level"));
		if (!Rosa || !Safe)
		{
			return true;
		}
		// One entry in the menu adds both steps: pick the lock, then search.
		Select(Rosa);
		OpenPickupMenu(Safe);
		const bool bChosen = RadialMenu && RadialMenu->ChooseOption(TEXT("knacken und durchsuchen"));
		Expect(bChosen, TEXT("the safe menu offers \u201eknacken und durchsuchen\u201c"));
		const TArray<FDCPlanTask>& Steps = Plans->GetTasks(Rosa->GetFName());
		Expect(Steps.Num() == 2 && Steps[0].Type == EDCPlanTaskType::Item && Steps[0].Action == static_cast<uint8>(EDCItemAction::PickLock)
			&& Steps[1].Type == EDCPlanTaskType::Loot, TEXT("... which plans two steps: pick the lock, then search"));
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		const FDCCrewSchedule* Plan = Heist->GetSchedule().Find(Rosa->GetFName());
		const bool bTwo = Plan && Plan->Tasks.Num() == 2;
		if (bTwo)
		{
			const FDCScheduledTask& Step = Plan->Tasks[1];
			*PlannedSearch = Step.End - Step.WorkStart;
			UE_LOG(LogDerClou, Display, TEXT("Self-test: planned search '%s' – %s / %s (%.1f s)"), *Step.Label.ToString(), *Step.Detail.ToString(), *Step.Carry.ToString(), *PlannedSearch);
		}
		Expect(bTwo && Plan->Tasks[1].Detail.ToString().Contains(TEXT("unbekannt")), TEXT("the plan does not know what is inside"));
		Expect(bTwo && !Plan->Tasks[1].bBlocked, TEXT("searching the safe can happen"));
		const UDCInventoryComponent* PlannedInventory = Heist->GetPlannedInventory(Rosa->GetFName());
		Expect(PlannedInventory && !PlannedInventory->HasTool(TEXT("watch")), TEXT("nothing from the safe in the planned inventory"));
		Select(Rosa);
		Plans->SetSpeed(2.f);
		StartPlayback();
		return true;
	});
	// The heist: wait until Rosa has found something and the game stands still.
	SelfTestSteps.Add([this, Expect, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		const bool bFind = ContainerPanel && ContainerPanel->IsFindMode();
		if (bFind || Now - Started > 90.0)
		{
			Expect(bFind, TEXT("searching during the heist opens the find window"));
			Expect(IsPlaybackPaused(), TEXT("the game is paused while the player decides"));
			Expect(bFind && ContainerPanel->DescribeFound().ToString().Contains(TEXT("Armbanduhr")), FString::Printf(TEXT("found: %s"), bFind ? *ContainerPanel->DescribeFound().ToString() : TEXT("-")));
			return true;
		}
		return false;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("find_pause.png")));
	SelfTestSteps.Add(Wait(0.5f));
	// The player's decision: take the necklace, leave the watch, leave the torch behind in the safe.
	SelfTestSteps.Add([this, Expect, CrewNamed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		UDCItem* Root = Safe ? Safe->GetItem() : nullptr;
		UDCItem* Necklace = Root ? Root->FindInside(TEXT("necklace")) : nullptr;
		UDCItem* Torch = Rosa ? Rosa->GetInventory()->FindItem(TEXT("flashlight")) : nullptr;
		if (!Rosa || !Root || !ContainerPanel)
		{
			Expect(false, TEXT("Rosa, safe and window are there"));
			return true;
		}
		ContainerPanel->DoAction(Necklace, EDCItemAction::Take);
		Expect(Necklace && Rosa->GetInventory()->Owns(Necklace), TEXT("necklace taken while paused"));
		// The torch from her pockets into the safe, as a drop on the safe's grid does.
		if (InventoryPanel && Torch)
		{
			InventoryPanel->MoveItem(Torch, EDCEquipSlot::None, Root);
		}
		Expect(Torch && Torch->GetParent() == Root, TEXT("torch left in the safe"));
		const float Handling = ContainerPanel->GetHandlingSeconds();
		Expect(FMath::IsNearlyEqual(Handling, DCItems::FindTakeSeconds + DCItems::FindRepackSeconds), FString::Printf(TEXT("two handlings cost %.1f s"), Handling));
		Expect(IsPlaybackPaused(), TEXT("still paused while deciding"));
		return true;
	});
	SelfTestSteps.Add(Wait(0.6f));
	SelfTestSteps.Add(Shot(TEXT("find_decided.png")));
	SelfTestSteps.Add(Wait(0.4f));
	SelfTestSteps.Add([this, Expect]()
	{
		const FText Result = ContainerPanel->DescribeFindResult();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: result '%s'"), *Result.ToString());
		const FString Text = Result.ToString();
		Expect(Text.Contains(TEXT("mitgenommen: Goldkette")) && Text.Contains(TEXT("Armbanduhr")) && Text.Contains(TEXT("dagelassen: Taschenlampe")), TEXT("the result names what was taken, left and left behind"));
		ContainerPanel->ContinueFind();
		Expect(!IsPlaybackPaused() && !ContainerPanel->IsOpen(), TEXT("Weiter: window closed, the heist goes on"));
		return true;
	});
	// Rosa still stands there for the handling time, then the plan is done.
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 30.0;
	});
	SelfTestSteps.Add([this, Expect, CrewNamed, bPassed]()
	{
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s)"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration);
		Expect(Heist->IsPlaybackFinished(), TEXT("Rosa is through with the plan"));
		Expect(Heist->GetPlaybackTime() >= Heist->GetSchedule().Duration + 1.f, TEXT("the handling made the heist longer than planned"));
		Expect(Rosa && Rosa->GetInventory()->HasTool(TEXT("necklace")) && !Rosa->GetInventory()->HasTool(TEXT("watch")), TEXT("Rosa has the necklace, not the watch"));
		Expect(Rosa && Rosa->GetPlanRunner()->GetStatus(1) == EDCTaskStatus::Done, TEXT("the search step is done"));
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartPlanViewSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test PlanView started"));
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
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
	const auto Choose = [this, Expect](const TCHAR* Pickup, const TCHAR* Label)
	{
		return [this, Expect, Pickup, Label]()
		{
			OpenPickupMenu(FindPickup(Pickup));
			const bool bChosen = RadialMenu && RadialMenu->ChooseOption(Label);
			Expect(bChosen, FString::Printf(TEXT("menu of %s has '%s'"), Pickup, Label));
			return true;
		};
	};

	SelfTestSteps.Add(Wait(1.f));
	// The user's case: Vic searches the sofa, then picks up the statue.
	SelfTestSteps.Add([this]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		if (InventoryPanel)
		{
			InventoryPanel->SetPlanView(true);
		}
		return true;
	});
	SelfTestSteps.Add(Choose(TEXT("sofa"), TEXT("Durchsuchen")));
	SelfTestSteps.Add(Choose(TEXT("statue"), TEXT("Aufheben")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, Expect]()
	{
		const TArray<FDCPlanTask>& Steps = UDCPlanSubsystem::Get(this)->GetTasks(Selected->GetFName());
		Expect(Steps.Num() == 2 && Steps[1].Type == EDCPlanTaskType::PickUp, TEXT("plan: search the sofa, pick up the statue"));
		const UDCInventoryComponent* Planned = UDCHeistStateSubsystem::Get(this)->GetPlannedInventory(Selected->GetFName());
		const UDCItem* Held = Planned ? Planned->GetSlotItem(EDCEquipSlot::RightHand) : nullptr;
		Expect(Held && Held->GetDefId() == TEXT("statue") && Planned->HasBothHandsFull(), TEXT("after the plan Vic carries the statue in both hands"));
		Expect(InventoryPanel && InventoryPanel->IsPlanView(), TEXT("the profile shows the state after the plan"));
		Expect(!Selected->GetInventory()->HasTool(TEXT("statue")), TEXT("... while Vic himself does not have it yet"));
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Shot(TEXT("plan_view.png")));
	SelfTestSteps.Add(Wait(0.3f));
	// Picking it up again: the message says it is already in the plan, and where.
	SelfTestSteps.Add(Choose(TEXT("statue"), TEXT("Aufheben")));
	SelfTestSteps.Add([this, Expect]()
	{
		bool bProblem = false;
		const FString Toast = GetToast(bProblem).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: second pick-up -> '%s'"), *Toast);
		Expect(bProblem && Toast.Contains(TEXT("Vic")) && Toast.Contains(TEXT("Schritt 2")), TEXT("second pick-up: already in the plan, says who and which step"));
		Expect(UDCPlanSubsystem::Get(this)->GetTasks(Selected->GetFName()).Num() == 2, TEXT("... and adds nothing"));
		// "Jetzt" shows the real inventory again.
		InventoryPanel->SetPlanView(false);
		Expect(!InventoryPanel->IsPlanView(), TEXT("switching to \u201eJetzt\u201c shows the real inventory"));
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Shot(TEXT("plan_view_now.png")));
	SelfTestSteps.Add(Wait(0.3f));
	SelfTestSteps.Add([this, bPassed]()
	{
		InventoryPanel->SetPlanView(true);
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartDoorWaySelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test DoorWay started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
			return Now >= End;
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
	const TSharedRef<FVector> Outside = MakeShared<FVector>(FVector::ZeroVector);

	SelfTestSteps.Add(Wait(1.f));
	// The user's case: Vic picks and opens the back door; Rosa walks to a spot outside meanwhile.
	SelfTestSteps.Add([this, Expect, CrewNamed, Outside]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		TActorIterator<ADCDoor> DoorIt(GetWorld());
		ADCDoor* Door = DoorIt ? *DoorIt : nullptr;
		Expect(Vic && Rosa && Door, TEXT("Vic, Rosa and the back door are in the level"));
		if (!Vic || !Rosa || !Door)
		{
			return true;
		}
		const FVector Behind = Door->GetStandPoint(Rosa->GetActorLocation(), true);
		*Outside = Behind + (Behind - Door->GetCenter()).GetSafeNormal2D() * 120.0;
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = Door->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = *Outside;
		// The user's case: Vic only PICKS the lock; Rosa should open the door herself on her way out.
		Plans->AddTask(Vic->GetFName(), Pick);
		Plans->AddTask(Rosa->GetFName(), Go);

		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		const FDCCrewSchedule* VicPlan = Heist->GetSchedule().Find(Vic->GetFName());
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		if (!VicPlan || !RosaPlan || VicPlan->Tasks.Num() != 1 || RosaPlan->Tasks.Num() != 1)
		{
			Expect(false, TEXT("both plans are scheduled"));
			return true;
		}
		const FDCScheduledTask& Walk = RosaPlan->Tasks[0];
		const float Unlocked = VicPlan->Tasks[0].End;
		UE_LOG(LogDerClou, Display, TEXT("Self-test: Rosa start %.1f, walks from %.1f, arrives %.1f (door unlocked at %.1f): %s / waits for %s / %s"),
			Walk.Start, Walk.WalkStart, Walk.Ready, Unlocked, *Walk.WaitReason.ToString(), *Walk.WaitForName.ToString(), *Walk.Detail.ToString());
		Expect(!Walk.bBlocked, TEXT("Rosa's walk outside can happen without an extra \u201e\u00f6ffnen\u201c step"));
		Expect(Walk.Legs.Num() == 2 && Walk.Legs[1].Start >= Unlocked - 0.6f, TEXT("... she walks up to the door, waits beside it and goes on once Vic has picked the lock"));
		Expect(Walk.WaitFor == Vic->GetFName(), TEXT("... and the plan says she waits for Vic"));
		Expect(Walk.Detail.ToString().Contains(TEXT("\u00f6ffnet unterwegs")), TEXT("... and opens the door herself on the way"));
		const FString WaitLine = UDCHeistStateSubsystem::DescribeWaited(Walk, Walk.WorkStart - Walk.Ready + Walk.WaitedOnTheWay()).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: wait line: %s"), *WaitLine);
		Expect(WaitLine.Contains(TEXT("auf Vic")) && WaitLine.Contains(TEXT("knacken")), TEXT("... and her wait line says whom and what she waits for"));
		float Closest = TNumericLimits<float>::Max();
		for (const FVector& Point : Walk.Path)
		{
			Closest = FMath::Min(Closest, static_cast<float>(FVector::Dist2D(Point, Door->GetCenter())));
		}
		bool bThroughDoor = false;
		for (int32 Point = 1; Point < Walk.Path.Num(); ++Point)
		{
			bThroughDoor |= Door->CrossesOpening(Walk.Path[Point - 1], Walk.Path[Point]);
		}
		Expect(bThroughDoor, FString::Printf(TEXT("the drawn way leads through the door (closest point %.0f cm, %d points)"), Closest, Walk.Path.Num()));

		// Without Vic picking it, the walk cannot happen – and the plan says why, at once.
		Select(Vic);
		RemoveStep(0);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: help after deleting the pick: %s"), *GetPlanHelp().ToString());
		Expect(GetPlanHelp().ToString().Contains(TEXT("Rosa")) && GetPlanHelp().ToString().Contains(TEXT("verschlossen")), TEXT("deleting Vic's pick opens the help at once: Rosa's walk does not happen"));
		Heist->Refresh();
		RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		const bool bBlocked = RosaPlan && RosaPlan->Tasks.Num() == 1 && RosaPlan->Tasks[0].bBlocked;
		UE_LOG(LogDerClou, Display, TEXT("Self-test: without picking: %s"), RosaPlan && RosaPlan->Tasks.Num() ? *RosaPlan->Tasks[0].WaitReason.ToString() : TEXT("-"));
		Expect(bBlocked && RosaPlan->Tasks[0].WaitReason.ToString().Contains(TEXT("bleibt verschlossen")) && RosaPlan->Tasks[0].WaitReason.ToString().Contains(TEXT("Vic k\u00f6nnte")),
			TEXT("lock never picked: \u201egeht nicht\u201c, the door stays locked - and the reason says Vic could pick it"));
		Expect(RosaPlan && !RosaPlan->Tasks.IsEmpty() && RosaPlan->Tasks[0].EndLocation.Size2D() > 100.0, TEXT("... and its red marker stands where she waits, not at the world origin"));
		Expect(Heist->GetSchedule().FindProblems().Num() == 1 && Heist->GetSchedule().FindProblems()[0].ShortReason.ToString().Contains(TEXT("bleibt verschlossen")),
			TEXT("... the plan status names the one problem: \u201eHintert\u00fcr bleibt verschlossen\u201c"));
		UndoPlan();
		Expect(GetPlanHelp().IsEmpty() && Heist->GetSchedule().FindProblems().IsEmpty() && Plans->GetTasks(Vic->GetFName()).Num() == 1, TEXT("one undo brings the pick back - the help is gone"));
		Heist->Refresh();
		Plans->SetSpeed(2.f);
		StartPlayback();
		return true;
	});
	// Playing it: Rosa waits for the door instead of giving up, then walks out.
	SelfTestSteps.Add([this, Expect, CrewNamed, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const bool bWaiting = Rosa && Rosa->GetPlanRunner()->GetWaitText().ToString().Contains(TEXT("Hintert\u00fcr"));
		if (bWaiting || Now - Started > 20.0)
		{
			Expect(bWaiting, FString::Printf(TEXT("during playback Rosa waits: %s"), Rosa ? *Rosa->GetPlanRunner()->GetWaitText().ToString() : TEXT("-")));
			return true;
		}
		return false;
	});
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 60.0;
	});
	SelfTestSteps.Add([this, Expect, CrewNamed, Outside, bPassed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s)"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration);
		Expect(Rosa && Rosa->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Done, TEXT("Rosa's walk is done, not failed"));
		const double Distance = Rosa ? FVector::Dist2D(Rosa->GetActorLocation(), *Outside) : 1e9;
		Expect(Distance < 80.0, FString::Printf(TEXT("Rosa stands outside (%.0f cm from the spot)"), Distance));
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartHouseSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test House started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
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
	const auto DoorNamed = [this](const TCHAR* Name) -> ADCDoor*
	{
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetInteractableName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto RoomNamed = [this](const TCHAR* Name) -> ADCRoomVolume*
	{
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			if (It->GetRoomName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};

	SelfTestSteps.Add(Wait(2.f));
	// The house as built by Scripts/Levels/build_house.py.
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed, RoomNamed]()
	{
		int32 Rooms = 0;
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			++Rooms;
		}
		Expect(Rooms == 4, FString::Printf(TEXT("four rooms on the floor plan (%d)"), Rooms));
		Expect(DoorNamed(TEXT("Hintertür")) && DoorNamed(TEXT("Bürotür")) && DoorNamed(TEXT("Schlafzimmertür")), TEXT("back door, office door, bedroom door"));
		for (const TCHAR* Name : { TEXT("Vic"), TEXT("Rosa") })
		{
			const ADCCrewCharacter* Member = CrewNamed(Name);
			const ADCRoomVolume* Room = Member ? ADCRoomVolume::FindAt(this, Member->GetActorLocation()) : nullptr;
			Expect(Room && Room->GetRoomName().ToString() == TEXT("Garten"), FString::Printf(TEXT("%s starts in the garden"), Name));
		}
		const ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		const ADCRoomVolume* SafeRoom = Safe ? ADCRoomVolume::FindAt(this, Safe->GetActorLocation()) : nullptr;
		Expect(SafeRoom && SafeRoom->GetRoomName().ToString() == TEXT("Büro"), TEXT("the safe stands in the office"));

		// Vic plans to walk into the office: two locked doors on the way.
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCRoomVolume* Office = RoomNamed(TEXT("Büro"));
		ADCDoor* Back = DoorNamed(TEXT("Hintertür"));
		ADCDoor* OfficeDoor = DoorNamed(TEXT("Bürotür"));
		if (!Vic || !Office || !Back || !OfficeDoor)
		{
			return true;
		}
		Select(Vic);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Office->GetActorLocation() - FVector(0.0, 0.0, 120.0);
		Plans->AddTask(Vic->GetFName(), Go);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		const FDCCrewSchedule* Plan = Heist->GetSchedule().Find(Vic->GetFName());
		const bool bBlocked = Plan && Plan->Tasks.Num() == 1 && Plan->Tasks[0].bBlocked;
		UE_LOG(LogDerClou, Display, TEXT("Self-test: straight into the office: %s"), Plan && Plan->Tasks.Num() ? *Plan->Tasks[0].WaitReason.ToString() : TEXT("-"));
		Expect(bBlocked && Plan->Tasks[0].WaitReason.ToString().Contains(TEXT("Hintertür")) && Plan->Tasks[0].WaitReason.ToString().Contains(TEXT("Vic könnte")),
			TEXT("walking straight into the office waits at the locked back door - and the reason says Vic could pick it"));

		// Now with the doors: pick the back door, then the office door, then walk in - no "open" steps.
		Plans->ClearAll();
		const auto DoorStep = [](ADCDoor* Door, EDCDoorAction Action)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Door;
			Task.Target = Door->GetFName();
			Task.Action = static_cast<uint8>(Action);
			return Task;
		};
		Plans->AddTask(Vic->GetFName(), DoorStep(Back, EDCDoorAction::PickLock));
		Plans->AddTask(Vic->GetFName(), DoorStep(OfficeDoor, EDCDoorAction::PickLock));
		Plans->AddTask(Vic->GetFName(), Go);
		Heist->Refresh();
		Plan = Heist->GetSchedule().Find(Vic->GetFName());
		bool bAllPossible = Plan && Plan->Tasks.Num() == 3;
		for (const FDCScheduledTask& Task : Plan ? Plan->Tasks : TArray<FDCScheduledTask>())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Vic %d. %s %.1f-%.1f%s %s"), Task.Index + 1, *Task.Label.ToString(), Task.Start, Task.End,
				Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString());
			bAllPossible &= !Task.bBlocked;
		}
		Expect(bAllPossible, TEXT("with both doors picked, Vic's way into the office can happen (he opens them on the way)"));
		Plans->SetSpeed(2.f);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("house_plan.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]() { StartPlayback(); return true; });
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 90.0;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Shot(TEXT("house_done.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, bPassed]()
	{
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		const ADCRoomVolume* Room = Vic ? ADCRoomVolume::FindAt(this, Vic->GetActorLocation()) : nullptr;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s), Vic in %s"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration,
			Room ? *Room->GetRoomName().ToString() : TEXT("-"));
		Expect(Room && Room->GetRoomName().ToString() == TEXT("Büro"), TEXT("Vic stands in the office"));
		for (int32 Index = 0; Vic && Index < 3; ++Index)
		{
			Expect(Vic->GetPlanRunner()->GetStatus(Index) == EDCTaskStatus::Done, FString::Printf(TEXT("Vic step %d done"), Index + 1));
		}
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartAutoDoorSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test AutoDoor started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [this](float Seconds)
	{
		return [this, Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
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
	const auto RoomNamed = [this](const TCHAR* Name) -> ADCRoomVolume*
	{
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			if (It->GetRoomName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};

	SelfTestSteps.Add(Wait(2.f));
	// Both hands full (the painting): Rosa cannot open the bedroom door. She waits there - and Vic's
	// later, unrelated steps must not free her or move her wait.
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCRoomVolume* Bedroom = RoomNamed(TEXT("Schlafzimmer"));
		ADCItemPickup* Painting = FindPickup(TEXT("painting"));
		ADCDoor* Back = nullptr;
		ADCDoor* OfficeDoor = nullptr;
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			Back = It->GetInteractableName().ToString() == TEXT("Hintert\u00fcr") ? *It : Back;
			OfficeDoor = It->GetInteractableName().ToString() == TEXT("B\u00fcrot\u00fcr") ? *It : OfficeDoor;
		}
		Expect(Vic && Rosa && Bedroom && Painting && Back && OfficeDoor, TEXT("painting, bedroom, back door and office door are there"));
		if (!Vic || !Rosa || !Bedroom || !Painting || !Back || !OfficeDoor)
		{
			return true;
		}
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = Back->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		Plans->AddTask(Vic->GetFName(), Pick);
		FDCPlanTask PickOffice = Pick;
		PickOffice.Target = OfficeDoor->GetFName();
		Plans->AddTask(Vic->GetFName(), PickOffice);
		FDCPlanTask Linger;
		Linger.Type = EDCPlanTaskType::Wait;
		Linger.Seconds = 30.f;
		Plans->AddTask(Vic->GetFName(), Linger);
		Plans->AddTask(Vic->GetFName(), Pick);
		FDCPlanTask Lift;
		Lift.Type = EDCPlanTaskType::PickUp;
		Lift.Target = Painting->GetFName();
		Plans->AddTask(Rosa->GetFName(), Lift);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Bedroom->GetActorLocation() - FVector(0.0, 0.0, 120.0);
		Plans->AddTask(Rosa->GetFName(), Go);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		for (const FDCScheduledTask& Task : RosaPlan ? RosaPlan->Tasks : TArray<FDCScheduledTask>())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: hands full: Rosa %d. %s %.1f-%.1f ready %.1f%s | %s | waits for %s"), Task.Index + 1, *Task.Label.ToString(), Task.Start, Task.End, Task.Ready,
				Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString(), *Task.WaitForName.ToString());
		}
		const FDCScheduledTask* Walk = RosaPlan && RosaPlan->Tasks.Num() == 2 ? &RosaPlan->Tasks[1] : nullptr;
		Expect(RosaPlan && RosaPlan->Tasks.Num() == 2 && !RosaPlan->Tasks[0].bBlocked, TEXT("Rosa can take the painting once Vic has picked the back door"));
		Expect(Walk && Walk->bBlocked && Walk->WaitReason.ToString().Contains(TEXT("keine Hand frei")), TEXT("with the painting she cannot open the bedroom door: \u201ekeine Hand frei\u201c"));
		Expect(Walk && Walk->WaitFor.IsNone(), TEXT("... and nobody counts as the one she waits for"));
		Expect(Walk && FMath::Abs(Walk->Ready - (Walk->Legs.IsEmpty() ? Walk->Start : Walk->Legs.Last().End)) < 0.1f, TEXT("... and her wait at the door does not move with Vic's later steps"));
		Plans->ClearAll();
		return true;
	});
	// The door menu: "knacken und durchgehen" is one click for two steps; the title names door and state.
	SelfTestSteps.Add([this, Expect, CrewNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCDoor* Back = nullptr;
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			Back = It->GetInteractableName().ToString() == TEXT("Hintert\u00fcr") ? *It : Back;
		}
		if (!Vic || !Back || !RadialMenu)
		{
			Expect(false, TEXT("Vic, back door and menu are there"));
			return true;
		}
		Select(Vic);
		UDCHeistStateSubsystem::Get(this)->Refresh();
		OpenDoorMenu(Back);
		const FString Title = RadialMenu->GetTitle().ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: door menu title: %s"), *Title.Replace(TEXT("\n"), TEXT(" / ")));
		Expect(Title.Contains(TEXT("Hintert\u00fcr")) && Title.Contains(TEXT("verschlossen")), TEXT("the menu title names the door and its state"));
		Expect(RadialMenu->ChooseOption(TEXT("knacken und durchgehen")), TEXT("the locked door offers \u201eknacken und durchgehen\u201c"));
		const TArray<FDCPlanTask>& Steps = Plans->GetTasks(Vic->GetFName());
		Expect(Steps.Num() == 2 && Steps[0].Action == static_cast<uint8>(EDCDoorAction::PickLock) && Steps[1].Action == static_cast<uint8>(EDCDoorAction::GoThrough),
			TEXT("... one click, two steps: pick the lock, then go through"));
		UDCHeistStateSubsystem::Get(this)->Refresh();
		OpenDoorMenu(Back);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: door menu title after the pick: %s"), *RadialMenu->GetTitle().ToString().Replace(TEXT("\n"), TEXT(" / ")));
		Expect(RadialMenu->GetTitle().ToString().Contains(TEXT("offen")), TEXT("... after which the plan leaves the door open"));
		RadialMenu->Close();
		Plans->ClearAll();
		return true;
	});
	// The user's plan: Vic picks the back door; Rosa waits until he has, then walks into the hallway
	// and searches the dresser - without any "open" step.
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCRoomVolume* Hall = RoomNamed(TEXT("Flur"));
		ADCItemPickup* Dresser = FindPickup(TEXT("dresser"));
		ADCDoor* Back = nullptr;
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			Back = It->GetInteractableName().ToString() == TEXT("Hintert\u00fcr") ? *It : Back;
		}
		Expect(Vic && Rosa && Hall && Dresser && Back, TEXT("Vic, Rosa, hallway, dresser and back door are there"));
		if (!Vic || !Rosa || !Hall || !Dresser || !Back)
		{
			return true;
		}
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = Back->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		Plans->AddTask(Vic->GetFName(), Pick);
		// Rosa: "Warten ..." - until Vic has done step 1 (as the user did).
		Select(Rosa);
		FDCWaitChoice Choice;
		Choice.Kind = FDCWaitChoice::EKind::AfterStep;
		Choice.Crew = Vic->GetFName();
		Choice.Step = 0;
		ApplyWaitChoice(Choice);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Hall->GetActorLocation() - FVector(0.0, 0.0, 120.0);
		Plans->AddTask(Rosa->GetFName(), Go);
		FDCPlanTask Search;
		Search.Type = EDCPlanTaskType::Loot;
		Search.Target = Dresser->GetFName();
		Plans->AddTask(Rosa->GetFName(), Search);

		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		bool bAllPossible = true;
		for (const FDCCrewSchedule& Crew : Heist->GetSchedule().Crews)
		{
			for (const FDCScheduledTask& Task : Crew.Tasks)
			{
				UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %d. %s %.1f-%.1f%s | %s | %s"), *Crew.Name.ToString(), Task.Index + 1, *Task.Label.ToString(), Task.Start, Task.End,
					Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString(), *Task.Detail.ToString());
				bAllPossible &= !Task.bBlocked;
			}
		}
		Expect(bAllPossible, TEXT("the user's plan can happen: nothing is red"));
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		const FDCScheduledTask* Walk = RosaPlan ? RosaPlan->Tasks.FindByPredicate([](const FDCScheduledTask& Task) { return Task.Type == EDCPlanTaskType::Move; }) : nullptr;
		Expect(Walk && Walk->Detail.ToString().Contains(TEXT("\u00f6ffnet unterwegs")), TEXT("Rosa opens the back door herself on her way into the hallway"));
		Expect(Walk && Walk->Label.ToString().Contains(TEXT("Flur")), TEXT("her walk is called \u201egehen (Flur)\u201c"));
		Plans->SetSpeed(2.f);
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("autodoor_plan.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]() { StartPlayback(); return true; });
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 90.0;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add(Shot(TEXT("autodoor_done.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, bPassed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const ADCRoomVolume* Room = Rosa ? ADCRoomVolume::FindAt(this, Rosa->GetActorLocation()) : nullptr;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s), Rosa in %s"), Heist->GetPlaybackTime(), Heist->GetSchedule().Duration,
			Room ? *Room->GetRoomName().ToString() : TEXT("-"));
		Expect(Heist->IsPlaybackFinished(), TEXT("everyone is through with the plan"));
		Expect(Room && Room->GetRoomName().ToString() == TEXT("Flur"), TEXT("Rosa ends up in the hallway"));
		for (int32 Index = 0; Rosa && Index < UDCPlanSubsystem::Get(this)->GetTasks(Rosa->GetFName()).Num(); ++Index)
		{
			Expect(Rosa->GetPlanRunner()->GetStatus(Index) == EDCTaskStatus::Done, FString::Printf(TEXT("Rosa step %d done"), Index + 1));
		}
		Expect(Rosa && Rosa->GetInventory()->HasTool(TEXT("cash")), TEXT("Rosa took the cash from the dresser"));
		return true;
	});
	// After that: the back door stands open, the bedroom door is shut (not locked). Vic (in the garden)
	// tries walking into the bedroom: past the open door, the shut one he opens himself.
	const auto DoorNamed = [this](const TCHAR* Name) -> ADCDoor*
	{
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetInteractableName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto Floor = [](const ADCRoomVolume* Room) { return Room->GetActorLocation() - FVector(0.0, 0.0, 120.0); };
	const auto UntilIdle = [this, CrewNamed](const TCHAR* Name, double Seconds)
	{
		return [this, CrewNamed, Name, Seconds, Started = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			Started = Started < 0.0 ? Now : Started;
			const ADCCrewCharacter* Crew = CrewNamed(Name);
			return !Crew || !Crew->GetPlanRunner()->IsRunning() || Now - Started > Seconds;
		};
	};
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed, DoorNamed, Floor]()
	{
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCRoomVolume* Bedroom = RoomNamed(TEXT("Schlafzimmer"));
		ADCDoor* Back = DoorNamed(TEXT("Hintert\u00fcr"));
		ADCDoor* BedroomDoor = DoorNamed(TEXT("Schlafzimmert\u00fcr"));
		if (!Vic || !Bedroom || !Back || !BedroomDoor)
		{
			Expect(false, TEXT("Vic, bedroom and doors are there"));
			return true;
		}
		Expect(Back->IsOpen() && !BedroomDoor->IsOpen() && !BedroomDoor->IsLocked(), TEXT("back door open, bedroom door shut but not locked"));
		UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Direct);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Floor(Bedroom);
		Vic->GetPlanRunner()->Start({ Go });
		return true;
	});
	SelfTestSteps.Add(UntilIdle(TEXT("Vic"), 30.0));
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed, DoorNamed, Floor]()
	{
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const ADCRoomVolume* Room = Vic ? ADCRoomVolume::FindAt(this, Vic->GetActorLocation()) : nullptr;
		UE_LOG(LogDerClou, Display, TEXT("Self-test: Vic tried the way into the bedroom: %s, in %s"), Vic && Vic->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Done ? TEXT("done") : TEXT("not done"),
			Room ? *Room->GetRoomName().ToString() : TEXT("-"));
		Expect(Vic && Vic->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Done && Room && Room->GetRoomName().ToString() == TEXT("Schlafzimmer"),
			TEXT("past the open back door Vic opens the bedroom door himself and walks in"));

		// Waiting in vain: Vic's picks fail on purpose, Rosa waits at the office door - and gives up
		// once Vic has given up, with a red message.
		ADCRoomVolume* Office = RoomNamed(TEXT("B\u00fcro"));
		ADCDoor* OfficeDoor = DoorNamed(TEXT("B\u00fcrot\u00fcr"));
		if (!Vic || !Rosa || !Office || !OfficeDoor)
		{
			Expect(false, TEXT("Rosa, office and office door are there"));
			return true;
		}
		Expect(OfficeDoor->IsLocked(), TEXT("the office door is still locked"));
		UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Playing);
		Vic->bTestFailTasks = true;
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = OfficeDoor->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		Vic->GetPlanRunner()->Start({ Pick });
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Floor(Office);
		Rosa->GetPlanRunner()->Start({ Go });
		return true;
	});
	SelfTestSteps.Add(UntilIdle(TEXT("Rosa"), 60.0));
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed, Floor]()
	{
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		bool bProblem = false;
		const FString Toast = GetToast(bProblem).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: in vain: toast \"%s\""), *Toast);
		Expect(Vic && Vic->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Failed, TEXT("Vic gives up on the office door (his picks fail on purpose)"));
		Expect(Rosa && !Rosa->GetPlanRunner()->IsRunning() && Rosa->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Failed, TEXT("Rosa stops waiting for nothing"));
		Expect(bProblem && Toast.Contains(TEXT("kommt nicht weiter")), TEXT("... and a red message says \u201ekommt nicht weiter\u201c"));
		if (Vic)
		{
			Vic->bTestFailTasks = false;
		}
		// Trying things out: Rosa into the office (locked door) - she says so at once.
		UDCPlanSubsystem::Get(this)->SetMode(EDCPlanMode::Direct);
		ADCRoomVolume* Office = RoomNamed(TEXT("B\u00fcro"));
		if (Rosa && Office)
		{
			Select(Rosa);
			TryMoveTo(Floor(Office));
		}
		return true;
	});
	SelfTestSteps.Add(UntilIdle(TEXT("Rosa"), 10.0));
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const FString Notice = Rosa ? Rosa->GetNotice().ToString() : FString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: trying the office: Rosa says \"%s\""), *Notice);
		Expect(Rosa && Rosa->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Failed && Notice.Contains(TEXT("ist verschlossen")), TEXT("trying it out: she says the office door is locked"));
		// The bedroom door shut again, so that trying the bedroom has a door to open.
		if (ADCDoor* BedroomDoor = DoorNamed(TEXT("Schlafzimmert\u00fcr")))
		{
			BedroomDoor->Close();
		}
		return true;
	});
	SelfTestSteps.Add(Wait(2.5f));
	SelfTestSteps.Add([this, CrewNamed, RoomNamed, Floor]()
	{
		ADCRoomVolume* Bedroom = RoomNamed(TEXT("Schlafzimmer"));
		if (Bedroom && CrewNamed(TEXT("Rosa")))
		{
			TryMoveTo(Floor(Bedroom));
		}
		return true;
	});
	SelfTestSteps.Add(UntilIdle(TEXT("Rosa"), 20.0));
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed, bPassed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const ADCRoomVolume* Room = Rosa ? ADCRoomVolume::FindAt(this, Rosa->GetActorLocation()) : nullptr;
		const ADCDoor* BedroomDoor = DoorNamed(TEXT("Schlafzimmert\u00fcr"));
		UE_LOG(LogDerClou, Display, TEXT("Self-test: trying the bedroom: Rosa in %s, door %s"), Room ? *Room->GetRoomName().ToString() : TEXT("-"), BedroomDoor && BedroomDoor->IsOpen() ? TEXT("open") : TEXT("shut"));
		Expect(Room && Room->GetRoomName().ToString() == TEXT("Schlafzimmer") && BedroomDoor && BedroomDoor->IsOpen(), TEXT("trying it out: into the bedroom she opens the door on the way"));
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartUndoSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Undo started"));
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		return Now - Started > 2.0;
	});
	SelfTestSteps.Add([this, Expect, bPassed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		while (Plans->Undo())
		{
		}
		ADCCrewCharacter* Vic = nullptr;
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			Vic = It->GetCrewName().ToString() == TEXT("Vic") ? *It : Vic;
		}
		if (!Vic)
		{
			Expect(false, TEXT("Vic is there"));
			FinishSelfTest(*bPassed);
			return true;
		}
		const FName V = Vic->GetFName();
		Select(Vic);
		const auto Ids = [Plans, V]()
		{
			TArray<FGuid> Out;
			for (const FDCPlanTask& Task : Plans->GetTasks(V))
			{
				Out.Add(Task.Id);
			}
			return Out;
		};
		const auto WaitTask = [](float Seconds)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Wait;
			Task.Seconds = Seconds;
			return Task;
		};
		Expect(!Plans->CanUndo() && !Plans->CanRedo(), TEXT("an empty history: nothing to take back (the button is greyed out)"));
		IssueTask(WaitTask(5.f));
		IssueTask(WaitTask(10.f));
		IssueTask(WaitTask(20.f));
		const TArray<FGuid> Three = Ids();
		Expect(Three.Num() == 3 && Plans->CanUndo(), TEXT("three steps, each one can be taken back"));

		// Deleting a step and taking it back: the same steps in the same order.
		RemoveStep(1);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: undo text after deleting: %s"), *Plans->GetUndoText().ToString());
		Expect(Plans->GetUndoText().ToString().Contains(TEXT("warten 10 s")) && Plans->GetUndoText().ToString().Contains(TEXT("gel\u00f6scht")), TEXT("the undo text names the deleted step"));
		UndoPlan();
		Expect(Ids() == Three, TEXT("undo brings the deleted step back in its place, with its id"));
		RedoPlan();
		Expect(Ids().Num() == 2 && !Ids().Contains(Three[1]), TEXT("redo deletes it again"));
		UndoPlan();

		// A click that makes two steps is one undo step.
		IssueTasks({ WaitTask(1.f), WaitTask(2.f) });
		Expect(Ids().Num() == 5, TEXT("one click, two steps"));
		UndoPlan();
		Expect(Ids() == Three, TEXT("... and one undo takes both back"));

		// Moving a step.
		MoveStep(0, 1);
		Expect(Ids().Num() == 3 && Ids()[1] == Three[0], TEXT("moved one step later"));
		UndoPlan();
		Expect(Ids() == Three, TEXT("... undo puts it back"));

		// "Plan leeren" with one click - and back.
		ClearPlan();
		Expect(Plans->GetTasks(V).IsEmpty() && Plans->GetUndoText().ToString().Contains(TEXT("Plan geleert")), TEXT("\u201ePlan leeren\u201c clears at once and can be taken back"));
		UndoPlan();
		Expect(Ids() == Three, TEXT("... undo brings the whole plan back"));
		RedoPlan();
		Expect(Plans->GetTasks(V).IsEmpty(), TEXT("... redo clears it again"));
		UndoPlan();

		// A new change after an undo: there is nothing to bring back any more.
		UndoPlan();
		Expect(Plans->CanRedo(), TEXT("after an undo there is something to bring back"));
		IssueTask(WaitTask(3.f));
		Expect(!Plans->CanRedo(), TEXT("... until something new is planned"));

		// Everything back to the start.
		int32 Steps = 0;
		while (Plans->CanUndo() && Steps < 100)
		{
			UndoPlan();
			++Steps;
		}
		Expect(Plans->GetTasks(V).IsEmpty() && !Plans->CanUndo(), FString::Printf(TEXT("%d undos back to the empty plan"), Steps));
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartPlanHelpSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test PlanHelp started"));
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
	const auto RoomNamed = [this](const TCHAR* Name) -> ADCRoomVolume*
	{
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			if (It->GetRoomName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto DoorNamed = [this](const TCHAR* Name) -> ADCDoor*
	{
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetInteractableName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};

	SelfTestSteps.Add(Wait(2.f));
	// Rosa walks into the office - nobody unlocks the two doors: the click says so at once.
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCRoomVolume* Office = RoomNamed(TEXT("B\u00fcro"));
		ADCRoomVolume* Hall = RoomNamed(TEXT("Flur"));
		if (!Rosa || !Office || !Hall)
		{
			Expect(false, TEXT("Rosa, office and hallway are there"));
			return true;
		}
		Select(Rosa);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Office->GetActorLocation() - FVector(0.0, 0.0, 120.0);
		IssueTask(Go);
		const FString Help = GetPlanHelp().ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: help: %s"), *Help);
		Expect(bLastEditProblem, TEXT("the click ring turns red"));
		Expect(Help.Contains(TEXT("Rosa")) && Help.Contains(TEXT("Hintert\u00fcr")), TEXT("the help bar names Rosa and the back door at once"));
		const TArray<FDCPlanProblem> Problems = UDCHeistStateSubsystem::Get(this)->GetSchedule().FindProblems();
		Expect(Problems.Num() == 1 && Problems[0].Crew == Rosa->GetFName() && Problems[0].Index == 0, TEXT("the status: \u201ePlan h\u00e4ngt: Rosa ab Schritt 1\u201c"));
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("planhelp_bar.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomNamed, DoorNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCRoomVolume* Hall = RoomNamed(TEXT("Flur"));
		ADCDoor* Back = DoorNamed(TEXT("Hintert\u00fcr"));
		ADCDoor* OfficeDoor = DoorNamed(TEXT("B\u00fcrot\u00fcr"));
		if (!Rosa || !Vic || !Hall || !Back || !OfficeDoor)
		{
			Expect(false, TEXT("Vic and the doors are there"));
			return true;
		}
		// Two more steps behind it: no second help, they just come too late.
		DismissPlanHelp();
		FDCPlanTask Linger;
		Linger.Type = EDCPlanTaskType::Wait;
		Linger.Seconds = 5.f;
		IssueTask(Linger);
		FDCPlanTask Back2Hall;
		Back2Hall.Type = EDCPlanTaskType::Move;
		Back2Hall.Location = Hall->GetActorLocation() - FVector(0.0, 0.0, 120.0);
		IssueTask(Back2Hall);
		bool bProblem = false;
		const FString Toast = GetToast(bProblem).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: toast behind the problem: %s"), *Toast);
		Expect(GetPlanHelp().IsEmpty(), TEXT("no second help for steps behind the one that hangs"));
		Expect(Toast.Contains(TEXT("kommt erst dran")), TEXT("... the message says they come only once step 1 works"));
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		Expect(RosaPlan && RosaPlan->Tasks.Num() == 3 && !RosaPlan->Tasks[0].bAfterBlocked && RosaPlan->Tasks[1].bAfterBlocked && RosaPlan->Tasks[2].bAfterBlocked,
			TEXT("one red step, the ones after it grey (\u201ekommt nicht dran\u201c)"));

		// Vic picks both doors: the plan works again - and says so.
		Select(Vic);
		const auto PickStep = [](const ADCDoor* Door)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Door;
			Task.Target = Door->GetFName();
			Task.Action = static_cast<uint8>(EDCDoorAction::PickLock);
			return Task;
		};
		IssueTask(PickStep(Back));
		Expect(Heist->GetSchedule().FindProblems().Num() == 1, TEXT("with only the back door picked, Rosa still hangs (now at the office door)"));
		IssueTask(PickStep(OfficeDoor));
		const FString Fine = GetToast(bProblem).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: after both picks: %s"), *Fine);
		Expect(Heist->GetSchedule().FindProblems().IsEmpty(), TEXT("with both doors picked the plan works out"));
		Expect(Fine.Contains(TEXT("Passt jetzt")) && !bProblem, TEXT("... and the message says \u201ePasst jetzt \u2013 Plan geht auf\u201c"));
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("planhelp_fine.png")));
	SelfTestSteps.Add([this, bPassed]()
	{
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartPlanLookSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test PlanLook started"));
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
	const auto DoorNamed = [this](const TCHAR* Name) -> ADCDoor*
	{
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetInteractableName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};

	SelfTestSteps.Add(Wait(2.f));
	// Vic picks the back door, Rosa walks into the hallway (and opens it on the way).
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCDoor* Back = DoorNamed(TEXT("Hintert\u00fcr"));
		ADCRoomVolume* Hall = nullptr;
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			Hall = It->GetRoomName().ToString() == TEXT("Flur") ? *It : Hall;
		}
		if (!Vic || !Rosa || !Back || !Hall)
		{
			Expect(false, TEXT("Vic, Rosa, back door and hallway are there"));
			return true;
		}
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = Back->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		Plans->AddTask(Vic->GetFName(), Pick);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Hall->GetActorLocation() - FVector(0.0, 0.0, 120.0);
		Plans->AddTask(Rosa->GetFName(), Go);
		Heist->SetFocus(NAME_None);
		Heist->Refresh();
		const FDCSchedule& Schedule = Heist->GetSchedule();
		const FDCCrewSchedule* VicPlan = Schedule.Find(Vic->GetFName());
		const float Picked = VicPlan && VicPlan->Tasks.Num() == 1 ? VicPlan->Tasks[0].End : -1.f;
		FDCSimDoor At;
		const FDCDoorChange* By = nullptr;
		Expect(Schedule.GetDoorAt(Back->GetFName(), 0.f, At, &By) && At.bLocked && !At.bOpen && !By, TEXT("at 0:00 the back door is locked"));
		Expect(Schedule.GetDoorAt(Back->GetFName(), Picked + 0.05f, At, &By) && !At.bLocked && !At.bOpen && By && Schedule.Crews.IsValidIndex(By->CrewIndex)
			&& Schedule.Crews[By->CrewIndex].Crew == Vic->GetFName(), TEXT("after Vic's pick it is unlocked, still shut - by Vic"));
		Expect(Schedule.GetDoorAt(Back->GetFName(), Schedule.Duration, At, &By) && At.bOpen && By && Schedule.Crews.IsValidIndex(By->CrewIndex)
			&& Schedule.Crews[By->CrewIndex].Crew == Rosa->GetFName() && At.Swing != 0.f, TEXT("at the end it is open - Rosa opened it on her way"));
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s / %s"), *Heist->DescribeViewMoment().ToString(), *Heist->DescribePlannedDoor(Back).ToString());
		return true;
	});
	SelfTestSteps.Add(Wait(1.2f));
	SelfTestSteps.Add(Shot(TEXT("plan_look_end.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed]()
	{
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCDoor* Back = DoorNamed(TEXT("Hintert\u00fcr"));
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		if (!Back || !Vic)
		{
			return true;
		}
		UE_LOG(LogDerClou, Display, TEXT("Self-test: end look: padlock %d, shown %.2f, sign \"%s\", real open %d locked %d"), Back->IsPadlockShown() ? 1 : 0, Back->GetShownOpenAmount(),
			*Back->GetPlanLabel().ToString(), Back->IsOpen() ? 1 : 0, Back->IsLocked() ? 1 : 0);
		Expect(Back->IsPlanLookOn() && !Back->IsPadlockShown(), TEXT("the room shows the end of the plan: no padlock on the back door"));
		Expect(Back->GetShownOpenAmount() > 0.95f, TEXT("... its panel stands open"));
		Expect(Back->GetPlanLabel().ToString().Contains(TEXT("Rosa")), TEXT("... the sign says Rosa opens it"));
		Expect(!Back->IsOpen() && Back->IsLocked(), TEXT("... only the look: the real door is still shut and locked"));
		Expect(Heist->DescribeViewMoment().ToString().Contains(TEXT("nach dem Plan")), TEXT("the frame says \u201eRaum zeigt: nach dem Plan\u201c"));

		// The ghost of the selected crew member (Rosa): where her next step begins - the end of her plan.
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCRoomVolume* Hall = nullptr;
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			Hall = It->GetRoomName().ToString() == TEXT("Flur") ? *It : Hall;
		}
		if (Rosa && Hall)
		{
			FocusStep(Rosa->GetFName(), INDEX_NONE);
			const ADCPlanGhost* Ghost = Heist->GetFigureGhost();
			UE_LOG(LogDerClou, Display, TEXT("Self-test: ghost \"%s\" at %s"), Ghost ? *Ghost->GetText().ToString() : TEXT("-"), Ghost ? *Ghost->GetActorLocation().ToCompactString() : TEXT("-"));
			Expect(Ghost && Ghost->GetCrew() == Rosa->GetFName() && !Ghost->IsStuck() && FVector::Dist2D(Ghost->GetActorLocation(), Hall->GetActorLocation()) < 80.0,
				TEXT("Rosa's ghost stands where her plan ends (in the hallway)"));
			// Without Vic's pick she hangs at the back door: the ghost stands there, red.
			UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
			Plans->RemoveTask(Vic->GetFName(), 0);
			Ghost = Heist->GetFigureGhost();
			UE_LOG(LogDerClou, Display, TEXT("Self-test: stuck ghost \"%s\" at %s"), Ghost ? *Ghost->GetText().ToString() : TEXT("-"), Ghost ? *Ghost->GetActorLocation().ToCompactString() : TEXT("-"));
			Expect(Ghost && Ghost->IsStuck() && Ghost->GetText().ToString().Contains(TEXT("h\u00e4ngt")) && FVector::Dist2D(Ghost->GetActorLocation(), Back->GetCenter()) < 260.0,
				TEXT("... without Vic's pick it stands red at the back door, where she hangs"));
			Plans->Undo();
			// A preview time shows the pins of everybody instead.
			Heist->SetPreviewTime(1.f);
			Expect(!Heist->GetFigureGhost(), TEXT("with a preview time there is no figure ghost, only the pins"));
			Heist->SetPreviewTime(-1.f);
		}
		// Picking Vic's step 1: the room shows the moment after it - unlocked, still shut.
		FocusStep(Vic->GetFName(), 0);
		return true;
	});
	SelfTestSteps.Add(Wait(1.2f));
	SelfTestSteps.Add(Shot(TEXT("plan_look_step1.png")));
	SelfTestSteps.Add([this, Expect]()
	{
		const ADCPlanGhost* Ghost = UDCHeistStateSubsystem::Get(this)->GetFigureGhost();
		Expect(Ghost && Ghost->GetText().ToString().Contains(TEXT("nach 1")), TEXT("Vic's step 1 picked: his ghost says \u201eVic nach 1\u201c"));
		return true;
	});
	SelfTestSteps.Add([this, Expect, DoorNamed]()
	{
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCDoor* Back = DoorNamed(TEXT("Hintert\u00fcr"));
		if (!Back)
		{
			return true;
		}
		UE_LOG(LogDerClou, Display, TEXT("Self-test: step look: %s, padlock %d, shown %.2f"), *Heist->DescribeViewMoment().ToString(), Back->IsPadlockShown() ? 1 : 0, Back->GetShownOpenAmount());
		Expect(Heist->DescribeViewMoment().ToString().Contains(TEXT("nach Vics Schritt 1")), TEXT("the frame says \u201eRaum zeigt: nach Vics Schritt 1\u201c"));
		Expect(!Back->IsPadlockShown() && Back->GetShownOpenAmount() < 0.05f, TEXT("... the back door: no padlock, still shut"));
		// Playing shows the real doors again.
		StartPlayback();
		return true;
	});
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, Expect, DoorNamed, bPassed]()
	{
		const ADCDoor* Back = DoorNamed(TEXT("Hintert\u00fcr"));
		Expect(Back && !Back->IsPlanLookOn(), TEXT("playing: the doors show their real state again"));
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartInsertSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Insert started"));
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	SelfTestSteps.Add([Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		return Now - Started > 2.0;
	});
	SelfTestSteps.Add([this, Expect, bPassed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		ADCCrewCharacter* Vic = nullptr;
		ADCCrewCharacter* Rosa = nullptr;
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			Vic = It->GetCrewName().ToString() == TEXT("Vic") ? *It : Vic;
			Rosa = It->GetCrewName().ToString() == TEXT("Rosa") ? *It : Rosa;
		}
		ADCDoor* Back = nullptr;
		ADCDoor* OfficeDoor = nullptr;
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			Back = It->GetInteractableName().ToString() == TEXT("Hintert\u00fcr") ? *It : Back;
			OfficeDoor = It->GetInteractableName().ToString() == TEXT("B\u00fcrot\u00fcr") ? *It : OfficeDoor;
		}
		ADCRoomVolume* Hall = nullptr;
		ADCRoomVolume* Office = nullptr;
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			Hall = It->GetRoomName().ToString() == TEXT("Flur") ? *It : Hall;
			Office = It->GetRoomName().ToString() == TEXT("B\u00fcro") ? *It : Office;
		}
		ADCItemPickup* Dresser = FindPickup(TEXT("dresser"));
		if (!Vic || !Rosa || !Back || !OfficeDoor || !Hall || !Office || !Dresser)
		{
			Expect(false, TEXT("crew, doors, rooms and dresser are there"));
			FinishSelfTest(false);
			return true;
		}
		const FName V = Vic->GetFName();
		const FName R = Rosa->GetFName();
		const auto PickStep = [](const ADCDoor* Door)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Door;
			Task.Target = Door->GetFName();
			Task.Action = static_cast<uint8>(EDCDoorAction::PickLock);
			return Task;
		};
		const auto WalkStep = [](const ADCRoomVolume* Room)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Move;
			Task.Location = Room->GetActorLocation() - FVector(0.0, 0.0, 120.0);
			return Task;
		};
		Select(Vic);
		IssueTask(PickStep(Back));
		IssueTask(PickStep(OfficeDoor));
		Select(Rosa);
		IssueTask(WalkStep(Hall));
		IssueTask(WalkStep(Office));
		Expect(GetInsertIndex() == INDEX_NONE, TEXT("nothing picked: new steps go to the end"));

		// Pick Rosa's step 1: the next click goes right after it, and is then the picked one.
		FocusStep(R, 0);
		Expect(GetInsertIndex() == 1, TEXT("Rosa's step 1 picked: new steps come after it"));
		FDCPlanTask Search;
		Search.Type = EDCPlanTaskType::Loot;
		Search.Target = Dresser->GetFName();
		IssueTask(Search);
		bool bProblem = false;
		const FString Toast = GetToast(bProblem).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: insert toast: %s"), *Toast);
		const TArray<FDCPlanTask>& RosaSteps = Plans->GetTasks(R);
		Expect(RosaSteps.Num() == 3 && RosaSteps[1].Type == EDCPlanTaskType::Loot && RosaSteps[2].Type == EDCPlanTaskType::Move, TEXT("the dresser search became Rosa's step 2, the office walk step 3"));
		Expect(Heist->GetFocusStep() == 1 && GetInsertIndex() == 2, TEXT("... and is picked now, so the next click follows it"));
		Expect(Toast.Contains(TEXT("nach")), TEXT("... the message says after which step"));
		Expect(Heist->GetSchedule().FindProblems().IsEmpty(), TEXT("... and the plan still works out"));

		// Rücktaste / Entf delete the picked step; Esc goes back to "at the end".
		RemoveStep(GetInsertIndex() - 1);
		Expect(Plans->GetTasks(R).Num() == 2 && Plans->GetTasks(R)[1].Type == EDCPlanTaskType::Move, TEXT("deleting the picked step takes the search out again"));
		Heist->SetFocus(R);
		Expect(GetInsertIndex() == INDEX_NONE, TEXT("no step picked: at the end again"));

		// "Rosa wartet hierauf" at Vic's step 2 (office door): her wait goes before her first step.
		const FGuid OfficePick = Plans->GetTasks(V)[1].Id;
		ToggleWaitOn(V, 1, R);
		const TArray<FDCPlanTask>& Waiting = Plans->GetTasks(R);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: wait toast: %s"), *GetToast(bProblem).ToString());
		Expect(Waiting.Num() == 3 && Waiting[0].Type == EDCPlanTaskType::WaitFor && Waiting[0].Ref == OfficePick, TEXT("one click: Rosa waits for Vic's office door before her first step"));
		Expect(!Plans->GetTasks(V).ContainsByPredicate([](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::Signal; }), TEXT("... without an extra signal step at Vic"));
		const FDCCrewSchedule* VicPlan = Heist->GetSchedule().Find(V);
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(R);
		Expect(VicPlan && RosaPlan && RosaPlan->Tasks.Num() == 3 && RosaPlan->Tasks[1].Start >= VicPlan->Tasks[1].End - 0.05f, TEXT("... her walk starts once Vic has picked the office door"));

		// Moving Vic's step keeps the link; a second click takes the wait away.
		Select(Vic);
		FocusStep(V, 1);
		MoveStep(1, -1);
		Expect(Plans->GetTasks(V)[0].Id == OfficePick && Plans->GetTasks(R).Num() == 3 && Plans->GetTasks(R)[0].Type == EDCPlanTaskType::WaitFor && Plans->GetTasks(R)[0].Ref == OfficePick,
			TEXT("Vic's step moved to 1: Rosa still waits for exactly that step"));
		ToggleWaitOn(V, 0, R);
		Expect(Plans->GetTasks(R).Num() == 2, TEXT("a second click: Rosa does not wait any more"));

		// Deleting the step takes the wait for it along - and one undo brings both back.
		ToggleWaitOn(V, 0, R);
		Heist->SetFocus(V);
		RemoveStep(0);
		const FString Removed = GetToast(bProblem).ToString();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: delete toast: %s"), *Removed);
		Expect(Plans->GetTasks(V).Num() == 1 && Plans->GetTasks(R).Num() == 2 && Removed.Contains(TEXT("Warten darauf auch")), TEXT("deleting Vic's step takes Rosa's wait along, and says so"));
		UndoPlan();
		Expect(Plans->GetTasks(V).Num() == 2 && Plans->GetTasks(R).Num() == 3 && Plans->GetTasks(R)[0].Ref == OfficePick, TEXT("... one undo brings both back"));
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::StartVariantsSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Variants started"));
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
	const auto Setup = [this]() -> TPair<ADCCrewCharacter*, ADCCrewCharacter*>
	{
		ADCCrewCharacter* Vic = nullptr;
		ADCCrewCharacter* Rosa = nullptr;
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			Vic = It->GetCrewName().ToString() == TEXT("Vic") ? *It : Vic;
			Rosa = It->GetCrewName().ToString() == TEXT("Rosa") ? *It : Rosa;
		}
		return TPair<ADCCrewCharacter*, ADCCrewCharacter*>(Vic, Rosa);
	};
	const auto RoomFloor = [this](const TCHAR* Name)
	{
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			if (It->GetRoomName().ToString() == Name)
			{
				return It->GetActorLocation() - FVector(0.0, 0.0, 120.0);
			}
		}
		return FVector::ZeroVector;
	};

	SelfTestSteps.Add(Wait(2.f));
	// Rosa walks into the hallway (Vic picks the back door); the ghost as a figure, then as a pin.
	SelfTestSteps.Add([this, Expect, Setup, RoomFloor]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		const TPair<ADCCrewCharacter*, ADCCrewCharacter*> Crew = Setup();
		ADCCrewCharacter* Vic = Crew.Key;
		ADCCrewCharacter* Rosa = Crew.Value;
		ADCDoor* Back = nullptr;
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			Back = It->GetInteractableName().ToString() == TEXT("Hintert\u00fcr") ? *It : Back;
		}
		if (!Vic || !Rosa || !Back)
		{
			Expect(false, TEXT("Vic, Rosa and the back door are there"));
			return true;
		}
		Expect(!Plans->bGhostAsPin && !Plans->bHelpAsMenu && !Plans->bInsertOnlyWithButton, TEXT("self-tests start with the default variants"));
		Select(Vic);
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = Back->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		IssueTask(Pick);
		Select(Rosa);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = RoomFloor(TEXT("Flur"));
		IssueTask(Go);
		InsertAtEnd();
		Expect(UDCHeistStateSubsystem::Get(this)->GetFigureGhost() != nullptr, TEXT("Rosa's ghost stands at the end of her plan"));
		return true;
	});
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("ghost_figure.png")));
	SelfTestSteps.Add([this]() { SetVariant(0, true); return true; });
	SelfTestSteps.Add(Wait(1.f));
	SelfTestSteps.Add(Shot(TEXT("ghost_pin.png")));
	// The help as a menu at the mouse: a click that makes the plan hang opens it.
	SelfTestSteps.Add([this, Expect, Setup, RoomFloor]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		SetVariant(0, false);
		SetVariant(1, true);
		ADCCrewCharacter* Rosa = Setup().Value;
		if (!Rosa || !RadialMenu)
		{
			return true;
		}
		Select(Rosa);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = RoomFloor(TEXT("B\u00fcro"));
		IssueTask(Go);
		Expect(RadialMenu->IsOpen() && RadialMenu->GetTitle().ToString().Contains(TEXT("Geht so nicht")), TEXT("variant: the help opens as a menu at the mouse"));
		Expect(GetPlanHelp().IsEmpty(), TEXT("... and not as a bar"));
		const int32 Before = Plans->GetTasks(Rosa->GetFName()).Num();
		Expect(RadialMenu->ChooseOption(TEXT("R\u00fcckg\u00e4ngig")) && Plans->GetTasks(Rosa->GetFName()).Num() == Before - 1, TEXT("... its \u201eR\u00fcckg\u00e4ngig\u201c takes the click back"));
		SetVariant(1, false);
		return true;
	});
	// Insert only with the button: picking a step alone changes nothing.
	SelfTestSteps.Add([this, Expect, Setup, RoomFloor, bPassed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		SetVariant(2, true);
		ADCCrewCharacter* Rosa = Setup().Value;
		if (!Rosa)
		{
			FinishSelfTest(false);
			return true;
		}
		const FName R = Rosa->GetFName();
		Select(Rosa);
		FDCPlanTask Linger;
		Linger.Type = EDCPlanTaskType::Wait;
		Linger.Seconds = 3.f;
		IssueTask(Linger);
		FocusStep(R, 0);
		Expect(GetInsertIndex() == INDEX_NONE, TEXT("variant \u201enur per Knopf\u201c: picking a step alone does not change where new steps go"));
		InsertAfterStep(0);
		Expect(GetInsertIndex() == 1, TEXT("... \u201e+ danach einf\u00fcgen\u201c does"));
		FDCPlanTask Short;
		Short.Type = EDCPlanTaskType::Wait;
		Short.Seconds = 1.f;
		IssueTask(Short);
		Expect(Plans->GetTasks(R).Num() == 3 && Plans->GetTasks(R)[1].Seconds == 1.f && GetInsertIndex() == 2, TEXT("... the new step came after step 1, the next one follows it"));
		InsertAtEnd();
		Expect(GetInsertIndex() == INDEX_NONE, TEXT("... \u201eans Ende\u201c / Esc: at the end again"));
		SetVariant(2, false);
		FinishSelfTest(*bPassed);
		return true;
	});
}

void ADCPlayerController::FinishSelfTest(bool bPassed)
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test %s"), bPassed ? TEXT("PASSED") : TEXT("FAILED"));
	FPlatformMisc::RequestExit(false, TEXT("DCSelfTest"));
}

#undef LOCTEXT_NAMESPACE
