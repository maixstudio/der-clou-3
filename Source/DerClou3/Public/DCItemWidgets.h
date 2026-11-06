#pragma once

#include "CoreMinimal.h"
#include "DCItemTypes.h"
#include "Styling/SlateTypes.h"

class SWidget;
class UDCItem;

/** What the item tree in a window does when the player drags or clicks something. */
struct FDCItemTreeContext
{
	/** Empty if Item may go into Into, otherwise the reason (drives the green/red highlight). */
	TFunction<FText(UDCItem* Item, UDCItem* Into)> CheckInto;
	TFunction<void(UDCItem* Item, UDCItem* Into)> MoveInto;
	/** Buttons on an item's tile. */
	TFunction<TArray<EDCItemAction>(UDCItem* Item)> GetActions;
	TFunction<void(UDCItem* Item, EDCItemAction Action)> DoAction;
	/** Whether the selected crew member can do it, and how long / how likely ("ca. 3 s · Erfolg 75 %"). */
	TFunction<FDCTaskCheck(UDCItem* Item, EDCItemAction Action)> CheckAction;
	/** An item from this window dropped outside every window; unset = nothing happens. */
	TFunction<void(UDCItem* Item)> DropOutside;
};

/**
 * Slate building blocks shared by the inventory and the container window: draggable item tiles,
 * drop zones that turn green or red, and the nested tree of an item with its contents.
 */
namespace DCItemWidgets
{
	DERCLOU3_API FSlateFontInfo Font(int32 Size, bool bBold = false);
	DERCLOU3_API const FSlateBrush* Brush();
	DERCLOU3_API const FProgressBarStyle& BarStyle();

	DERCLOU3_API extern const FLinearColor TitleColor;
	DERCLOU3_API extern const FLinearColor MutedColor;
	DERCLOU3_API extern const FLinearColor TextColor;
	DERCLOU3_API extern const FLinearColor LootColor;
	DERCLOU3_API extern const FLinearColor PanelColor;
	DERCLOU3_API extern const FLinearColor ZoneColor;
	DERCLOU3_API extern const FLinearColor MessageColor;

	DERCLOU3_API TSharedRef<SWidget> MakeMuted(const FText& Text, int32 Size = 11);

	/** Takes dropped items; Check decides green or red while hovering. */
	DERCLOU3_API TSharedRef<SWidget> MakeDropZone(TSharedRef<SWidget> Content, TFunction<FText(UDCItem*)> Check,
		TFunction<void(UDCItem*)> OnDropped, const FLinearColor& Color, const FMargin& Padding);
	/** Swallows drops (window background), so they do not count as "dropped outside". */
	DERCLOU3_API TSharedRef<SWidget> MakeSwallowZone(TSharedRef<SWidget> Content);

	/**
	 * An item tile with its action buttons; for containers also the fill level and, while open,
	 * the contents below it (recursively). Containers take dropped items.
	 */
	DERCLOU3_API TSharedRef<SWidget> MakeItem(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable = true);

	/** Default buttons: open/close, pick/force a lock; plus Take or Drop as given. */
	DERCLOU3_API TArray<EDCItemAction> DefaultActions(const UDCItem* Item, bool bTake, bool bDrop);
}
