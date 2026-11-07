#pragma once

#include "CoreMinimal.h"
#include "DCDoor.h"
#include "DCItemTypes.h"
#include "DCSkills.h"
#include "GameFramework/Character.h"

#include "DCCrewCharacter.generated.h"

class ADCItemPickup;
class UDCInventoryComponent;
class UDCPlanRunnerComponent;
class UDCItem;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnContainerReached, ADCItemPickup*);

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
	/** Walks to a container in the world (desk, safe, ...); OnContainerReached fires on arrival. */
	void Search(ADCItemPickup* Pickup);
	FDCOnContainerReached OnContainerReached;
	/**
	 * Does something with an item on the crew member or in a container next to them. Opening and
	 * closing is instant; picking or breaking a lock takes a while. Source: the pickup the item
	 * belongs to, if it lies in the world. Returns why it is not possible (empty = done or started).
	 */
	FText PerformItemAction(UDCItem* Item, EDCItemAction Action, ADCItemPickup* Source = nullptr);
	/** The item being worked on right now (lock being picked), if any. */
	const UDCItem* GetWorkItem() const;

	const FDCSkills& GetSkills() const { return Skills; }
	/** Can this crew member do it (tool at hand, hands free, skill high enough) – and how long, how likely? */
	FDCTaskCheck CheckItemAction(const UDCItem* Item, EDCItemAction Action) const;
	FDCTaskCheck CheckDoorAction(const ADCDoor* Door, EDCDoorAction Action) const;
	/**
	 * For planning: what the action would take on an item of this kind, whatever state it is in
	 * now (the plan may change it first). Tools and hands are judged by what is carried now.
	 */
	FDCTaskCheck PlanCheckItem(const FDCItemDef& Def, EDCItemAction Action) const;
	FDCTaskCheck PlanCheckDoor(const ADCDoor* Door, EDCDoorAction Action) const { return CheckDoorAction(Door, Action); }

	/** Plays this crew member's part of the plan. */
	UDCPlanRunnerComponent* GetPlanRunner() const { return PlanRunner; }
	const FLinearColor& GetBodyColor() const { return BodyColor; }
	/** Short feedback under the name ("gibt auf"). */
	void Say(const FText& Text) { ShowNotice(Text); }

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

	/** What this crew member is good at (0..10). */
	UPROPERTY(EditAnywhere, Category = "Crew")
	FDCSkills Skills;

	/** Walking speed without load. */
	UPROPERTY(EditAnywhere, Category = "Crew")
	float BaseWalkSpeed = 250.f;

	UFUNCTION()
	TArray<FName> GetItemIdOptions() const;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UDCInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UDCPlanRunnerComponent> PlanRunner;

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

	/** What is carried, so one can see it: in front (large things, both hands), right/left hand, on the back. */
	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> CarriedFront;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> CarriedRight;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> CarriedLeft;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> CarriedBack;

private:
	enum class ETaskPhase : uint8
	{
		Idle,
		Walking,         // plain move order
		ToDoor,          // walking to the stand point in front of the door
		AtDoor,          // hand on the door
		WaitingForDoor,  // door swings open, then walk through
		ThroughDoor,     // walking to the other side
		ToItem,          // walking to an item on the floor or a container
		PickingUp,       // bending down for it
		ItemWork         // picking or breaking a lock
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
	void WalkToPickup(ADCItemPickup* Pickup, bool bSearch);
	void StartItemWork(UDCItem* Item, EDCItemAction Action, const FDCTaskCheck& Check);
	/** Tool at hand and enough free hands; empty = fine. */
	FText CheckToolAndHands(FName Tool, int32 Hands, const FText& What) const;
	/** Rolls against the current task's fail chance. */
	bool RollSuccess() const;
	void FinishItemWork();
	/** Noise at the crew member's feet. */
	void EmitNoise(float Range, const FText& What, bool bLabel = true);
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateCarriedLook();
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
	/** Walking to TaskPickup to search it rather than to pick it up. */
	bool bSearchTask = false;
	TWeakObjectPtr<UDCItem> TaskItem;
	EDCItemAction TaskItemAction = EDCItemAction::PickLock;
	float TaskWorkSeconds = 0.f;
	float TaskFailChance = 0.f;
	float TaskNoiseFactor = 1.f;
	float StepTimer = 0.f;
	float ClickTimer = 0.f;

	FTimerHandle SelfTestTimer;
	int32 SelfTestStep = 0;
	float SelfTestTime = 0.f;
	double SelfTestStartSide = 0.0;

	/** Short feedback such as "Tür ist verschlossen", shown for a moment instead of the activity. */
	FText Notice;
	float NoticeTime = 0.f;
};
