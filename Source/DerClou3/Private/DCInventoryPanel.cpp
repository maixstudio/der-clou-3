#include "DCInventoryPanel.h"

#include "DCCrewCharacter.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "Input/DragAndDrop.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCInventoryPanel"

namespace
{
	const FLinearColor TitleColor(1.f, 0.85f, 0.25f);
	const FLinearColor MutedColor(0.6f, 0.63f, 0.68f);
	const FLinearColor TextColor(0.93f, 0.94f, 0.96f);
	const FLinearColor LootColor(1.f, 0.78f, 0.3f);
	const FLinearColor PanelColor(0.03f, 0.035f, 0.05f, 0.92f);
	const FLinearColor ZoneColor(0.07f, 0.075f, 0.095f);
	const FLinearColor TileColor(0.14f, 0.15f, 0.19f);
	const FLinearColor AcceptColor(0.12f, 0.32f, 0.14f);
	const FLinearColor RejectColor(0.38f, 0.1f, 0.08f);
	const FLinearColor MessageColor(1.f, 0.45f, 0.35f);
	constexpr float PanelWidth = 400.f;
	constexpr float SlotLabelWidth = 95.f;

	const FSlateBrush* PanelBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo PanelFont(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	/** An item being dragged. Dropped where nothing takes it (the game world), it goes on the floor. */
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
				.BorderImage(PanelBrush())
				.BorderBackgroundColor(TileColor)
				.Padding(FMargin(10.f, 5.f))
				[
					SNew(STextBlock)
					.Text(Item.IsValid() ? Item->GetDisplayName() : FText::GetEmpty())
					.Font(PanelFont(12, true))
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

	/** An item in the window that can be dragged. */
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
	 * Without OnItemDropped it just swallows the drop (so dropping on the window background does
	 * not put the item on the floor).
	 */
	class SDCDropZone : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCDropZone) {}
			SLATE_ARGUMENT(TFunction<FText(UDCItem*)>, Check)
			SLATE_ARGUMENT(TFunction<void(UDCItem*)>, OnItemDropped)
			SLATE_ARGUMENT(FMargin, Padding)
			SLATE_ARGUMENT(FLinearColor, Color)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Check = InArgs._Check;
			OnItemDropped = InArgs._OnItemDropped;
			Color = InArgs._Color;
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(PanelBrush())
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
			bHovered = Operation.IsValid() && OnItemDropped;
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
			if (OnItemDropped && Operation->Item.IsValid())
			{
				OnItemDropped(Operation->Item.Get());
			}
			return FReply::Handled();
		}

	private:
		TFunction<FText(UDCItem*)> Check;
		TFunction<void(UDCItem*)> OnItemDropped;
		FLinearColor Color = FLinearColor::Transparent;
		bool bHovered = false;
		bool bAccepts = false;
	};

	TSharedRef<SWidget> MutedText(const FText& Text, int32 Size = 11)
	{
		return SNew(STextBlock).Text(Text).Font(PanelFont(Size)).ColorAndOpacity(MutedColor).AutoWrapText(true);
	}

	/** Plain flat bar; the default style tints the fill blue. */
	const FProgressBarStyle& FillBarStyle()
	{
		static const FProgressBarStyle Style = FProgressBarStyle()
			.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.5f)))
			.SetFillImage(FSlateColorBrush(FLinearColor::White))
			.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		return Style;
	}
}

TSharedRef<SWidget> UDCInventoryPanel::RebuildWidget()
{
	Super::RebuildWidget();
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;

	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(20.f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.OnClicked_Lambda([WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->Toggle();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("OpenButton", "Inventar (I)")).Font(PanelFont(12, true))
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(20.f)
		[
			SNew(SBox)
			.WidthOverride(PanelWidth)
			.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsShown() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SDCDropZone)
				[
					SNew(SBorder)
					.BorderImage(PanelBrush())
					.BorderBackgroundColor(PanelColor)
					.Padding(FMargin(14.f, 12.f))
					// Clicks on the window must not reach the game (they would send the crew there).
					.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([WeakThis]()
								{
									const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
									return Member ? FText::Format(LOCTEXT("Title", "Inventar – {0}"), Member->GetCrewName()) : LOCTEXT("TitleNobody", "Inventar");
								})
								.Font(PanelFont(15, true))
								.ColorAndOpacity(TitleColor)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.IsFocusable(false)
								.ToolTipText(LOCTEXT("CloseTip", "Schließen (I)"))
								.OnClicked_Lambda([WeakThis]()
								{
									if (WeakThis.IsValid())
									{
										WeakThis->Hide();
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("×"))).Font(PanelFont(13, true))
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 10.f)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]()
							{
								const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
								const UDCInventoryComponent* Inventory = Member ? Member->GetInventory() : nullptr;
								if (!Inventory)
								{
									return FText::GetEmpty();
								}
								return FText::Format(LOCTEXT("Summary", "Gewicht {0} / {1}  ·  Tempo {2} %  ·  Wert {3}"),
									DCItems::FormatKg(Inventory->GetTotalMass()), DCItems::FormatKg(UDCInventoryComponent::MaxCarryMassKg),
									FText::AsNumber(FMath::RoundToInt(Inventory->GetSpeedFactor() * 100.f)), DCItems::FormatEuro(Inventory->GetTotalValue()));
							})
							.Font(PanelFont(11))
							.ColorAndOpacity(MutedColor)
						]
						+ SVerticalBox::Slot().MaxHeight(640.f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(Content, SVerticalBox)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->Message : FText::GetEmpty(); })
							.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->MessageTime > 0.f ? EVisibility::Visible : EVisibility::Collapsed; })
							.AutoWrapText(true)
							.Font(PanelFont(12, true))
							.ColorAndOpacity(MessageColor)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
						[
							MutedText(LOCTEXT("Hint", "Ziehen zum Umpacken · aus dem Fenster ziehen = auf den Boden legen"), 10)
						]
					]
				]
			]
		];
	bDirty = true;
	return Root.ToSharedRef();
}

void UDCInventoryPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	Content.Reset();
}

void UDCInventoryPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	MessageTime = FMath::Max(0.f, MessageTime - InDeltaTime);
	if (bDirty && Content.IsValid())
	{
		bDirty = false;
		RebuildContent();
	}
}

void UDCInventoryPanel::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UDCInventoryPanel::Unbind()
{
	if (ADCCrewCharacter* Member = Crew.Get())
	{
		if (UDCInventoryComponent* Inventory = Member->GetInventory())
		{
			Inventory->OnChanged.Remove(ChangedHandle);
		}
	}
	ChangedHandle.Reset();
}

void UDCInventoryPanel::SetCrew(ADCCrewCharacter* InCrew)
{
	Unbind();
	Crew = InCrew;
	if (InCrew && InCrew->GetInventory())
	{
		ChangedHandle = InCrew->GetInventory()->OnChanged.AddWeakLambda(this, [this]() { bDirty = true; });
	}
	Message = FText::GetEmpty();
	MessageTime = 0.f;
	bDirty = true;
}

void UDCInventoryPanel::Toggle()
{
	bShown ? Hide() : Show();
}

void UDCInventoryPanel::Show()
{
	bShown = true;
	bDirty = true;
}

void UDCInventoryPanel::Hide()
{
	bShown = false;
}

void UDCInventoryPanel::ShowMessage(const FText& Text)
{
	Message = Text;
	MessageTime = 4.f;
}

void UDCInventoryPanel::MoveItem(UDCItem* Item, EDCEquipSlot EquipSlot, UDCItem* Container)
{
	ADCCrewCharacter* Member = Crew.Get();
	UDCInventoryComponent* Inventory = Member ? Member->GetInventory() : nullptr;
	if (!Inventory || !Item)
	{
		return;
	}
	FText Reason;
	const bool bMoved = Container ? Inventory->MoveToContainer(Item, Container, Reason) : Inventory->MoveToSlot(Item, EquipSlot, Reason);
	if (!bMoved)
	{
		ShowMessage(Reason);
	}
}

void UDCInventoryPanel::DropToFloor(UDCItem* Item)
{
	if (ADCCrewCharacter* Member = Crew.Get())
	{
		Member->DropItem(Item);
	}
}

void UDCInventoryPanel::RebuildContent()
{
	Content->ClearChildren();
	const ADCCrewCharacter* Member = Crew.Get();
	const UDCInventoryComponent* Inventory = Member ? Member->GetInventory() : nullptr;
	if (!Inventory)
	{
		Content->AddSlot().AutoHeight()[MutedText(LOCTEXT("Nobody", "Niemand ausgewählt."), 12)];
		return;
	}

	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[SNew(STextBlock).Text(LOCTEXT("Worn", "Am Körper")).Font(PanelFont(12, true)).ColorAndOpacity(TextColor)];
	for (const EDCEquipSlot EquipSlot : { EDCEquipSlot::Head, EDCEquipSlot::Body, EDCEquipSlot::Vest, EDCEquipSlot::Back, EDCEquipSlot::LeftHand, EDCEquipSlot::RightHand })
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)[MakeSlotRow(EquipSlot)];
	}
	for (UDCItem* Container : Inventory->GetContainers())
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[MakeContainerSection(Container)];
	}
}

