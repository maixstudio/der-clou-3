#include "DCDoor.h"

#include "Components/StaticMeshComponent.h"
#include "DCNavBlockerComponent.h"
#include "DCVisuals.h"
#include "AI/NavigationSystemBase.h"

#define LOCTEXT_NAMESPACE "DCDoor"

namespace
{
	constexpr float PanelThickness = 6.f;
	constexpr float FrameSize = 8.f;
	constexpr float SwingSpeed = 2.5f;
	constexpr float LockHeight = 125.f;
	/** Distance of the stand points from the door plane. */
	constexpr double StandDistance = 70.0;
	constexpr double PassDistance = 90.0;
	/** Closing from the swing side: stand this far beside the free edge and out from the wall. */
	constexpr double BesideEdge = 30.0;
	constexpr double OutFromWall = 55.0;
	/** Part of the open panel next to the hinge that is left out of the nav blocker. */
	constexpr double PanelBlockerHingeGap = 70.0;

	const FLinearColor LockBodyColor(0.85f, 0.08f, 0.05f);
	const FLinearColor LockShackleColor(0.8f, 0.8f, 0.82f);

	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, bool bCollision = true)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(DCVisuals::Cube());
		// Walking is controlled by the nav blocker, not by the (moving) meshes.
		Mesh->SetCanEverAffectNavigation(false);
		if (!bCollision)
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return Mesh;
	}
}

ADCDoor::ADCDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	DisplayName = LOCTEXT("DefaultName", "Tür");
	LockDescription = LOCTEXT("DefaultLock", "Einfaches Zylinderschloss");

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	Panel = MakeMesh(this, Hinge, TEXT("Panel"));
	// The crew never gets stuck on the swinging panel: paths avoid it (OpenPanelBlocker), and
	// where they still brush it, it does not push back.
	Panel->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Handle = MakeMesh(this, Panel, TEXT("Handle"), false);

	FrameLeft = MakeMesh(this, Root, TEXT("FrameLeft"));
	FrameRight = MakeMesh(this, Root, TEXT("FrameRight"));
	FrameTop = MakeMesh(this, Root, TEXT("FrameTop"));

	// Padlock: body and shackle on each face of the panel. Attached to the (unscaled) hinge.
	for (const TCHAR* Name : { TEXT("LockBodyFront"), TEXT("LockShackleFront"), TEXT("LockBodyBack"), TEXT("LockShackleBack") })
	{
		LockParts.Add(MakeMesh(this, Hinge, Name, false));
	}

	// Cuts the opening out of the nav mesh while the door is closed.
	NavBlocker = CreateDefaultSubobject<UDCNavBlockerComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(Root);

	// Keeps paths clear of the open panel (the nav system adds the agent radius around it).
	OpenPanelBlocker = CreateDefaultSubobject<UDCNavBlockerComponent>(TEXT("OpenPanelBlocker"));
	OpenPanelBlocker->SetupAttachment(Root);
	OpenPanelBlocker->SetCanEverAffectNavigation(false);
}

void ADCDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bLocked)
	{
		bOpen = false;
	}
	OpenAmount = bOpen ? 1.f : 0.f;
	ApplyLayout();
	UpdateState();

	DCVisuals::Paint(Panel, PanelColor);
	DCVisuals::Paint(Handle, FLinearColor(0.85f, 0.7f, 0.25f));
	for (UStaticMeshComponent* Frame : { FrameLeft.Get(), FrameRight.Get(), FrameTop.Get() })
	{
		DCVisuals::Paint(Frame, FLinearColor(0.3f, 0.17f, 0.07f));
	}
	for (int32 Index = 0; Index < LockParts.Num(); ++Index)
	{
		DCVisuals::Paint(LockParts[Index], Index % 2 == 0 ? LockBodyColor : LockShackleColor);
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

	// Padlock above the handle: red body 14 x 16 cm with a light shackle on top, on both faces.
	const double LockY = Width * 0.86;
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const double X = (Face == 0 ? -1.0 : 1.0) * (PanelThickness * 0.5 + 4.0);
		LockParts[Face * 2]->SetRelativeLocation(FVector(X, LockY, LockHeight));
		LockParts[Face * 2]->SetRelativeScale3D(FVector(8.0, 14.0, 16.0) / 100.0);
		LockParts[Face * 2 + 1]->SetRelativeLocation(FVector(X, LockY, LockHeight + 13.0));
		LockParts[Face * 2 + 1]->SetRelativeScale3D(FVector(4.0, 10.0, 10.0) / 100.0);
	}

	// Blocker: fills the opening (and a bit of the wall depth) up to door height.
	NavBlocker->SetRelativeLocation(FVector(0.0, Width * 0.5, Height * 0.5));
	NavBlocker->SetBoxExtent(FVector(20.0, Width * 0.5, Height * 0.5));

	Hinge->SetRelativeRotation(FRotator(0.0, SwingSign * OpenAngle * OpenAmount, 0.0));
}

