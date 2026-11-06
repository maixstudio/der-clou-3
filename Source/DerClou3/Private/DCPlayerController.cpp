#include "DCPlayerController.h"

#include "DCClickMarker.h"
#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCInfoPanel.h"
#include "DCInventoryComponent.h"
#include "DCInventoryPanel.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCRadialMenu.h"
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
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHoverCursor();

	if (!SelfTestSteps.IsEmpty() && SelfTestSteps[0]())
	{
		SelfTestSteps.RemoveAt(0);
	}
	if (InventoryPanel && WasInputKeyJustPressed(EKeys::I))
	{
		InventoryPanel->Toggle();
	}

	const bool bMenuOpen = RadialMenu && RadialMenu->IsOpen();
	if (bMenuOpen && (WasInputKeyJustPressed(EKeys::RightMouseButton) || WasInputKeyJustPressed(EKeys::Escape)))
	{
		RadialMenu->Close();
		return;
	}
	if (!bMenuOpen && InfoPanel && InfoPanel->IsShown() && WasInputKeyJustPressed(EKeys::Escape))
	{
		InfoPanel->Hide();
		return;
	}
	if (!bMenuOpen && InventoryPanel && InventoryPanel->IsShown() && WasInputKeyJustPressed(EKeys::Escape))
	{
		InventoryPanel->Hide();
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
	const bool bOverDoor = bHit && Cast<ADCDoor>(Hit.GetActor());
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
	if (!RadialMenu || !Door)
	{
		return;
	}
	TWeakObjectPtr<ADCDoor> WeakDoor = Door;
	TWeakObjectPtr<ADCCrewCharacter> WeakCrew = Selected.Get();
	TArray<FDCRadialOption> Options;
	for (const EDCDoorAction Action : Door->GetActions())
	{
		FDCRadialOption& Option = Options.AddDefaulted_GetRef();
		Option.Label = ADCDoor::DescribeAction(Action);
		Option.OnSelected = [WeakDoor, WeakCrew, Action]()
		{
			if (WeakDoor.IsValid() && WeakCrew.IsValid())
			{
				WeakCrew->InteractWithDoor(WeakDoor.Get(), Action);
			}
		};
	}
	// Info is always the last entry, for every interactable.
	TWeakObjectPtr<UDCInfoPanel> WeakPanel = InfoPanel.Get();
	FDCRadialOption& Info = Options.AddDefaulted_GetRef();
	Info.Label = LOCTEXT("Info", "Info");
	Info.OnSelected = [WeakPanel, WeakDoor]()
	{
		if (WeakPanel.IsValid() && WeakDoor.IsValid())
		{
			WeakPanel->ShowFor(WeakDoor.Get());
		}
	};
	RadialMenu->Open(Door->DescribeState(), MoveTemp(Options));
}

void ADCPlayerController::OpenPickupMenu(ADCItemPickup* Pickup)
{
	if (!RadialMenu || !Pickup)
	{
		return;
	}
	TWeakObjectPtr<ADCItemPickup> WeakPickup = Pickup;
	TWeakObjectPtr<ADCCrewCharacter> WeakCrew = Selected.Get();
	TWeakObjectPtr<UDCInfoPanel> WeakPanel = InfoPanel.Get();
	TArray<FDCRadialOption> Options;
	FDCRadialOption& Take = Options.AddDefaulted_GetRef();
	Take.Label = LOCTEXT("PickUp", "Aufheben");
	Take.OnSelected = [WeakPickup, WeakCrew]()
	{
		if (WeakPickup.IsValid() && WeakCrew.IsValid())
		{
			WeakCrew->PickUp(WeakPickup.Get());
		}
	};
	// Info is always the last entry, for every interactable.
	FDCRadialOption& Info = Options.AddDefaulted_GetRef();
	Info.Label = LOCTEXT("Info", "Info");
	Info.OnSelected = [WeakPanel, WeakPickup]()
	{
		if (WeakPanel.IsValid() && WeakPickup.IsValid())
		{
			WeakPanel->ShowFor(WeakPickup.Get());
		}
	};
	RadialMenu->Open(Pickup->GetInteractableName(), MoveTemp(Options));
}

void ADCPlayerController::Select(ADCCrewCharacter* Crew)
{
	if (Selected)
	{
		Selected->SetSelected(false);
	}
	Selected = Crew;
	if (Selected)
	{
		Selected->SetSelected(true);
	}
	if (InventoryPanel)
	{
		InventoryPanel->SetCrew(Selected);
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
	if (!Selected)
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
	const bool bReachable = Selected->MoveToLocation(Hit.ImpactPoint);
	if (Marker)
	{
		Marker->Show(Hit.ImpactPoint, bReachable);
	}
	UE_LOG(LogDerClou, Verbose, TEXT("%s -> %s (%s)"), *Selected->GetCrewName().ToString(), *Hit.ImpactPoint.ToString(), bReachable ? TEXT("ok") : TEXT("unreachable"));
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
			for (UDCItem* Item : Back->Contents)
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
			Expect(Back && Back->Contents.Num() == 2, TEXT("crowbar and laptop in the backpack"));
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

void ADCPlayerController::FinishSelfTest(bool bPassed)
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Inventory %s"), bPassed ? TEXT("PASSED") : TEXT("FAILED"));
	FPlatformMisc::RequestExit(false, TEXT("DCSelfTest"));
}

#undef LOCTEXT_NAMESPACE
