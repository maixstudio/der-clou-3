#include "DCItemWidgets.h"

#include "Brushes/SlateColorBrush.h"
#include "DCItem.h"
#include "Input/DragAndDrop.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCItemWidgets"

namespace DCItemWidgets
{
	const FLinearColor TitleColor(1.f, 0.85f, 0.25f);
	const FLinearColor MutedColor(0.6f, 0.63f, 0.68f);
	const FLinearColor TextColor(0.93f, 0.94f, 0.96f);
	const FLinearColor LootColor(1.f, 0.78f, 0.3f);
	const FLinearColor PanelColor(0.03f, 0.035f, 0.05f, 0.92f);
	const FLinearColor ZoneColor(0.07f, 0.075f, 0.095f);
	const FLinearColor MessageColor(1.f, 0.45f, 0.35f);
	const FLinearColor CoordinationColor(0.35f, 0.85f, 1.f);
	const FLinearColor DarkTextColor(0.05f, 0.05f, 0.06f);
	const FLinearColor TileColor(0.14f, 0.15f, 0.19f);
	const FLinearColor AcceptColor(0.12f, 0.32f, 0.14f);
	const FLinearColor RejectColor(0.38f, 0.1f, 0.08f);
	const FLinearColor LockedColor(1.f, 0.4f, 0.3f);
	constexpr float Indent = 14.f;

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	const FSlateBrush* Brush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	const FProgressBarStyle& BarStyle()
	{
		// Plain flat bar; the default style tints the fill blue.
		static const FProgressBarStyle Style = FProgressBarStyle()
			.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.5f)))
			.SetFillImage(FSlateColorBrush(FLinearColor::White))
			.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		return Style;
	}

	const FButtonStyle& PrimaryButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateColorBrush(FLinearColor(1.f, 0.85f, 0.25f)))
			.SetHovered(FSlateColorBrush(FLinearColor(1.f, 0.92f, 0.5f)))
			.SetPressed(FSlateColorBrush(FLinearColor(0.85f, 0.68f, 0.12f)))
			.SetDisabled(FSlateColorBrush(FLinearColor(0.25f, 0.25f, 0.27f)))
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	TSharedRef<SWidget> MakeMuted(const FText& Text, int32 Size)
	{
		return SNew(STextBlock).Text(Text).Font(Font(Size)).ColorAndOpacity(MutedColor).AutoWrapText(true);
	}
}

namespace
{
	using namespace DCItemWidgets;

