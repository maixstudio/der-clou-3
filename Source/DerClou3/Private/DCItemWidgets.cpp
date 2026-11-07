#include "DCItemWidgets.h"

#include "Brushes/SlateColorBrush.h"
#include "DCItem.h"
#include "Input/DragAndDrop.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"
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
	const FLinearColor SelectedTileColor(0.3f, 0.26f, 0.1f);
	const FLinearColor AcceptColor(0.12f, 0.32f, 0.14f);
	const FLinearColor RejectColor(0.38f, 0.1f, 0.08f);
	const FLinearColor LockedColor(1.f, 0.4f, 0.3f);
	const FLinearColor CellColor(0.1f, 0.105f, 0.13f);
	const FLinearColor HintOkColor(0.45f, 0.95f, 0.5f);
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

	/** What the place under the dragged item says: "→ Rucksack" in green, or the reason in red. */
	struct FDCDragHint
	{
		FText Text;
		bool bOk = true;
	};

	/** An item being dragged. Dropped where nothing takes it (the game world), DropOutside decides. */
	class FDCItemDragOp : public FDragDropOperation
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FDCItemDragOp, FDragDropOperation)

		TWeakObjectPtr<UDCItem> Item;
		TFunction<void(UDCItem*)> OnDroppedOutside;
		/** Which cell of the item the cursor holds (its footprint follows the cursor from there). */
		FIntPoint GrabCell = FIntPoint::ZeroValue;
		TSharedRef<FDCDragHint> Hint = MakeShared<FDCDragHint>();

		static TSharedRef<FDCItemDragOp> New(UDCItem* InItem, TFunction<void(UDCItem*)> InOnDroppedOutside, FIntPoint InGrabCell)
		{
			TSharedRef<FDCItemDragOp> Operation = MakeShared<FDCItemDragOp>();
			Operation->Item = InItem;
			Operation->OnDroppedOutside = MoveTemp(InOnDroppedOutside);
			Operation->GrabCell = InGrabCell;
			Operation->Construct();
			return Operation;
		}

		void SetHint(const FText& Text, bool bOk)
		{
			Hint->Text = Text;
			Hint->bOk = bOk;
		}

		virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
		{
			const TSharedRef<FDCDragHint> HintRef = Hint;
			return SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(FLinearColor(0.08f, 0.085f, 0.11f, 0.95f))
				.Padding(FMargin(10.f, 5.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Item.IsValid() ? FText::Format(LOCTEXT("DragName", "{0}  ({1})"), Item->GetDisplayName(),
							Item->GetDef().FitsInGrid() ? DCItems::FormatCells(Item->GetGridSize()) : LOCTEXT("DragHands", "nur Hände")) : FText::GetEmpty())
						.Font(Font(12, true))
						.ColorAndOpacity(TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(SBox)
						.MaxDesiredWidth(320.f)
						.Visibility_Lambda([HintRef]() { return HintRef->Text.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							SNew(STextBlock)
							.Text_Lambda([HintRef]() { return HintRef->Text; })
							.ColorAndOpacity_Lambda([HintRef]() { return FSlateColor(HintRef->bOk ? HintOkColor : MessageColor); })
							.Font(Font(11, true))
							.AutoWrapText(true)
						]
					]
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

	/** Makes its content draggable as an item; a click without dragging selects it. */
	class SDCItemTile : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCItemTile)
			: _Draggable(true)
			, _CellSize(0.f)
		{}
			SLATE_ARGUMENT(TWeakObjectPtr<UDCItem>, Item)
			SLATE_ARGUMENT(TSharedPtr<FDCItemTreeContext>, Context)
			SLATE_ARGUMENT(bool, Draggable)
			/** > 0: a rectangle in a grid; the cell under the click becomes the grab point. */
			SLATE_ARGUMENT(float, CellSize)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Item = InArgs._Item;
			Context = InArgs._Context;
			bDraggable = InArgs._Draggable;
			GridCellSize = InArgs._CellSize;
			ChildSlot[InArgs._Content.Widget];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			PressPoint = FVector2D(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
			return bDraggable ? FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton) : FReply::Handled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Item.IsValid() && Context.IsValid() && Context->Select)
			{
				Context->Select(Item.Get());
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (!Item.IsValid() || !bDraggable)
			{
				return FReply::Unhandled();
			}
			const FIntPoint Size = Item->GetGridSize();
			FIntPoint Grab(FMath::Max(0, Size.X - 1) / 2, FMath::Max(0, Size.Y - 1) / 2);
			if (GridCellSize > 0.f)
			{
				Grab.X = FMath::Clamp(FMath::FloorToInt(PressPoint.X / GridCellSize), 0, FMath::Max(0, Size.X - 1));
				Grab.Y = FMath::Clamp(FMath::FloorToInt(PressPoint.Y / GridCellSize), 0, FMath::Max(0, Size.Y - 1));
			}
			TFunction<void(UDCItem*)> Outside = Context.IsValid() ? Context->DropOutside : nullptr;
			return FReply::Handled().BeginDragDrop(FDCItemDragOp::New(Item.Get(), MoveTemp(Outside), Grab));
		}

	private:
		TWeakObjectPtr<UDCItem> Item;
		TSharedPtr<FDCItemTreeContext> Context;
		bool bDraggable = true;
		float GridCellSize = 0.f;
		FVector2D PressPoint = FVector2D::ZeroVector;
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
			SLATE_ARGUMENT(FText, Target)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Check = InArgs._Check;
			OnDropped = InArgs._OnDropped;
			Color = InArgs._Color;
			Target = InArgs._Target;
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
			if (!bHovered)
			{
				return;
			}
			const FText Reason = Check ? Check(Operation->Item.Get()) : FText::GetEmpty();
			bAccepts = Reason.IsEmpty();
			Operation->SetHint(bAccepts ? (Target.IsEmpty() ? LOCTEXT("Fits", "passt") : FText::Format(LOCTEXT("FitsTarget", "→ {0}"), Target)) : Reason, bAccepts);
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			if (bHovered)
			{
				if (const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>())
				{
					Operation->SetHint(FText::GetEmpty(), true);
				}
			}
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
		FText Target;
		bool bHovered = false;
		bool bAccepts = false;
	};

	FText Tooltip(const UDCItem* Item)
	{
		TArray<FDCInfoLine> Lines;
		Item->GetInfoLines(Lines);
		TArray<FString> Rows;
		Rows.Add(Item->GetDisplayName().ToString());
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

	FLinearColor NameColor(const UDCItem* Item)
	{
		return Item->IsLocked() ? LockedColor : (Item->GetDef().Value > 0 ? LootColor : TextColor);
	}

	bool IsSelected(const TSharedPtr<FDCItemTreeContext>& Context, const UDCItem* Item)
	{
		return Context.IsValid() && Context->GetSelected && Context->GetSelected() == Item;
	}

	/**
	 * A container's grid: cells underneath, the items on them as rectangles. While an item is dragged
	 * over it, its footprint shows where it would land, green or red; over a container lying in the
	 * grid it goes into that container instead.
	 */
	class SDCItemGrid : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCItemGrid) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UDCItem>, Container)
			SLATE_ARGUMENT(TSharedPtr<FDCItemTreeContext>, Context)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Container = InArgs._Container;
			Context = InArgs._Context;
			const UDCItem* Holder = Container.Get();
			Grid = Holder ? Holder->GetDef().Grid : FIntPoint(1, 1);

			TSharedRef<SCanvas> Canvas = SNew(SCanvas);
			if (Holder && Context.IsValid())
			{
				for (UDCItem* Item : Holder->GetContents())
				{
					if (Item->HasGridPos())
					{
						AddTile(Canvas, Item);
					}
				}
			}
			ChildSlot
			[
				SNew(SBox)
				.WidthOverride(Grid.X * CellSize)
				.HeightOverride(Grid.Y * CellSize)
				[
					Canvas
				]
			];
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			for (int32 Y = 0; Y < Grid.Y; ++Y)
			{
				for (int32 X = 0; X < Grid.X; ++X)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(FVector2f(CellSize - 2.f, CellSize - 2.f), FSlateLayoutTransform(FVector2f(X * CellSize + 1.f, Y * CellSize + 1.f))),
						Brush(), ESlateDrawEffect::None, CellColor);
				}
			}
			int32 Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
			if (bPreview)
			{
				FLinearColor Fill = bPreviewOk ? FLinearColor(0.2f, 0.85f, 0.3f, 0.45f) : FLinearColor(0.95f, 0.2f, 0.15f, 0.45f);
				// Only the part inside the grid.
				const FIntPoint Min(FMath::Max(0, PreviewMin.X), FMath::Max(0, PreviewMin.Y));
				const FIntPoint Max(FMath::Min(Grid.X, PreviewMin.X + PreviewSize.X), FMath::Min(Grid.Y, PreviewMin.Y + PreviewSize.Y));
				if (Max.X > Min.X && Max.Y > Min.Y)
				{
					++Layer;
					FSlateDrawElement::MakeBox(OutDrawElements, Layer,
						AllottedGeometry.ToPaintGeometry(FVector2f((Max.X - Min.X) * CellSize, (Max.Y - Min.Y) * CellSize), FSlateLayoutTransform(FVector2f(Min.X * CellSize, Min.Y * CellSize))),
						Brush(), ESlateDrawEffect::None, Fill);
				}
			}
			return Layer;
		}

		virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			UpdatePreview(MyGeometry, DragDropEvent, *Operation);
			return FReply::Handled();
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			bPreview = false;
			if (const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>())
			{
				Operation->SetHint(FText::GetEmpty(), true);
			}
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			UpdatePreview(MyGeometry, DragDropEvent, *Operation);
			bPreview = false;
			if (Operation->Item.IsValid() && Target.IsValid() && Context.IsValid() && Context->MoveInto)
			{
				Context->MoveInto(Operation->Item.Get(), Target.Get(), TargetCell);
			}
			return FReply::Handled();
		}

	private:
		void AddTile(const TSharedRef<SCanvas>& Canvas, UDCItem* Item)
		{
			const FIntPoint Pos = Item->GetGridPos();
			const FIntPoint Size = Item->GetGridSize();
			const FVector2D Pixels(Size.X * CellSize - 2.f, Size.Y * CellSize - 2.f);
			const TSharedRef<FDCItemTreeContext> Ctx = Context.ToSharedRef();
			TWeakObjectPtr<UDCItem> WeakItem = Item;
			TSharedPtr<FDCItemTreeContext> CtxPtr = Context;
			const FDCItemDef& Def = Item->GetDef();

			// Upright long things (crowbar, flashlight) get their label turned sideways.
			const bool bUpright = Size.X == 1 && Size.Y >= 2;
			const FVector2D LabelBox = bUpright ? FVector2D(Pixels.Y - 4.f, Pixels.X - 4.f) : FVector2D(Pixels.X - 4.f, Pixels.Y - 4.f);
			TSharedRef<SCanvas> Label = SNew(SCanvas).Visibility(EVisibility::HitTestInvisible);
			Label->AddSlot()
				.Position(FVector2D((Pixels.X - 4.f) * 0.5f, (Pixels.Y - 4.f) * 0.5f))
				.Size(LabelBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.RenderTransform(bUpright ? FSlateRenderTransform(FQuat2D(-UE_HALF_PI)) : FSlateRenderTransform())
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					[
						SNew(STextBlock)
						.Text(Def.GetShortName())
						.Font(Font(Size.X == 1 && Size.Y == 1 ? 8 : 10, true))
						.ColorAndOpacity(NameColor(Item))
						.Justification(ETextJustify::Center)
						.AutoWrapText(!bUpright && Size.X > 1)
					]
				];

			const FLinearColor Fill = FMath::Lerp(TileColor, Def.Color.CopyWithNewOpacity(1.f), 0.22f);
			TSharedRef<SWidget> Look = SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor_Lambda([CtxPtr, WeakItem]() { return IsSelected(CtxPtr, WeakItem.Get()) ? TitleColor : FLinearColor(0.32f, 0.34f, 0.4f); })
				.Padding(FMargin(1.f))
				.ToolTipText(Tooltip(Item))
				.Cursor(EMouseCursor::GrabHand)
				[
					SNew(SBorder)
					.BorderImage(Brush())
					.BorderBackgroundColor(Fill)
					.Padding(FMargin(1.f))
					[
						Label
					]
				];

			Canvas->AddSlot()
				.Position(FVector2D(Pos.X * CellSize + 1.f, Pos.Y * CellSize + 1.f))
				.Size(Pixels)
				[
					SNew(SDCItemTile).Item(WeakItem).Context(CtxPtr).Draggable(!Item->IsAttached()).CellSize(CellSize)
					[
						Look
					]
				];
		}

		void UpdatePreview(const FGeometry& MyGeometry, const FDragDropEvent& Event, FDCItemDragOp& Operation)
		{
			UDCItem* Holder = Container.Get();
			UDCItem* Item = Operation.Item.Get();
			bPreview = Holder && Item && Context.IsValid() && Context->CheckInto;
			if (!bPreview)
			{
				return;
			}
			const FVector2D Local(MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
			const FIntPoint Cursor(FMath::FloorToInt(Local.X / CellSize), FMath::FloorToInt(Local.Y / CellSize));

			// Over a container lying here: into that one.
			UDCItem* Under = Holder->GetItemAt(Cursor);
			if (Under && Under->IsContainer() && !Under->IsInside(Item))
			{
				Target = Under;
				TargetCell = DCItems::AnyCell;
				PreviewMin = Under->GetGridPos();
				PreviewSize = Under->GetGridSize();
				const FText Reason = Context->CheckInto(Item, Under, DCItems::AnyCell);
				bPreviewOk = Reason.IsEmpty();
				Operation.SetHint(bPreviewOk ? FText::Format(LOCTEXT("IntoItem", "→ in {0}"), Under->GetDisplayName()) : Reason, bPreviewOk);
				return;
			}

			const FIntPoint Size = Item->GetDef().FitsInGrid() ? Item->GetGridSize() : FIntPoint(1, 1);
			FIntPoint Cell = Cursor - Operation.GrabCell;
			// Keep the footprint inside the grid where it can be (friendlier near the edges).
			if (Size.X <= Grid.X && Size.Y <= Grid.Y)
			{
				Cell.X = FMath::Clamp(Cell.X, 0, Grid.X - Size.X);
				Cell.Y = FMath::Clamp(Cell.Y, 0, Grid.Y - Size.Y);
			}
			Target = Holder;
			TargetCell = Cell;
			PreviewMin = Cell;
			PreviewSize = Size;
			const FText Reason = Context->CheckInto(Item, Holder, Cell);
			bPreviewOk = Reason.IsEmpty();
			Operation.SetHint(bPreviewOk ? FText::Format(LOCTEXT("IntoGrid", "→ {0}"), Holder->GetDisplayName()) : Reason, bPreviewOk);
		}

		TWeakObjectPtr<UDCItem> Container;
		TSharedPtr<FDCItemTreeContext> Context;
		FIntPoint Grid = FIntPoint(1, 1);

		bool bPreview = false;
		bool bPreviewOk = false;
		FIntPoint PreviewMin = FIntPoint::ZeroValue;
		FIntPoint PreviewSize = FIntPoint(1, 1);
		TWeakObjectPtr<UDCItem> Target;
		FIntPoint TargetCell = DCItems::AnyCell;
	};
}

