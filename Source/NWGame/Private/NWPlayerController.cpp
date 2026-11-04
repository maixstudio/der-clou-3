#include "NWPlayerController.h"

#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "NWPlanEditor.h"
#include "NWPlannerScreen.h"
#include "NWRoomDirector.h"
#include "NWRoomScreen.h"
#include "NWRoomSession.h"
#include "NWSimulationDirector.h"
#include "Sim/RoomSim.h"

#define LOCTEXT_NAMESPACE "NWPlayerController"

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
	for (TActorIterator<ANWRoomDirector> It(GetWorld()); It; ++It)
	{
		// The screen opens in PlayerTick once the director has created its session.
		RoomDirector = *It;
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
	if (RoomDirector)
	{
		if (!RoomScreen && RoomDirector->GetSession() && RoomDirector->GetSession()->IsValid())
		{
			OpenRoom(*RoomDirector);
		}
		HandleRoomInput();
		return;
	}
	HandlePlannerClick();
}

void ANWPlayerController::OpenRoom(ANWRoomDirector& InDirector)
{
	RoomDirector = &InDirector;
	RoomScreen = CreateWidget<UNWRoomScreen>(this, UNWRoomScreen::StaticClass());
	if (!RoomScreen)
	{
		return;
	}
	RoomScreen->SetSession(RoomDirector->GetSession());
	RoomScreen->AddToViewport();
	RoomScreen->ActivateWidget();
}

void ANWPlayerController::HandleRoomInput()
{
	UNWRoomSession* Session = RoomDirector->GetSession();
	if (!Session || !RoomScreen)
	{
		return;
	}

	if (WasInputKeyJustPressed(EKeys::BackSpace) || (WasInputKeyJustPressed(EKeys::Z) && (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))))
	{
		RoomScreen->CloseRadialMenu();
		Session->Undo();
		return;
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) || WasInputKeyJustPressed(EKeys::Escape))
	{
		RoomScreen->CloseRadialMenu();
		return;
	}
	// Clicks on the UI (buttons, radial menu) are consumed by Slate and never arrive here.
	if (!WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		return;
	}
	if (RoomScreen->IsRadialMenuOpen())
	{
		// Clicking next to the menu just closes it.
		RoomScreen->CloseRadialMenu();
		return;
	}

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}

	const int32 Agent = RoomDirector->FindAgent(Hit.GetActor());
	if (Agent != INDEX_NONE)
	{
		if (Agent == Session->GetSelectedAgent())
		{
			OpenAgentMenu(Agent);
		}
		else
		{
			Session->SelectAgent(Agent);
		}
		return;
	}

	FIntPoint Cell;
	// Step slightly into the surface so the point lies inside the clicked cell.
	if (!RoomDirector->WorldToCell(Hit.ImpactPoint - Hit.ImpactNormal * 2.0, Cell))
	{
		return;
	}
	const nw::room::Layout& Layout = Session->GetLayout();
	switch (Layout.TypeAt({ Cell.X, Cell.Y }))
	{
	case nw::room::CellType::Floor:
	case nw::room::CellType::Ground:
		Session->IssueMoveTo(Cell);
		break;
	case nw::room::CellType::Door:
	case nw::room::CellType::Window:
		OpenOpeningMenu(Layout.OpeningAt({ Cell.X, Cell.Y }));
		break;
	default:
		break;
	}
}

void ANWPlayerController::OpenOpeningMenu(int32 Opening)
{
	UNWRoomSession* Session = RoomDirector->GetSession();
	TWeakObjectPtr<UNWRoomSession> WeakSession = Session;
	TArray<FNWRadialOption> Options;
	for (const nw::room::CommandType Type : Session->GetOptions(Opening))
	{
		FNWRadialOption& Option = Options.AddDefaulted_GetRef();
		Option.Label = Session->DescribeOption(Type, Opening);
		Option.OnSelected = [WeakSession, Type, Opening]()
		{
			if (WeakSession.IsValid())
			{
				WeakSession->IssueOpeningCommand(Type, Opening);
			}
		};
	}
	RoomScreen->OpenRadialMenu(Session->DescribeOpening(Opening), MoveTemp(Options));
}

void ANWPlayerController::OpenAgentMenu(int32 Agent)
{
	UNWRoomSession* Session = RoomDirector->GetSession();
	TWeakObjectPtr<UNWRoomSession> WeakSession = Session;
	TArray<FNWRadialOption> Options;
	for (const float Seconds : { 3.f, 5.f, 10.f })
	{
		FNWRadialOption& Option = Options.AddDefaulted_GetRef();
		Option.Label = FText::Format(LOCTEXT("Wait", "Warten {0} s"), FText::AsNumber(FMath::RoundToInt(Seconds)));
		Option.OnSelected = [WeakSession, Seconds]()
		{
			if (WeakSession.IsValid())
			{
				WeakSession->IssueWait(Seconds);
			}
		};
	}
	RoomScreen->OpenRadialMenu(Session->GetAgentName(Agent), MoveTemp(Options));
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

#undef LOCTEXT_NAMESPACE
