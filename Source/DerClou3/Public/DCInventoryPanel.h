#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DCItemTypes.h"

#include "DCInventoryPanel.generated.h"

class ADCCrewCharacter;
class SVerticalBox;
class UDCInventoryComponent;
class UDCItem;
struct FDCItemTreeContext;

/**
 * Profile and inventory of the selected crew member (docked on the right): skills, the body
 * slots with the grids of clothes, vest and backpack, both hands, total weight and speed.
 * Items are rectangles on the grids, moved by drag and drop (also to and from an open search
 * window); dropping one outside every window puts it on the floor. A click selects an item:
 * its buttons and, for a container, its grid show below. Built in Slate.
 */
UCLASS()
class DERCLOU3_API UDCInventoryPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCrew(ADCCrewCharacter* InCrew);
	void Toggle();
	void Show();
	void Hide();
	bool IsShown() const { return bShown; }

	/** Short red line at the bottom, e.g. why an item does not fit. */
	void ShowMessage(const FText& Text);

	/** What a drop does (also used by the self-test): into a slot, or into a container if given (on that cell or anywhere). */
	void MoveItem(UDCItem* Item, EDCEquipSlot EquipSlot, UDCItem* Container, FIntPoint Cell = DCItems::AnyCell);
	void SelectItem(UDCItem* Item);

	/** Planning: show the inventory as it will be after the plan (true) or as it is now (false). */
	void SetPlanView(bool bInPlanView);
	/** True while the window shows the planned inventory (read-only). */
	bool IsPlanView() const;

	/** Set while the player decides about a find: every move here costs play time (owned before, owned after). */
	TFunction<void(bool bWasOwned, bool bNowOwned)> HandlingHook;
	void DropToFloor(UDCItem* Item);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RebuildContent();
	/** The inventory the window shows: the real one, or the planned one during planning. */
	UDCInventoryComponent* GetShownInventory() const;
	TSharedRef<SWidget> MakeViewToggle();
	TSharedRef<SWidget> MakeSlotCard(EDCEquipSlot EquipSlot);
	TSharedRef<SWidget> MakeHands();
	TSharedRef<SWidget> MakeSkills();
	void Unbind();

	TWeakObjectPtr<ADCCrewCharacter> Crew;
	TWeakObjectPtr<UDCItem> Selected;
	bool bWantPlanView = true;
	/** What the last rebuild showed (the planned inventory is a fresh copy after every plan change). */
	TWeakObjectPtr<const UDCInventoryComponent> LastShown;
	/** Items of the planned inventory that the crew member does not have yet. */
	TSet<const UDCItem*> PlannedOnly;
	FDelegateHandle ChangedHandle;
	FDelegateHandle ItemsChangedHandle;
	TSharedPtr<FDCItemTreeContext> Context;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Content;
	/** Holds the portrait of the crew member shown; refilled when the crew member changes. */
	TSharedPtr<class SBox> PortraitBox;
	TWeakObjectPtr<ADCCrewCharacter> PortraitShown;
	bool bShown = false;
	/** Rebuilding waits for the next tick, never inside one of the window's own drop handlers. */
	bool bDirty = true;
	FText Message;
	float MessageTime = 0.f;
};
