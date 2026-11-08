#pragma once

#include "CoreMinimal.h"
#include "DCItemTypes.h"
#include "Styling/SlateTypes.h"

class SWidget;
class UDCItem;

/** What the item grids and headers in a window do when the player drags or clicks something. */
struct FDCItemTreeContext
{
	/** Empty if Item may go into Into (on Cell, or anywhere with DCItems::AnyCell), otherwise the reason (green/red footprint). */
	TFunction<FText(UDCItem* Item, UDCItem* Into, FIntPoint Cell)> CheckInto;
	TFunction<void(UDCItem* Item, UDCItem* Into, FIntPoint Cell)> MoveInto;
	/** Buttons on an item's header. */
	TFunction<TArray<EDCItemAction>(UDCItem* Item)> GetActions;
	TFunction<void(UDCItem* Item, EDCItemAction Action)> DoAction;
	/** Whether the selected crew member can do it, and how long / how likely ("ca. 3 s · Erfolg 75 %"). */
	TFunction<FDCTaskCheck(UDCItem* Item, EDCItemAction Action)> CheckAction;
	/** An item from this window dropped outside every window; unset = nothing happens. */
	TFunction<void(UDCItem* Item)> DropOutside;
	/** A click on an item: the window shows its buttons and, for a container, its grid. */
	TFunction<void(UDCItem* Item)> Select;
	TFunction<UDCItem*()> GetSelected;
	/** True for things that only come with the plan (picked up in a planned step): drawn in the coordination colour. */
	TFunction<bool(const UDCItem* Item)> IsPlanned;
};

/**
 * Slate building blocks shared by the profile and the search window: container grids whose items
 * are rectangles (drag them, the footprint shows green or red with the reason), drop zones for
 * slots and hands, and item headers with their action buttons.
 */
namespace DCItemWidgets
{
	/** One grid cell in Slate units. */
	constexpr float CellSize = 36.f;

	/** Body text: Barlow (Content/Slate/Fonts), the engine font if the files are missing. */
	DERCLOU3_API FSlateFontInfo Font(int32 Size, bool bBold = false);
	/** Titles and big labels: Barlow Condensed Bold, meant for capitals (see MakeTitle). */
	DERCLOU3_API FSlateFontInfo HeadFont(int32 Size, bool bBold = true);
	/** Plain white box: tint it with BorderBackgroundColor / the draw colour. */
	DERCLOU3_API const FSlateBrush* Brush();
	DERCLOU3_API const FProgressBarStyle& BarStyle();

	/**
	 * Rounded boxes (radius 3). The fill is white, so BorderBackgroundColor tints it; the outline keeps
	 * its own colour whatever the tint.
	 */
	DERCLOU3_API const FSlateBrush* RoundBrush();
	/** Rounded, thin brass outline: windows, cards, slots, list areas. */
	DERCLOU3_API const FSlateBrush* FramedBrush();
	/** Rounded, faint outline: rows and tiles inside a window. */
	DERCLOU3_API const FSlateBrush* SoftFramedBrush();
	/** Rounded, 2 px outline in the focus colour: what is selected (card, row, tile). */
	DERCLOU3_API const FSlateBrush* FocusFramedBrush();
	/** Rounded, 1 px outline in the coordination colour: planned things, waiting. */
	DERCLOU3_API const FSlateBrush* CoordinationFramedBrush();
	/** Rounded, 1 px outline in the problem colour. */
	DERCLOU3_API const FSlateBrush* ProblemFramedBrush();

	DERCLOU3_API extern const FLinearColor TitleColor;
	DERCLOU3_API extern const FLinearColor MutedColor;
	DERCLOU3_API extern const FLinearColor TextColor;
	DERCLOU3_API extern const FLinearColor LootColor;
	DERCLOU3_API extern const FLinearColor PanelColor;
	DERCLOU3_API extern const FLinearColor ZoneColor;
	DERCLOU3_API extern const FLinearColor MessageColor;
	/** Waiting for each other, signals, the links between them. */
	DERCLOU3_API extern const FLinearColor CoordinationColor;
	/** Text on light (focus-coloured) surfaces. */
	DERCLOU3_API extern const FLinearColor DarkTextColor;
	/** Rows and normal buttons inside a window. */
	DERCLOU3_API extern const FLinearColor TileColor;
	/** The solid frame around the scene (behind the windows). */
	DERCLOU3_API extern const FLinearColor BackdropColor;
	/** Hairlines: window outlines, separators, rules under titles. */
	DERCLOU3_API extern const FLinearColor BrassColor;
	DERCLOU3_API extern const FLinearColor BrassDimColor;
	/** Fits / done / plan works. */
	DERCLOU3_API extern const FLinearColor OkColor;

