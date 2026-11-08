#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "GameFramework/Actor.h"

#include "DCDoor.generated.h"

class UDCNavBlockerComponent;
class UStaticMeshComponent;

/** What a crew member can do with a door. */
UENUM(BlueprintType)
enum class EDCDoorAction : uint8
{
	Open,
	Close,
	GoThrough,
	PickLock
};

/**
 * A door that fits a 100 cm wall opening. The actor origin is the hinge at floor level; the
 * closed panel extends along +Y. While the door is closed an invisible blocker in the opening
 * removes it from the nav mesh; a locked door shows a red padlock on both sides.
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

	UPROPERTY(EditAnywhere, Category = "Door")
	FLinearColor PanelColor = FLinearColor(0.5f, 0.27f, 0.1f);

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> Panel;

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
	/** +1 or -1 along the door's local X axis: the side the open panel stands on. */
	double GetSwingSide() const { return -SwingSign; }
	/** +1 or -1 along the door's local X axis: the side From is on. */
	double GetSideOf(const FVector& From) const;

	/** 0 = closed, 1 = open; animated towards bOpen. */
	float OpenAmount = 0.f;
	/** +1 or -1: which way the panel swings (away from whoever opened it). */
	float SwingSign = 1.f;
};
