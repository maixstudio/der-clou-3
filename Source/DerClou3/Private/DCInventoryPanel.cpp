#include "DCInventoryPanel.h"

#include "DCFrame.h"

#include "DCCrewCharacter.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCInventoryPanel"

namespace
{
	constexpr float InventoryWidth = 430.f;
	constexpr float ColumnGap = 8.f;
	constexpr float HandHeight = 58.f;
}

void UDCInventoryPanel::NativeConstruct()
{
	Super::NativeConstruct();
	ItemsChangedHandle = UDCItem::OnAnyChanged().AddWeakLambda(this, [this]() { bDirty = true; });
}

TSharedRef<SWidget> UDCInventoryPanel::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;

	// What the item tree does in this window.
	Context = MakeShared<FDCItemTreeContext>();
	Context->CheckInto = [WeakThis](UDCItem* Item, UDCItem* Into, FIntPoint Cell)
	{
		const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
		return Member ? Member->GetInventory()->CheckInto(Item, Into, Cell) : FText::GetEmpty();
	};
	Context->MoveInto = [WeakThis](UDCItem* Item, UDCItem* Into, FIntPoint Cell)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->MoveItem(Item, EDCEquipSlot::None, Into, Cell);
		}
	};
	Context->Select = [WeakThis](UDCItem* Item)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SelectItem(Item);
		}
	};
	Context->GetSelected = [WeakThis]() { return WeakThis.IsValid() ? WeakThis->Selected.Get() : nullptr; };
	Context->GetActions = [](UDCItem* Item) { return DefaultActions(Item, false, true); };
	Context->CheckAction = [WeakThis](UDCItem* Item, EDCItemAction Action)
	{
		const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
		return Member ? Member->CheckItemAction(Item, Action) : FDCTaskCheck();
	};
	Context->DoAction = [WeakThis](UDCItem* Item, EDCItemAction Action)
	{
		ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
		if (Member)
		{
			const FText Reason = Member->PerformItemAction(Item, Action);
			if (!Reason.IsEmpty())
			{
				WeakThis->ShowMessage(Reason);
			}
		}
	};
	Context->DropOutside = [WeakThis](UDCItem* Item)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->DropToFloor(Item);
		}
	};

	// The button to open it sits in the timeline.
	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, UDCFrame::TopHeight + 8.f, (UDCFrame::RightWidth - InventoryWidth) * 0.5f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(InventoryWidth)
			.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsShown() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				MakeSwallowZone(
					SNew(SBorder)
					.BorderImage(Brush())
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
									return Member ? FText::Format(LOCTEXT("Title", "Profil – {0}"), Member->GetCrewName()) : LOCTEXT("TitleNobody", "Profil");
								})
								.Font(Font(15, true))
								.ColorAndOpacity(TitleColor)
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
									DCItems::FormatKg(Inventory->GetTotalMass()), DCItems::FormatKg(Inventory->GetMaxCarryMass()),
									FText::AsNumber(FMath::RoundToInt(Inventory->GetSpeedFactor() * 100.f)), DCItems::FormatEuro(Inventory->GetTotalValue()));
							})
							.Font(Font(11))
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
							.Font(Font(12, true))
							.ColorAndOpacity(MessageColor)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
						[
							MakeMuted(LOCTEXT("Hint", "Anklicken = Knöpfe · ziehen = umpacken (grün passt, rot sagt warum) · auf einen Behälter ziehen = hineinlegen · aus dem Fenster ziehen = auf den Boden"), 10)
						]
					])
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
	UDCItem::OnAnyChanged().Remove(ItemsChangedHandle);
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
	if (Crew.Get() != InCrew)
	{
		Selected.Reset();
	}
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
	// Docked in the frame: it always shows the selected crew member.
	Show();
}

void UDCInventoryPanel::Show()
{
	bShown = true;
	bDirty = true;
}

void UDCInventoryPanel::Hide()
{
	// Docked in the frame: stays.
}

void UDCInventoryPanel::ShowMessage(const FText& Text)
{
	Message = Text;
	MessageTime = 4.f;
}

void UDCInventoryPanel::SelectItem(UDCItem* Item)
{
	Selected = Selected.Get() == Item ? nullptr : Item;
	bDirty = true;
}

