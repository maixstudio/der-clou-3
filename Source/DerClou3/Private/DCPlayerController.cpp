#include "DCPlayerController.h"

#include "DCClickMarker.h"
#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCInfoPanel.h"
#include "DCRadialMenu.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

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

	// Start with the first crew member selected.
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		Select(*It);
		break;
	}
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHoverCursor();

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
	const bool bOverDoor = GetHitResultUnderCursor(ECC_Visibility, false, Hit) && Cast<ADCDoor>(Hit.GetActor());
	CurrentMouseCursor = bOverDoor ? EMouseCursor::Hand : EMouseCursor::Default;
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
	Info.Label = NSLOCTEXT("DCPlayerController", "Info", "Info");
	Info.OnSelected = [WeakPanel, WeakDoor]()
	{
		if (WeakPanel.IsValid() && WeakDoor.IsValid())
		{
			WeakPanel->ShowFor(WeakDoor.Get());
		}
	};
	RadialMenu->Open(Door->DescribeState(), MoveTemp(Options));
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
	const bool bReachable = Selected->MoveToLocation(Hit.ImpactPoint);
	if (Marker)
	{
		Marker->Show(Hit.ImpactPoint, bReachable);
	}
	UE_LOG(LogDerClou, Verbose, TEXT("%s -> %s (%s)"), *Selected->GetCrewName().ToString(), *Hit.ImpactPoint.ToString(), bReachable ? TEXT("ok") : TEXT("unreachable"));
}
