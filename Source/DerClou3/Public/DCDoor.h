#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "GameFramework/Actor.h"

#include "DCDoor.generated.h"

class UDCNavBlockerComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** What a crew member can do with a door. */
UENUM(BlueprintType)
enum class EDCDoorAction : uint8
{
	Open,
	Close,
	GoThrough,
	PickLock
};

/** How the panel moves when the door opens. Only the look: rules and nav mesh are the same. */
UENUM(BlueprintType)
enum class EDCDoorMotion : uint8
{
	/** On hinges at the jamb (a double door has a second leaf at the other jamb). */
	Swing,
	/** Sideways along +Y, a little in front of the wall (lift-and-slide door, driveway gate). */
	Slide,
	/** Up and over: the panel tilts about its top edge until it lies under the lintel (garage door). */
	TiltUp
};

/**
 * A door that fits a 100 cm wall opening. The actor origin is the hinge at floor level; the
 * closed panel extends along +Y. While the door is closed an invisible blocker in the opening
 * removes it from the nav mesh; a locked door shows a red padlock on both sides.
 *
 * Looks: the panel and frame are modelled meshes from Art/Items (SM_<LeafModel>, SM_<FrameModel>_post/_top)
 * fitted to Width x Height; without them the door falls back to coloured boxes.
 */
UCLASS()
class DERCLOU3_API ADCDoor : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCDoor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsOpen() const { return bOpen; }
	bool IsLocked() const { return bLocked; }
	/** Goes up whenever any door really opens, closes or gets unlocked (for caching ways that depend on doors). */
	static uint32 GetStateVersion() { return StateVersion; }
	/** Opens the door so that the panel swings away from the opener. False if it is locked. */
	bool OpenAwayFrom(const FVector& OpenerLocation);
	void Close();
	void Unlock();
	/** 0 = closed, 1 = fully open (the panel is still swinging in between). */
	float GetOpenAmount() const { return OpenAmount; }

	/** Actions that make sense right now. */
	TArray<EDCDoorAction> GetActions() const;
	static FText DescribeAction(EDCDoorAction Action);

	/** Short state for the menu title: "Tür (zu)", "Tür (offen)", "Tür (verschlossen)". */
	FText DescribeState() const;

	/** How long picking this lock takes. */
	float GetPickLockSeconds() const { return PickLockSeconds; }
	/** Lock skill needed (0..10). */
	int32 GetLockDifficulty() const { return LockDifficulty; }

	//~ IDCInteractable
	virtual FText GetInteractableName() const override { return DisplayName; }
	virtual FText GetInteractableKind() const override;
	virtual void GetInfoLines(TArray<FDCInfoLine>& OutLines) const override;

	/** Middle of the opening at floor level. */
	FVector GetCenter() const;
	/** True if the straight line from A to B passes through the door opening (seen from above). */
	bool CrossesOpening(const FVector& A, const FVector& B) const;
	/**
	 * Where to stand to use the door: in front of it on the side of From, or (bOtherSide)
	 * behind it on the opposite side.
	 */
	FVector GetStandPoint(const FVector& From, bool bOtherSide) const;
	/**
	 * Where to stand to close the open door from the side of From. On the side the panel swung
	 * to, that is next to the free edge, outside the swing arc; on the other side it is the
	 * normal stand point.
	 */
	FVector GetCloseStandPoint(const FVector& From) const;
	/** Stand point on the side the panel did not swing to (always clear of the panel). */
	FVector GetClearSideStandPoint() const;
	/** +1 or -1: the way the panel swings when someone standing at Opener opens it (away from them). */
	float SwingSignFor(const FVector& Opener) const;

	/**
	 * Planning: shows the door as the plan leaves it at the moment the room shows - padlock, panel
	 * and a small sign above it ("aufgesperrt 0:04 · Vic"). Only the look: the real state, the nav
	 * mesh and all rules stay as they are. bShow false: back to the real look.
	 */
	void SetPlanLook(bool bShow, bool bShowLocked, bool bShowOpen, float ShowSwing, const FText& Label);
	bool IsPlanLookOn() const { return bPlanLook; }
	/** What the door shows right now (self-tests): padlock visible, panel open (0..1), sign text. */
	bool IsPadlockShown() const;
	float GetShownOpenAmount() const { return ShownAmount; }
	FText GetPlanLabel() const { return bPlanLook ? LookLabel : FText::GetEmpty(); }
	/** Where the plan's sign for this door stands: above the middle of the opening. */
	FVector GetSignLocation() const { return GetCenter() + FVector(0.0, 0.0, Height + 30.0); }

