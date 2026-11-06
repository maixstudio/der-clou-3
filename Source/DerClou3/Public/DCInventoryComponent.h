#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "DCItemTypes.h"

#include "DCInventoryComponent.generated.h"

class UDCItem;

/**
 * What a crew member wears and carries: one item per body slot (head, clothes, vest, back) and
 * one per hand; a large item takes both hands. Worn containers (clothes pockets, vest, backpack)
 * hold items up to their volume and size class. Weight slows the crew member down.
 *
 * All Check* functions return an empty text if the move is allowed, otherwise the reason.
 */
UCLASS(ClassGroup = (DerClou), meta = (BlueprintSpawnableComponent))
class DERCLOU3_API UDCInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCInventoryComponent();

	/** Most one person can carry in total. */
	static constexpr float MaxCarryMassKg = 60.f;
	/** Heaviest single item one person can lift. */
	static constexpr float MaxItemMassKg = 40.f;

	UDCItem* GetSlotItem(EDCEquipSlot Slot) const;
	/** Worn containers in a fixed order: clothes, vest, backpack. */
	TArray<UDCItem*> GetContainers() const;
	/** True if the item is worn, held, or inside a worn container. */
	bool Owns(const UDCItem* Item) const;
	/** True if a large item fills both hands. */
	bool HasBothHandsFull() const;

	FText CheckCarry(const UDCItem* Item) const;
	FText CheckSlot(const UDCItem* Item, EDCEquipSlot Slot) const;
	FText CheckContainer(const UDCItem* Item, const UDCItem* Container) const;
	/** Would AddAnywhere succeed? */
	FText CheckAdd(const UDCItem* Item) const;

	/** Moves an item (owned or new) to a slot or into a worn container. False + reason if it does not fit. */
	bool MoveToSlot(UDCItem* Item, EDCEquipSlot Slot, FText& OutReason);
	bool MoveToContainer(UDCItem* Item, UDCItem* Container, FText& OutReason);
	/** Puts a new item where it fits best: wear it, then pockets/vest/backpack, then hands. */
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
	/** Detaches the item from wherever it is, without broadcasting. */
	bool Detach(UDCItem* Item);
	void Changed();

	/** Indexed by EDCEquipSlot (index 0 = None stays empty). A large item sits in both hands. */
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TArray<TObjectPtr<UDCItem>> Slots;
};
