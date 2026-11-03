#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "NWHeistObjectAsset.generated.h"

UENUM(BlueprintType)
enum class ENWLockType : uint8
{
	None,
	Simple,
	Security
};

UENUM(BlueprintType)
enum class ENWAlarmType : uint8
{
	Contact,
	LightBarrier
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWNodeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FName Label;

	/** Centimetres, relative to the object origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FIntPoint PositionCm = FIntPoint::ZeroValue;

	/** Nodes in the same room can see each other. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	int32 RoomId = 0;
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWEdgeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edge")
	int32 NodeA = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edge")
	int32 NodeB = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edge")
	int32 Door = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWDoorDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FName Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	ENWLockType Lock = ENWLockType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool bStartsLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	int32 Alarm = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0"))
	int32 OpenNoiseRadiusCm = 150;
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWAlarmDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm")
	FName Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm")
	ENWAlarmType Type = ENWAlarmType::Contact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm")
	int32 PanelNode = INDEX_NONE;

	/** Only for light barriers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm")
	int32 GuardedNode = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm", meta = (ClampMin = "1"))
	int32 BaseDisableSeconds = 90;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm", meta = (ClampMin = "1"))
	int32 PoliceResponseSeconds = 180;
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWSafeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safe")
	FName Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safe")
	int32 Node = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safe", meta = (ClampMin = "1"))
	int32 BaseCrackSeconds = 240;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safe", meta = (ClampMin = "0"))
	int32 LootValue = 0;
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWPatrolPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
	int32 Node = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol", meta = (ClampMin = "0"))
	float WaitMinSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol", meta = (ClampMin = "0"))
	float WaitMaxSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct NWPLANNING_API FNWGuardDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
	int32 StartNode = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
	TArray<FNWPatrolPoint> Route;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "10"))
	float SpeedCmPerSecond = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "0"))
	int32 SightRangeCm = 900;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "1"))
	int32 PoliceResponseSeconds = 120;
};

/**
 * A heist target as authored by designers: navigation graph, doors, alarms, safes and guards.
 * Converted to nw::sim::ObjectDefinition via NWSimConversion::ToSim.
 */
UCLASS(BlueprintType)
class NWPLANNING_API UNWHeistObjectAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graph")
	TArray<FNWNodeDef> Nodes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graph")
	TArray<FNWEdgeDef> Edges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Security")
	TArray<FNWDoorDef> Doors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Security")
	TArray<FNWAlarmDef> Alarms;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Security")
	TArray<FNWGuardDef> Guards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FNWSafeDef> Safes;

	/** Getaway car: crew starts and escapes here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	int32 EntryNode = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object", meta = (ClampMin = "60"))
	int32 TimeLimitSeconds = 1800;
};
