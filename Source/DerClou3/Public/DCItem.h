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
	/** Puts Item inside without any checks (use CheckCanHold first). */
	void AddContent(UDCItem* Item);
	bool RemoveContent(UDCItem* Item);
	/** First item with this id inside (any depth). */
	UDCItem* FindInside(FName Id) const;

	//~ State
	bool IsOpen() const { return !GetDef().bOpenable || bOpen; }
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
	/** Space taken up in another container; soft containers grow with their contents. */
	float GetEffectiveVolume() const;
	/** Volume taken by the contents. */
	float GetUsedVolume() const;
	/** Value including everything inside. */
	int32 GetTotalValue() const;

	//~ Rules
	/** Empty if Item may be put inside this, otherwise the reason. */
	FText CheckCanHold(const UDCItem* Item) const;
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
	bool bOpen = false;

	UPROPERTY()
	bool bLocked = false;

	UPROPERTY()
	bool bLockBroken = false;

	UPROPERTY()
	bool bAttached = false;
};