void ADCDoor::UpdateState()
{
	for (UStaticMeshComponent* Part : LockParts)
	{
		Part->SetVisibility(bLocked);
	}
	// The open panel's blocker sits where the panel ends up, not where it is right now, so that
	// the nav mesh is only rebuilt once per open/close and not on every frame of the swing.
	// It starts a bit away from the hinge: the nav system grows it by the agent radius, and in the
	// opening itself that would leave too narrow a gap to walk through. Near the hinge the wall
	// keeps the crew away from the panel anyway.
	const FRotator OpenRotation(0.0, SwingSign * OpenAngle, 0.0);
	const double BlockedLength = Width - PanelBlockerHingeGap;
	OpenPanelBlocker->SetRelativeLocationAndRotation(OpenRotation.RotateVector(FVector(0.0, Width - BlockedLength * 0.5, Height * 0.5)), OpenRotation);
	OpenPanelBlocker->SetBoxExtent(FVector(PanelThickness * 0.5 + 2.0, BlockedLength * 0.5, Height * 0.5));

	// Closed (or locked): the opening is not walkable. Open: the opening is free, the panel is not.
	NavBlocker->SetCanEverAffectNavigation(!bOpen);
	OpenPanelBlocker->SetCanEverAffectNavigation(bOpen);
	for (UDCNavBlockerComponent* Blocker : { NavBlocker.Get(), OpenPanelBlocker.Get() })
	{
		if (Blocker->IsRegistered())
		{
			// Make sure the nav mesh around the door is rebuilt right away.
			FNavigationSystem::UpdateComponentData(*Blocker);
		}
	}
}

bool ADCDoor::OpenAwayFrom(const FVector& OpenerLocation)
{
	if (bLocked)
	{
		return false;
	}
	if (!bOpen)
	{
		// Rotating the closed panel (+Y) by +90 degrees points it along -X, by -90 along +X.
		// Swing to the side the opener is not on.
		const bool bOpenerOnPlusX = FVector::DotProduct(OpenerLocation - GetCenter(), GetActorForwardVector()) >= 0.0;
		SwingSign = bOpenerOnPlusX ? 1.f : -1.f;
		bOpen = true;
		UpdateState();
	}
	return true;
}

void ADCDoor::Close()
{
	if (bOpen)
	{
		bOpen = false;
		UpdateState();
	}
}

void ADCDoor::Unlock()
{
	if (bLocked)
	{
		bLocked = false;
		UpdateState();
	}
}

TArray<EDCDoorAction> ADCDoor::GetActions() const
{
	if (bLocked)
	{
		return { EDCDoorAction::PickLock };
	}
	return { bOpen ? EDCDoorAction::Close : EDCDoorAction::Open, EDCDoorAction::GoThrough };
}

FText ADCDoor::DescribeAction(EDCDoorAction Action)
{
	switch (Action)
	{
	case EDCDoorAction::Open: return LOCTEXT("Open", "Öffnen");
	case EDCDoorAction::Close: return LOCTEXT("Close", "Schließen");
	case EDCDoorAction::GoThrough: return LOCTEXT("GoThrough", "Durchgehen");
	case EDCDoorAction::PickLock: return LOCTEXT("PickLock", "Schloss knacken");
	}
	return FText::GetEmpty();
}

