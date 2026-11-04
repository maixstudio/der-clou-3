#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "NWRoomDirector.generated.h"

class ANWRoomAgent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UNWRoomSession;

/**
 * Room prototype: builds walls, doors and windows from a text layout, owns the "plan by doing"
 * session and shows its simulation (figures walk, doors swing, windows slide).
 *
 * Layout characters: '#' wall, '.' floor, ',' outdoor ground, 'D' door, 'L' locked door,
 * 'W' window, '1'..'9' start of a crew member. Each character is a 50 cm cell; the first row is
 * the far side of the room as seen by the default camera.
 */
UCLASS()
class NWGAME_API ANWRoomDirector : public AActor
{
	GENERATED_BODY()

public:
	ANWRoomDirector();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UNWRoomSession* GetSession() const { return Session; }

	/** Cell under a world position (on the room's grid), false if outside the layout. */
	bool WorldToCell(const FVector& WorldPosition, FIntPoint& OutCell) const;
	FVector CellToWorld(const FIntPoint& Cell, float Height = 0.f) const;
	/** Crew member index of a clicked actor, or INDEX_NONE. */
	int32 FindAgent(const AActor* Actor) const;
	/** World position of a crew member (for placing menus). */
	FVector GetAgentLocation(int32 Agent) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Room")
	TArray<FString> Layout;

	/** Names of the crew members '1', '2', ... */
	UPROPERTY(EditAnywhere, Category = "Room")
	TArray<FText> CrewNames;

	/** Kept low (and without shadows) so the top-down camera sees into the whole room. */
	UPROPERTY(EditAnywhere, Category = "Room", meta = (ClampMin = "230"))
	float WallHeight = 250.f;

	UPROPERTY(EditAnywhere, Category = "Room|Colors")
	FLinearColor FloorColor = FLinearColor(0.55f, 0.5f, 0.42f);

	UPROPERTY(EditAnywhere, Category = "Room|Colors")
	FLinearColor GroundColor = FLinearColor(0.28f, 0.33f, 0.27f);

	UPROPERTY(EditAnywhere, Category = "Room|Colors")
	FLinearColor WallColor = FLinearColor(0.82f, 0.82f, 0.8f);

	UPROPERTY(EditAnywhere, Category = "Room|Colors")
	FLinearColor DoorColor = FLinearColor(0.45f, 0.25f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "Room|Colors")
	FLinearColor GlassColor = FLinearColor(0.45f, 0.75f, 0.95f);

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UInstancedStaticMeshComponent> FloorTiles;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UInstancedStaticMeshComponent> GroundTiles;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UInstancedStaticMeshComponent> Walls;

	/** One instance per door. */
	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UInstancedStaticMeshComponent> DoorPanels;

	/** Red block on locked doors. */
	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UInstancedStaticMeshComponent> Locks;

	/** One instance per window. */
	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UInstancedStaticMeshComponent> WindowPanes;

private:
	struct FOpeningVisual
	{
		int32 Opening = INDEX_NONE;
		bool bDoor = true;
		int32 Instance = INDEX_NONE;
		int32 LockInstance = INDEX_NONE;
		/** Local hinge (doors) or pane centre (windows). */
		FVector Anchor = FVector::ZeroVector;
		float ClosedYaw = 0.f;
		float Length = 0.f;
		/** 0 = closed, 1 = open (animated). */
		float OpenAmount = 0.f;
	};

	void BuildGeometry();
	void UpdateOpenings(float DeltaSeconds, bool bSnap);
	void UpdateAgents(float DeltaSeconds);
	void SpawnAgents();
	FVector CellToLocal(int32 X, int32 Y, float Height) const;
	FVector SimToLocal(int32 PosX, int32 PosY) const;
	void ApplyColor(UInstancedStaticMeshComponent* Mesh, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UNWRoomSession> Session;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANWRoomAgent>> Agents;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	TArray<FOpeningVisual> OpeningVisuals;
	TArray<FVector> PreviousAgentLocations;
	TArray<FVector> CurrentAgentLocations;
	int32 LayoutHeight = 0;
	int32 LastSimTick = -1;
};
