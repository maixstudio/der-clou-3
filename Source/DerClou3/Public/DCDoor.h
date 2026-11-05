#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCDoor.generated.h"

class UStaticMeshComponent;

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
	void SetOpen(bool bInOpen);

protected:
	UPROPERTY(EditAnywhere, Category = "Door")
	bool bOpen = false;

	UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = "60"))
	float Width = 100.f;

	UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = "150"))
	float Height = 210.f;

	/** Opening angle; negative swings the other way. */
	UPROPERTY(EditAnywhere, Category = "Door")
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
};
