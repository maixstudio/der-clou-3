#include "DCPlayerController.h"

#include "DCClickMarker.h"
#include "DCHeistState.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCTimelinePanel.h"
#include "DCCameraPawn.h"
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

	// Coming back from "Abspielen" after a reload: play right away.
	UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (Plans && Plans->bPlayAfterLoad)
	{
		Plans->bPlayAfterLoad = false;
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &ADCPlayerController::StartPlayback, 0.5f, false);
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
	if (WasInputKeyJustPressed(EKeys::BackSpace))
	{
		RemoveLastStep();
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
	const bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, Hit);
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
		Actions = { bOpen ? EDCDoorAction::Close : EDCDoorAction::Open, EDCDoorAction::GoThrough };
	}

	TArray<FDCRadialOption> Options;
	for (const EDCDoorAction Action : Actions)
	{
		FDCPlanTask Task;
		Task.Type = EDCPlanTaskType::Door;
		Task.Target = Door->GetFName();
		Task.Action = static_cast<uint8>(Action);
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
	Info.OnSelected = [WeakPanel, WeakDoor]()
	{
		if (WeakPanel.IsValid() && WeakDoor.IsValid())
		{
			WeakPanel->ShowFor(WeakDoor.Get());
		}
	};
	const FText Title = FText::Format(bPlanning ? LOCTEXT("PlanTitle", "{0}\nplant: {1}") : LOCTEXT("DirectTitle", "{0}\n{1}"), Door->DescribeState(), Selected->GetCrewName());
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
		Option.Detail = bGoneInPlan ? LootCheck.Reason : LOCTEXT("LootDetail", "Inhalt unbekannt · Funde entscheidest du live");
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
	Option.Detail = Check.DescribeShort();
	Option.Tooltip = Check.Describe();
	Option.bEnabled = Check.bPossible;
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
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}
	if (ADCCrewCharacter* Crew = Cast<ADCCrewCharacter>(Hit.GetActor()))
	{
		Select(Crew);
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
		IssueTask(Task);
		if (Marker)
		{
			Marker->Show(Hit.ImpactPoint, true);
		}
		return;
	}
	Selected->GetPlanRunner()->Stop();
	const bool bReachable = Selected->MoveToLocation(Hit.ImpactPoint);
	if (UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this))
	{
		Heist->MarkWorldUsed();
	}
	if (Marker)
	{
		Marker->Show(Hit.ImpactPoint, bReachable);
	}
	UE_LOG(LogDerClou, Verbose, TEXT("%s -> %s (%s)"), *Selected->GetCrewName().ToString(), *Hit.ImpactPoint.ToString(), bReachable ? TEXT("ok") : TEXT("unreachable"));
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
		const int32 First = Plans->GetTasks(Selected->GetFName()).Num() + 1;
		TArray<FString> Names;
		for (const FDCPlanTask& Task : Tasks)
		{
			Plans->AddTask(Selected->GetFName(), Task);
			Names.Add(UDCHeistStateSubsystem::Get(this)->DescribeTask(Task).ToString());
		}
		ShowToast(FText::Format(LOCTEXT("StepsAdded", "{0}, Schritte {1}–{2}: {3}"), Selected->GetCrewName(), FText::AsNumber(First),
			FText::AsNumber(First + Tasks.Num() - 1), FText::FromString(FString::Join(Names, TEXT(", dann ")))));
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
		Plans->AddTask(Selected->GetFName(), Task);
		ShowToast(FText::Format(LOCTEXT("StepAdded", "{0}, Schritt {1}: {2}"), Selected->GetCrewName(),
			FText::AsNumber(Plans->GetTasks(Selected->GetFName()).Num()), UDCHeistStateSubsystem::Get(this)->DescribeTask(Task)));
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
	const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	UGameplayStatics::OpenLevel(this, FName(*Map));
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
	if (Selected && GetMode() == EDCPlanMode::Planning)
	{
		UDCPlanSubsystem::Get(this)->RemoveLastTask(Selected->GetFName());
	}
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
	if (Selected && GetMode() == EDCPlanMode::Planning)
	{
		UDCPlanSubsystem::Get(this)->RemoveTask(Selected->GetFName(), Index);
		UDCHeistStateSubsystem::Get(this)->SetFocus(Selected->GetFName());
	}
}