	/** The one main action of an area: filled in the focus colour (use DarkTextColor for its text). */
	DERCLOU3_API const FButtonStyle& PrimaryButtonStyle();
	/** Every other button: dark, faint outline, brass when hovered. Text colour comes from the style. */
	DERCLOU3_API const FButtonStyle& NormalButtonStyle();
	/** Destructive buttons ("Plan leeren"): like the normal one with a red outline; text in MessageColor. */
	DERCLOU3_API const FButtonStyle& DangerButtonStyle();
	/** No ground of its own, only a faint hover: for buttons that sit on a coloured box (segments, rows). */
	DERCLOU3_API const FButtonStyle& FlatButtonStyle();
	/**
	 * Two or more segments in one rounded box, the active one filled amber ("Jetzt | Nach dem Plan").
	 * Labels, tooltips, IsActive and OnClick per segment.
	 */
	DERCLOU3_API TSharedRef<SWidget> MakeSegments(const TArray<FText>& Labels, const TArray<FText>& Tips,
		TFunction<bool(int32 Index)> IsActive, TFunction<void(int32 Index)> OnClick);

	DERCLOU3_API TSharedRef<SWidget> MakeMuted(const FText& Text, int32 Size = 11);
	/** A window or area title in capitals (Barlow Condensed). */
	DERCLOU3_API TSharedRef<SWidget> MakeTitle(const TAttribute<FText>& Text, int32 Size = 18, const FLinearColor& Color = FLinearColor(0.8388f, 0.8148f, 0.7682f));
	/** A heading inside a window in small capitals with a brass rule after it: "FÄHIGKEITEN ————". */
	DERCLOU3_API TSharedRef<SWidget> MakeSectionTitle(const FText& Text);
	/** A 1 px brass line (horizontal or vertical) to separate areas. */
	DERCLOU3_API TSharedRef<SWidget> MakeRule(bool bHorizontal = true, bool bDim = true);

	/**
	 * Takes dropped items; Check decides green or red while hovering, and the reason shows next to
	 * the dragged item. Target names the place in that line ("→ Rechte Hand").
	 */
	DERCLOU3_API TSharedRef<SWidget> MakeDropZone(TSharedRef<SWidget> Content, TFunction<FText(UDCItem*)> Check,
		TFunction<void(UDCItem*)> OnDropped, const FLinearColor& Color, const FMargin& Padding, const FText& Target = FText::GetEmpty());
	/** Swallows drops (window background), so they do not count as "dropped outside". */
	DERCLOU3_API TSharedRef<SWidget> MakeSwallowZone(TSharedRef<SWidget> Content);

	/** A draggable, clickable item (click = select). Content is what it looks like. */
	DERCLOU3_API TSharedRef<SWidget> MakeTile(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, TSharedRef<SWidget> Content, bool bDraggable = true);

	/** One line: name (state), weight and – if wanted – the action buttons. Click = select. */
	DERCLOU3_API TSharedRef<SWidget> MakeHeader(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable, bool bButtons);

	/** The grid of a container: cells, the items on them as rectangles; takes dropped items (also onto a container lying in it). */
	DERCLOU3_API TSharedRef<SWidget> MakeGrid(UDCItem* Container, const TSharedRef<FDCItemTreeContext>& Context);

	/**
	 * Header with buttons and, for a container, its grid below (or "zu" while closed). Things with
	 * fixed parts (the desk's drawer) list each part as a section of its own.
	 */
	DERCLOU3_API TSharedRef<SWidget> MakeSection(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable, bool bGrid = true);

	/**
	 * Sections for the selected item and the containers it lies in, below the ones the window
	 * always shows (Shown). Nothing if the selection is not inside one of them.
	 */
	DERCLOU3_API TSharedRef<SWidget> MakeSelection(UDCItem* Selected, const TArray<UDCItem*>& Shown, const TSharedRef<FDCItemTreeContext>& Context);

	/** Default buttons: open/close, pick/force a lock; plus Take or Drop as given. */
	DERCLOU3_API TArray<EDCItemAction> DefaultActions(const UDCItem* Item, bool bTake, bool bDrop);
}
