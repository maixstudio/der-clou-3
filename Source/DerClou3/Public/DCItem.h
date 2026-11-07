#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "DCItemTypes.h"
#include "UObject/Object.h"

#include "DCItem.generated.h"

/**
 * One concrete item ("this crowbar", "this desk"). The shared facts come from its FDCItemDef.
 * Containers hold other items, as deep as needed: desk > drawer > jewellery box > ring. Contents
 * travel with their container. Openable containers only give access while open; a lock keeps
 * them shut until it is picked or broken.
 */
UCLASS()
class DERCLOU3_API UDCItem : public UObject
{
	GENERATED_BODY()

public:
	/** New item of the given kind, with its fixed parts; nullptr if the id is unknown. */
	static UDCItem* Create(FName DefId) { return Create(nullptr, DefId); }
	static UDCItem* Create(UObject* Outer, FName DefId);
	/** For the planning simulation: an item in the given state, without contents or fixed parts. */
	static UDCItem* CreateBare(FName DefId, bool bInOpen, bool bInLocked, bool bInDestroyed, bool bInAttached);
	/** A copy of this item with everything inside it, in the same state and on the same cells. */
	UDCItem* Clone() const;

	/**
	 * While one of these lives, item changes do not notify the windows (the planning simulation
	 * builds and changes many throwaway items).
	 */
	struct DERCLOU3_API FMuteScope
	{
		FMuteScope();
		~FMuteScope();
	};

	/** Fires whenever any item changes its contents or state (the windows listen to it). */
	static FSimpleMulticastDelegate& OnAnyChanged();

	FName GetDefId() const { return DefId; }
	const FDCItemDef& GetDef() const;
	FText GetDisplayName() const { return GetDef().Name; }
	bool IsContainer() const { return GetDef().IsContainer(); }

	//~ Contents
	const TArray<TObjectPtr<UDCItem>>& GetContents() const { return Contents; }
	/** The item this one lies in (nullptr at the top: worn, held or lying in the world). */
	UDCItem* GetParent() const { return Parent.Get(); }
	UDCItem* GetRoot();
	/** True if this is Other or lies (somewhere) inside it. */
	bool IsInside(const UDCItem* Other) const;
	/**
	 * Puts Item inside without any rule checks (use CheckCanHold first): on the given cell, or with
	 * DCItems::AnyCell on the first free one (if none is free, it lies there without a cell).
	 * Fixed parts never get a cell. Also moves an item that is already inside to another cell.
	 */
	void AddContent(UDCItem* Item, FIntPoint Cell = DCItems::AnyCell);
	bool RemoveContent(UDCItem* Item);
	/** First item with this id inside (any depth). */
	UDCItem* FindInside(FName Id) const;

	//~ Grid
	/** Top-left cell inside the parent container; DCItems::AnyCell if it has none (worn, held, fixed part, in the world). */
	FIntPoint GetGridPos() const { return GridPos; }
	/** Cells it takes up in a container (0 × 0: fits in no grid). */
	FIntPoint GetGridSize() const { return GetDef().GridSize; }
	bool HasGridPos() const { return GridPos.X >= 0 && GridPos.Y >= 0; }
	/** The content lying on this cell (nullptr if free). */
	UDCItem* GetItemAt(FIntPoint Cell) const;
	/** Empty if Item's footprint lies inside the grid on free cells at Cell (Item itself does not block). Geometry only. */
	FText CheckPlaceAt(const UDCItem* Item, FIntPoint Cell) const;
	/** First free cell for Item, row by row from the top left. False if there is none. Geometry only. */
	bool FindFreeSpot(const UDCItem* Item, FIntPoint& OutCell) const;
	/** Cells taken by contents. */
	int32 GetUsedCells() const;

	//~ State
	bool IsOpen() const { return !GetDef().bOpenable || bOpen || bDestroyed; }
	/** Glass smashed: open for good. */
	bool IsDestroyed() const { return bDestroyed; }
	void Smash();
	bool IsLocked() const { return bLocked; }
	bool IsLockBroken() const { return bLockBroken; }
	/** A fixed part of its parent (a drawer): cannot be taken out. */
	bool IsAttached() const { return bAttached; }
	/** Contents can be reached: open, and every container around it open too. */
	bool IsReachable() const;
	void SetOpen(bool bInOpen);
	void SetLocked(bool bInLocked);
	/** The lock was broken open: unlocked and open for good. */
	void BreakLock();
	/** "versperrt", "zu", "offen", "aufgebrochen" – empty for things without a door or lid. */
	FText DescribeState() const;

	//~ Size
	/** Own mass plus everything inside. */
	float GetTotalMass() const;
	/** Value including everything inside. */
	int32 GetTotalValue() const;

	//~ Rules
	/** Empty if Item may be put inside this (on that cell, or anywhere with AnyCell), otherwise the reason. */
	FText CheckCanHold(const UDCItem* Item, FIntPoint Cell = DCItems::AnyCell) const;
	/** Empty if this may be taken out of where it lies, otherwise the reason. */
	FText CheckCanTakeOut() const;

	/** Facts for the info panel and tooltips (contents only if they can be seen). */
	void GetInfoLines(TArray<FDCInfoLine>& OutLines) const;
	/** How one person can carry it: "passt in jede Tasche", "nur mit beiden Händen", … */
	FText DescribeCarrying() const;

private:
	void NotifyChanged();

	UPROPERTY()
	FName DefId;

	UPROPERTY()
	TArray<TObjectPtr<UDCItem>> Contents;

	TWeakObjectPtr<UDCItem> Parent;

	UPROPERTY()
	FIntPoint GridPos = FIntPoint(-1, -1);

	UPROPERTY()
	bool bOpen = false;

	UPROPERTY()
	bool bLocked = false;

	UPROPERTY()
	bool bLockBroken = false;

	UPROPERTY()
	bool bAttached = false;

	UPROPERTY()
	bool bDestroyed = false;
};
