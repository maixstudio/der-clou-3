#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "DCItemTypes.h"

#include "DCInventoryComponent.generated.h"

class UDCItem;

/**
 * What a crew member wears and carries: one item per body slot (head, clothes, vest, back) and
 * one per hand; a large item takes both hands. Containers can be nested (a jewellery box in the
 * backpack). Weight slows the crew member down.
 *
 * Items can also come from or go to containers in the world (a drawer, a safe): the caller makes
 * sure the crew member stands next to it. All Check* functions return an empty text if the move
 * is allowed, otherwise the reason.
 */
UCLASS(ClassGroup = (DerClou), meta = (BlueprintSpawnableComponent))
class DERCLOU3_API UDCInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCInventoryComponent();

	/** Strength 0..10 of whoever carries this: sets the carry limits. */
	void SetStrength(int32 InStrength) { Strength = InStrength; }
	/** Most this person can carry in total (60 kg at strength 5). */
	float GetMaxCarryMass() const { return 40.f + 4.f * Strength; }
	/** Heaviest single item this person can lift (40 kg at strength 5). */
	float GetMaxItemMass() const { return 25.f + 3.f * Strength; }

	/** Makes this a copy of another inventory (same items, cloned, in the same places). For planning. */
	void CopyFrom(const UDCInventoryComponent& Other);

	UDCItem* GetSlotItem(EDCEquipSlot Slot) const;
	/** The first item of this kind anywhere on the crew member (held, worn or packed). */
	UDCItem* FindItem(FName DefId) const;
	/** Where an owned item is: "rechte Hand", "beide Hände", "Kopf", "in Arbeitskleidung". */
	FText DescribeWhere(const UDCItem* Item) const;
	/** Worn containers in a fixed order: clothes, vest, backpack. */
	TArray<UDCItem*> GetContainers() const;
	/** True if the item is worn, held, or (at any depth) inside something worn or held. */
	bool Owns(const UDCItem* Item) const;
	/** True if a large item fills both hands. */
	bool HasBothHandsFull() const;
	/** True if the crew member has an item of this kind anywhere on them. */
	bool HasTool(FName DefId) const;
	/** The tool if it is at hand: held, or directly in clothes or vest pockets (not deep in the backpack). */
	UDCItem* FindReadyTool(FName DefId) const;
	/** Hands that are empty or hold the given tool (the tool's own hand counts as free for using it). */
	int32 CountFreeHands(FName ToolId = NAME_None) const;

	/** Can this person take the item at all (not fixed, not too big or heavy, reachable)? */
	FText CheckCarry(const UDCItem* Item) const;
	FText CheckSlot(const UDCItem* Item, EDCEquipSlot Slot) const;
	/** Into any container: one the crew member has on them, or one in the world next to them. */
	FText CheckInto(const UDCItem* Item, const UDCItem* Container) const;
	/** Would AddAnywhere succeed? */
	FText CheckAdd(const UDCItem* Item) const;

	/** Moves an item (owned, or from the world) to a slot. False + reason if it does not fit. */
	bool MoveToSlot(UDCItem* Item, EDCEquipSlot Slot, FText& OutReason);
	/** Moves an item (owned, or from the world) into a container (owned, or in the world). */
	bool MoveInto(UDCItem* Item, UDCItem* Container, FText& OutReason);
	/** Puts an item where it fits best: wear it, then pockets/vest/backpack, then hands. */
	bool AddAnywhere(UDCItem* Item, FText& OutReason);
	/** Takes the item out of the inventory (it keeps its own contents). */
	bool Remove(UDCItem* Item);

	float GetTotalMass() const;
	int32 GetTotalValue() const;
	/** 1 = full walking speed; less with heavy load or both hands full. */
	float GetSpeedFactor() const;

	/** Fires after every change. */
	FSimpleMulticastDelegate OnChanged;

private:
	static int32 SlotIndex(EDCEquipSlot Slot) { return static_cast<int32>(Slot); }
	static bool IsHand(EDCEquipSlot Slot) { return Slot == EDCEquipSlot::LeftHand || Slot == EDCEquipSlot::RightHand; }
	/** The top-level items, a large item in both hands counted once. */
	TArray<UDCItem*> GetTopItems() const;
	/** Detaches the item from wherever it is (slot or parent container), without broadcasting. */
	void Detach(UDCItem* Item);
	void Changed();

	/** Indexed by EDCEquipSlot (index 0 = None stays empty). A large item sits in both hands. */
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TArray<TObjectPtr<UDCItem>> Slots;

	int32 Strength = 5;
};
