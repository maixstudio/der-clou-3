#include "DCNoiseListener.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DCNoise.h"
#include "DCVisuals.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "DCNoiseListener"

namespace
{
	/** Suspicion from one noise heard right next to them, for a noise of full weight. */
	constexpr float MaxGainPerNoise = 60.f;
	/** Noises of this range count fully; quieter ones less, louder ones up to 1.5×. */
	constexpr float FullWeightRange = 800.f;
	/** Calming down starts after this long without a noise. */
	constexpr float CalmDelaySeconds = 5.f;
	constexpr float SuspiciousAt = 25.f;
	constexpr float AlarmedAt = 70.f;
}

ADCNoiseListener::ADCNoiseListener()
{
	PrimaryActorTick.bCanEverTick = true;
	DisplayName = LOCTEXT("DefaultName", "Nachbar");
	Activity = LOCTEXT("DefaultActivity", "sieht fern");

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(DCVisuals::Cylinder());
	Body->SetRelativeLocation(FVector(0.0, 0.0, 72.0));
	Body->SetRelativeScale3D(FVector(0.5, 0.5, 1.45));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCanEverAffectNavigation(false);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetStaticMesh(DCVisuals::Sphere());
	Head->SetRelativeLocation(FVector(0.0, 0.0, 160.0));
	Head->SetRelativeScale3D(FVector(0.36));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Symbol = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Symbol"));
	Symbol->SetupAttachment(Root);
	Symbol->SetRelativeLocation(FVector(0.0, 0.0, 250.0));
	Symbol->SetHorizontalAlignment(EHTA_Center);
	Symbol->SetVerticalAlignment(EVRTA_TextCenter);
	Symbol->SetWorldSize(70.f);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 200.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.f);
	Label->SetTextRenderColor(FColor::White);
}

void ADCNoiseListener::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DCVisuals::Paint(Body, FLinearColor(0.35f, 0.4f, 0.55f));
	DCVisuals::Paint(Head, FLinearColor(0.85f, 0.68f, 0.52f));
	UpdateLabels();
}

void ADCNoiseListener::BeginPlay()
{
	Super::BeginPlay();
	if (UDCNoiseSubsystem* Noise = UDCNoiseSubsystem::Get(this))
	{
		NoiseHandle = Noise->OnNoise.AddUObject(this, &ADCNoiseListener::OnNoise);
	}
	UpdateLabels();
}

void ADCNoiseListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDCNoiseSubsystem* Noise = UDCNoiseSubsystem::Get(this))
	{
		Noise->OnNoise.Remove(NoiseHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ADCNoiseListener::OnNoise(const FDCNoiseEvent& Noise)
{
	UDCNoiseSubsystem* Subsystem = UDCNoiseSubsystem::Get(this);
	if (!Subsystem || bCalled)
	{
		return;
	}
	// How far the sound carries towards them, after walls and doors.
	const float Reach = Noise.Range * Hearing * Subsystem->GetAttenuation(Noise.Location, GetActorLocation());
	const float Distance = FVector::Dist2D(Noise.Location, GetActorLocation());
	if (Distance >= Reach)
	{
		return;
	}
	const float Weight = FMath::Clamp(Noise.Range / FullWeightRange, 0.1f, 1.5f);
	const float Gain = MaxGainPerNoise * (1.f - Distance / Reach) * Weight;
	Suspicion = FMath::Min(100.f, Suspicion + Gain);
	SinceHeard = 0.f;
	LastHeard = Noise.What;
	UE_LOG(LogDerClou, Log, TEXT("%s hears '%s' (+%.0f, now %.0f %%)"), *GetName(), *Noise.What.ToString(), Gain, Suspicion);
	if (Suspicion >= 100.f)
	{
		bCalled = true;
		Subsystem->RaiseAlarm(FText::Format(LOCTEXT("Alarm", "{0} hat den Lärm gehört und ruft die Polizei!"), DisplayName));
	}
	UpdateLabels();
}

void ADCNoiseListener::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SinceHeard += DeltaSeconds;
	if (!bCalled && SinceHeard > CalmDelaySeconds && Suspicion > 0.f)
	{
		Suspicion = FMath::Max(0.f, Suspicion - CalmDownPerSecond * DeltaSeconds);
		UpdateLabels();
	}
	// Labels face the camera.
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Player && Player->PlayerCameraManager)
	{
		const FQuat CameraRotation = Player->PlayerCameraManager->GetCameraRotation().Quaternion();
		const FRotator Facing = FRotationMatrix::MakeFromXZ(-CameraRotation.GetForwardVector(), CameraRotation.GetUpVector()).Rotator();
		Symbol->SetWorldRotation(Facing);
		Label->SetWorldRotation(Facing);
	}
}

void ADCNoiseListener::UpdateLabels()
{
	FText Mark = LOCTEXT("Calm", "zzz");
	FColor Color(150, 160, 175);
	if (bCalled)
	{
		Mark = LOCTEXT("Calling", "!!");
		Color = FColor(255, 40, 30);
	}
	else if (Suspicion >= AlarmedAt)
	{
		Mark = LOCTEXT("Alarmed", "!");
		Color = FColor(255, 110, 20);
	}
	else if (Suspicion >= SuspiciousAt)
	{
		Mark = LOCTEXT("Suspicious", "?");
		Color = FColor(255, 215, 60);
	}
	Symbol->SetText(Mark);
	Symbol->SetTextRenderColor(Color);
	Label->SetText(FText::Format(LOCTEXT("Label", "{0} – Misstrauen {1} %"), DisplayName, FText::AsNumber(FMath::RoundToInt(Suspicion))));
}

FText ADCNoiseListener::GetInteractableKind() const
{
	return LOCTEXT("Kind", "Person (hört mit)");
}

void ADCNoiseListener::GetInfoLines(TArray<FDCInfoLine>& OutLines) const
{
	OutLines.Add({ LOCTEXT("LabelActivity", "Tut gerade"), bCalled ? LOCTEXT("ActivityCalling", "telefoniert mit der Polizei") : Activity });
	OutLines.Add({ LOCTEXT("LabelSuspicion", "Misstrauen"), FText::Format(LOCTEXT("Percent", "{0} %"), FText::AsNumber(FMath::RoundToInt(Suspicion))) });
	OutLines.Add({ LOCTEXT("LabelHearing", "Gehör"), Hearing >= 1.5f ? LOCTEXT("HearingSharp", "sehr gut") : Hearing >= 0.9f ? LOCTEXT("HearingNormal", "normal") : LOCTEXT("HearingWeak", "schwach") });
	OutLines.Add({ LOCTEXT("LabelHeard", "Zuletzt gehört"), LastHeard.IsEmpty() ? LOCTEXT("Nothing", "nichts") : LastHeard });
	OutLines.Add({ LOCTEXT("LabelHint", "Hinweis"), LOCTEXT("Hint", "Wände und geschlossene Türen dämpfen den Lärm. Bei 100 % wird die Polizei gerufen.") });
}

#undef LOCTEXT_NAMESPACE