void UDCInventoryPanel::MoveItem(UDCItem* Item, EDCEquipSlot EquipSlot, UDCItem* Container, FIntPoint Cell)
{
	ADCCrewCharacter* Member = Crew.Get();
	UDCInventoryComponent* Inventory = Member ? Member->GetInventory() : nullptr;
	if (!Inventory || !Item)
	{
		return;
	}
	FText Reason;
	const bool bWasOwned = Inventory->Owns(Item);
	const FIntPoint OldCell = Item->GetGridPos();
	const UDCItem* OldParent = Item->GetParent();
	const bool bMoved = Container ? Inventory->MoveInto(Item, Container, Reason, Cell) : Inventory->MoveToSlot(Item, EquipSlot, Reason);
	if (!bMoved)
	{
		ShowMessage(Reason);
	}
	else if (HandlingHook && (Item->GetParent() != OldParent || Item->GetGridPos() != OldCell || bWasOwned != Inventory->Owns(Item) || !Container))
	{
		HandlingHook(bWasOwned, Inventory->Owns(Item));
	}
}

void UDCInventoryPanel::DropToFloor(UDCItem* Item)
{
	if (ADCCrewCharacter* Member = Crew.Get())
	{
		const bool bWasOwned = Member->GetInventory()->Owns(Item);
		Member->DropItem(Item);
		if (HandlingHook && bWasOwned)
		{
			HandlingHook(true, false);
		}
	}
}

void UDCInventoryPanel::RebuildContent()
{
	using namespace DCItemWidgets;
	Content->ClearChildren();
	const ADCCrewCharacter* Member = Crew.Get();
	const UDCInventoryComponent* Inventory = Member ? Member->GetInventory() : nullptr;
	if (!Inventory || !Context.IsValid())
	{
		Content->AddSlot().AutoHeight()[MakeMuted(LOCTEXT("Nobody", "Niemand ausgewählt."), 12)];
		return;
	}
	if (Selected.IsValid() && !Inventory->Owns(Selected.Get()))
	{
		Selected.Reset();
	}
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[MakeSkills()];

	// Two columns: clothes and backpack on the left; head, vest and hands on the right.
	Content->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, ColumnGap * 0.5f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeSlotCard(EDCEquipSlot::Body)]
			+ SVerticalBox::Slot().AutoHeight()[MakeSlotCard(EDCEquipSlot::Back)]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(ColumnGap * 0.5f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeSlotCard(EDCEquipSlot::Head)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeSlotCard(EDCEquipSlot::Vest)]
			+ SVerticalBox::Slot().AutoHeight()[MakeHands()]
		]
	];

	// The selected item: its buttons, and the grid of a container held or packed somewhere.
	TArray<UDCItem*> Shown;
	for (UDCItem* Worn : Inventory->GetContainers())
	{
		Shown.Add(Worn);
	}
	if (UDCItem* Item = Selected.Get())
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[MakeSelection(Item, Shown, Context.ToSharedRef())];
	}
}

TSharedRef<SWidget> UDCInventoryPanel::MakeSkills()
{
	using namespace DCItemWidgets;
	const FDCSkills& Skills = Crew->GetSkills();
	// Two columns: name, bar, value. Skills nothing uses yet are dimmed.
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	TSharedPtr<SHorizontalBox> Row;
	for (int32 Index = 0; Index < static_cast<int32>(EDCSkill::Count); ++Index)
	{
		const EDCSkill Skill = static_cast<EDCSkill>(Index);
		if (Index % 2 == 0)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, 1.f)[SAssignNew(Row, SHorizontalBox)];
		}
		const int32 Value = Skills.Get(Skill);
		const bool bUnused = DCSkills::IsUnused(Skill);
		Row->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SHorizontalBox)
			.ToolTipText(bUnused ? LOCTEXT("SkillUnused", "Wird noch nicht gebraucht.") : FText::GetEmpty())
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(80.f)
				[
					SNew(STextBlock).Text(DCSkills::Name(Skill)).Font(Font(10, !bUnused)).ColorAndOpacity(bUnused ? MutedColor : TextColor)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(5.f)
				[
					SNew(SProgressBar).Style(&BarStyle()).Percent(Value / 10.f).FillColorAndOpacity(bUnused ? MutedColor : TitleColor)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(16.f)
				[
					SNew(STextBlock).Text(FText::AsNumber(Value)).Font(Font(10, true)).ColorAndOpacity(bUnused ? MutedColor : TextColor)
				]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(STextBlock).Text(LOCTEXT("SkillsTitle", "Fähigkeiten")).Font(Font(12, true)).ColorAndOpacity(TextColor)
		]
		+ SVerticalBox::Slot().AutoHeight()[Rows];
}

