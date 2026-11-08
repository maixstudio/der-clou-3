#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCRoomVolume.generated.h"

class UBoxComponent;
class UTextRenderComponent;

/**
 * One room of the building as the floor plan shows it: its name and its floor area. The name
 * lies on the floor (readable from above), so even a room nobody has seen yet can be told apart
 * in the planning. Rooms are what the fog and "Raum durchsuchen" work with.
 */
UCLASS()
class DERCLOU3_API ADCRoomVolume : public AActor
{
	GENERATED_BODY()

public:
	ADCRoomVolume();

	virtual void OnConstruction(const FTransform& Transform) override;

	const FText& GetRoomName() const { return RoomName; }
	bool IsOutdoors() const { return bOutdoors; }
	/** True if the point lies inside the room (seen from above). */
	bool ContainsPoint(const FVector& Location) const;
	/** The room around this point (nullptr outside every room). */
	static ADCRoomVolume* FindAt(const UObject* WorldContext, const FVector& Location);

protected:
	/** As the floor plan names it: "Flur", "Büro". */
	UPROPERTY(EditAnywhere, Category = "Room")
	FText RoomName;

	/** Half the room's size in cm (X, Y); the actor sits in the middle of the floor. */
	UPROPERTY(EditAnywhere, Category = "Room")
	FVector2D HalfSize = FVector2D(200.0, 200.0);

	/** Outdoors (the garden): never in the fog. */
	UPROPERTY(EditAnywhere, Category = "Room")
	bool bOutdoors = false;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UBoxComponent> Area;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UTextRenderComponent> NameLabel;
};
