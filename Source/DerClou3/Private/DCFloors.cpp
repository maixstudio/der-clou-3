#include "DCFloors.h"

#include "DCCameraPawn.h"
#include "DCCrewCharacter.h"
#include "DCPlayerController.h"
#include "DCRoomVolume.h"
#include "DerClou3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "DCFloors"

namespace
{
	const FName FloorTags[] = { TEXT("DCFloor_KG"), TEXT("DCFloor_EG"), TEXT("DCFloor_OG") };
	/** Storey index of a tag position: -1, 0, 1. */
	constexpr int32 FloorTagBase = -1;
	/** Crew members are measured at their middle; the cellar starts this far below the ground floor. */
	constexpr double CellarBelow = -150.0;
	constexpr double UpperAbove = 280.0;
}

UDCFloorSubsystem* UDCFloorSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDCFloorSubsystem>() : nullptr;
}

bool UDCFloorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDCFloorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	CollectFloorActors();
}

TStatId UDCFloorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDCFloorSubsystem, STATGROUP_Tickables);
}

int32 UDCFloorSubsystem::FloorOfHeight(double Z)
{
	return Z < CellarBelow ? -1 : (Z < UpperAbove ? 0 : 1);
}

double UDCFloorSubsystem::HeightOfFloor(int32 Floor)
{
	return Floor < 0 ? -290.0 : (Floor > 0 ? 310.0 : 0.0);
}

FText UDCFloorSubsystem::FloorName(int32 Floor)
{
	if (Floor < 0)
	{
		return LOCTEXT("Cellar", "Keller");
	}
	return Floor > 0 ? LOCTEXT("Upper", "1. Stock") : LOCTEXT("Ground", "Erdgeschoss");
}

bool UDCFloorSubsystem::CanStep(int32 Delta) const
{
	const int32 Target = ShownFloor + Delta;
	return Target >= MinFloor && Target <= MaxFloor;
}

void UDCFloorSubsystem::CollectFloorActors()
{
	FloorActors.Reset();
	MinFloor = 0;
	MaxFloor = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FloorTags); ++Index)
		{
			if (It->ActorHasTag(FloorTags[Index]))
			{
				const int32 Floor = FloorTagBase + Index;
				FloorActors.Emplace(*It, Floor);
				MinFloor = FMath::Min(MinFloor, Floor);
				MaxFloor = FMath::Max(MaxFloor, Floor);
				break;
			}
		}
	}
	bCollected = true;
	if (HasSeveralFloors())
	{
		UE_LOG(LogDerClou, Display, TEXT("Floors: %d actors on storeys %d..%d"), FloorActors.Num(), MinFloor, MaxFloor);
	}
}

void UDCFloorSubsystem::ShowFloor(int32 Floor, bool bInManual)
{
	if (!HasSeveralFloors())
	{
		return;
	}
	Floor = FMath::Clamp(Floor, MinFloor, MaxFloor);
	bManual = bInManual;
	if (Floor == ShownFloor && bApplied)
	{
		return;
	}
	ShownFloor = Floor;
	Apply();
	UE_LOG(LogDerClou, Display, TEXT("Floors: showing %s%s"), *FloorName(ShownFloor).ToString(), bManual ? TEXT(" (chosen)") : TEXT(""));
}

void UDCFloorSubsystem::Apply()
{
	bApplied = true;
	HiddenActors.Reset();
	for (const TPair<TWeakObjectPtr<AActor>, int32>& Entry : FloorActors)
	{
		AActor* Actor = Entry.Key.Get();
		if (!Actor)
		{
			continue;
		}
		const bool bHide = Entry.Value > ShownFloor;
		Actor->SetActorHiddenInGame(bHide);
		if (bHide)
		{
			HiddenActors.Add(Actor);
		}
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ADCCameraPawn* Camera = Cast<ADCCameraPawn>(PC->GetPawn()))
		{
			Camera->SetFloorHeight(HeightOfFloor(ShownFloor));
		}
	}
	UpdateCrew();
}

bool UDCFloorSubsystem::IsHiddenByFloor(const AActor* Actor) const
{
	for (const TWeakObjectPtr<AActor>& Hidden : HiddenActors)
	{
		if (Hidden.Get() == Actor)
		{
			return true;
		}
	}
	for (const TWeakObjectPtr<AActor>& Hidden : HiddenCrew)
	{
		if (Hidden.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}

void UDCFloorSubsystem::UpdateCrew()
{
	TArray<TWeakObjectPtr<AActor>> NowHidden;
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Location = It->GetActorLocation();
		bool bHide = false;
		if (FloorOfHeight(Location.Z) > ShownFloor)
		{
			// Only inside the house: in the garden she stands beside the cut-away storeys, not on top of them.
			const ADCRoomVolume* Room = ADCRoomVolume::FindAt(this, Location);
			bHide = Room && !Room->IsOutdoors();
		}
		if (It->IsHidden() != bHide)
		{
			It->SetActorHiddenInGame(bHide);
		}
		if (bHide)
		{
			NowHidden.Add(*It);
		}
	}
	HiddenCrew = MoveTemp(NowHidden);
}

void UDCFloorSubsystem::Tick(float DeltaTime)
{
	if (!bCollected)
	{
		CollectFloorActors();
	}
	if (!HasSeveralFloors())
	{
		return;
	}
	ADCPlayerController* PC = Cast<ADCPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::PageUp))
	{
		StepFloor(1);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::PageDown))
	{
		StepFloor(-1);
	}

	// Follow the selected crew member: a new selection or her own change of storey ends a manual choice.
	ADCCrewCharacter* Selected = PC->GetSelectedCrew();
	if (Selected != Followed.Get())
	{
		Followed = Selected;
		FollowedFloor = Selected ? FloorOfHeight(Selected->GetActorLocation().Z) : ShownFloor;
		bManual = false;
	}
	else if (Selected)
	{
		const int32 Floor = FloorOfHeight(Selected->GetActorLocation().Z);
		if (Floor != FollowedFloor)
		{
			FollowedFloor = Floor;
			bManual = false;
		}
	}
	if (!bManual && Selected && FollowedFloor != ShownFloor)
	{
		ShowFloor(FollowedFloor, false);
	}
	else if (!bApplied)
	{
		// First tick: hide what lies above the start storey.
		Apply();
	}
	UpdateCrew();
}

bool UDCFloorSubsystem::HitUnderCursor(APlayerController* Controller, FHitResult& OutHit)
{
	if (!Controller)
	{
		return false;
	}
	const UDCFloorSubsystem* Floors = Get(Controller);
	if (!Floors || (Floors->HiddenActors.IsEmpty() && Floors->HiddenCrew.IsEmpty()))
	{
		return Controller->GetHitResultUnderCursor(ECC_Visibility, false, OutHit);
	}
	FVector Origin;
	FVector Direction;
	if (!Controller->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCFloorCursor), false);
	for (const TWeakObjectPtr<AActor>& Hidden : Floors->HiddenActors)
	{
		if (const AActor* Actor = Hidden.Get())
		{
			Params.AddIgnoredActor(Actor);
		}
	}
	for (const TWeakObjectPtr<AActor>& Hidden : Floors->HiddenCrew)
	{
		if (const AActor* Actor = Hidden.Get())
		{
			Params.AddIgnoredActor(Actor);
		}
	}
	return Controller->GetWorld()->LineTraceSingleByChannel(OutHit, Origin, Origin + Direction * 100000.0, ECC_Visibility, Params);
}

#undef LOCTEXT_NAMESPACE