protected:
	UPROPERTY(EditAnywhere, Category = "Door")
	bool bOpen = false;

	/** Locked doors cannot be opened until the lock is picked. */
	UPROPERTY(EditAnywhere, Category = "Door")
	bool bLocked = false;

	/** Shown in the info panel. */
	UPROPERTY(EditAnywhere, Category = "Door|Info")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Door|Info")
	FText LockDescription;

	UPROPERTY(EditAnywhere, Category = "Door|Info", meta = (ClampMin = "0.5"))
	float PickLockSeconds = 4.f;

	/** Lock skill needed to pick it (0..10). */
	UPROPERTY(EditAnywhere, Category = "Door|Info", meta = (ClampMin = "0", ClampMax = "10"))
	int32 LockDifficulty = 3;

	UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = "60"))
	float Width = 100.f;

	UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = "150"))
	float Height = 210.f;

	/** Opening angle in degrees. The direction is chosen when the door is opened. */
	UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = "30", ClampMax = "120"))
	float OpenAngle = 90.f;

	/** Colour of the box panel when there is no model. */
	UPROPERTY(EditAnywhere, Category = "Door")
	FLinearColor PanelColor = FLinearColor(0.5f, 0.27f, 0.1f);

	/** Modelled panel SM_<LeafModel> (origin at the hinge, leaf along +Y). None or not imported: coloured box. */
	UPROPERTY(EditAnywhere, Category = "Door|Look")
	FName LeafModel = TEXT("Door_inner");

	/** Modelled frame SM_<FrameModel>_post and SM_<FrameModel>_top. None or not imported: coloured boxes. */
	UPROPERTY(EditAnywhere, Category = "Door|Look")
	FName FrameModel = TEXT("Door_frame");

	/** False: no frame at all (the opening has its own, e.g. the lift-and-slide door or a gate between posts). */
	UPROPERTY(EditAnywhere, Category = "Door|Look")
	bool bShowFrame = true;

	/** Two leaves hinged at both jambs that open the same way (only with Swing). */
	UPROPERTY(EditAnywhere, Category = "Door|Look")
	bool bDoubleLeaf = false;

	/** Turns the model's faces round (its outside, -X, faces +X instead). */
	UPROPERTY(EditAnywhere, Category = "Door|Look")
	bool bMirrorLeaf = false;

	UPROPERTY(EditAnywhere, Category = "Door|Look")
	EDCDoorMotion Motion = EDCDoorMotion::Swing;

	/** Depth of the frame across the wall (about the wall thickness + 2 cm, so the trim covers both faces). */
	UPROPERTY(EditAnywhere, Category = "Door|Look", meta = (ClampMin = "4"))
	float FrameDepth = 16.f;

	/** Slide: how far the panel first moves out of the wall line, towards -X (negative: towards +X). */
	UPROPERTY(EditAnywhere, Category = "Door|Look")
	float SlideOut = 10.f;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> Panel;

	/** Second leaf of a double door, hinged at the far jamb. */
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> Hinge2;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> Panel2;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> Handle;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> FrameLeft;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> FrameRight;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> FrameTop;

	/** Invisible box in the opening: removes the door from the nav mesh while it is closed. */
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UDCNavBlockerComponent> NavBlocker;

	/** Invisible box where the fully open panel stands: paths lead around it while the door is open. */
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UDCNavBlockerComponent> OpenPanelBlocker;

	/** Padlock on both faces of the panel (body + shackle), visible while locked. */
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TArray<TObjectPtr<UStaticMeshComponent>> LockParts;

private:
	void ApplyLayout();
	void UpdateState();
	/** Puts the panel(s) where they stand at Amount (0 closed .. 1 open), swinging to side -Sign. */
	void ApplyOpenPose(float Amount, float Sign);
	/** Width of one leaf: half the opening for a double door. */
	double GetLeafWidth() const;
	bool IsDoubleSwing() const { return bDoubleLeaf && Motion == EDCDoorMotion::Swing; }
	/** Height of the hinge above the floor (the top edge for TiltUp). */
	double GetHingeZ() const { return Motion == EDCDoorMotion::TiltUp ? Height : 0.0; }
	/** True while the panel and frame show models (no box colours, no separate handle). */
	bool bLeafModel = false;
	bool bFrameModel = false;
	/** +1 or -1 along the door's local X axis: the side the open panel stands on. */
	double GetSwingSide() const { return -SwingSign; }
	/** +1 or -1 along the door's local X axis: the side From is on. */
	double GetSideOf(const FVector& From) const;

	static uint32 StateVersion;

	/** 0 = closed, 1 = open; animated towards bOpen. */
	float OpenAmount = 0.f;
	/** +1 or -1: which way the panel swings (away from whoever opened it). */
	float SwingSign = 1.f;

	/** Padlock and sign as the real state or the planned look say. */
	void RefreshLook();
	bool bPlanLook = false;
	bool bLookLocked = false;
	bool bLookOpen = false;
	float LookSwing = 1.f;
	FText LookLabel;
	/** What the panel shows (0..1) and which way: the real swing or the planned one. */
	float ShownAmount = 0.f;
	float ShownSign = 1.f;
};
