#include "DCDoor.h"

#include "Components/StaticMeshComponent.h"
#include "DCVisuals.h"

namespace
{
	constexpr float PanelThickness = 6.f;
	constexpr float FrameSize = 8.f;
	constexpr float SwingSpeed = 2.5f;

	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(DCVisuals::Cube());
		return Mesh;
	}
}

ADCDoor::ADCDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	Panel = MakeMesh(this, Hinge, TEXT("Panel"));
	Handle = MakeMesh(this, Panel, TEXT("Handle"));
	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FrameLeft = MakeMesh(this, Root, TEXT("FrameLeft"));
	FrameRight = MakeMesh(this, Root, TEXT("FrameRight"));
	FrameTop = MakeMesh(this, Root, TEXT("FrameTop"));
}

void ADCDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	OpenAmount = bOpen ? 1.f : 0.f;
	ApplyLayout();
	DCVisuals::Paint(Panel, PanelColor);
	DCVisuals::Paint(Handle, FLinearColor(0.85f, 0.7f, 0.25f));
	for (UStaticMeshComponent* Frame : { FrameLeft.Get(), FrameRight.Get(), FrameTop.Get() })
	{
		DCVisuals::Paint(Frame, FLinearColor(0.3f, 0.17f, 0.07f));
	}
}

void ADCDoor::ApplyLayout()
{
	// Cube mesh is 100 cm with its pivot in the centre.
	Panel->SetRelativeLocation(FVector(0.0, Width * 0.5, Height * 0.5));
	Panel->SetRelativeScale3D(FVector(PanelThickness, Width - 2.f, Height - 2.f) / 100.0);
	// The handle hangs on the scaled panel: location in cube units (-50..50), scale relative to it.
	// Result: 12 x 8 x 3 cm, near the free edge, at about 95 cm height.
	Handle->SetRelativeLocation(FVector(0.0, 38.0, -5.0));
	Handle->SetRelativeScale3D(FVector(12.0 / PanelThickness, 8.0 / (Width - 2.0), 3.0 / (Height - 2.0)));

	FrameLeft->SetRelativeLocation(FVector(0.0, -FrameSize * 0.5, (Height + FrameSize) * 0.5));
	FrameLeft->SetRelativeScale3D(FVector(FrameSize * 2.f, FrameSize, Height + FrameSize) / 100.0);
	FrameRight->SetRelativeLocation(FVector(0.0, Width + FrameSize * 0.5, (Height + FrameSize) * 0.5));
	FrameRight->SetRelativeScale3D(FVector(FrameSize * 2.f, FrameSize, Height + FrameSize) / 100.0);
	FrameTop->SetRelativeLocation(FVector(0.0, Width * 0.5, Height + FrameSize * 0.5));
	FrameTop->SetRelativeScale3D(FVector(FrameSize * 2.f, Width, FrameSize) / 100.0);

	Hinge->SetRelativeRotation(FRotator(0.0, SwingSign * OpenAngle * OpenAmount, 0.0));
}

void ADCDoor::OpenAwayFrom(const FVector& OpenerLocation)
{
	if (!bOpen)
	{
		// Rotating the closed panel (+Y) by +90 degrees points it along -X, by -90 along +X.
		// Swing to the side the opener is not on.
		const bool bOpenerOnPlusX = FVector::DotProduct(OpenerLocation - GetCenter(), GetActorForwardVector()) >= 0.0;
		SwingSign = bOpenerOnPlusX ? 1.f : -1.f;
	}
	bOpen = true;
}

void ADCDoor::Close()
{
	bOpen = false;
}

TArray<EDCDoorAction> ADCDoor::GetActions() const
{
	return { bOpen ? EDCDoorAction::Close : EDCDoorAction::Open, EDCDoorAction::GoThrough };
}

FText ADCDoor::DescribeAction(EDCDoorAction Action)
{
	switch (Action)
	{
	case EDCDoorAction::Open: return NSLOCTEXT("DCDoor", "Open", "Öffnen");
	case EDCDoorAction::Close: return NSLOCTEXT("DCDoor", "Close", "Schließen");
	case EDCDoorAction::GoThrough: return NSLOCTEXT("DCDoor", "GoThrough", "Durchgehen");
	}
	return FText::GetEmpty();
}

FVector ADCDoor::GetCenter() const
{
	return GetActorTransform().TransformPosition(FVector(0.0, Width * 0.5, 0.0));
}

FVector ADCDoor::GetStandPoint(const FVector& From, bool bOtherSide) const
{
	// The door's local X axis is the wall normal.
	const FVector Normal = GetActorForwardVector().GetSafeNormal2D();
	const FVector Center = GetCenter();
	const double Side = FVector::DotProduct(From - Center, Normal) >= 0.0 ? 1.0 : -1.0;
	return bOtherSide ? Center - Normal * Side * 90.0 : Center + Normal * Side * 70.0;
}

void ADCDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(OpenAmount, Target))
	{
		OpenAmount = FMath::FInterpConstantTo(OpenAmount, Target, DeltaSeconds, SwingSpeed);
		Hinge->SetRelativeRotation(FRotator(0.0, SwingSign * OpenAngle * OpenAmount, 0.0));
	}
}
