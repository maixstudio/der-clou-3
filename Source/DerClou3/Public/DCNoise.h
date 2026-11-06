#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"

#include "DCNoise.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** A sound in the world. Range: how far it carries in open space (cm); walls and closed doors cut it down. */
struct FDCNoiseEvent
{
	FVector Location = FVector::ZeroVector;
	float Range = 0.f;
	FText What;
	TWeakObjectPtr<AActor> Source;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnNoise, const FDCNoiseEvent&);

/**
 * Collects all noise: shows it as an expanding ring on the floor (grey = quiet … red = very loud)
 * and tells everyone listening (neighbours, later guards). Also holds the alarm state.
 */
UCLASS()
class DERCLOU3_API UDCNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDCNoiseSubsystem* Get(const UObject* WorldContext);

	/** Makes a noise at Location (on the floor). bLabel: show What next to the ring. */
	void Emit(const FVector& Location, float Range, const FText& What, AActor* Source, bool bLabel = true);

	/** Fraction of a noise's range that is left between two points (1 = nothing in between). */
	float GetAttenuation(const FVector& From, const FVector& To) const;

	/** "leise", "hörbar", "laut", "sehr laut". */
	static FText DescribeLoudness(float Range);
	static FLinearColor ColorFor(float Range);

	void RaiseAlarm(const FText& Text);
	bool IsAlarm() const { return bAlarm; }
	const FText& GetAlarmText() const { return AlarmText; }

	/** Last noise loud enough to mention in the HUD. */
	const FDCNoiseEvent& GetLastLoudNoise() const { return LastLoud; }
	double GetLastLoudNoiseTime() const { return LastLoudTime; }
	int32 GetNoiseCount() const { return NoiseCount; }

	FDCOnNoise OnNoise;

	/** From this range on a noise is named in the HUD. */
	static constexpr float MentionRange = 300.f;

private:
	FDCNoiseEvent LastLoud;
	double LastLoudTime = -100.0;
	int32 NoiseCount = 0;
	bool bAlarm = false;
	FText AlarmText;
};

/** The visible part of a noise: a ring that grows to the noise's range and disappears. */
UCLASS(NotPlaceable)
class DERCLOU3_API ADCNoiseRing : public AActor
{
	GENERATED_BODY()

public:
	ADCNoiseRing();

	void Init(float InRange, const FLinearColor& Color, const FText& Text);
	virtual void Tick(float DeltaSeconds) override;

private:
	void UpdateRing();

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	float Range = 100.f;
	float Duration = 1.f;
	float Age = 0.f;
};
