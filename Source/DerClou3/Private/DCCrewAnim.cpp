#include "DCCrewAnim.h"

#include "AIController.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DCCrewCharacter.h"
#include "DCVisuals.h"
#include "DerClou3.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/MemStack.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	void SampleCrewPose(const UAnimSequence* Sequence, float Time, bool bLooping, FPoseContext& Out)
	{
		FAnimationPoseData Data(Out);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, {}, bLooping));
	}

	/** Into = Into blended towards Other by Alpha (bones only; the sequences carry no curves). */
	void BlendCrewPose(FPoseContext& Into, const FPoseContext& Other, float Alpha)
	{
		if (Alpha <= ZERO_ANIMWEIGHT_THRESH)
		{
			return;
		}
		for (const FCompactPoseBoneIndex Bone : Into.Pose.ForEachBoneIndex())
		{
			Into.Pose[Bone].BlendWith(Other.Pose[Bone], Alpha);
		}
	}

	float WrapTime(float Time, const UAnimSequence* Sequence)
	{
		const float Length = Sequence ? Sequence->GetPlayLength() : 0.f;
		return Length > KINDA_SMALL_NUMBER ? FMath::Fmod(Time, Length) : 0.f;
	}
}

// -------------------------------------------------------------------------------------------------
// Proxy (worker thread)
// -------------------------------------------------------------------------------------------------

void FDCCrewAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	State = CastChecked<UDCCrewAnimInstance>(InAnimInstance)->State;
}

bool FDCCrewAnimProxy::Evaluate(FPoseContext& Output)
{
	const FDCCrewAnimState& S = State;
	if (!S.Idle)
	{
		Output.ResetToRefPose();
		return true;
	}
	FMemMark Mark(FMemStack::Get());
	SampleCrewPose(S.Idle, WrapTime(S.IdleTime, S.Idle), true, Output);
	if (S.Walk && S.MoveWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		FPoseContext Moving(this);
		SampleCrewPose(S.Walk, S.Phase * S.Walk->GetPlayLength(), true, Moving);
		if (S.Jog && S.JogAlpha > ZERO_ANIMWEIGHT_THRESH)
		{
			FPoseContext Fast(this);
			SampleCrewPose(S.Jog, S.Phase * S.Jog->GetPlayLength(), true, Fast);
			BlendCrewPose(Moving, Fast, S.JogAlpha);
		}
		BlendCrewPose(Output, Moving, S.MoveWeight);
	}
	if (S.Action && S.ActionWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		FPoseContext Acting(this);
		SampleCrewPose(S.Action, S.ActionTime, S.bActionLoops, Acting);
		BlendCrewPose(Output, Acting, S.ActionWeight);
	}
	return true;
}

// -------------------------------------------------------------------------------------------------
// Anim instance (game thread)
// -------------------------------------------------------------------------------------------------

FAnimInstanceProxy* UDCCrewAnimInstance::CreateAnimInstanceProxy()
{
	return new FDCCrewAnimProxy(this);
}

void UDCCrewAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	LoadSequences();
}

void UDCCrewAnimInstance::LoadSequences()
{
	const ADCCrewCharacter* Crew = Cast<ADCCrewCharacter>(GetOwningActor());
	const FString Name = Crew ? Crew->GetCrewName().ToString() : FString();
	if (Name == LoadedFor)
	{
		return;
	}
	LoadedFor = Name;
	FDCCrewModel Model;
	DCVisuals::CrewModel(Name, Model);
	Idle = Model.Idle;
	Walk = Model.Walk ? Model.Walk : Model.Idle;
	Jog = Model.Jog ? Model.Jog : Walk.Get();
	Work = Model.Work;
	PickUp = Model.PickUp;
	State = FDCCrewAnimState();
	State.Idle = Idle;
	State.Walk = Walk;
	State.Jog = Jog;
}

void UDCCrewAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	LoadSequences();
	const ADCCrewCharacter* Crew = Cast<ADCCrewCharacter>(GetOwningActor());
	if (!Crew || !Idle)
	{
		return;
	}
	const float Dt = FMath::Max(DeltaSeconds, 0.f);
	const float Speed = static_cast<float>(Crew->GetVelocity().Size2D());
	State.IdleTime = WrapTime(State.IdleTime + Dt, Idle);

	// Moving: the cycle advances by the distance covered per stride, blended between walk and jog strides.
	State.MoveWeight = FMath::FInterpTo(State.MoveWeight, FMath::Clamp((Speed - 10.f) / 50.f, 0.f, 1.f), Dt, 10.f);
	const float Alpha = FMath::Clamp((Speed - WalkSpeed) / (JogSpeed - WalkSpeed), 0.f, 1.f);
	State.JogAlpha = FMath::FInterpTo(State.JogAlpha, Alpha, Dt, 6.f);
	const float Stride = FMath::Lerp(WalkSpeed * Walk->GetPlayLength(), JogSpeed * Jog->GetPlayLength(), State.JogAlpha);
	if (Stride > 1.f)
	{
		State.Phase = FMath::Fractional(State.Phase + Speed * Dt / Stride);
	}

	// Action on top: a pick-up plays through once from its start; work loops while it lasts.
	const EDCCrewAnimPose Pose = Crew->GetAnimPose();
	if (Pose == EDCCrewAnimPose::PickUp && LastPose != EDCCrewAnimPose::PickUp && PickUp)
	{
		PickUpClock = 0.f;
	}
	LastPose = Pose;
	float Target = 0.f;
	float BlendSeconds = 0.35f;
	if (PickUpClock >= 0.f)
	{
		PickUpClock += Dt;
		if (PickUpClock < PickUpShowSeconds)
		{
			State.Action = PickUp;
			State.bActionLoops = false;
			State.ActionTime = PickUpClock / PickUpShowSeconds * PickUp->GetPlayLength();
			Target = 1.f;
			BlendSeconds = 0.12f;
		}
		else
		{
			PickUpClock = -1.f;
		}
	}
	if (PickUpClock < 0.f && Pose == EDCCrewAnimPose::Work && Work)
	{
		if (State.Action != Work)
		{
			State.Action = Work;
			State.ActionTime = 0.f;
		}
		State.bActionLoops = true;
		State.ActionTime = WrapTime(State.ActionTime + Dt, Work);
		Target = 1.f;
	}
	State.ActionWeight = FMath::FInterpConstantTo(State.ActionWeight, Target, Dt, 1.f / BlendSeconds);
	if (State.ActionWeight <= 0.f)
	{
		State.Action = nullptr;
	}
}

// -------------------------------------------------------------------------------------------------
// Self-test -DCSelfTest=CrewLook
// -------------------------------------------------------------------------------------------------

namespace DCCrewLook
{
	namespace
	{
		struct FLookTest
		{
			TWeakObjectPtr<ADCCrewCharacter> Crew;
			TWeakObjectPtr<ACameraActor> Camera;
			FTimerHandle Timer;
			float Time = 0.f;
			int32 Step = 0;
			bool bPassed = true;
			/** Bone positions in the actor's frame, standing still. */
			FVector IdleHand = FVector::ZeroVector;
			FVector IdlePelvis = FVector::ZeroVector;
			FVector IdleHead = FVector::ZeroVector;
			FVector WalkStart = FVector::ZeroVector;
			float MaxSwing = 0.f;
		};

		void Expect(FLookTest& T, bool bCondition, const FString& What)
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
			T.bPassed &= bCondition;
		}

		FVector BoneInActor(const ADCCrewCharacter* Crew, FName Bone)
		{
			const USkeletalMeshComponent* Mesh = Crew->GetMesh();
			return Crew->GetActorTransform().InverseTransformPosition(Mesh->GetBoneLocation(Bone));
		}