TSharedRef<SWidget> UDCInventoryPanel::MakeSlotCard(EDCEquipSlot EquipSlot)
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	UDCInventoryComponent* Inventory = Crew->GetInventory();
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Inventory;
	UDCItem* Item = Inventory->GetSlotItem(EquipSlot);
	const TSharedRef<FDCItemTreeContext> Ctx = Context.ToSharedRef();

	// The slot itself takes things to wear; its grid (clothes, vest, backpack) takes things to pack.
	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeDropZone(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeMuted(DCItems::DescribeSlot(EquipSlot), 10)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
				[
					Item ? MakeHeader(Item, Ctx, true, false) : MakeMuted(LOCTEXT("EmptySlot", "–"))
				],
				[WeakInventory, EquipSlot](UDCItem* Dragged) { return WeakInventory.IsValid() ? WeakInventory->CheckSlot(Dragged, EquipSlot) : FText::GetEmpty(); },
				[WeakThis, EquipSlot](UDCItem* Dragged)
				{
					if (WeakThis.IsValid())
					{
						WeakThis->MoveItem(Dragged, EquipSlot, nullptr);
					}
				},
				ZoneColor, FMargin(5.f, 3.f), DCItems::DescribeSlot(EquipSlot))
		];
	if (Item && Item->IsContainer())
	{
		Card->AddSlot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[MakeGrid(Item, Ctx)];
	}
	return Card;
}

TSharedRef<SWidget> UDCInventoryPanel::MakeHands()
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	UDCInventoryComponent* Inventory = Crew->GetInventory();
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Inventory;
	const TSharedRef<FDCItemTreeContext> Ctx = Context.ToSharedRef();

	// One big place per hand; something large fills both.
	const auto Hand = [&](EDCEquipSlot EquipSlot, const FText& Label, UDCItem* Item)
	{
		TSharedRef<SWidget> Filling = MakeMuted(LOCTEXT("EmptyHand", "frei"), 10);
		if (Item)
		{
			Filling = MakeTile(Item, Ctx,
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor_Lambda([Ctx, Weak = TWeakObjectPtr<UDCItem>(Item)]() { return Ctx->GetSelected && Ctx->GetSelected() == Weak.Get() ? FLinearColor(0.3f, 0.26f, 0.1f) : FLinearColor(0.14f, 0.15f, 0.19f); })
				.Padding(FMargin(5.f, 3.f))
				.Cursor(EMouseCursor::GrabHand)
				.ToolTipText(FText::Format(LOCTEXT("HandTip", "{0} · {1}"), Item->GetDisplayName(), DCItems::FormatKg(Item->GetTotalMass())))
				[
					SNew(STextBlock)
					.Text(Item->GetDisplayName())
					.Font(Font(11, true))
					.ColorAndOpacity(Item->GetDef().Value > 0 ? LootColor : TextColor)
					.AutoWrapText(true)
				]);
		}
		return MakeDropZone(
			SNew(SBox)
			.MinDesiredHeight(HandHeight)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeMuted(Label, 10)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Filling]
			],
			[WeakInventory, EquipSlot](UDCItem* Dragged) { return WeakInventory.IsValid() ? WeakInventory->CheckSlot(Dragged, EquipSlot) : FText::GetEmpty(); },
			[WeakThis, EquipSlot](UDCItem* Dragged)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->MoveItem(Dragged, EquipSlot, nullptr);
				}
			},
			ZoneColor, FMargin(5.f, 3.f), Label);
	};

	if (Inventory->HasBothHandsFull())
	{
		return Hand(EDCEquipSlot::RightHand, LOCTEXT("BothHands", "Beide Hände"), Inventory->GetSlotItem(EDCEquipSlot::RightHand));
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 3.f, 0.f)
		[
			Hand(EDCEquipSlot::LeftHand, DCItems::DescribeSlot(EDCEquipSlot::LeftHand), Inventory->GetSlotItem(EDCEquipSlot::LeftHand))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 0.f, 0.f, 0.f)
		[
			Hand(EDCEquipSlot::RightHand, DCItems::DescribeSlot(EDCEquipSlot::RightHand), Inventory->GetSlotItem(EDCEquipSlot::RightHand))
		];
}

#undef LOCTEXT_NAMESPACE
