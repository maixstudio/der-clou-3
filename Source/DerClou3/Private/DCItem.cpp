#include "DCItem.h"

#include "DerClou3.h"

#define LOCTEXT_NAMESPACE "DCItem"

UDCItem* UDCItem::Create(UObject* Outer, FName DefId)
{
	const FDCItemDef* Def = DCItems::Find(DefId);
	if (!Def)
	{
		UE_LOG(LogDerClou, Warning, TEXT("Unknown item id '%s'"), *DefId.ToString());
		return nullptr;
	}
	// Items move between pickups, crew members and containers, so none of those owns them:
	// they live in the transient package and are kept alive by whoever references them.
	UDCItem* Item = NewObject<UDCItem>(Outer ? Outer : GetTransientPackage());
	Item->DefId = DefId;
	for (const FName PartId : Def->FixedParts)
	{
		if (UDCItem* Part = Create(nullptr, PartId))
		{
			Part->bAttached = true;
			Item->AddContent(Part);
		}
	}
	return Item;
}

FSimpleMulticastDelegate& UDCItem::OnAnyChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void UDCItem::NotifyChanged()
{
	OnAnyChanged().Broadcast();
}

const FDCItemDef& UDCItem::GetDef() const
{
	static const FDCItemDef Missing;
	const FDCItemDef* Def = DCItems::Find(DefId);
	return Def ? *Def : Missing;
}

UDCItem* UDCItem::GetRoot()
{
	UDCItem* Item = this;
	while (Item->GetParent())
	{
		Item = Item->GetParent();
	}
	return Item;
}

bool UDCItem::IsInside(const UDCItem* Other) const
{
	for (const UDCItem* Item = this; Item; Item = Item->GetParent())
	{
		if (Item == Other)
		{
			return true;
		}
	}
	return false;
}

void UDCItem::AddContent(UDCItem* Item)
{
	if (!Item || Contents.Contains(Item))
	{
		return;
	}
	if (UDCItem* OldParent = Item->GetParent())
	{
		OldParent->Contents.Remove(Item);
	}
	Contents.Add(Item);
	Item->Parent = this;
	NotifyChanged();
}

bool UDCItem::RemoveContent(UDCItem* Item)
{
	if (!Item || Contents.Remove(Item) == 0)
	{
		return false;
	}
	Item->Parent.Reset();
	NotifyChanged();
	return true;
}

UDCItem* UDCItem::FindInside(FName Id) const
{
	for (UDCItem* Item : Contents)
	{
		if (Item->GetDefId() == Id)
		{
			return Item;
		}
		if (UDCItem* Found = Item->FindInside(Id))
		{
			return Found;
		}
	}
	return nullptr;
}

bool UDCItem::IsReachable() const
{
	for (const UDCItem* Item = this; Item; Item = Item->GetParent())
	{
		if (!Item->IsOpen())
		{
			return false;
		}
	}
	return true;
}

void UDCItem::SetOpen(bool bInOpen)
{
	if (bInOpen && bLocked)
	{
		return;
	}
	bOpen = bInOpen;
	NotifyChanged();
}

void UDCItem::SetLocked(bool bInLocked)
{
	bLocked = bInLocked && GetDef().HasLock() && !bLockBroken;
	if (bLocked)
	{
		bOpen = false;
	}
	NotifyChanged();
}

void UDCItem::BreakLock()
{
	bLocked = false;
	bLockBroken = true;
	bOpen = true;
	NotifyChanged();
}

FText UDCItem::DescribeState() const
{
	if (!GetDef().bOpenable)
	{
		return FText::GetEmpty();
	}
	if (bLocked)
	{
		return LOCTEXT("StateLocked", "versperrt");
	}
	if (bLockBroken)
	{
		return LOCTEXT("StateBroken", "aufgebrochen");
	}
	return bOpen ? LOCTEXT("StateOpen", "offen") : LOCTEXT("StateClosed", "zu");
}

float UDCItem::GetTotalMass() const
{
	float Mass = GetDef().MassKg;
	for (const UDCItem* Item : Contents)
	{
		Mass += Item->GetTotalMass();
	}
	return Mass;
}

float UDCItem::GetUsedVolume() const
{
	float Volume = 0.f;
	for (const UDCItem* Item : Contents)
	{
		Volume += Item->GetEffectiveVolume();
	}
	return Volume;
}

float UDCItem::GetEffectiveVolume() const
{
	const FDCItemDef& Def = GetDef();
	return Def.bRigid ? Def.VolumeLiters : Def.VolumeLiters + GetUsedVolume();
}

int32 UDCItem::GetTotalValue() const
{
	int32 Value = GetDef().Value;
	for (const UDCItem* Item : Contents)
	{
		Value += Item->GetTotalValue();
	}
	return Value;
}

