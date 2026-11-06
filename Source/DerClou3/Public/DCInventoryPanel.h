#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DCItemTypes.h"

#include "DCInventoryPanel.generated.h"

class ADCCrewCharacter;
class SVerticalBox;
class UDCItem;
struct FDCItemTreeContext;

/**
 * Inventory of the selected crew member (left side, toggled with I or the button bottom left):
 * body slots and hands with everything inside them as a tree, total weight and speed.
 * Items are moved by drag and drop (also to and from an open container window); dropping one
 * outside every window puts it on the floor. Built in Slate.
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

	/** What a drop does (also used by the self-test): into a slot, or into a container if given. */
	void MoveItem(UDCItem* Item, EDCEquipSlot EquipSlot, UDCItem* Container);
	void DropToFloor(UDCItem* Item);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RebuildContent();
	TSharedRef<SWidget> MakeSlotRow(EDCEquipSlot EquipSlot);
	void Unbind();

	TWeakObjectPtr<ADCCrewCharacter> Crew;
	FDelegateHandle ChangedHandle;
	FDelegateHandle ItemsChangedHandle;
	TSharedPtr<FDCItemTreeContext> Context;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Content;
	bool bShown = false;
	/** Rebuilding waits for the next tick, never inside one of the window's own drop handlers. */
	bool bDirty = true;
	FText Message;
	float MessageTime = 0.f;
};
