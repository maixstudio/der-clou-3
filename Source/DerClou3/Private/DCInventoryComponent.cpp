#include "DCInventoryComponent.h"

#include "DCItem.h"

#define LOCTEXT_NAMESPACE "DCInventory"

namespace
{
	/** Load up to this mass does not slow down at all. */
	constexpr float FreeLoadKg = 15.f;
	/** Speed at full load (MaxCarryMassKg). */
	constexpr float FullLoadSpeed = 0.5f;
	/** Extra slowdown while both hands carry something large. */
	constexpr float BothHandsSpeed = 0.75f;
	constexpr int32 SlotCount = static_cast<int32>(EDCEquipSlot::RightHand) + 1;
}

UDCInventoryComponent::UDCInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Slots.SetNum(SlotCount);
}

UDCItem* UDCInventoryComponent::GetSlotItem(EDCEquipSlot Slot) const
{
	return Slots.IsValidIndex(SlotIndex(Slot)) ? Slots[SlotIndex(Slot)].Get() : nullptr;
}

TArray<UDCItem*> UDCInventoryComponent::GetContainers() const
{
	TArray<UDCItem*> Result;
	for (const EDCEquipSlot Slot : { EDCEquipSlot::Body, EDCEquipSlot::Vest, EDCEquipSlot::Back })
	{
		UDCItem* Item = GetSlotItem(Slot);
		if (Item && Item->IsContainer())
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<UDCItem*> UDCInventoryComponent::GetTopItems() const
{
	TArray<UDCItem*> Items;
	for (const TObjectPtr<UDCItem>& Item : Slots)
	{
		if (Item)
		{
			Items.AddUnique(Item);
		}
	}
	return Items;
}

bool UDCInventoryComponent::Owns(const UDCItem* Item) const
{
	if (!Item)
	{
		return false;
	}
	for (const UDCItem* Top : GetTopItems())
	{
		if (Item->IsInside(Top))
		{
			return true;
		}
	}
	return false;
}

bool UDCInventoryComponent::HasBothHandsFull() const
{
	const UDCItem* Left = GetSlotItem(EDCEquipSlot::LeftHand);
	return Left && Left == GetSlotItem(EDCEquipSlot::RightHand);
}

bool UDCInventoryComponent::HasTool(FName DefId) const
{
	for (const UDCItem* Top : GetTopItems())
	{
		if (Top->GetDefId() == DefId || Top->FindInside(DefId))
		{
			return true;
		}
	}
	return false;
}

FText UDCInventoryComponent::CheckCarry(const UDCItem* Item) const
{
	if (!Item)
	{
		return LOCTEXT("NoItem", "Kein Gegenstand");
	}
	if (Owns(Item))
	{
		return FText::GetEmpty();
	}
	const FDCItemDef& Def = Item->GetDef();
	if (!Def.bMovable)
	{
		return FText::Format(LOCTEXT("Fixed", "{0} ist fest eingebaut und lässt sich nicht bewegen"), Def.Name);
	}
	FText Reason = Item->CheckCanTakeOut();
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	if (Item->GetTotalMass() > MaxItemMassKg)
	{
		return FText::Format(LOCTEXT("TooHeavyAlone", "{0} ist zu schwer für eine Person ({1})"), Def.Name, DCItems::FormatKg(Item->GetTotalMass()));
	}
	if (Def.Size == EDCItemSize::Huge)
	{
		return FText::Format(LOCTEXT("TooBulkyAlone", "{0} ist zu sperrig für eine Person"), Def.Name);
	}
	if (GetTotalMass() + Item->GetTotalMass() > MaxCarryMassKg)
	{
		return FText::Format(LOCTEXT("Overloaded", "Zu viel Gewicht – mehr als {0} schafft niemand"), DCItems::FormatKg(MaxCarryMassKg));
	}
	return FText::GetEmpty();
}

FText UDCInventoryComponent::CheckSlot(const UDCItem* Item, EDCEquipSlot Slot) const
{
	FText Reason = CheckCarry(Item);
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	if (Slot == EDCEquipSlot::None)
	{
		return LOCTEXT("NoSlot", "Kein Platz");
	}
	// Taking something out of a container the crew member carries.
	Reason = Item->CheckCanTakeOut();
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	const FDCItemDef& Def = Item->GetDef();
	const UDCItem* Current = GetSlotItem(Slot);

	if (IsHand(Slot))
	{
		if (Def.Size == EDCItemSize::Large)
		{
			const UDCItem* Left = GetSlotItem(EDCEquipSlot::LeftHand);
			const UDCItem* Right = GetSlotItem(EDCEquipSlot::RightHand);
			if ((Left && Left != Item) || (Right && Right != Item))
			{
				return FText::Format(LOCTEXT("NeedBothHands", "Für {0} braucht es beide Hände frei"), Def.Name);
			}
			return FText::GetEmpty();
		}
		if (Current && Current != Item)
		{
			return HasBothHandsFull()
				? FText::Format(LOCTEXT("HandsFull", "Die Hände tragen schon {0}"), Current->GetDisplayName())
				: FText::Format(LOCTEXT("HandBusy", "{0} hält schon {1}"), DCItems::DescribeSlot(Slot), Current->GetDisplayName());
		}
		return FText::GetEmpty();
	}

	if (Def.WearSlot != Slot)
	{
		return FText::Format(LOCTEXT("WrongSlot", "{0} passt nicht an „{1}“"), Def.Name, DCItems::DescribeSlot(Slot));
	}
	if (Current && Current != Item)
	{
		return FText::Format(LOCTEXT("SlotBusy", "„{0}“ ist schon belegt ({1})"), DCItems::DescribeSlot(Slot), Current->GetDisplayName());
	}
	return FText::GetEmpty();
}

FText UDCInventoryComponent::CheckInto(const UDCItem* Item, const UDCItem* Container) const
{
	if (!Item || !Container)
	{
		return LOCTEXT("NoItem", "Kein Gegenstand");
	}
	if (Item->GetParent() == Container)
	{
		return FText::GetEmpty();
	}
	FText Reason = Item->CheckCanTakeOut();
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	Reason = Container->CheckCanHold(Item);
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	// From the world onto oneself: the usual carrying limits apply.
	if (Owns(Container) && !Owns(Item))
	{
		return CheckCarry(Item);
	}
	// From the world into the world (one drawer into another): only movable things.
	if (!Owns(Item) && !Item->GetDef().bMovable)
	{
		return FText::Format(LOCTEXT("Fixed", "{0} ist fest eingebaut und lässt sich nicht bewegen"), Item->GetDisplayName());
	}
	return FText::GetEmpty();
}

FText UDCInventoryComponent::CheckAdd(const UDCItem* Item) const
{
	FText Reason = CheckCarry(Item);
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	// Same order as AddAnywhere.
	const FDCItemDef& Def = Item->GetDef();
	if (Def.WearSlot != EDCEquipSlot::None && !GetSlotItem(Def.WearSlot))
	{
		return FText::GetEmpty();
	}
	for (const UDCItem* Container : GetContainers())
	{
		if (Container->CheckCanHold(Item).IsEmpty())
		{
			return FText::GetEmpty();
		}
	}
	if (Def.Size == EDCItemSize::Large)
	{
		return CheckSlot(Item, EDCEquipSlot::RightHand);
	}
	if (!GetSlotItem(EDCEquipSlot::RightHand) || !GetSlotItem(EDCEquipSlot::LeftHand))
	{
		return FText::GetEmpty();
	}
	return LOCTEXT("NoRoom", "Kein Platz mehr: Taschen voll, Hände belegt");
}

bool UDCInventoryComponent::MoveToSlot(UDCItem* Item, EDCEquipSlot Slot, FText& OutReason)
{
	OutReason = CheckSlot(Item, Slot);
	if (!OutReason.IsEmpty())
	{
		return false;
	}
	Detach(Item);
	if (IsHand(Slot) && Item->GetDef().Size == EDCItemSize::Large)
	{
		Slots[SlotIndex(EDCEquipSlot::LeftHand)] = Item;
		Slots[SlotIndex(EDCEquipSlot::RightHand)] = Item;
	}
	else
	{
		Slots[SlotIndex(Slot)] = Item;
	}
	Changed();
	return true;
}

bool UDCInventoryComponent::MoveInto(UDCItem* Item, UDCItem* Container, FText& OutReason)
{
	OutReason = CheckInto(Item, Container);
	if (!OutReason.IsEmpty())
	{
		return false;
	}
	if (Item->GetParent() == Container)
	{
		return true;
	}
	Detach(Item);
	Container->AddContent(Item);
	Changed();
	return true;
}

bool UDCInventoryComponent::AddAnywhere(UDCItem* Item, FText& OutReason)
{
	OutReason = CheckCarry(Item);
	if (!OutReason.IsEmpty())
	{
		return false;
	}
	const FDCItemDef& Def = Item->GetDef();
	FText Ignored;
	// Wear it if that slot is free.
	if (Def.WearSlot != EDCEquipSlot::None && !GetSlotItem(Def.WearSlot) && MoveToSlot(Item, Def.WearSlot, Ignored))
	{
		return true;
	}
	// Pockets, vest, backpack: the first one with room.
	for (UDCItem* Container : GetContainers())
	{
		if (Container->CheckCanHold(Item).IsEmpty())
		{
			return MoveInto(Item, Container, OutReason);
		}
	}
	// Otherwise carry it.
	if (Def.Size == EDCItemSize::Large)
	{
		return MoveToSlot(Item, EDCEquipSlot::RightHand, OutReason);
	}
	for (const EDCEquipSlot Hand : { EDCEquipSlot::RightHand, EDCEquipSlot::LeftHand })
	{
		if (!GetSlotItem(Hand))
		{
			return MoveToSlot(Item, Hand, OutReason);
		}
	}
	OutReason = LOCTEXT("NoRoom", "Kein Platz mehr: Taschen voll, Hände belegt");
	return false;
}

bool UDCInventoryComponent::Remove(UDCItem* Item)
{
	if (!Owns(Item))
	{
		return false;
	}
	Detach(Item);
	Changed();
	return true;
}

void UDCInventoryComponent::Detach(UDCItem* Item)
{
	for (TObjectPtr<UDCItem>& SlotItem : Slots)
	{
		if (SlotItem == Item)
		{
			SlotItem = nullptr;
		}
	}
	if (UDCItem* Parent = Item->GetParent())
	{
		Parent->RemoveContent(Item);
	}
}

float UDCInventoryComponent::GetTotalMass() const
{
	float Mass = 0.f;
	for (const UDCItem* Item : GetTopItems())
	{
		Mass += Item->GetTotalMass();
	}
	return Mass;
}

int32 UDCInventoryComponent::GetTotalValue() const
{
	int32 Value = 0;
	for (const UDCItem* Item : GetTopItems())
	{
		Value += Item->GetTotalValue();
	}
	return Value;
}

float UDCInventoryComponent::GetSpeedFactor() const
{
	const float Load = FMath::Clamp((GetTotalMass() - FreeLoadKg) / (MaxCarryMassKg - FreeLoadKg), 0.f, 1.f);
	float Factor = FMath::Lerp(1.f, FullLoadSpeed, Load);
	if (HasBothHandsFull())
	{
		Factor *= BothHandsSpeed;
	}
	return Factor;
}

void UDCInventoryComponent::Changed()
{
	OnChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
