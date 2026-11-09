#include "DCDoor.h"

#include "Components/StaticMeshComponent.h"
#include "DCNavBlockerComponent.h"
#include "DCVisuals.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
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

	/**
	 * Scales and moves the mesh so that its bounding box fills Target (in the parent's space). Works for
	 * the 100 cm cube and for models alike. bKeepThickness: X stays as modelled (scale 1, own position
	 * relative to the hinge line), e.g. a deep bookshelf door. MirrorX/MirrorY = -1 flips the mesh.
	 */
	void DoorFitToBox(UStaticMeshComponent* Mesh, const FBox& Target, bool bKeepThickness = false, double MirrorX = 1.0, double MirrorY = 1.0)
	{
		const UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
		const FBox Bounds = StaticMesh ? StaticMesh->GetBoundingBox() : FBox(FVector(-50.0), FVector(50.0));
		const FVector Size = Bounds.GetSize().ComponentMax(FVector(0.01));
		FVector Scale = Target.GetSize() / Size;
		if (bKeepThickness)
		{
			Scale.X = 1.0;
		}
		Scale.X *= MirrorX;
		Scale.Y *= MirrorY;
		FVector Location = Target.GetCenter() - Bounds.GetCenter() * Scale;
		if (bKeepThickness)
		{
			Location.X = 0.0;
		}
		Mesh->SetRelativeScale3D(Scale);
		Mesh->SetRelativeLocation(Location);
	}

	void DoorShowMesh(UStaticMeshComponent* Mesh, bool bShow)
	{
		Mesh->SetVisibility(bShow);
		Mesh->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

uint32 ADCDoor::StateVersion = 0;

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

	Hinge2 = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge2"));
	Hinge2->SetupAttachment(Root);
	Panel2 = MakeMesh(this, Hinge2, TEXT("Panel2"));
	Panel2->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

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

	// Models bring their own materials; only the box fallback is painted.
	if (!bLeafModel)
	{
		DCVisuals::Paint(Panel, PanelColor);
		DCVisuals::Paint(Panel2, PanelColor);
		DCVisuals::Paint(Handle, FLinearColor(0.85f, 0.7f, 0.25f));
	}
	if (!bFrameModel)
	{
		for (UStaticMeshComponent* Frame : { FrameLeft.Get(), FrameRight.Get(), FrameTop.Get() })
		{
			DCVisuals::Paint(Frame, FLinearColor(0.3f, 0.17f, 0.07f));
		}
	}
	for (int32 Index = 0; Index < LockParts.Num(); ++Index)
	{
		DCVisuals::Paint(LockParts[Index], Index % 2 == 0 ? LockBodyColor : LockShackleColor);
	}
}

double ADCDoor::GetLeafWidth() const
{
	return IsDoubleSwing() ? Width * 0.5 : Width;
}

