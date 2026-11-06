#include "DCNoise.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCDoor.h"
#include "DCVisuals.h"
#include "DerClou3.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "DCNoise"

namespace
{
	constexpr int32 SegmentCount = 40;
	/** A wall lets this much of a noise's range through, a closed door a bit more. */
	constexpr float WallFactor = 0.35f;
	constexpr float ClosedDoorFactor = 0.5f;
	/** Height at which sound travels (for the wall check). */
	constexpr double EarHeight = 120.0;
}

UDCNoiseSubsystem* UDCNoiseSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDCNoiseSubsystem>() : nullptr;
}

FText UDCNoiseSubsystem::DescribeLoudness(float Range)
{
	if (Range < 200.f)
	{
		return LOCTEXT("Quiet", "leise");
	}
	if (Range < 600.f)
	{
		return LOCTEXT("Audible", "hörbar");
	}
	if (Range < 1200.f)
	{
		return LOCTEXT("Loud", "laut");
	}
	return LOCTEXT("VeryLoud", "sehr laut");
}

FLinearColor UDCNoiseSubsystem::ColorFor(float Range)
{
	if (Range < 200.f)
	{
		return FLinearColor(0.75f, 0.78f, 0.82f);
	}
	if (Range < 600.f)
	{
		return FLinearColor(1.f, 0.85f, 0.2f);
	}
	if (Range < 1200.f)
	{
		return FLinearColor(1.f, 0.45f, 0.05f);
	}
	return FLinearColor(1.f, 0.08f, 0.05f);
}

void UDCNoiseSubsystem::Emit(const FVector& Location, float Range, const FText& What, AActor* Source, bool bLabel)
{
	if (Range <= 0.f)
	{
		return;
	}
	FDCNoiseEvent Event;
	Event.Location = Location;
	Event.Range = Range;
	Event.What = What;
	Event.Source = Source;
	++NoiseCount;
	if (Range >= MentionRange)
	{
		LastLoud = Event;
		LastLoudTime = GetWorld()->GetTimeSeconds();
		UE_LOG(LogDerClou, Log, TEXT("Noise '%s' (%.0f cm) at %s"), *What.ToString(), Range, *Location.ToCompactString());
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADCNoiseRing* Ring = GetWorld()->SpawnActor<ADCNoiseRing>(Location + FVector(0.0, 0.0, 2.0), FRotator::ZeroRotator, Params))
	{
		// Noises the crew makes are named in the HUD and under the crew member already.
		const bool bShowLabel = bLabel && !(Source && Source->IsA<APawn>());
		const FText Text = bShowLabel ? FText::Format(LOCTEXT("RingLabel", "{0} ({1})"), What, DescribeLoudness(Range)) : FText::GetEmpty();
		Ring->Init(Range, ColorFor(Range), Text);
	}
	OnNoise.Broadcast(Event);
}

float UDCNoiseSubsystem::GetAttenuation(const FVector& From, const FVector& To) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	const FVector Start(From.X, From.Y, From.Z + EarHeight);
	const FVector End(To.X, To.Y, To.Z + EarHeight);
	GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, Objects);

	// Every wall and closed door in between (each counted once) swallows part of the sound.
	float Factor = 1.f;
	TSet<const AActor*> Counted;
	for (const FHitResult& Hit : Hits)
	{
		const AActor* Actor = Hit.GetActor();
		if (!Actor || Counted.Contains(Actor))
		{
			continue;
		}
		if (const ADCDoor* Door = Cast<ADCDoor>(Actor))
		{
			Factor *= Door->IsOpen() ? 1.f : ClosedDoorFactor;
			Counted.Add(Actor);
		}
		else if (Actor->IsA<AStaticMeshActor>())
		{
			Factor *= WallFactor;
			Counted.Add(Actor);
		}
	}
	return Factor;
}

void UDCNoiseSubsystem::RaiseAlarm(const FText& Text)
{
	if (!bAlarm)
	{
		bAlarm = true;
		AlarmText = Text;
		UE_LOG(LogDerClou, Warning, TEXT("ALARM: %s"), *Text.ToString());
	}
}

ADCNoiseRing::ADCNoiseRing()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Segment%d"), Index));
		Segment->SetupAttachment(Root);
		Segment->SetStaticMesh(DCVisuals::Cube());
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCanEverAffectNavigation(false);
		Segment->SetCastShadow(false);
		Segments.Add(Segment);
	}
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(26.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 170.0));
}

void ADCNoiseRing::Init(float InRange, const FLinearColor& Color, const FText& Text)
{
	Range = InRange;
	// Loud noises stay visible longer.
	Duration = FMath::GetMappedRangeValueClamped(FVector2f(150.f, 1500.f), FVector2f(0.7f, 2.2f), Range);
	for (UStaticMeshComponent* Segment : Segments)
	{
		DCVisuals::Paint(Segment, Color);
	}
	Label->SetText(Text);
	Label->SetTextRenderColor(Color.ToFColor(true));
	Label->SetVisibility(!Text.IsEmpty());
	UpdateRing();
}

void ADCNoiseRing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Duration)
	{
		Destroy();
		return;
	}
	UpdateRing();

	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager && Label->IsVisible())
	{
		const FQuat CameraRotation = Player->PlayerCameraManager->GetCameraRotation().Quaternion();
		Label->SetWorldRotation(FRotationMatrix::MakeFromXZ(-CameraRotation.GetForwardVector(), CameraRotation.GetUpVector()).Rotator());
	}
}

void ADCNoiseRing::UpdateRing()
{
	// Grows quickly to the full range, then thins out.
	const float T = FMath::Clamp(Age / Duration, 0.f, 1.f);
	const double Radius = FMath::Max(10.0, Range * FMath::InterpEaseOut(0.0, 1.0, FMath::Min(1.f, T / 0.6f), 2.0));
	const double Thickness = FMath::Lerp(10.0, 2.0, T);
	const double SegmentLength = 2.0 * UE_DOUBLE_PI * Radius / SegmentCount * 1.08;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * Index / SegmentCount;
		Segments[Index]->SetRelativeLocationAndRotation(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0),
			FRotator(0.0, FMath::RadiansToDegrees(Angle) + 90.0, 0.0));
		Segments[Index]->SetRelativeScale3D(FVector(SegmentLength, Thickness, 2.0) / 100.0);
	}
}

#undef LOCTEXT_NAMESPACE