FText UDCItem::CheckCanHold(const UDCItem* Item) const
{
	const FDCItemDef& Def = GetDef();
	if (!Item)
	{
		return LOCTEXT("NoItem", "Kein Gegenstand");
	}
	if (!Def.IsContainer())
	{
		return FText::Format(LOCTEXT("NotContainer", "In {0} kann man nichts hineintun"), Def.Name);
	}
	if (Contents.Contains(Item))
	{
		return FText::GetEmpty();
	}
	if (!IsReachable())
	{
		return FText::Format(bLocked ? LOCTEXT("HoldLocked", "{0} ist versperrt") : LOCTEXT("HoldClosed", "{0} ist zu"), Def.Name);
	}
	if (IsInside(Item))
	{
		return FText::Format(LOCTEXT("IntoItself", "{0} passt nicht in sich selbst"), Item->GetDisplayName());
	}
	if (Item->GetDef().Size > Def.MaxContentSize)
	{
		return FText::Format(LOCTEXT("TooBig", "{0} ist zu groß für {1} (passt nur bis „{2}“)"),
			Item->GetDisplayName(), Def.Name, DCItems::DescribeSize(Def.MaxContentSize));
	}
	if (GetUsedVolume() + Item->GetEffectiveVolume() > Def.CapacityLiters + KINDA_SMALL_NUMBER)
	{
		return FText::Format(LOCTEXT("TooFull", "{0} ist zu voll ({1} von {2} belegt, {3} braucht {4})"),
			Def.Name, DCItems::FormatLiters(GetUsedVolume()), DCItems::FormatLiters(Def.CapacityLiters),
			Item->GetDisplayName(), DCItems::FormatLiters(Item->GetEffectiveVolume()));
	}
	return FText::GetEmpty();
}

FText UDCItem::CheckCanTakeOut() const
{
	if (bAttached)
	{
		return FText::Format(LOCTEXT("Attached", "{0} ist fest eingebaut"), GetDisplayName());
	}
	const UDCItem* Holder = GetParent();
	if (Holder && !Holder->IsReachable())
	{
		// Name the outermost container that is in the way.
		const UDCItem* Blocking = Holder;
		for (const UDCItem* Item = Holder; Item; Item = Item->GetParent())
		{
			Blocking = Item->IsOpen() ? Blocking : Item;
		}
		return FText::Format(Blocking->IsLocked() ? LOCTEXT("OutLocked", "{0} ist versperrt") : LOCTEXT("OutClosed", "{0} ist zu"), Blocking->GetDisplayName());
	}
	return FText::GetEmpty();
}

FText UDCItem::DescribeCarrying() const
{
	const FDCItemDef& Def = GetDef();
	if (!Def.bMovable)
	{
		return LOCTEXT("CarryFixed", "fest eingebaut");
	}
	switch (Def.Size)
	{
	case EDCItemSize::Small: return LOCTEXT("CarrySmall", "passt in jede Tasche");
	case EDCItemSize::Medium: return LOCTEXT("CarryMedium", "Rucksack oder eine Hand");
	case EDCItemSize::Large: return LOCTEXT("CarryLarge", "nur mit beiden Händen");
	case EDCItemSize::Huge: return LOCTEXT("CarryHuge", "zu sperrig für eine Person");
	}
	return FText::GetEmpty();
}

void UDCItem::GetInfoLines(TArray<FDCInfoLine>& OutLines) const
{
	const FDCItemDef& Def = GetDef();
	if (!DescribeState().IsEmpty())
	{
		OutLines.Add({ LOCTEXT("LabelState", "Zustand"), DescribeState() });
	}
	OutLines.Add({ LOCTEXT("LabelMass", "Gewicht"), DCItems::FormatKg(GetTotalMass()) });
	OutLines.Add({ LOCTEXT("LabelVolume", "Volumen"), DCItems::FormatLiters(GetEffectiveVolume()) });
	OutLines.Add({ LOCTEXT("LabelSize", "Größe"), DCItems::DescribeSize(Def.Size) });
	OutLines.Add({ LOCTEXT("LabelCarry", "Tragen"), DescribeCarrying() });
	if (Def.WearSlot != EDCEquipSlot::None)
	{
		OutLines.Add({ LOCTEXT("LabelWear", "Tragen am"), DCItems::DescribeSlot(Def.WearSlot) });
	}
	if (Def.HasLock())
	{
		const FText Lock = Def.bForceable ? LOCTEXT("LockForceable", "knackbar (ca. {0} s) oder aufzubrechen") : LOCTEXT("LockPickOnly", "nur zu knacken (ca. {0} s)");
		OutLines.Add({ LOCTEXT("LabelLock", "Schloss"), FText::Format(Lock, FText::AsNumber(FMath::RoundToInt(Def.LockPickSeconds))) });
	}
	if (Def.IsContainer())
	{
		OutLines.Add({ LOCTEXT("LabelCapacity", "Fasst"), FText::Format(LOCTEXT("Capacity", "{0}, bis „{1}“"),
			DCItems::FormatLiters(Def.CapacityLiters), DCItems::DescribeSize(Def.MaxContentSize)) });
		FText Inside;
		if (!IsReachable())
		{
			Inside = LOCTEXT("InsideUnknown", "unbekannt (zu)");
		}
		else if (Contents.IsEmpty())
		{
			Inside = LOCTEXT("InsideEmpty", "leer");
		}
		else
		{
			TArray<FString> Names;
			for (const UDCItem* Item : Contents)
			{
				Names.Add(Item->GetDisplayName().ToString());
			}
			Inside = FText::FromString(FString::Join(Names, TEXT(", ")));
		}
		OutLines.Add({ LOCTEXT("LabelInside", "Inhalt"), Inside });
	}
	// Only what can be seen counts.
	const int32 Value = IsReachable() ? GetTotalValue() : Def.Value;
	if (Value > 0)
	{
		OutLines.Add({ LOCTEXT("LabelValue", "Wert"), DCItems::FormatEuro(Value) });
	}
}

#undef LOCTEXT_NAMESPACE