void ADCDoor::ApplyLayout()
{
	const double LeafWidth = GetLeafWidth();
	const double HingeZ = GetHingeZ();

	// Leaves: in hinge space the first one runs along +Y from the near jamb, the second (double doors)
	// along -Y from the far jamb. A model keeps its own thickness; the box gets PanelThickness.
	UStaticMesh* LeafMesh = DCVisuals::ItemModel(LeafModel);
	bLeafModel = LeafMesh != nullptr;
	const double Far = LeafWidth - (IsDoubleSwing() ? 0.5 : 1.0);
	const double HalfThickness = PanelThickness * 0.5;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UStaticMeshComponent* Leaf = Index == 0 ? Panel.Get() : Panel2.Get();
		DoorShowMesh(Leaf, Index == 0 || IsDoubleSwing());
		Leaf->SetStaticMesh(bLeafModel ? LeafMesh : DCVisuals::Cube());
		if (bLeafModel)
		{
			Leaf->EmptyOverrideMaterials();
		}
		const FBox Target = Index == 0
			? FBox(FVector(-HalfThickness, 1.0, 1.0 - HingeZ), FVector(HalfThickness, Far, Height - 1.0 - HingeZ))
			: FBox(FVector(-HalfThickness, -Far, 1.0 - HingeZ), FVector(HalfThickness, -1.0, Height - 1.0 - HingeZ));
		DoorFitToBox(Leaf, Target, bLeafModel, bLeafModel && bMirrorLeaf ? -1.0 : 1.0, Index == 0 ? 1.0 : -1.0);
	}
	// The box panel gets a box handle; models have their own fittings.
	// The handle hangs on the scaled panel: location in cube units (-50..50), scale relative to it.
	// Result: 12 x 8 x 3 cm, near the free edge, at about 95 cm height.
	Handle->SetVisibility(!bLeafModel);
	Handle->SetRelativeLocation(FVector(0.0, 38.0, -5.0));
	Handle->SetRelativeScale3D(FVector(12.0 / PanelThickness, 8.0 / (LeafWidth - 2.0), 3.0 / (Height - 2.0)));

	// Frame: two posts and the lintel, as models (fixed profile, stretched along their length) or boxes.
	UStaticMesh* PostMesh = FrameModel.IsNone() ? nullptr : DCVisuals::ItemModel(FName(*(FrameModel.ToString() + TEXT("_post"))));
	UStaticMesh* TopMesh = FrameModel.IsNone() ? nullptr : DCVisuals::ItemModel(FName(*(FrameModel.ToString() + TEXT("_top"))));
	bFrameModel = PostMesh && TopMesh;
	for (UStaticMeshComponent* Frame : { FrameLeft.Get(), FrameRight.Get(), FrameTop.Get() })
	{
		DoorShowMesh(Frame, bShowFrame);
		Frame->SetStaticMesh(bFrameModel ? (Frame == FrameTop.Get() ? TopMesh : PostMesh) : DCVisuals::Cube());
		if (bFrameModel)
		{
			Frame->EmptyOverrideMaterials();
		}
	}
	const double F = FrameSize;
	const double D = FrameDepth * 0.5;
	DoorFitToBox(FrameLeft, FBox(FVector(-D, -F, 0.0), FVector(D, 0.0, Height + F)));
	DoorFitToBox(FrameRight, FBox(FVector(-D, Width, 0.0), FVector(D, Width + F, Height + F)));
	DoorFitToBox(FrameTop, FBox(FVector(-D, 0.0, Height), FVector(D, Width, Height + F)));

	// Padlock above the handle: red body 14 x 16 cm with a light shackle on top, on both faces.
	const double LockY = LeafWidth * 0.86;
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const double X = (Face == 0 ? -1.0 : 1.0) * (PanelThickness * 0.5 + 4.0);
		LockParts[Face * 2]->SetRelativeLocation(FVector(X, LockY, LockHeight - HingeZ));
		LockParts[Face * 2]->SetRelativeScale3D(FVector(8.0, 14.0, 16.0) / 100.0);
		LockParts[Face * 2 + 1]->SetRelativeLocation(FVector(X, LockY, LockHeight + 13.0 - HingeZ));
		LockParts[Face * 2 + 1]->SetRelativeScale3D(FVector(4.0, 10.0, 10.0) / 100.0);
	}

	// Blocker: fills the opening (and a bit of the wall depth) up to door height.
	NavBlocker->SetRelativeLocation(FVector(0.0, Width * 0.5, Height * 0.5));
	NavBlocker->SetBoxExtent(FVector(20.0, Width * 0.5, Height * 0.5));

	Hinge2->SetRelativeLocation(FVector(0.0, Width, 0.0));
	ApplyOpenPose(OpenAmount, SwingSign);
}

void ADCDoor::ApplyOpenPose(float Amount, float Sign)
{
	const double HingeZ = GetHingeZ();
	switch (Motion)
	{
	case EDCDoorMotion::Swing:
		// Both leaves end up on the same side: the second one turns the other way round.
		Hinge->SetRelativeLocationAndRotation(FVector(0.0, 0.0, HingeZ), FRotator(0.0, Sign * OpenAngle * Amount, 0.0));
		Hinge2->SetRelativeRotation(FRotator(0.0, -Sign * OpenAngle * Amount, 0.0));
		break;
	case EDCDoorMotion::Slide:
		// Out of the wall line first (SlideOut, towards -X), then along it past the jamb.
		Hinge->SetRelativeLocationAndRotation(FVector(-SlideOut * FMath::Min(Amount * 5.0, 1.0), Width * Amount, HingeZ), FRotator::ZeroRotator);
		break;
	case EDCDoorMotion::TiltUp:
		// About the top edge: pitch -90 lays the hanging panel towards -X, +90 towards +X (side -Sign).
		Hinge->SetRelativeLocationAndRotation(FVector(0.0, 0.0, HingeZ), FRotator(-Sign * 90.0 * Amount, 0.0, 0.0));
		break;
	}
}

