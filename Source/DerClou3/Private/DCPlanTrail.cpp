#include "DCPlanTrail.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DCFloors.h"
#include "DCVisuals.h"
#include "DerClou3.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* const TrailFootprintPath = TEXT("/Game/DerClou/Items/Meshes/SM_Plan_footprint.SM_Plan_footprint");
	const TCHAR* const TrailRingPath = TEXT("/Game/DerClou/Items/Meshes/SM_Plan_ring.SM_Plan_ring");
	const TCHAR* const TrailMaterialPath = TEXT("/Game/DerClou/Items/Materials/M_PlanMark.M_PlanMark");
	/** The footprint model points its toes along +Y (Blender -Y); the walking direction is along +X of the yaw. */
	constexpr double TrailFootprintYawOffset = -90.0;
	/** Marks lie this far above the floor (no flicker with the floor boards). */
	constexpr double TrailLift = 1.0;
	/** The ring model's outer radius in cm. */
	constexpr double TrailRingModelRadius = 50.0;
}

ADCPlanTrail::ADCPlanTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
}

void ADCPlanTrail::Clear()
{
	Footprints.Reset();
	Rings.Reset();
}

FVector ADCPlanTrail::OnFloor(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	FHitResult Hit;
	// Only what is built in (floors, stairs): not the crew, not the doors.
	if (World && World->LineTraceSingleByObjectType(Hit, Location + FVector(0.0, 0.0, 60.0), Location - FVector(0.0, 0.0, 200.0),
		FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void ADCPlanTrail::AddFootprint(const FVector& Location, double Yaw, bool bLeft, const FLinearColor& Color, float Opacity, float Scale)
{
	FMark& Mark = Footprints.AddDefaulted_GetRef();
	const FVector Floor = OnFloor(Location);
	Mark.Transform = FTransform(FRotator(0.0, Yaw + TrailFootprintYawOffset, 0.0), Floor + FVector(0.0, 0.0, TrailLift),
		FVector(bLeft ? -Scale : Scale, Scale, 1.0));
	Mark.Color = Color;
	Mark.Opacity = Opacity;
	Mark.Storey = UDCFloorSubsystem::FloorOfHeight(Floor.Z + 90.0);
}

void ADCPlanTrail::AddRing(const FVector& Location, const FLinearColor& Color, float Opacity, float Radius)
{
	FMark& Mark = Rings.AddDefaulted_GetRef();
	const FVector Floor = OnFloor(Location);
	const double Scale = Radius / TrailRingModelRadius;
	Mark.Transform = FTransform(FRotator::ZeroRotator, Floor + FVector(0.0, 0.0, TrailLift + 0.3), FVector(Scale, Scale, 1.0));
	Mark.Color = Color;
	Mark.Opacity = Opacity;
	Mark.Storey = UDCFloorSubsystem::FloorOfHeight(Floor.Z + 90.0);
}

UInstancedStaticMeshComponent* ADCPlanTrail::GetLayer(bool bRing, int32 Storey)
{
	for (int32 Index = 0; Index < MarkLayerKeys.Num(); ++Index)
	{
		if (MarkLayerKeys[Index].Key == bRing && MarkLayerKeys[Index].Value == Storey && MarkLayers.IsValidIndex(Index) && MarkLayers[Index])
		{
			return MarkLayers[Index];
		}
	}
	if (!MarkMaterial)
	{
		FootprintMesh = LoadObject<UStaticMesh>(nullptr, TrailFootprintPath);
		RingMesh = LoadObject<UStaticMesh>(nullptr, TrailRingPath);
		MarkMaterial = LoadObject<UMaterialInterface>(nullptr, TrailMaterialPath);
		if (!FootprintMesh || !RingMesh || !MarkMaterial)
		{
			UE_LOG(LogDerClou, Warning, TEXT("Plan trail: footprint, ring or M_PlanMark missing - run Art/Items/ue_import_items.py (items=Plan_footprint,Plan_ring)"));
		}
	}
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(this);
	Layer->SetStaticMesh(bRing ? (RingMesh ? RingMesh.Get() : DCVisuals::Cylinder()) : (FootprintMesh ? FootprintMesh.Get() : DCVisuals::Cylinder()));
	if (MarkMaterial)
	{
		Layer->SetMaterial(0, MarkMaterial);
	}
	Layer->NumCustomDataFloats = 4;
	Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Layer->SetCanEverAffectNavigation(false);
	Layer->SetCastShadow(false);
	Layer->SetReceivesDecals(false);
	// Rings over footprints where they meet.
	Layer->SetTranslucentSortPriority(bRing ? 2 : 1);
	Layer->SetupAttachment(RootComponent);
	Layer->RegisterComponent();
	MarkLayers.Add(Layer);
	MarkLayerKeys.Emplace(bRing, Storey);
	ShownStorey = TNumericLimits<int32>::Max();
	return Layer;
}

void ADCPlanTrail::Fill(bool bRing, const TArray<FMark>& Marks)
{
	TMap<int32, TArray<const FMark*>> ByStorey;
	for (const FMark& Mark : Marks)
	{
		ByStorey.FindOrAdd(Mark.Storey).Add(&Mark);
	}
	// Empty the layers of this kind that get nothing now.
	for (int32 Index = 0; Index < MarkLayerKeys.Num(); ++Index)
	{
		if (MarkLayerKeys[Index].Key == bRing && !ByStorey.Contains(MarkLayerKeys[Index].Value) && MarkLayers[Index])
		{
			MarkLayers[Index]->ClearInstances();
		}
	}
	for (const TPair<int32, TArray<const FMark*>>& Entry : ByStorey)
	{
		UInstancedStaticMeshComponent* Layer = GetLayer(bRing, Entry.Key);
		Layer->ClearInstances();
		TArray<FTransform> Transforms;
		Transforms.Reserve(Entry.Value.Num());
		for (const FMark* Mark : Entry.Value)
		{
			Transforms.Add(Mark->Transform);
		}
		Layer->AddInstances(Transforms, false, true, false);
		for (int32 Index = 0; Index < Entry.Value.Num(); ++Index)
		{
			const FMark& Mark = *Entry.Value[Index];
			const float Data[4] = { Mark.Color.R, Mark.Color.G, Mark.Color.B, Mark.Opacity };
			Layer->SetCustomData(Index, MakeArrayView(Data, 4), false);
		}
		Layer->MarkRenderStateDirty();
	}
}

void ADCPlanTrail::Finish()
{
	Fill(false, Footprints);
	Fill(true, Rings);
}

void ADCPlanTrail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Storeys above the shown one are hidden, like the building itself.
	const UDCFloorSubsystem* Floors = UDCFloorSubsystem::Get(this);
	const int32 Shown = Floors && Floors->HasSeveralFloors() ? Floors->GetShownFloor() : TNumericLimits<int32>::Max();
	if (Shown == ShownStorey)
	{
		return;
	}
	ShownStorey = Shown;
	for (int32 Index = 0; Index < MarkLayerKeys.Num(); ++Index)
	{
		if (MarkLayers.IsValidIndex(Index) && MarkLayers[Index])
		{
			MarkLayers[Index]->SetVisibility(MarkLayerKeys[Index].Value <= Shown);
		}
	}
}