	/** An item being dragged. Dropped where nothing takes it (the game world), DropOutside decides. */
	class FDCItemDragOp : public FDragDropOperation
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FDCItemDragOp, FDragDropOperation)

		TWeakObjectPtr<UDCItem> Item;
		TFunction<void(UDCItem*)> OnDroppedOutside;

		static TSharedRef<FDCItemDragOp> New(UDCItem* InItem, TFunction<void(UDCItem*)> InOnDroppedOutside)
		{
			TSharedRef<FDCItemDragOp> Operation = MakeShared<FDCItemDragOp>();
			Operation->Item = InItem;
			Operation->OnDroppedOutside = MoveTemp(InOnDroppedOutside);
			Operation->Construct();
			return Operation;
		}

		virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
		{
			return SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(TileColor)
				.Padding(FMargin(10.f, 5.f))
				[
					SNew(STextBlock)
					.Text(Item.IsValid() ? Item->GetDisplayName() : FText::GetEmpty())
					.Font(Font(12, true))
					.ColorAndOpacity(TextColor)
				];
		}

		virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override
		{
			if (!bDropWasHandled && OnDroppedOutside && Item.IsValid())
			{
				OnDroppedOutside(Item.Get());
			}
			FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
		}
	};

	/** Makes its content draggable as an item. */
	class SDCItemTile : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCItemTile) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UDCItem>, Item)
			SLATE_ARGUMENT(TFunction<void(UDCItem*)>, OnDroppedOutside)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Item = InArgs._Item;
			OnDroppedOutside = InArgs._OnDroppedOutside;
			ChildSlot[InArgs._Content.Widget];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
			}
			return FReply::Unhandled();
		}

		virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (!Item.IsValid())
			{
				return FReply::Unhandled();
			}
			return FReply::Handled().BeginDragDrop(FDCItemDragOp::New(Item.Get(), OnDroppedOutside));
		}

	private:
		TWeakObjectPtr<UDCItem> Item;
		TFunction<void(UDCItem*)> OnDroppedOutside;
	};

	/**
	 * Takes dropped items. While an item hovers over it, it turns green (fits) or red (does not).
	 * Without OnDropped it just swallows the drop.
	 */
	class SDCDropZone : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCDropZone) {}
			SLATE_ARGUMENT(TFunction<FText(UDCItem*)>, Check)
			SLATE_ARGUMENT(TFunction<void(UDCItem*)>, OnDropped)
			SLATE_ARGUMENT(FMargin, Padding)
			SLATE_ARGUMENT(FLinearColor, Color)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Check = InArgs._Check;
			OnDropped = InArgs._OnDropped;
			Color = InArgs._Color;
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor_Lambda([this]() { return !bHovered ? Color : (bAccepts ? AcceptColor : RejectColor); })
				.Padding(InArgs._Padding)
				[
					InArgs._Content.Widget
				]
			];
		}

		virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			bHovered = Operation.IsValid() && OnDropped;
			bAccepts = bHovered && (!Check || Check(Operation->Item.Get()).IsEmpty());
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			bHovered = false;
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			bHovered = false;
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			if (OnDropped && Operation->Item.IsValid())
			{
				OnDropped(Operation->Item.Get());
			}
			return FReply::Handled();
		}

	private:
		TFunction<FText(UDCItem*)> Check;
		TFunction<void(UDCItem*)> OnDropped;
		FLinearColor Color = FLinearColor::Transparent;
		bool bHovered = false;
		bool bAccepts = false;
	};

	FText Tooltip(const UDCItem* Item)
	{
		TArray<FDCInfoLine> Lines;
		Item->GetInfoLines(Lines);
		TArray<FString> Rows;
		for (const FDCInfoLine& Line : Lines)
		{
			Rows.Add(Line.Label.ToString() + TEXT(": ") + Line.Value.ToString());
		}
		if (!Item->GetDef().Description.IsEmpty())
		{
			Rows.Add(Item->GetDef().Description.ToString());
		}
		return FText::FromString(FString::Join(Rows, TEXT("\n")));
	}
}

namespace DCItemWidgets
{
	TSharedRef<SWidget> MakeDropZone(TSharedRef<SWidget> Content, TFunction<FText(UDCItem*)> Check, TFunction<void(UDCItem*)> OnDropped,
		const FLinearColor& Color, const FMargin& Padding)
	{
		return SNew(SDCDropZone).Check(MoveTemp(Check)).OnDropped(MoveTemp(OnDropped)).Color(Color).Padding(Padding)[Content];
	}

	TSharedRef<SWidget> MakeSwallowZone(TSharedRef<SWidget> Content)
	{
		return SNew(SDCDropZone).Padding(FMargin(0.f))[Content];
	}

	TArray<EDCItemAction> DefaultActions(const UDCItem* Item, bool bTake, bool bDrop)
	{
		TArray<EDCItemAction> Actions;
		const FDCItemDef& Def = Item->GetDef();
		if (Item->IsLocked())
		{
			Actions.Add(EDCItemAction::PickLock);
			if (Def.bForceable)
			{
				Actions.Add(EDCItemAction::Force);
			}
		}
		else if (Def.bOpenable && !Item->IsDestroyed())
		{
			Actions.Add(Item->IsOpen() ? EDCItemAction::Close : EDCItemAction::Open);
		}
		if (Def.bBreakable && !Item->IsDestroyed() && !Item->IsOpen())
		{
			Actions.Add(EDCItemAction::Smash);
		}
		if (!Item->IsAttached() && Def.bMovable)
		{
			if (bTake)
			{
				Actions.Add(EDCItemAction::Take);
			}
			if (bDrop)
			{
				Actions.Add(EDCItemAction::Drop);
			}
		}
		return Actions;
	}