FText ADCDoor::DescribeState() const
{
	if (bLocked)
	{
		return LOCTEXT("StateLocked", "Tür (verschlossen)");
	}
	return bOpen ? LOCTEXT("StateOpen", "Tür (offen)") : LOCTEXT("StateClosed", "Tür (zu)");
}

FText ADCDoor::GetInteractableKind() const
{
	return LOCTEXT("Kind", "Tür");
}

void ADCDoor::GetInfoLines(TArray<FDCInfoLine>& OutLines) const
{
	const FText State = bLocked ? LOCTEXT("InfoLocked", "verschlossen")
		: (bOpen ? LOCTEXT("InfoOpen", "offen") : LOCTEXT("InfoClosed", "geschlossen"));
	OutLines.Add({ LOCTEXT("LabelState", "Zustand"), State });
	OutLines.Add({ LOCTEXT("LabelLock", "Schloss"), LockDescription });
	OutLines.Add({ LOCTEXT("LabelLocked", "Abgeschlossen"), bLocked ? LOCTEXT("Yes", "ja") : LOCTEXT("No", "nein") });
	if (bLocked)
	{
		OutLines.Add({ LOCTEXT("LabelPick", "Knacken"), FText::Format(LOCTEXT("PickInfo", "Schlösser {0} · ca. {1} s · Dietrich-Set, beide Hände"),
			FText::AsNumber(LockDifficulty), FText::AsNumber(FMath::RoundToInt(PickLockSeconds))) });
	}
	OutLines.Add({ LOCTEXT("LabelSize", "Größe"), FText::Format(LOCTEXT("Size", "{0} × {1} cm"), FText::AsNumber(FMath::RoundToInt(Width)), FText::AsNumber(FMath::RoundToInt(Height))) });
}

FVector ADCDoor::GetCenter() const
{
	return GetActorTransform().TransformPosition(FVector(0.0, Width * 0.5, 0.0));
}

bool ADCDoor::CrossesOpening(const FVector& A, const FVector& B) const
{
	// In the door's frame: the opening is X within the wall, Y from hinge to the other jamb.
	const FTransform& Frame = GetActorTransform();
	FVector LocalA = Frame.InverseTransformPosition(A);
	FVector LocalB = Frame.InverseTransformPosition(B);
	LocalA.Z = LocalB.Z = 0.0;
	const FBox Opening(FVector(-25.0, -5.0, -1.0), FVector(25.0, Width + 5.0, 1.0));
	if (Opening.IsInside(LocalA) || Opening.IsInside(LocalB))
	{
		return true;
	}
	return FMath::LineBoxIntersection(Opening, LocalA, LocalB, LocalB - LocalA);
}

double ADCDoor::GetSideOf(const FVector& From) const
{
	// The door's local X axis is the wall normal.
	return FVector::DotProduct(From - GetCenter(), GetActorForwardVector().GetSafeNormal2D()) >= 0.0 ? 1.0 : -1.0;
}

FVector ADCDoor::GetStandPoint(const FVector& From, bool bOtherSide) const
{
	const FVector Normal = GetActorForwardVector().GetSafeNormal2D();
	const double Side = GetSideOf(From);
	return bOtherSide ? GetCenter() - Normal * Side * PassDistance : GetCenter() + Normal * Side * StandDistance;
}

FVector ADCDoor::GetCloseStandPoint(const FVector& From) const
{
	const double Side = GetSideOf(From);
	if (!bOpen || Side != GetSwingSide())
	{
		return GetStandPoint(From, false);
	}
	// In front of the door the closing panel would sweep right through the crew member, so
	// stand beside the free edge, out of its arc.
	return GetActorTransform().TransformPosition(FVector(Side * OutFromWall, Width + BesideEdge, 0.0));
}

FVector ADCDoor::GetClearSideStandPoint() const
{
	return GetCenter() - GetActorForwardVector().GetSafeNormal2D() * GetSwingSide() * StandDistance;
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

#undef LOCTEXT_NAMESPACE
