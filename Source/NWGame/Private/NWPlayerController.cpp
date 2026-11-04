#include "NWPlayerController.h"

#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "NWPlanEditor.h"
#include "NWPlannerScreen.h"
#include "NWSimulationDirector.h"

ANWPlayerController::ANWPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ANWPlayerController::BeginPlay()
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
	for (TActorIterator<ANWSimulationDirector> It(GetWorld()); It; ++It)
	{
		if (It->StartsInPlanner() && It->GetPlanEditor())
		{
			OpenPlanner(**It);
			break;
		}
	}
}

void ANWPlayerController::OpenPlanner(ANWSimulationDirector& InDirector)
{
	Director = &InDirector;
	PlannerScreen = CreateWidget<UNWPlannerScreen>(this, UNWPlannerScreen::StaticClass());
	if (!PlannerScreen)
	{
		return;
	}
	PlannerScreen->SetPlanEditor(Director->GetPlanEditor());
	PlannerScreen->AddToViewport();
	PlannerScreen->ActivateWidget();
}

void ANWPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	HandlePlannerClick();
}

void ANWPlayerController::HandlePlannerClick()
{
	UNWPlanEditor* Editor = Director ? Director->GetPlanEditor() : nullptr;
	if (!Editor || !Editor->IsPicking())
	{
		return;
	}

	// Clicks on the planner UI are consumed by Slate and never reach the player input.
	if (WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		Editor->CancelPick();
		return;
	}
	if (!WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		return;
	}

	FVector Origin;
	FVector Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
	{
		return;
	}
	// Intersect with the ground plane the simulation lives on.
	const double GroundZ = Director->GetActorLocation().Z;
	const double Distance = (GroundZ - Origin.Z) / Direction.Z;
	if (Distance <= 0.0)
	{
		return;
	}
	Editor->PickAt(Director->WorldToSim(Origin + Direction * Distance));
}
