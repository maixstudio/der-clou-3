#pragma once

#include "CoreMinimal.h"
#include "DCDoor.h"
#include "GameFramework/Character.h"

#include "DCCrewCharacter.generated.h"

class ADCItemPickup;
class UDCInventoryComponent;
class UDCItem;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A crew member: a real Unreal character, controlled by an AI controller and walking on the
 * nav mesh. Graybox look until there is a proper mesh: body, head, facing marker, name and a
 * selection ring.
 */
UCLASS()
class DERCLOU3_API ADCCrewCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADCCrewCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	void SetSelected(bool bInSelected);
	bool IsSelected() const { return bSelected; }

	/** Walks to the location (projected onto the nav mesh). Returns false if it is not reachable. */
	bool MoveToLocation(const FVector& Location);

	/** Walks to the door and uses it. Replaces whatever the crew member was doing. */
	void InteractWithDoor(ADCDoor* Door, EDCDoorAction Action);
	/** Like InteractWithDoor, but runs after the current and already queued door actions. */
	void QueueDoorAction(ADCDoor* Door, EDCDoorAction Action);

	/** Walks to the item and picks it up (if it can be carried). Replaces whatever the crew member was doing. */
	void PickUp(ADCItemPickup* Pickup);
	/** Puts the item down at the crew member's feet. */
	void DropItem(UDCItem* Item);

	UDCInventoryComponent* GetInventory() const { return Inventory; }
	bool IsBusy() const { return Phase != ETaskPhase::Idle; }
	/** Last short feedback ("Klavier ist zu schwer für eine Person"), even after it faded. */
	const FText& GetLastNotice() const { return Notice; }

	/** What the crew member is doing right now (shown under the name). */
	FText GetActivityText() const;

	const FText& GetCrewName() const { return CrewName; }

protected:
	UPROPERTY(EditAnywhere, Category = "Crew")
	FText CrewName;

	UPROPERTY(EditAnywhere, Category = "Crew")
	FLinearColor BodyColor = FLinearColor(1.f, 0.42f, 0.05f);

	/** What the crew member has on them when the game starts. */
	UPROPERTY(EditAnywhere, Category = "Crew", meta = (GetOptions = "GetItemIdOptions"))
	TArray<FName> StartItems = { TEXT("work_clothes"), TEXT("lockpicks"), TEXT("flashlight") };

	/** Walking speed without load. */
	UPROPERTY(EditAnywhere, Category = "Crew")
	float BaseWalkSpeed = 250.f;

	UFUNCTION()
	TArray<FName> GetItemIdOptions() const;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UDCInventoryComponent> Inventory;

	/** Self-test: on play start, pick the lock of the nearest door (if needed) and walk through. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugWalkThroughDoorOnStart = false;

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> Nose;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> SelectionRing;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UTextRenderComponent> NameLabel;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UTextRenderComponent> ActivityLabel;

private:
	enum class ETaskPhase : uint8
	{
		Idle,
		Walking,         // plain move order
		ToDoor,          // walking to the stand point in front of the door
		AtDoor,          // hand on the door
		WaitingForDoor,  // door swings open, then walk through
		ThroughDoor,     // walking to the other side
		ToItem,          // walking to an item on the floor
		PickingUp        // bending down for it
	};

	/** Starts an AI move; false if the target cannot be reached. OutGoal: the target on the nav mesh. */
	bool StartMove(const FVector& Location, FVector* OutGoal = nullptr);
	bool HasArrived() const;
	void SetPhase(ETaskPhase InPhase);
	void UpdateTask(float DeltaSeconds);
	void UpdateLabels(float DeltaSeconds);
	void StartDoorTask(ADCDoor* Door, EDCDoorAction Action);
	void StartDebugWalkThrough();
	ADCDoor* FindNearestDoor() const;
	void TickCloseFromSideTest();
	void ShowNotice(const FText& Text);
	void FinishPickUp();
	void OnInventoryChanged();

	bool bSelected = false;
	ETaskPhase Phase = ETaskPhase::Idle;
	TWeakObjectPtr<ADCDoor> TaskDoor;
	EDCDoorAction TaskAction = EDCDoorAction::Open;
	FVector PassTarget = FVector::ZeroVector;
	/** Where the crew member stands to use the door (on the nav mesh). */
	FVector DoorGoal = FVector::ZeroVector;
	float PhaseTime = 0.f;
	struct FQueuedDoorAction
	{
		TWeakObjectPtr<ADCDoor> Door;
		EDCDoorAction Action = EDCDoorAction::Open;
	};
	TArray<FQueuedDoorAction> DoorQueue;
	TWeakObjectPtr<ADCItemPickup> TaskPickup;

	FTimerHandle SelfTestTimer;
	int32 SelfTestStep = 0;
	float SelfTestTime = 0.f;
	double SelfTestStartSide = 0.0;

	/** Short feedback such as "Tür ist verschlossen", shown for a moment instead of the activity. */
	FText Notice;
	float NoticeTime = 0.f;
};
