#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "DCItemTypes.h"
#include "UObject/Object.h"

#include "DCItem.generated.h"

/**
 * One concrete item ("this crowbar"). The shared facts come from its FDCItemDef; containers
 * (clothes, vest, backpack) also hold their contents, which travel with them.
 */
UCLASS()
class DERCLOU3_API UDCItem : public UObject
{
	GENERATED_BODY()

public:
	/** New item of the given kind; nullptr if the id is unknown. */
	static UDCItem* Create(UObject* Outer, FName DefId);

	FName GetDefId() const { return DefId; }
	const FDCItemDef& GetDef() const;
	FText GetDisplayName() const { return GetDef().Name; }
	bool IsContainer() const { return GetDef().IsContainer(); }

	/** Own mass plus everything inside. */
	float GetTotalMass() const;
	/** Volume taken by the contents. */
	float GetUsedVolume() const;
	/** Value including everything inside. */
	int32 GetTotalValue() const;

	/** Facts for the info panel and tooltips. */
	void GetInfoLines(TArray<FDCInfoLine>& OutLines) const;
	/** How one person can carry it: "passt in Taschen", "nur mit beiden Händen", … */
	FText DescribeCarrying() const;

	UPROPERTY()
	TArray<TObjectPtr<UDCItem>> Contents;

private:
	UPROPERTY()
	FName DefId;
};