	TSharedRef<SWidget> MakeItem(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable)
	{
		TWeakObjectPtr<UDCItem> WeakItem = Item;
		const FDCItemDef& Def = Item->GetDef();

		// Buttons for what can be done with it.
		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		if (Context->GetActions)
		{
			for (const EDCItemAction Action : Context->GetActions(Item))
			{
				// Greyed out when not possible; the tooltip says why or how long and how likely.
				const FDCTaskCheck Check = Context->CheckAction ? Context->CheckAction(Item, Action) : FDCTaskCheck();
				const FText Hint = Check.Describe();
				Buttons->AddSlot().AutoWidth().Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.IsFocusable(false)
					.ToolTipText(Hint)
					.ForegroundColor(Check.bPossible ? FLinearColor::White : MutedColor)
					.ContentPadding(FMargin(4.f, 0.f))
					.OnClicked_Lambda([Context, WeakItem, Action]()
					{
						if (WeakItem.IsValid() && Context->DoAction)
						{
							Context->DoAction(WeakItem.Get(), Action);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(DCItems::DescribeAction(Action)).Font(Font(10))
						.ColorAndOpacity(Check.bPossible ? FLinearColor::White : MutedColor)
					]
				];
			}
		}

		const FText State = Item->DescribeState();
		TSharedRef<SWidget> TileContent = SNew(SBorder)
			.BorderImage(Brush())
			.BorderBackgroundColor(TileColor)
			.Padding(FMargin(8.f, 3.f, 4.f, 3.f))
			.ToolTipText(Tooltip(Item))
			.Cursor(bDraggable && !Item->IsAttached() ? EMouseCursor::GrabHand : EMouseCursor::Default)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(State.IsEmpty() ? Def.Name : FText::Format(LOCTEXT("NameState", "{0}  ({1})"), Def.Name, State))
					.Font(Font(12, true))
					.ColorAndOpacity(Item->IsLocked() ? LockedColor : (Def.Value > 0 ? LootColor : TextColor))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 4.f, 0.f)
				[
					MakeMuted(DCItems::FormatKg(Item->GetTotalMass()))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Buttons
				]
			];

		TSharedRef<SWidget> Tile = TileContent;
		if (bDraggable && !Item->IsAttached())
		{
			Tile = SNew(SDCItemTile).Item(Item).OnDroppedOutside(Context->DropOutside)[TileContent];
		}
		if (!Item->IsContainer())
		{
			if (Item->GetContents().IsEmpty())
			{
				return Tile;
			}
			// Not a container itself, but it has parts (the desk's drawer): list them.
			TSharedRef<SVerticalBox> Parts = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Tile];
			for (UDCItem* Part : Item->GetContents())
			{
				Parts->AddSlot().AutoHeight().Padding(Indent, 2.f, 0.f, 0.f)[MakeItem(Part, Context, true)];
			}
			return Parts;
		}

		// Container: fill level and, while open, what is inside.
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Tile];
		if (Item->IsOpen())
		{
			TSharedRef<SVerticalBox> Inside = SNew(SVerticalBox);
			Inside->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SBox).HeightOverride(5.f)
					[
						SNew(SProgressBar)
						.Style(&BarStyle())
						.Percent(Def.CapacityLiters > 0.f ? FMath::Min(1.f, Item->GetUsedVolume() / Def.CapacityLiters) : 0.f)
						.FillColorAndOpacity(TitleColor)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeMuted(FText::Format(LOCTEXT("Fill", "{0} / {1} · bis {2}"), DCItems::FormatLiters(Item->GetUsedVolume()),
						DCItems::FormatLiters(Def.CapacityLiters), DCItems::DescribeSize(Def.MaxContentSize)), 10)
				]
			];
			for (UDCItem* Child : Item->GetContents())
			{
				Inside->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[MakeItem(Child, Context, true)];
			}
			if (Item->GetContents().IsEmpty())
			{
				Inside->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[MakeMuted(LOCTEXT("Empty", "leer"), 10)];
			}
			Box->AddSlot().AutoHeight().Padding(Indent, 0.f, 0.f, 2.f)[Inside];
		}

		return MakeDropZone(Box,
			[Context, WeakItem](UDCItem* Dragged) { return WeakItem.IsValid() && Context->CheckInto ? Context->CheckInto(Dragged, WeakItem.Get()) : FText::GetEmpty(); },
			[Context, WeakItem](UDCItem* Dragged)
			{
				if (WeakItem.IsValid() && Context->MoveInto)
				{
					Context->MoveInto(Dragged, WeakItem.Get());
				}
			},
			FLinearColor::Transparent, FMargin(0.f));
	}
}

#undef LOCTEXT_NAMESPACE
