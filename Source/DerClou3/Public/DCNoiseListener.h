#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "GameFramework/Actor.h"

#include "DCNoiseListener.generated.h"

struct FDCNoiseEvent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Someone who hears what the crew does (a neighbour behind the wall; later guards and dogs).
 * Every noise that still reaches them through walls and doors raises their suspicion; it slowly
 * calms down again. At 100 % they call the police (alarm). Shown as a figure with a big symbol:
 * "zzz" calm, "?" suspicious, "!" about to call.
 */
UCLASS()
class DERCLOU3_API ADCNoiseListener : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCNoiseListener();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0..100. */
	float GetSuspicion() const { return Suspicion; }

	//~ IDCInteractable
	virtual FText GetInteractableName() const override { return DisplayName; }
	virtual FText GetInteractableKind() const override;
	virtual void GetInfoLines(TArray<FDCInfoLine>& OutLines) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Listener")
	FText DisplayName;

	/** What they are doing (info panel), e.g. "sieht fern", "schläft". */
	UPROPERTY(EditAnywhere, Category = "Listener")
	FText Activity;

	/** 1 = normal hearing; asleep about 0.5, a guard dog 2. */
	UPROPERTY(EditAnywhere, Category = "Listener", meta = (ClampMin = "0.1"))
	float Hearing = 1.f;

	/** Suspicion lost per second once it has been quiet for a while. */
	UPROPERTY(EditAnywhere, Category = "Listener")
	float CalmDownPerSecond = 2.f;

	UPROPERTY(VisibleAnywhere, Category = "Listener")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Listener")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Listener")
	TObjectPtr<UTextRenderComponent> Symbol;

	UPROPERTY(VisibleAnywhere, Category = "Listener")
	TObjectPtr<UTextRenderComponent> Label;

private:
	void OnNoise(const FDCNoiseEvent& Noise);
	void UpdateLabels();

	float Suspicion = 0.f;
	/** Seconds since the last noise they heard. */
	float SinceHeard = 100.f;
	FText LastHeard;
	bool bCalled = false;
	FDelegateHandle NoiseHandle;
};