void ADCPlayerController::MoveStep(int32 Index, int32 Delta)
{
	if (Selected && GetMode() == EDCPlanMode::Planning)
	{
		const int32 Count = UDCPlanSubsystem::Get(this)->GetTasks(Selected->GetFName()).Num();
		UDCPlanSubsystem::Get(this)->MoveTask(Selected->GetFName(), Index, Delta);
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
	FDCPlanTask Wait;
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
	Plans->AddTask(Own, Wait);
	ShowToast(FText::Format(LOCTEXT("WaitAdded", "{0}, Schritt {1}: {2}"), Selected->GetCrewName(), FText::AsNumber(Plans->GetTasks(Own).Num()),
		UDCHeistStateSubsystem::Get(this)->DescribeTask(Wait)));
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
	Select(Crew);
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
	if (GetMode() == EDCPlanMode::Planning)
	{
		UDCPlanSubsystem::Get(this)->ClearAll();
		ShowToast(LOCTEXT("PlanCleared", "Alle Pläne gelöscht"));
	}
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

		// Without Vic picking it, the walk cannot happen – and the plan says why.
		Plans->RemoveTask(Vic->GetFName(), 0);
		Heist->Refresh();
		RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		const bool bBlocked = RosaPlan && RosaPlan->Tasks.Num() == 1 && RosaPlan->Tasks[0].bBlocked;
		UE_LOG(LogDerClou, Display, TEXT("Self-test: without picking: %s"), RosaPlan && RosaPlan->Tasks.Num() ? *RosaPlan->Tasks[0].WaitReason.ToString() : TEXT("-"));
		Expect(bBlocked && RosaPlan->Tasks[0].WaitReason.ToString().Contains(TEXT("aufgesperrt")), TEXT("lock never picked: \u201egeht nicht\u201c, because the door stays locked"));
		Expect(RosaPlan && !RosaPlan->Tasks.IsEmpty() && RosaPlan->Tasks[0].EndLocation.Size2D() > 100.0, TEXT("... and its red marker stands where she waits, not at the world origin"));
		Plans->AddTask(Vic->GetFName(), Pick);
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
		Expect(bBlocked && Plan->Tasks[0].WaitReason.ToString().Contains(TEXT("Hintertür")),
			TEXT("walking straight into the office waits at the locked back door - and nobody unlocks it"));

		// Now with the doors: pick and open the back door, then the office door, then walk in.
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
		Plans->AddTask(Vic->GetFName(), DoorStep(Back, EDCDoorAction::Open));
		Plans->AddTask(Vic->GetFName(), DoorStep(OfficeDoor, EDCDoorAction::PickLock));
		Plans->AddTask(Vic->GetFName(), DoorStep(OfficeDoor, EDCDoorAction::Open));
		Plans->AddTask(Vic->GetFName(), Go);
		Heist->Refresh();
		Plan = Heist->GetSchedule().Find(Vic->GetFName());
		bool bAllPossible = Plan && Plan->Tasks.Num() == 5;
		for (const FDCScheduledTask& Task : Plan ? Plan->Tasks : TArray<FDCScheduledTask>())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Vic %d. %s %.1f-%.1f%s %s"), Task.Index + 1, *Task.Label.ToString(), Task.Start, Task.End,
				Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString());
			bAllPossible &= !Task.bBlocked;
		}
		Expect(bAllPossible, TEXT("with both doors picked and opened, Vic's way into the office can happen"));
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
		for (int32 Index = 0; Vic && Index < 5; ++Index)
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
