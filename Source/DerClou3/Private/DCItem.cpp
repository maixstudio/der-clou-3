#include "DCItem.h"

#include "DerClou3.h"

#define LOCTEXT_NAMESPACE "DCItem"

UDCItem* UDCItem::Create(UObject* Outer, FName DefId)
{
	if (!DCItems::Find(DefId))
	{
		UE_LOG(LogDerClou, Warning, TEXT("Unknown item id '%s'"), *DefId.ToString());
		return nullptr;
	}
	UDCItem* Item = NewObject<UDCItem>(Outer);
	Item->DefId = DefId;
	return Item;
}

const FDCItemDef& UDCItem::GetDef() const
{
	static const FDCItemDef Missing;
	const FDCItemDef* Def = DCItems::Find(DefId);
	return Def ? *Def : Missing;
}

float UDCItem::GetTotalMass() const
{
	float Mass = GetDef().MassKg;
	for (const UDCItem* Item : Contents)
	{
		Mass += Item ? Item->GetTotalMass() : 0.f;
	}
	return Mass;
}

float UDCItem::GetUsedVolume() const
{
	float Volume = 0.f;
	for (const UDCItem* Item : Contents)
	{
		Volume += Item ? Item->GetDef().VolumeLiters : 0.f;
	}
	return Volume;
}

int32 UDCItem::GetTotalValue() const
{
	int32 Value = GetDef().Value;
	for (const UDCItem* Item : Contents)
	{
		Value += Item ? Item->GetTotalValue() : 0;
	}
	return Value;
}

FText UDCItem::DescribeCarrying() const
{
	const FDCItemDef& Def = GetDef();
	switch (Def.Size)
	{
	case EDCItemSize::Small: return LOCTEXT("CarrySmall", "passt in jede Tasche");
	case EDCItemSize::Medium: return LOCTEXT("CarryMedium", "Rucksack oder eine Hand");
	case EDCItemSize::Large: return LOCTEXT("CarryLarge", "nur mit beiden Händen");
	case EDCItemSize::Huge: return LOCTEXT("CarryHuge", "zu schwer für eine Person");
	}
	return FText::GetEmpty();
}

void UDCItem::GetInfoLines(TArray<FDCInfoLine>& OutLines) const
{
	const FDCItemDef& Def = GetDef();
	OutLines.Add({ LOCTEXT("LabelMass", "Gewicht"), DCItems::FormatKg(GetTotalMass()) });
	OutLines.Add({ LOCTEXT("LabelVolume", "Volumen"), DCItems::FormatLiters(Def.VolumeLiters) });
	OutLines.Add({ LOCTEXT("LabelSize", "Größe"), DCItems::DescribeSize(Def.Size) });
	OutLines.Add({ LOCTEXT("LabelCarry", "Tragen"), DescribeCarrying() });
	if (Def.WearSlot != EDCEquipSlot::None)
	{
		OutLines.Add({ LOCTEXT("LabelWear", "Tragen am"), DCItems::DescribeSlot(Def.WearSlot) });
	}
	if (Def.IsContainer())
	{
		OutLines.Add({ LOCTEXT("LabelCapacity", "Fasst"), FText::Format(LOCTEXT("Capacity", "{0} / {1}, bis {2}"),
			DCItems::FormatLiters(GetUsedVolume()), DCItems::FormatLiters(Def.CapacityLiters), DCItems::DescribeSize(Def.MaxContentSize)) });
	}
	if (GetTotalValue() > 0)
	{
		OutLines.Add({ LOCTEXT("LabelValue", "Wert"), DCItems::FormatEuro(GetTotalValue()) });
	}
}

#undef LOCTEXT_NAMESPACE
