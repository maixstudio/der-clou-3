#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCDoor.generated.h"

class UStaticMeshComponent;

/** What a crew member can do with a door. */
UENUM(BlueprintType)
enum class EDCDoorAction : uint8
{
	Open,
	Close,
	GoThrough
};

/**
 * A door that fits a 100 cm wall opening. The actor origin is the hinge at floor level; the
 * closed panel extends along +Y. A closed panel blocks walking (it carves the nav mesh).
 */
UCLASS()
class DERCLOU3_API ADCDoor : public AActor
{
	GENERATED_BODY()

public:
	ADCDoor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsOpen() const { return bOpen; }
	/** Opens the door so that the panel swings away from the given location (the opener). */
	void OpenAwayFrom(const FVector& OpenerLocation);
	void Close();
	/** 0 = closed, 1 = fully open (the panel is still swinging in between). */
	float GetOpenAmount() const { return OpenAmount; }

	/** Actions that make sense right now. */
	TArray<EDCDoorAction> GetActions() const;
	static FText DescribeAction(EDCDoorAction Action);

	/** Middle of the opening at floor level. */
	FVector GetCenter() const;
	/**
	 * Where to stand to use the door: in front of it on the side of From, or (bOtherSide)
	 * behind it on the opposite side.
	 */
	FVector GetStandPoint(const FVector& From, bool bOtherSide) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Door")
	bool bOpen = false;

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

private:
	void ApplyLayout();

	/** 0 = closed, 1 = open; animated towards bOpen. */
	float OpenAmount = 0.f;
	/** +1 or -1: which way the panel swings (away from whoever opened it). */
	float SwingSign = 1.f;
};