TSharedRef<SWidget> UDCInventoryPanel::MakeSlotRow(EDCEquipSlot EquipSlot)
{
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	UDCInventoryComponent* Inventory = Crew->GetInventory();
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Inventory;
	UDCItem* Item = Inventory->GetSlotItem(EquipSlot);

	TSharedRef<SWidget> Filling = MutedText(LOCTEXT("EmptySlot", "–"));
	if (Item && EquipSlot == EDCEquipSlot::LeftHand && Inventory->HasBothHandsFull())
	{
		Filling = MutedText(FText::Format(LOCTEXT("BothHands", "trägt {0} mit beiden Händen"), Item->GetDisplayName()));
	}
	else if (Item)
	{
		Filling = MakeItemTile(Item);
	}

	return SNew(SDCDropZone)
		.Color(ZoneColor)
		.Padding(FMargin(6.f, 3.f))
		.Check([WeakInventory, EquipSlot](UDCItem* Dragged) { return WeakInventory.IsValid() ? WeakInventory->CheckSlot(Dragged, EquipSlot) : FText::GetEmpty(); })
		.OnItemDropped([WeakThis, EquipSlot](UDCItem* Dragged)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->MoveItem(Dragged, EquipSlot, nullptr);
			}
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(SlotLabelWidth)[MutedText(DCItems::DescribeSlot(EquipSlot))]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				Filling
			]
		];
}

TSharedRef<SWidget> UDCInventoryPanel::MakeContainerSection(UDCItem* Container)
{
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Crew->GetInventory();
	TWeakObjectPtr<UDCItem> WeakContainer = Container;
	const FDCItemDef& Def = Container->GetDef();

	TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	for (UDCItem* Item : Container->Contents)
	{
		Items->AddSlot().AutoHeight().Padding(0.f, 2.f)[MakeItemTile(Item)];
	}
	if (Container->Contents.IsEmpty())
	{
		Items->AddSlot().AutoHeight().Padding(0.f, 2.f)[MutedText(LOCTEXT("EmptyContainer", "leer – Gegenstände hierher ziehen"))];
	}

	return SNew(SDCDropZone)
		.Color(ZoneColor)
		.Padding(FMargin(8.f, 6.f))
		.Check([WeakInventory, WeakContainer](UDCItem* Dragged)
		{
			return WeakInventory.IsValid() ? WeakInventory->CheckContainer(Dragged, WeakContainer.Get()) : FText::GetEmpty();
		})
		.OnItemDropped([WeakThis, WeakContainer](UDCItem* Dragged)
		{
			if (WeakThis.IsValid() && WeakContainer.IsValid())
			{
				WeakThis->MoveItem(Dragged, EDCEquipSlot::None, WeakContainer.Get());
			}
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(Def.Name).Font(PanelFont(12, true)).ColorAndOpacity(TextColor)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MutedText(FText::Format(LOCTEXT("Fill", "{0} / {1} · bis {2}"),
						DCItems::FormatLiters(Container->GetUsedVolume()), DCItems::FormatLiters(Def.CapacityLiters), DCItems::DescribeSize(Def.MaxContentSize)))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				SNew(SBox).HeightOverride(6.f)
				[
					SNew(SProgressBar)
					.Style(&FillBarStyle())
					.Percent(Def.CapacityLiters > 0.f ? Container->GetUsedVolume() / Def.CapacityLiters : 0.f)
					.FillColorAndOpacity(TitleColor)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Items
			]
		];
}

TSharedRef<SWidget> UDCInventoryPanel::MakeItemTile(UDCItem* Item)
{
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	TWeakObjectPtr<UDCItem> WeakItem = Item;

	// Tooltip: all facts, one per line.
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
	const FText Tooltip = FText::FromString(FString::Join(Rows, TEXT("\n")));
	const auto DropOutside = [WeakThis](UDCItem* Dragged)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->DropToFloor(Dragged);
		}
	};

	return SNew(SDCItemTile)
		.Item(Item)
		.OnDroppedOutside(DropOutside)
		[
			SNew(SBorder)
			.BorderImage(PanelBrush())
			.BorderBackgroundColor(TileColor)
			.Padding(FMargin(8.f, 3.f, 4.f, 3.f))
			.ToolTipText(Tooltip)
			.Cursor(EMouseCursor::GrabHand)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Item->GetDisplayName())
					.Font(PanelFont(12, true))
					.ColorAndOpacity(Item->GetDef().Value > 0 ? LootColor : TextColor)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
				[
					MutedText(DCItems::FormatKg(Item->GetTotalMass()))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.IsFocusable(false)
					.ToolTipText(LOCTEXT("DropTip", "Auf den Boden legen"))
					.ContentPadding(FMargin(4.f, 0.f))
					.OnClicked_Lambda([WeakThis, WeakItem]()
					{
						if (WeakThis.IsValid() && WeakItem.IsValid())
						{
							WeakThis->DropToFloor(WeakItem.Get());
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(LOCTEXT("Drop", "ablegen")).Font(PanelFont(10))
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
