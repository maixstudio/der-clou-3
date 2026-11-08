#pragma once

#include "CoreMinimal.h"
#include "DCInteractable.h"
#include "GameFramework/Actor.h"

#include "DCItemPickup.generated.h"

class UDCItem;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Something placed inside a pickup when the level starts. Inside is the path of item ids from the
 * pickup's own item down to the container, e.g. "" (directly inside), "desk_drawer" or
 * "desk_drawer/jewelry_box". If the container already has a fixed part of this kind that is not
 * configured yet (the desk's drawer), the entry configures that part instead of adding a new item.
 */
USTRUCT(BlueprintType)
struct FDCContentSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, Category = "Item")
	FString Inside;

	UPROPERTY(EditAnywhere, Category = "Item")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, Category = "Item")
	bool bOpen = false;
};

/**
 * An item in the world: loot on the floor, but also furniture and safes, which are items too
 * (just too big, too heavy or fixed in place). Click it to search it (containers), pick it up
 * or get info. Items dropped by the crew spawn one of these with the existing item.
 */
UCLASS()
class DERCLOU3_API ADCItemPickup : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCItemPickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The item here, with its contents (created from the settings on first use). */
	UDCItem* GetItem();
	const UDCItem* GetItem() const { return Item; }
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	TArray<FName> GetItemIdOptions() const;

	/** Which item lies or stands here. */
	UPROPERTY(EditAnywhere, Category = "Item", meta = (GetOptions = "GetItemIdOptions"))
	FName ItemId = TEXT("crowbar");

	/** Start state of the item itself (if it has a lid, door or lock). */
	UPROPERTY(EditAnywhere, Category = "Item")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, Category = "Item")
	bool bOpen = false;

	/** What is inside at the start (also: state of fixed parts such as drawers). */
	UPROPERTY(EditAnywhere, Category = "Item")
	TArray<FDCContentSpec> Contents;

	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UTextRenderComponent> Label;

private:
	void ApplyLook();
	void BuildItem();
	void OnItemChanged();

	UPROPERTY(Transient)
	TObjectPtr<UDCItem> Item;

	bool bHighlighted = false;
	FDelegateHandle ChangedHandle;
};