void ADCDoor::RefreshLook()
{
	const bool bShowLock = bPlanLook ? bLookLocked : bLocked;
	for (UStaticMeshComponent* Part : LockParts)
	{
		Part->SetVisibility(bShowLock);
	}
}

bool ADCDoor::IsPadlockShown() const
{
	return !LockParts.IsEmpty() && LockParts[0]->IsVisible();
}

float ADCDoor::SwingSignFor(const FVector& Opener) const
{
	// Rotating the closed panel (+Y) by +90 degrees points it along -X, by -90 along +X.
	return FVector::DotProduct(Opener - GetCenter(), GetActorForwardVector()) >= 0.0 ? 1.f : -1.f;
}

void ADCDoor::SetPlanLook(bool bShow, bool bShowLocked, bool bShowOpen, float ShowSwing, const FText& Label)
{
	const float Swing = ShowSwing == 0.f ? SwingSign : ShowSwing;
	if (bPlanLook == bShow && (!bShow || (bLookLocked == bShowLocked && bLookOpen == bShowOpen && LookSwing == Swing && LookLabel.EqualTo(Label))))
	{
		return;
	}
	bPlanLook = bShow;
	bLookLocked = bShowLocked;
	bLookOpen = bShowOpen;
	LookSwing = Swing;
	LookLabel = bShow ? Label : FText::GetEmpty();
	RefreshLook();
}

void ADCDoor::UpdateState()
{
	RefreshLook();
	// The open panel's blocker sits where the panel ends up, not where it is right now, so that
	// the nav mesh is only rebuilt once per open/close and not on every frame of the swing.
	// It starts a bit away from the hinge: the nav system grows it by the agent radius, and in the
	// opening itself that would leave too narrow a gap to walk through. Near the hinge the wall
	// keeps the crew away from the panel anyway.
	// Short leaves (double doors) stay near the wall anyway; sliding and tilting panels never stand in the room.
	const FRotator OpenRotation(0.0, SwingSign * OpenAngle, 0.0);
	const double LeafWidth = GetLeafWidth();
	const double BlockedLength = LeafWidth - PanelBlockerHingeGap;
	const bool bPanelBlocks = Motion == EDCDoorMotion::Swing && !IsDoubleSwing() && BlockedLength > 10.0;
	OpenPanelBlocker->SetRelativeLocationAndRotation(OpenRotation.RotateVector(FVector(0.0, LeafWidth - BlockedLength * 0.5, Height * 0.5)), OpenRotation);
	OpenPanelBlocker->SetBoxExtent(FVector(PanelThickness * 0.5 + 2.0, FMath::Max(BlockedLength, 2.0) * 0.5, Height * 0.5));

	// Closed (or locked): the opening is not walkable. Open: the opening is free, the panel is not.
	NavBlocker->SetCanEverAffectNavigation(!bOpen);
	OpenPanelBlocker->SetCanEverAffectNavigation(bOpen && bPanelBlocks);
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
		// Swing to the side the opener is not on.
		SwingSign = SwingSignFor(OpenerLocation);
		bOpen = true;
		++StateVersion;
		UpdateState();
	}
	return true;
}

void ADCDoor::Close()
{
	if (bOpen)
	{
		bOpen = false;
		++StateVersion;
		UpdateState();
	}
}

void ADCDoor::Unlock()
{
	if (bLocked)
	{
		bLocked = false;
		++StateVersion;
		// Only the look: a door that stays shut blocks the way just as before, so the nav mesh is left alone
		// (every rebuild makes the engine check all paths that cross it).
		RefreshLook();
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
	// Only a single swinging leaf sweeps through the normal stand point (double leaves are half as long).
	if (!bOpen || Side != GetSwingSide() || Motion != EDCDoorMotion::Swing || IsDoubleSwing())
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
	}
	// The panel shows the real state - or, while planning, the state the plan leads to.
	const float Before = ShownAmount;
	const float Sign = bPlanLook ? LookSwing : SwingSign;
	ShownAmount = bPlanLook ? FMath::FInterpConstantTo(ShownAmount, bLookOpen ? 1.f : 0.f, DeltaSeconds, SwingSpeed) : OpenAmount;
	if (!FMath::IsNearlyEqual(Before, ShownAmount) || Sign != ShownSign)
	{
		ShownSign = Sign;
		ApplyOpenPose(ShownAmount, Sign);
	}
}

#undef LOCTEXT_NAMESPACE
