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

bool UDCInventoryComponent::Owns(const UDCItem* Item) const
{
	if (!Item)
	{
		return false;
	}
	if (Slots.Contains(Item))
	{
		return true;
	}
	for (const UDCItem* Container : GetContainers())
	{
		if (Container->Contents.Contains(Item))
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
	if (Def.Size == EDCItemSize::Huge || Item->GetTotalMass() > MaxItemMassKg)
	{
		return FText::Format(LOCTEXT("TooHeavyAlone", "{0} ist zu schwer für eine Person"), Def.Name);
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

FText UDCInventoryComponent::CheckContainer(const UDCItem* Item, const UDCItem* Container) const
{
	FText Reason = CheckCarry(Item);
	if (!Reason.IsEmpty())
	{
		return Reason;
	}
	if (!Container || !GetContainers().Contains(Container))
	{
		return LOCTEXT("NotWorn", "Dieser Behälter wird nicht getragen");
	}
	if (Container->Contents.Contains(Item))
	{
		return FText::GetEmpty();
	}
	const FDCItemDef& Def = Item->GetDef();
	const FDCItemDef& ContainerDef = Container->GetDef();
	if (Item == Container)
	{
		return FText::Format(LOCTEXT("IntoItself", "{0} passt nicht in sich selbst"), Def.Name);
	}
	if (Item->IsContainer() && !Item->Contents.IsEmpty())
	{
		return FText::Format(LOCTEXT("EmptyFirst", "{0} erst ausleeren"), Def.Name);
	}
	if (Def.Size > ContainerDef.MaxContentSize)
	{
		return FText::Format(LOCTEXT("TooBig", "{0} ist zu groß für {1} (passt nur bis „{2}“)"),
			Def.Name, ContainerDef.Name, DCItems::DescribeSize(ContainerDef.MaxContentSize));
	}
	if (Container->GetUsedVolume() + Def.VolumeLiters > ContainerDef.CapacityLiters + KINDA_SMALL_NUMBER)
	{
		return FText::Format(LOCTEXT("TooFull", "{0} ist zu voll ({1} von {2} belegt, {3} braucht {4})"),
			ContainerDef.Name, DCItems::FormatLiters(Container->GetUsedVolume()), DCItems::FormatLiters(ContainerDef.CapacityLiters),
			Def.Name, DCItems::FormatLiters(Def.VolumeLiters));
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
		if (CheckContainer(Item, Container).IsEmpty())
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

bool UDCInventoryComponent::MoveToContainer(UDCItem* Item, UDCItem* Container, FText& OutReason)
{
	OutReason = CheckContainer(Item, Container);
	if (!OutReason.IsEmpty())
	{
		return false;
	}
	if (Container->Contents.Contains(Item))
	{
		return true;
	}
	Detach(Item);
	Container->Contents.Add(Item);
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
		if (CheckContainer(Item, Container).IsEmpty())
		{
			return MoveToContainer(Item, Container, OutReason);
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
	if (!Detach(Item))
	{
		return false;
	}
	Changed();
	return true;
}

bool UDCInventoryComponent::Detach(UDCItem* Item)
{
	bool bFound = false;
	for (TObjectPtr<UDCItem>& SlotItem : Slots)
	{
		if (SlotItem == Item)
		{
			SlotItem = nullptr;
			bFound = true;
		}
	}
	for (UDCItem* Container : GetContainers())
	{
		bFound |= Container->Contents.Remove(Item) > 0;
	}
	return bFound;
}

float UDCInventoryComponent::GetTotalMass() const
{
	float Mass = 0.f;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const UDCItem* Item = Slots[Index];
		// A large item sits in both hands; count it once.
		if (Item && !(Index == SlotIndex(EDCEquipSlot::RightHand) && Item == GetSlotItem(EDCEquipSlot::LeftHand)))
		{
			Mass += Item->GetTotalMass();
		}
	}
	return Mass;
}

int32 UDCInventoryComponent::GetTotalValue() const
{
	int32 Value = 0;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const UDCItem* Item = Slots[Index];
		if (Item && !(Index == SlotIndex(EDCEquipSlot::RightHand) && Item == GetSlotItem(EDCEquipSlot::LeftHand)))
		{
			Value += Item->GetTotalValue();
		}
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
