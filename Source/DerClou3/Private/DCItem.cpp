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

namespace
{
	int32 ItemMuteCount = 0;
}

UDCItem::FMuteScope::FMuteScope()
{
	++ItemMuteCount;
}

UDCItem::FMuteScope::~FMuteScope()
{
	--ItemMuteCount;
}

UDCItem* UDCItem::CreateBare(FName DefId, bool bInOpen, bool bInLocked, bool bInDestroyed, bool bInAttached)
{
	if (!DCItems::Find(DefId))
	{
		return nullptr;
	}
	UDCItem* Item = NewObject<UDCItem>(GetTransientPackage());
	Item->DefId = DefId;
	Item->bOpen = bInOpen;
	Item->bLocked = bInLocked;
	Item->bDestroyed = bInDestroyed;
	Item->bAttached = bInAttached;
	return Item;
}

UDCItem* UDCItem::Clone() const
{
	UDCItem* Copy = NewObject<UDCItem>(GetTransientPackage());
	Copy->DefId = DefId;
	Copy->bOpen = bOpen;
	Copy->bLocked = bLocked;
	Copy->bLockBroken = bLockBroken;
	Copy->bAttached = bAttached;
	Copy->bDestroyed = bDestroyed;
	Copy->GridPos = GridPos;
	for (const UDCItem* Child : Contents)
	{
		UDCItem* ChildCopy = Child->Clone();
		Copy->Contents.Add(ChildCopy);
		ChildCopy->Parent = Copy;
	}
	return Copy;
}

