#include "DCPlayerController.h"

#include "DCClickMarker.h"
#include "DCCrewCharacter.h"
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
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandleClick();
	}
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
	const bool bReachable = Selected->MoveToLocation(Hit.ImpactPoint);
	if (Marker)
	{
		Marker->Show(Hit.ImpactPoint, bReachable);
	}
	UE_LOG(LogDerClou, Verbose, TEXT("%s -> %s (%s)"), *Selected->GetCrewName().ToString(), *Hit.ImpactPoint.ToString(), bReachable ? TEXT("ok") : TEXT("unreachable"));
}
