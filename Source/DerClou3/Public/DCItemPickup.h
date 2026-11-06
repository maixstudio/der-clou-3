#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "GameFramework/Actor.h"

#include "DCItemPickup.generated.h"

class UDCItem;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * An item lying in the world. Click it to pick it up (or get info). Placed in the editor by
 * choosing an item id; items dropped by the crew spawn one of these with the existing item
 * (a dropped backpack keeps its contents).
 */
UCLASS()
class DERCLOU3_API ADCItemPickup : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCItemPickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The item lying here (created from ItemId on first use). */
	UDCItem* GetItem();
	/** Sets the item before spawning finishes (dropping). */
	void SetItem(UDCItem* InItem);

	/** Radius of the footprint on the floor, for finding a spot next to it. */
	float GetFootprintRadius() const;

	/** Name label while the mouse is over it. */
	void SetHighlighted(bool bInHighlighted);

	//~ IDCInteractable
	virtual FText GetInteractableName() const override;
	virtual FText GetInteractableKind() const override;
	virtual void GetInfoLines(TArray<FDCInfoLine>& OutLines) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	TArray<FName> GetItemIdOptions() const;

	/** Which item lies here. */
	UPROPERTY(EditAnywhere, Category = "Item", meta = (GetOptions = "GetItemIdOptions"))
	FName ItemId = TEXT("crowbar");

	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UTextRenderComponent> Label;

private:
	void ApplyLook();

	UPROPERTY(Transient)
	TObjectPtr<UDCItem> Item;

	bool bHighlighted = false;
};