namespace DCItemWidgets
{
	TSharedRef<SWidget> MakeDropZone(TSharedRef<SWidget> Content, TFunction<FText(UDCItem*)> Check, TFunction<void(UDCItem*)> OnDropped,
		const FLinearColor& Color, const FMargin& Padding, const FText& Target)
	{
		return SNew(SDCDropZone).Check(MoveTemp(Check)).OnDropped(MoveTemp(OnDropped)).Color(Color).Padding(Padding).Target(Target)[Content];
	}

	TSharedRef<SWidget> MakeSwallowZone(TSharedRef<SWidget> Content)
	{
		return SNew(SDCDropZone).Padding(FMargin(0.f))[Content];
	}

	TSharedRef<SWidget> MakeTile(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, TSharedRef<SWidget> Content, bool bDraggable)
	{
		return SNew(SDCItemTile).Item(Item).Context(Context).Draggable(bDraggable && Item && !Item->IsAttached())[Content];
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

	TSharedRef<SWidget> MakeHeader(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable, bool bButtons)
	{
		TWeakObjectPtr<UDCItem> WeakItem = Item;
		TSharedPtr<FDCItemTreeContext> CtxPtr = Context;
		const FDCItemDef& Def = Item->GetDef();

		// Buttons for what can be done with it.
		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		if (bButtons && Context->GetActions)
		{
			for (const EDCItemAction Action : Context->GetActions(Item))
			{
				// Greyed out when not possible; the tooltip says why or how long and how likely.
				const FDCTaskCheck Check = Context->CheckAction ? Context->CheckAction(Item, Action) : FDCTaskCheck();
				Buttons->AddSlot().AutoWidth().Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.IsFocusable(false)
					.ToolTipText(Check.Describe())
					.ForegroundColor(Check.bPossible ? FLinearColor::White : MutedColor)
					.ContentPadding(FMargin(4.f, 0.f))
					.OnClicked_Lambda([CtxPtr, WeakItem, Action]()
					{
						if (WeakItem.IsValid() && CtxPtr->DoAction)
						{
							CtxPtr->DoAction(WeakItem.Get(), Action);
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
		const bool bCanDrag = bDraggable && !Item->IsAttached() && Def.bMovable;
		TSharedRef<SWidget> Look = SNew(SBorder)
			.BorderImage(Brush())
			.BorderBackgroundColor_Lambda([CtxPtr, WeakItem]() { return IsSelected(CtxPtr, WeakItem.Get()) ? SelectedTileColor : TileColor; })
			.Padding(FMargin(8.f, 3.f, 4.f, 3.f))
			.ToolTipText(Tooltip(Item))
			.Cursor(bCanDrag ? EMouseCursor::GrabHand : EMouseCursor::Default)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(State.IsEmpty() ? Def.Name : FText::Format(LOCTEXT("NameState", "{0}  ({1})"), Def.Name, State))
					.Font(Font(11, true))
					.ColorAndOpacity(NameColor(Item))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 2.f, 0.f)
				[
					MakeMuted(DCItems::FormatKg(Item->GetTotalMass()), 10)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Buttons
				]
			];
		return SNew(SDCItemTile).Item(WeakItem).Context(CtxPtr).Draggable(bCanDrag)[Look];
	}

	TSharedRef<SWidget> MakeGrid(UDCItem* Container, const TSharedRef<FDCItemTreeContext>& Context)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HAlign(HAlign_Left)[SNew(SDCItemGrid).Container(Container).Context(Context)]
			];
		// Things without a cell (put there by the level, more than fits): still reachable.
		for (UDCItem* Loose : Container->GetContents())
		{
			if (!Loose->HasGridPos() && !Loose->IsAttached())
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[MakeHeader(Loose, Context, true, false)];
			}
		}
		return Box;
	}

	TSharedRef<SWidget> MakeSection(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable, bool bGrid)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[MakeHeader(Item, Context, bDraggable, true)];
		if (Item->IsContainer() && bGrid)
		{
			const FDCItemDef& Def = Item->GetDef();
			if (Item->IsReachable())
			{
				Box->AddSlot().AutoHeight().Padding(Indent, 3.f, 0.f, 0.f)[MakeGrid(Item, Context)];
				Box->AddSlot().AutoHeight().Padding(Indent, 1.f, 0.f, 0.f)
				[
					MakeMuted(FText::Format(LOCTEXT("GridInfo", "Raster {0} · {1} von {2} Feldern belegt · bis „{3}“"), DCItems::FormatCells(Def.Grid),
						FText::AsNumber(Item->GetUsedCells()), FText::AsNumber(Def.GetCellCount()), DCItems::DescribeSize(Def.MaxContentSize)), 9)
				];
			}
			else
			{
				Box->AddSlot().AutoHeight().Padding(Indent, 3.f, 0.f, 0.f)
				[
					MakeMuted(Item->IsLocked() ? LOCTEXT("GridLocked", "versperrt – was drin ist, sieht man erst offen") : LOCTEXT("GridClosed", "zu – öffnen, um hineinzusehen"), 10)
				];
			}
		}
		// Fixed parts (the desk's drawer) each with their own grid.
		for (UDCItem* Part : Item->GetContents())
		{
			if (Part->IsAttached())
			{
				Box->AddSlot().AutoHeight().Padding(Indent, 4.f, 0.f, 0.f)[MakeSection(Part, Context, false, true)];
			}
		}
		return Box;
	}

	TSharedRef<SWidget> MakeSelection(UDCItem* Selected, const TArray<UDCItem*>& Shown, const TSharedRef<FDCItemTreeContext>& Context)
	{
		if (!Selected)
		{
			return SNullWidget::NullWidget;
		}
		// The selected item and every container it lies in, up to one the window shows anyway.
		TArray<UDCItem*> Chain;
		for (UDCItem* Item = Selected; Item && !Shown.Contains(Item); Item = Item->GetParent())
		{
			Chain.Insert(Item, 0);
		}
		const bool bAlreadyShown = Chain.IsEmpty();
		if (bAlreadyShown)
		{
			Chain.Add(Selected);
		}
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (UDCItem* Item : Chain)
		{
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[MakeSection(Item, Context, true, !bAlreadyShown)];
		}
		return SNew(SBorder)
			.BorderImage(Brush())
			.BorderBackgroundColor(ZoneColor)
			.Padding(FMargin(6.f, 5.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock).Text(LOCTEXT("SelectionTitle", "Ausgewählt")).Font(Font(10, true)).ColorAndOpacity(TitleColor)
				]
				+ SVerticalBox::Slot().AutoHeight()[Box]
			];
	}
}

#undef LOCTEXT_NAMESPACE
