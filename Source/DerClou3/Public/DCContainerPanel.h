#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DCItemTypes.h"

#include "DCContainerPanel.generated.h"

class ADCCrewCharacter;
class ADCItemPickup;
class SVerticalBox;
class UDCItem;
struct FDCItemTreeContext;

/**
 * Contents of a container in the world that a crew member is searching (desk, safe, wardrobe…):
 * its grid (and those of its drawers) with the items as rectangles, locks and buttons. Open, pick
 * or force from here; drag items to the profile and back. A click selects an item (a box inside:
 * its grid shows below). Closes when the crew member walks away.
 */
UCLASS()
class DERCLOU3_API UDCContainerPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(ADCCrewCharacter* InCrew, ADCItemPickup* InPickup);
	void Close();
	bool IsOpen() const { return Pickup.IsValid(); }
	ADCItemPickup* GetPickup() const { return Pickup.Get(); }

	void ShowMessage(const FText& Text);
	/** What a drop or button does (also used by the self-test). */
	void MoveInto(UDCItem* Item, UDCItem* Into, FIntPoint Cell = DCItems::AnyCell);
	void SelectItem(UDCItem* Item);
	void DoAction(UDCItem* Item, EDCItemAction Action);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RebuildContent();

	TWeakObjectPtr<ADCCrewCharacter> Crew;
	TWeakObjectPtr<ADCItemPickup> Pickup;
	TWeakObjectPtr<UDCItem> Selected;
	TSharedPtr<FDCItemTreeContext> Context;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Content;
	FDelegateHandle ItemsChangedHandle;
	bool bDirty = true;
	FText Message;
	float MessageTime = 0.f;
};
