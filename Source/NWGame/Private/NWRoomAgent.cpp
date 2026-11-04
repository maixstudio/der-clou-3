#include "NWRoomAgent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (UMaterialInterface* Base = Mesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Mesh);
			Instance->SetVectorParameterValue(TEXT("Color"), Color);
			Mesh->SetMaterial(0, Instance);
		}
	}
}

ANWRoomAgent::ANWRoomAgent()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Body: 44 cm wide, 140 cm tall, standing on the floor.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	Body->SetRelativeScale3D(FVector(0.44, 0.44, 1.4));

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetStaticMesh(Sphere.Object);
	Head->SetRelativeLocation(FVector(0.0, 0.0, 162.0));
	Head->SetRelativeScale3D(FVector(0.34));

	// Shows where the figure is looking.
	Nose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Nose"));
	Nose->SetupAttachment(Root);
	Nose->SetStaticMesh(Cube.Object);
	Nose->SetRelativeLocation(FVector(20.0, 0.0, 160.0));
	Nose->SetRelativeScale3D(FVector(0.14, 0.1, 0.08));
	Nose->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SelectionRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionRing"));
	SelectionRing->SetupAttachment(Root);
	SelectionRing->SetStaticMesh(Cylinder.Object);
	SelectionRing->SetRelativeLocation(FVector(0.0, 0.0, 1.5));
	SelectionRing->SetRelativeScale3D(FVector(0.8, 0.8, 0.02));
	SelectionRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionRing->SetCastShadow(false);
	SelectionRing->SetVisibility(false);

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(Root);
	NameLabel->SetRelativeLocation(FVector(0.0, 0.0, 215.0));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetWorldSize(32.f);
	NameLabel->SetTextRenderColor(FColor::White);
	NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANWRoomAgent::Setup(int32 InAgentIndex, const FText& InName, const FLinearColor& InColor)
{
	AgentIndex = InAgentIndex;
	NameLabel->SetText(InName);
	Paint(Body, InColor);
	Paint(Head, FLinearColor(0.85f, 0.7f, 0.55f));
	Paint(Nose, FLinearColor(0.1f, 0.1f, 0.1f));
	Paint(SelectionRing, FLinearColor(1.f, 0.85f, 0.f));
}

void ANWRoomAgent::SetSelected(bool bSelected)
{
	SelectionRing->SetVisibility(bSelected);
}

void ANWRoomAgent::UpdatePose(const FVector& Location, float DeltaSeconds)
{
	const FVector Delta = Location - GetActorLocation();
	if (Delta.SizeSquared2D() > 1.0)
	{
		TargetYaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
	}
	const FRotator Current = GetActorRotation();
	const float Yaw = FMath::FixedTurn(static_cast<float>(Current.Yaw), TargetYaw, 720.f * DeltaSeconds);
	SetActorLocationAndRotation(Location, FRotator(0.0, Yaw, 0.0));

	// Keep the name readable for the top-down camera.
	NameLabel->SetWorldRotation(FRotator(60.0, 180.0, 0.0));
}
