#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DCItemTypes.h"

#include "DCInventoryPanel.generated.h"

class ADCCrewCharacter;
class SVerticalBox;
class UDCItem;

/**
 * Inventory of the selected crew member (left side, toggled with I or the button bottom left):
 * body slots and hands, worn containers with their fill level, total weight and speed.
 * Items are moved by drag and drop; dropping one outside the window puts it on the floor.
 * Built in Slate.
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

	/** What a drop does (also used by the self-test). */
	void MoveItem(UDCItem* Item, EDCEquipSlot EquipSlot, UDCItem* Container);
	void DropToFloor(UDCItem* Item);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void RebuildContent();
	TSharedRef<SWidget> MakeSlotRow(EDCEquipSlot EquipSlot);
	TSharedRef<SWidget> MakeContainerSection(UDCItem* Container);
	TSharedRef<SWidget> MakeItemTile(UDCItem* Item);
	void Unbind();

	TWeakObjectPtr<ADCCrewCharacter> Crew;
	FDelegateHandle ChangedHandle;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Content;
	bool bShown = false;
	/** Rebuilding waits for the next tick, never inside one of the window's own drop handlers. */
	bool bDirty = true;
	FText Message;
	float MessageTime = 0.f;
};