FSimpleMulticastDelegate& UDCItem::OnAnyChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void UDCItem::NotifyChanged()
{
	if (ItemMuteCount > 0)
	{
		return;
	}
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

void UDCItem::AddContent(UDCItem* Item, FIntPoint Cell)
{
	if (!Item || Item == this)
	{
		return;
	}
	if (UDCItem* OldParent = Item->GetParent(); OldParent && OldParent != this)
	{
		OldParent->Contents.Remove(Item);
	}
	if (!Contents.Contains(Item))
	{
		Contents.Add(Item);
	}
	Item->Parent = this;
	// Fixed parts sit outside the grid; everything else gets a cell if there is one.
	Item->GridPos = DCItems::AnyCell;
	if (!Item->bAttached && IsContainer())
	{
		FIntPoint Free;
		if (Cell.X >= 0 && Cell.Y >= 0)
		{
			Item->GridPos = Cell;
		}
		else if (FindFreeSpot(Item, Free))
		{
			Item->GridPos = Free;
		}
	}
	NotifyChanged();
}

bool UDCItem::RemoveContent(UDCItem* Item)
{
	if (!Item || Contents.Remove(Item) == 0)
	{
		return false;
	}
	Item->Parent.Reset();
	Item->GridPos = DCItems::AnyCell;
	NotifyChanged();
	return true;
}

UDCItem* UDCItem::GetItemAt(FIntPoint Cell) const
{
	for (UDCItem* Item : Contents)
	{
		if (!Item->HasGridPos())
		{
			continue;
		}
		const FIntPoint Min = Item->GridPos;
		const FIntPoint Max = Min + Item->GetGridSize();
		if (Cell.X >= Min.X && Cell.Y >= Min.Y && Cell.X < Max.X && Cell.Y < Max.Y)
		{
			return Item;
		}
	}
	return nullptr;
}

FText UDCItem::CheckPlaceAt(const UDCItem* Item, FIntPoint Cell) const
{
	const FDCItemDef& Def = GetDef();
	if (!Item || !Item->GetDef().FitsInGrid())
	{
		return FText::Format(LOCTEXT("NoGrid", "{0} passt in keine Tasche – nur mit beiden Händen"), Item ? Item->GetDisplayName() : FText::GetEmpty());
	}
	const FIntPoint Size = Item->GetGridSize();
	if (Size.X > Def.Grid.X || Size.Y > Def.Grid.Y)
	{
		return FText::Format(LOCTEXT("GridTooSmall", "{0} ({1}) passt nicht in {2} ({3})"),
			Item->GetDisplayName(), DCItems::FormatCells(Size), Def.Name, DCItems::FormatCells(Def.Grid));
	}
	if (Cell.X < 0 || Cell.Y < 0 || Cell.X + Size.X > Def.Grid.X || Cell.Y + Size.Y > Def.Grid.Y)
	{
		return LOCTEXT("OverEdge", "Ragt über den Rand");
	}
	for (int32 Y = Cell.Y; Y < Cell.Y + Size.Y; ++Y)
	{
		for (int32 X = Cell.X; X < Cell.X + Size.X; ++X)
		{
			const UDCItem* Taken = GetItemAt(FIntPoint(X, Y));
			if (Taken && Taken != Item)
			{
				return FText::Format(LOCTEXT("CellTaken", "Dort liegt schon {0}"), Taken->GetDisplayName());
			}
		}
	}
	return FText::GetEmpty();
}

bool UDCItem::FindFreeSpot(const UDCItem* Item, FIntPoint& OutCell) const
{
	if (!Item || !IsContainer() || !Item->GetDef().FitsInGrid())
	{
		return false;
	}
	const FIntPoint Grid = GetDef().Grid;
	const FIntPoint Size = Item->GetGridSize();
	for (int32 Y = 0; Y + Size.Y <= Grid.Y; ++Y)
	{
		for (int32 X = 0; X + Size.X <= Grid.X; ++X)
		{
			if (CheckPlaceAt(Item, FIntPoint(X, Y)).IsEmpty())
			{
				OutCell = FIntPoint(X, Y);
				return true;
			}
		}
	}
	return false;
}

int32 UDCItem::GetUsedCells() const
{
	int32 Used = 0;
	for (const UDCItem* Item : Contents)
	{
		if (Item->HasGridPos())
		{
			Used += Item->GetGridSize().X * Item->GetGridSize().Y;
		}
	}
	return Used;
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
	if ((bInOpen && bLocked) || bDestroyed)
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

void UDCItem::Smash()
{
	bDestroyed = true;
	bLocked = false;
	bOpen = true;
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
	if (bDestroyed)
	{
		return LOCTEXT("StateDestroyed", "eingeschlagen");
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

int32 UDCItem::GetTotalValue() const
{
	int32 Value = GetDef().Value;
	for (const UDCItem* Item : Contents)
	{
		Value += Item->GetTotalValue();
	}
	return Value;
}

FText UDCItem::CheckCanHold(const UDCItem* Item, FIntPoint Cell) const
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
	const bool bAnywhere = Cell.X < 0 || Cell.Y < 0;
	if (Contents.Contains(Item) && (bAnywhere || Item->GridPos == Cell))
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
	if (!bAnywhere)
	{
		return CheckPlaceAt(Item, Cell);
	}
	FIntPoint Free;
	if (!FindFreeSpot(Item, Free))
	{
		// Name the real problem: does not fit at all, or no room left.
		const FIntPoint Size = Item->GetGridSize();
		if (!Item->GetDef().FitsInGrid() || Size.X > Def.Grid.X || Size.Y > Def.Grid.Y)
		{
			return CheckPlaceAt(Item, FIntPoint::ZeroValue);
		}
		return FText::Format(LOCTEXT("TooFull", "{0} ist zu voll – kein freier Platz für {1} ({2})"),
			Def.Name, Item->GetDisplayName(), DCItems::FormatCells(Item->GetGridSize()));
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
	OutLines.Add({ LOCTEXT("LabelCells", "Platz"), Def.FitsInGrid() ? FText::Format(LOCTEXT("Cells", "{0} Felder"), DCItems::FormatCells(Def.GridSize)) : LOCTEXT("NoCells", "passt in kein Raster") });
	OutLines.Add({ LOCTEXT("LabelSize", "Größe"), DCItems::DescribeSize(Def.Size) });
	OutLines.Add({ LOCTEXT("LabelCarry", "Tragen"), DescribeCarrying() });
	if (Def.WearSlot != EDCEquipSlot::None)
	{
		OutLines.Add({ LOCTEXT("LabelWear", "Tragen am"), DCItems::DescribeSlot(Def.WearSlot) });
	}
	if (Def.HasLock())
	{
		const FText Lock = Def.bForceable ? LOCTEXT("LockForceable", "{0} {1} · aufzubrechen (Kraft {2})") : LOCTEXT("LockPickOnly", "{0} {1} · nicht aufzubrechen");
		OutLines.Add({ LOCTEXT("LabelLock", "Schloss"), FText::Format(Lock, DCSkills::Name(Def.LockSkill), FText::AsNumber(Def.LockDifficulty), FText::AsNumber(Def.ForceDifficulty)) });
	}
	if (!Def.IsContainer() && !Contents.IsEmpty())
	{
		TArray<FString> Parts;
		for (const UDCItem* Item : Contents)
		{
			const FText PartState = Item->DescribeState();
			Parts.Add(PartState.IsEmpty() ? Item->GetDisplayName().ToString() : FString::Printf(TEXT("%s (%s)"), *Item->GetDisplayName().ToString(), *PartState.ToString()));
		}
		OutLines.Add({ LOCTEXT("LabelParts", "Teile"), FText::FromString(FString::Join(Parts, TEXT(", "))) });
	}
	if (Def.IsContainer())
	{
		OutLines.Add({ LOCTEXT("LabelCapacity", "Fasst"), FText::Format(LOCTEXT("Capacity", "Raster {0}, bis „{1}“"),
			DCItems::FormatCells(Def.Grid), DCItems::DescribeSize(Def.MaxContentSize)) });
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
