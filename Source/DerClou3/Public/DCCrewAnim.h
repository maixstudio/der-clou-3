#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "CoreMinimal.h"

#include "DCCrewAnim.generated.h"

class ADCCrewCharacter;
class UAnimSequence;
enum class EDCCrewAnimPose : uint8;

/** Everything the pose of one frame depends on; worked out on the game thread, evaluated on a worker. */
struct FDCCrewAnimState
{
	const UAnimSequence* Idle = nullptr;
	const UAnimSequence* Walk = nullptr;
	const UAnimSequence* Jog = nullptr;
	float IdleTime = 0.f;
	/** Walk and jog share one step cycle (0..1), so the feet match while they blend. */
	float Phase = 0.f;
	/** 0 = walk, 1 = jog. */
	float JogAlpha = 0.f;
	/** 0 = standing (idle), 1 = moving. */
	float MoveWeight = 0.f;
	/** Work (crouched at a lock or drawer) or picking up, laid over the rest. */
	const UAnimSequence* Action = nullptr;
	float ActionTime = 0.f;
	float ActionWeight = 0.f;
	bool bActionLoops = false;
};

/** Samples the sequences and blends them; no animation graph involved. */
struct FDCCrewAnimProxy : public FAnimInstanceProxy
{
	FDCCrewAnimProxy() = default;
	explicit FDCCrewAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FDCCrewAnimState State;
};

/**
 * Animation of a modelled crew member, driven from code instead of an Anim Blueprint: idle when standing,
 * walk and jog blended by speed (the step cycle follows the distance walked, so the feet do not slide), and
 * the crew member's action on top - crouched work at a lock or a container, bending down to pick something up.
 * The sequences come from DCVisuals::CrewModel for the owner's crew name.
 */
UCLASS(Transient, NotBlueprintable)
class DERCLOU3_API UDCCrewAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Walk and jog cycles are made for these speeds (cm/s; Blender: 1.4 and 2.5 m/s). */
	static constexpr float WalkSpeed = 140.f;
	static constexpr float JogSpeed = 250.f;
	/** Picking up takes this long on screen (the crew member's task is shorter; she stands up afterwards). */
	static constexpr float PickUpShowSeconds = 1.0f;

	const FDCCrewAnimState& GetState() const { return State; }

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	friend struct FDCCrewAnimProxy;

	void LoadSequences();

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Jog;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Work;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PickUp;

	FString LoadedFor;
	FDCCrewAnimState State;
	EDCCrewAnimPose LastPose{};
	/** Seconds into the pick-up, or below zero when none is playing. */
	float PickUpClock = -1.f;
};

/** -DCSelfTest=CrewLook (L_Room01): close-ups of Rosa standing, walking, working and picking up, then quits. */
namespace DCCrewLook
{
	void StartSelfTest(ADCCrewCharacter* Crew);
}
