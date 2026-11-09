#include "DCPlanGhost.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCVisuals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** Clickable (visibility traces), but nothing walks into it and the nav mesh never sees it. */
	UStaticMeshComponent* MakeGhostPart(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetGenerateOverlapEvents(false);
		return Part;
	}
}

ADCPlanGhost::ADCPlanGhost()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// A figure a bit smaller than the crew (they stand about 180 cm tall).
	Body = MakeGhostPart(this, TEXT("Body"), DCVisuals::Cylinder(), Root);
	Body->SetRelativeLocation(FVector(0.0, 0.0, 62.0));
	Body->SetRelativeScale3D(FVector(0.48, 0.48, 1.24));
	Head = MakeGhostPart(this, TEXT("Head"), DCVisuals::Sphere(), Root);
	Head->SetRelativeLocation(FVector(0.0, 0.0, 143.0));
	Head->SetRelativeScale3D(FVector(0.34, 0.34, 0.34));
	Ring = MakeGhostPart(this, TEXT("Ring"), DCVisuals::Cylinder(), Root);
	Ring->SetRelativeLocation(FVector(0.0, 0.0, 1.5));
	Ring->SetRelativeScale3D(FVector(0.95, 0.95, 0.02));
	// The pin variant: a thin pole with the disc at its foot.
	Pole = MakeGhostPart(this, TEXT("Pole"), DCVisuals::Cylinder(), Root);
	Pole->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	Pole->SetRelativeScale3D(FVector(0.05, 0.05, 1.4));
	Pole->SetVisibility(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 185.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCastShadow(false);
	// The sign over the same spot (UDCWorldBadges) says it already; the text stays for the self-tests (GetText).
	Label->SetVisibility(false);
}

void ADCPlanGhost::Show(FName InCrew, const FLinearColor& Color, const FVector& FloorLocation, const FText& InText, bool bStuck, bool bAsPin)
{
	Crew = InCrew;
	SetActorLocation(FloorLocation);
	// Dimmed crew colour (or red where the crew member gets stuck), a light ring underneath.
	const FLinearColor Shade = bStuck ? FLinearColor(0.75f, 0.1f, 0.08f) : Color * 0.45f;
	if (Shade != ShownColor || bStuck != bShownStuck)
	{
		DCVisuals::Paint(Body, Shade);
		DCVisuals::Paint(Head, Shade);
		DCVisuals::Paint(Pole, Shade);
		DCVisuals::Paint(Ring, bStuck ? FLinearColor(1.f, 0.35f, 0.3f) : FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.7f));
		Label->SetTextRenderColor(bStuck ? FColor(255, 120, 105) : FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.75f).ToFColor(true));
		ShownColor = Shade;
	}
	if (bAsPin != bShownAsPin || !Body->IsVisible() == !bAsPin)
	{
		Body->SetVisibility(!bAsPin);
		Head->SetVisibility(!bAsPin);
		Pole->SetVisibility(bAsPin);
		Ring->SetRelativeScale3D(bAsPin ? FVector(0.8, 0.8, 0.02) : FVector(0.95, 0.95, 0.02));
		bShownAsPin = bAsPin;
	}
	bShownStuck = bStuck;
	if (!Text.EqualTo(InText))
	{
		Text = InText;
		Label->SetText(DCVisuals::PlainText(Text));
	}
}

void ADCPlanGhost::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The text faces the camera, like the names over the crew.
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager)
	{
		const FQuat CameraRotation = Player->PlayerCameraManager->GetCameraRotation().Quaternion();
		Label->SetWorldRotation(FRotationMatrix::MakeFromXZ(-CameraRotation.GetForwardVector(), CameraRotation.GetUpVector()).Rotator());
	}
}