		/**
		 * Camera Distance cm from the crew member at Yaw degrees from her facing, Height above her feet - or the
		 * nearest angle (then a shorter distance) from which nothing hides her knees, chest and head.
		 */
		void Frame(FLookTest& T, double Yaw, double Distance, double Height, double LookHeight = 95.0)
		{
			ADCCrewCharacter* Crew = T.Crew.Get();
			ACameraActor* Camera = T.Camera.Get();
			if (!Crew || !Camera)
			{
				return;
			}
			const FVector Feet = Crew->GetActorLocation() - FVector(0.0, 0.0, Crew->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector Target = Feet + FVector(0.0, 0.0, LookHeight);
			const auto EyeAt = [&](double Turn, double Scale)
			{
				const FVector Dir = FRotator(0.0, Crew->GetActorRotation().Yaw + Yaw + Turn, 0.0).Vector();
				return Feet + Dir * Distance * Scale + FVector(0.0, 0.0, Height);
			};
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CrewLookCamera), false, Crew);
			const auto Clear = [&](const FVector& Eye)
			{
				for (const double Z : { 40.0, LookHeight, 150.0 })
				{
					if (Crew->GetWorld()->LineTraceTestByChannel(Eye, Feet + FVector(0.0, 0.0, Z), ECC_Visibility, Params))
					{
						return false;
					}
				}
				return true;
			};
			FVector Eye = EyeAt(0.0, 1.0);
			bool bFound = false;
			for (const double Scale : { 1.0, 0.75, 0.55 })
			{
				for (const double Turn : { 0.0, 30.0, -30.0, 60.0, -60.0, 90.0, -90.0, 130.0, -130.0, 180.0 })
				{
					if (!bFound && Clear(EyeAt(Turn, Scale)))
					{
						Eye = EyeAt(Turn, Scale);
						bFound = true;
					}
				}
			}
			Camera->SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
		}

		void Shot(const TCHAR* Name)
		{
			const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name);
			FScreenshotRequest::RequestScreenshot(File, false, false);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), *File);
		}

		void Tick(const TSharedRef<FLookTest>& T)
		{
			ADCCrewCharacter* Crew = T->Crew.Get();
			UWorld* World = Crew ? Crew->GetWorld() : nullptr;
			if (!World)
			{
				return;
			}
			T->Time += 0.05f;
			const auto Finish = [&T, World, Crew]()
			{
				UE_LOG(LogDerClou, Display, TEXT("Self-test CrewLook %s"), T->bPassed ? TEXT("PASSED") : TEXT("FAILED"));
				World->GetTimerManager().ClearTimer(T->Timer);
				Crew->bTestAnimPose = false;
				FPlatformMisc::RequestExit(false, TEXT("DCSelfTest"));
			};
			// Steps run in order; each fires once when its time has come.
			struct FStep { float Time; TFunction<void()> Run; };
			const TArray<FStep> Steps = {
				{ 0.5f, [&]()
				{
					USkeletalMeshComponent* Mesh = Crew->GetMesh();
					const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
					Expect(*T, Asset != nullptr, TEXT("Rosa has her model"));
					Expect(*T, Mesh && Cast<UDCCrewAnimInstance>(Mesh->GetAnimInstance()) != nullptr, TEXT("animation is driven by UDCCrewAnimInstance"));
					if (Asset)
					{
						const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
						UE_LOG(LogDerClou, Display, TEXT("Self-test: %d bones, root %s, height %.1f cm"), Ref.GetNum(),
							*Ref.GetBoneName(0).ToString(), Asset->GetBounds().BoxExtent.Z * 2.0);
						Expect(*T, Ref.GetBoneName(0) == TEXT("root") && Ref.GetNum() == 53, TEXT("skeleton: root bone 'root', 53 bones"));
					}
					ACameraActor* Camera = World->SpawnActor<ACameraActor>(Crew->GetActorLocation(), FRotator::ZeroRotator);
					Camera->GetCameraComponent()->SetFieldOfView(38.f);
					Camera->GetCameraComponent()->bConstrainAspectRatio = false;
					T->Camera = Camera;
					if (APlayerController* Player = World->GetFirstPlayerController())
					{
						Player->SetViewTarget(Camera);
					}
					Frame(*T, -35.0, 430.0, 150.0, 88.0);
				} },
				{ 2.0f, [&]()
				{
					T->IdleHand = BoneInActor(Crew, TEXT("hand_r"));
					T->IdlePelvis = BoneInActor(Crew, TEXT("pelvis"));
					T->IdleHead = BoneInActor(Crew, TEXT("head"));
					UE_LOG(LogDerClou, Display, TEXT("Self-test: idle hand_r %s pelvis %s head %s"), *T->IdleHand.ToCompactString(),
						*T->IdlePelvis.ToCompactString(), *T->IdleHead.ToCompactString());
					Expect(*T, T->IdleHand.Z < T->IdlePelvis.Z + 20.0, TEXT("arms hang down when standing (no T-pose)"));
					Shot(TEXT("crew_idle.png"));
				} },
				{ 2.5f, [&]()
				{
					// Walk to a free spot a few metres away.
					T->WalkStart = Crew->GetActorLocation();
					bool bMoving = false;
					for (int32 Turn = 0; Turn < 8 && !bMoving; ++Turn)
					{
						const FVector Goal = T->WalkStart + FRotator(0.0, Crew->GetActorRotation().Yaw + 45.0 * Turn, 0.0).Vector() * 380.0;
						bMoving = Crew->MoveToLocation(Goal);
					}
					Expect(*T, bMoving, TEXT("found somewhere to walk to"));
				} },
				{ 3.3f, [&]()
				{
					Frame(*T, 90.0, 430.0, 130.0, 88.0);
					Shot(TEXT("crew_walk.png"));
				} },
				{ 3.35f, [&]()
				{
					const float Speed = static_cast<float>(Crew->GetVelocity().Size2D());
					UE_LOG(LogDerClou, Display, TEXT("Self-test: walking at %.0f cm/s, largest hand swing %.1f cm"), Speed, T->MaxSwing);
					Expect(*T, T->MaxSwing > 6.f, TEXT("arms swing while walking"));
				} },
				{ 5.5f, [&]()
				{
					if (AAIController* AI = Cast<AAIController>(Crew->GetController()))
					{
						AI->StopMovement();
					}
					Crew->TestAnimPose = EDCCrewAnimPose::Work;
					Crew->bTestAnimPose = true;
				} },
				{ 7.0f, [&]()
				{
					const FVector Pelvis = BoneInActor(Crew, TEXT("pelvis"));
					UE_LOG(LogDerClou, Display, TEXT("Self-test: work pelvis %s"), *Pelvis.ToCompactString());
					Expect(*T, Pelvis.Z < T->IdlePelvis.Z - 12.0, TEXT("crouches to work"));
					Frame(*T, -60.0, 360.0, 140.0, 65.0);
					Shot(TEXT("crew_work.png"));
				} },
				{ 7.6f, [&]()
				{
					Crew->TestAnimPose = EDCCrewAnimPose::PickUp;
				} },
				{ 8.05f, [&]()
				{
					const FVector Head = BoneInActor(Crew, TEXT("head"));
					UE_LOG(LogDerClou, Display, TEXT("Self-test: pick-up head %s"), *Head.ToCompactString());
					Expect(*T, Head.Z < T->IdleHead.Z - 30.0, TEXT("bends down to pick something up"));
					Frame(*T, 70.0, 380.0, 120.0, 55.0);
					Shot(TEXT("crew_pickup.png"));
				} },
				{ 9.0f, [&]()
				{
					Crew->bTestAnimPose = false;
				} },
				{ 10.0f, [&]()
				{
					const FVector Head = BoneInActor(Crew, TEXT("head"));
					Expect(*T, FMath::Abs(Head.Z - T->IdleHead.Z) < 6.0, TEXT("stands up again"));
					// Roughly the game camera: steep from above.
					Frame(*T, 20.0, 500.0, 900.0, 60.0);
					Shot(TEXT("crew_top.png"));
				} },
				{ 10.6f, [&]() { Finish(); } },
			};
			// Arm swing while walking (between the walk start and the measurement).
			if (T->Step >= 3 && T->Step <= 4)
			{
				const FVector Hand = BoneInActor(Crew, TEXT("hand_r"));
				T->MaxSwing = FMath::Max(T->MaxSwing, static_cast<float>(FMath::Abs(Hand.X - T->IdleHand.X)));
			}
			while (T->Step < Steps.Num() && T->Time >= Steps[T->Step].Time)
			{
				const int32 Index = T->Step++;
				Steps[Index].Run();
				if (!T->Crew.IsValid() || Index == Steps.Num() - 1)
				{
					return;
				}
			}
		}
	}

	void StartSelfTest(ADCCrewCharacter* Crew)
	{
		if (!Crew || !Crew->GetWorld())
		{
			return;
		}
		UE_LOG(LogDerClou, Display, TEXT("Self-test CrewLook started with %s"), *Crew->GetCrewName().ToString());
		const TSharedRef<FLookTest> Test = MakeShared<FLookTest>();
		Test->Crew = Crew;
		Crew->GetWorldTimerManager().SetTimer(Test->Timer, FTimerDelegate::CreateLambda([Test]() { Tick(Test); }), 0.05f, true, 1.0f);
	}
}
