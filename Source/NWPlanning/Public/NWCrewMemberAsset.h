#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "NWCrewMemberAsset.generated.h"

/** A recruitable crew member. Skills 0..5; 0 means "cannot do this at all". */
UCLASS(BlueprintType)
class NWPLANNING_API UNWCrewMemberAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills", meta = (ClampMin = "0", ClampMax = "5"))
	int32 Lockpicking = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills", meta = (ClampMin = "0", ClampMax = "5"))
	int32 Electrics = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills", meta = (ClampMin = "0", ClampMax = "5"))
	int32 Safecracking = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = "0", ClampMax = "5"))
	int32 Nerves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10"))
	float SpeedCmPerSecond = 100.f;
};
