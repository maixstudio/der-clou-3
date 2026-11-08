#include "DCMenuController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DCCrewCharacter.h"
#include "DCGameFlow.h"
#include "DCGameViewportClient.h"
#include "DCMenuWidget.h"
#include "DCNoiseListener.h"
#include "DCRoomVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

namespace
{
	/** Plan metres (x east, y north) to Unreal cm, as in build_villa.py. */
	FVector MenuVillaPoint(double X, double Y, double Z)
	{
		return FVector(Y * 100.0, X * 100.0, Z);
	}

	/** One sweep along the street there and back, in seconds. */
	constexpr double MenuDriftPeriod = 70.0;
}

ADCMenuPlayerController::ADCMenuPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	// The menu camera stays the view, whatever pawn the game mode hands over.
	bAutoManageActiveCameraTarget = false;
}

void ADCMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	// The whole window shows the scene (the planning frame narrows it).
	if (UDCGameViewportClient* Client = Cast<UDCGameViewportClient>(GetWorld()->GetGameViewport()))
	{
		Client->SetSceneRect(FVector2D::ZeroVector, FVector2D(1.0, 1.0));
	}

	// A quiet scene: no crew on the pavement, no names on the floor.
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(true);
	}
	for (TActorIterator<ADCNoiseListener> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(true);
	}
	for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(true);
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	MenuCamera = GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (MenuCamera)
	{
		MenuCamera->GetCameraComponent()->SetFieldOfView(62.f);
		MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		PlaceCamera();
		SetViewTarget(MenuCamera);
	}

	// On the very first map of a session (not the villa) the menu comes only after the next load.
	const UDCGameFlowSubsystem* Flow = UDCGameFlowSubsystem::Get(this);
	if (Flow && Flow->IsLeavingForMenu())
	{
		return;
	}
	Menu = CreateWidget<UDCMenuWidget>(this, UDCMenuWidget::StaticClass());
	if (Menu)
	{
		Menu->Setup(false);
		Menu->AddToViewport(50);
	}
}

void ADCMenuPlayerController::PlaceCamera()
{
	if (!MenuCamera)
	{
		return;
	}
	// Across the street, slowly along it: the garden gate, the lamps, the front of the house behind the trees.
	const double Phase = FMath::Sin(DriftTime * UE_DOUBLE_TWO_PI / MenuDriftPeriod);
	const double X = 31.0 + 4.5 * Phase;
	// A little above the street, looking down over the fence: garden, gate and lamps fill the picture, not the black sky.
	const FVector Eye = MenuVillaPoint(X, -11.0, 600.0 + 50.0 * FMath::Cos(DriftTime * UE_DOUBLE_TWO_PI / (MenuDriftPeriod * 0.7)));
	// The menu column covers the left: the house sits right of the middle.
	const FVector Look = MenuVillaPoint(X - 3.0, 14.0, 0.0);
	MenuCamera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
}

void ADCMenuPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	DriftTime += DeltaTime;
	PlaceCamera();
	if (MenuCamera && GetViewTarget() != MenuCamera)
	{
		SetViewTarget(MenuCamera);
	}
	if (Menu && WasInputKeyJustPressed(EKeys::Escape))
	{
		Menu->Back();
	}
}
