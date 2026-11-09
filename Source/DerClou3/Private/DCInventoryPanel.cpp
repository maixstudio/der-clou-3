#include "DCInventoryPanel.h"

#include "DCFrame.h"

#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCPlan.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
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
		if (WeakThis.IsValid() && WeakThis->IsPlanView())
		{
			return LOCTEXT("PlanViewOnly", "Das ist der Stand nach dem Plan \u2013 zum Umpacken oben auf \u201eJetzt\u201c schalten");
		}
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
	Context->GetActions = [WeakThis](UDCItem* Item)
	{
		return WeakThis.IsValid() && WeakThis->IsPlanView() ? TArray<EDCItemAction>() : DefaultActions(Item, false, true);
	};
	Context->IsPlanned = [WeakThis](const UDCItem* Item) { return WeakThis.IsValid() && WeakThis->PlannedOnly.Contains(Item); };
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
					.BorderImage(FramedBrush())
					.BorderBackgroundColor(PanelColor)
					.Padding(FMargin(16.f, 12.f))
					// Clicks on the window must not reach the game (they would send the crew there).
					.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					[
						SNew(SVerticalBox)
						// Portrait on the left, next to it title, view toggle and the summary line.
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 12.f, 0.f)
							[
								SAssignNew(PortraitBox, SBox)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top)
							[
								SNew(SVerticalBox)
								// Title, then the view toggle, then the summary – like the mockup, so long names still fit.
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeTitle(TAttribute<FText>::CreateLambda([WeakThis]()
									{
										const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
										return Member ? FText::Format(LOCTEXT("Title", "Profil – {0}"), Member->GetCrewName()) : LOCTEXT("TitleNobody", "Profil");
									}), 22)
								]
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 4.f, 0.f, 0.f)
								[
									MakeViewToggle()
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Text_Lambda([WeakThis]()
									{
										const UDCInventoryComponent* Inventory = WeakThis.IsValid() ? WeakThis->GetShownInventory() : nullptr;
										if (!Inventory)
										{
											return FText::GetEmpty();
										}
										return FText::Format(WeakThis->IsPlanView() ? LOCTEXT("SummaryPlan", "Nach dem Plan: {0} / {1} · Tempo {2} % · Wert {3}")
											: LOCTEXT("Summary", "Gewicht {0} / {1}  ·  Tempo {2} %  ·  Wert {3}"),
											DCItems::FormatKg(Inventory->GetTotalMass()), DCItems::FormatKg(Inventory->GetMaxCarryMass()),
											FText::AsNumber(FMath::RoundToInt(Inventory->GetSpeedFactor() * 100.f)), DCItems::FormatEuro(Inventory->GetTotalValue()));
									})
									.AutoWrapText(true)
									.Font(Font(12))
									.ColorAndOpacity(MutedColor)
								]
							]
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
							MakeMuted(LOCTEXT("Hint", "Anklicken = Knöpfe · ziehen = umpacken (grün passt, rot sagt warum) · auf einen Behälter ziehen = hineinlegen · aus dem Fenster ziehen = auf den Boden"), 11)
						]
					])
			]
		];
	bDirty = true;
	PortraitShown.Reset();
	return Root.ToSharedRef();
}

void UDCInventoryPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	Content.Reset();
	PortraitBox.Reset();
	PortraitShown.Reset();
}

void UDCInventoryPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	MessageTime = FMath::Max(0.f, MessageTime - InDeltaTime);
	// Portrait of whoever the window shows now.
	if (PortraitBox.IsValid() && Crew.Get() != PortraitShown.Get())
	{
		PortraitShown = Crew;
		const ADCCrewCharacter* Member = Crew.Get();
		PortraitBox->SetContent(Member ? DCItemWidgets::MakePortrait(Member->GetCrewName().ToString(), Member->GetBodyColor(), 60.f) : SNullWidget::NullWidget);
	}
	// The planned inventory is rebuilt after every plan change: show the new one.
	if (GetShownInventory() != LastShown.Get())
	{
		bDirty = true;
	}
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

void UDCInventoryPanel::SetPlanView(bool bInPlanView)
{
	bWantPlanView = bInPlanView;
	Selected.Reset();
	bDirty = true;
}

bool UDCInventoryPanel::IsPlanView() const
{
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const ADCCrewCharacter* Member = Crew.Get();
	return bWantPlanView && Plans && Heist && Member && Plans->GetMode() == EDCPlanMode::Planning && Heist->GetPlannedInventory(Member->GetFName());
}

UDCInventoryComponent* UDCInventoryPanel::GetShownInventory() const
{
	const ADCCrewCharacter* Member = Crew.Get();
	if (!Member)
	{
		return nullptr;
	}
	if (IsPlanView())
	{
		// Read-only: the window does not change it (moves are refused in this view).
		return const_cast<UDCInventoryComponent*>(UDCHeistStateSubsystem::Get(this)->GetPlannedInventory(Member->GetFName()));
	}
	return Member->GetInventory();
}

TSharedRef<SWidget> UDCInventoryPanel::MakeViewToggle()
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	// Segment 0 = now, 1 = after the plan.
	return SNew(SBox)
		.Visibility_Lambda([WeakThis]()
		{
			const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
			return Plans && Plans->GetMode() == EDCPlanMode::Planning ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			MakeSegments(
				{ LOCTEXT("ViewNow", "Jetzt"), LOCTEXT("ViewPlan", "Nach dem Plan") },
				{ LOCTEXT("ViewNowTip", "Was die Figur jetzt dabei hat \u2013 hier kannst du umpacken"),
					LOCTEXT("ViewPlanTip", "Was sie nach ihren geplanten Schritten dabei hat (Geplantes ist blau markiert)") },
				[WeakThis](int32 Index) { return WeakThis.IsValid() && WeakThis->IsPlanView() == (Index == 1); },
				[WeakThis](int32 Index)
				{
					if (WeakThis.IsValid())
					{
						WeakThis->SetPlanView(Index == 1);
					}
				})
		];
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
	if (IsPlanView())
	{
		ShowMessage(LOCTEXT("PlanViewMove", "Das ist der Stand nach dem Plan \u2013 zum Umpacken oben auf \u201eJetzt\u201c schalten"));
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
	if (IsPlanView())
	{
		return;
	}
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
	const UDCInventoryComponent* Inventory = GetShownInventory();
	LastShown = Inventory;
	// What comes only with the plan: everything the real inventory has fewer of.
	PlannedOnly.Reset();
	if (Inventory && Member && Inventory != Member->GetInventory())
	{
		TMap<FName, int32> Have;
		TFunction<void(const UDCItem*, TFunction<void(const UDCItem*)>)> Walk = [&Walk](const UDCItem* Item, TFunction<void(const UDCItem*)> Visit)
		{
			Visit(Item);
			for (const UDCItem* Child : Item->GetContents())
			{
				Walk(Child, Visit);
			}
		};
		for (const EDCEquipSlot EquipSlot : { EDCEquipSlot::Head, EDCEquipSlot::Body, EDCEquipSlot::Vest, EDCEquipSlot::Back, EDCEquipSlot::LeftHand, EDCEquipSlot::RightHand })
		{
			const UDCItem* Real = Member->GetInventory()->GetSlotItem(EquipSlot);
			if (Real && !(EquipSlot == EDCEquipSlot::LeftHand && Member->GetInventory()->HasBothHandsFull()))
			{
				Walk(Real, [&Have](const UDCItem* Item) { ++Have.FindOrAdd(Item->GetDefId()); });
			}
		}
		for (const EDCEquipSlot EquipSlot : { EDCEquipSlot::Head, EDCEquipSlot::Body, EDCEquipSlot::Vest, EDCEquipSlot::Back, EDCEquipSlot::LeftHand, EDCEquipSlot::RightHand })
		{
			const UDCItem* Planned = Inventory->GetSlotItem(EquipSlot);
			if (Planned && !(EquipSlot == EDCEquipSlot::LeftHand && Inventory->HasBothHandsFull()))
			{
				Walk(Planned, [this, &Have](const UDCItem* Item)
				{
					int32& Count = Have.FindOrAdd(Item->GetDefId());
					if (Count > 0)
					{
						--Count;
					}
					else
					{
						PlannedOnly.Add(Item);
					}
				});
			}
		}
	}
	if (!Inventory || !Context.IsValid())
	{
		Content->AddSlot().AutoHeight()[MakeMuted(LOCTEXT("Nobody", "Niemand ausgewählt."), 12)];
		return;
	}
	if (Selected.IsValid() && !Inventory->Owns(Selected.Get()))
	{
		Selected.Reset();
	}
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MakeSkills()];

	// Two columns: clothes and backpack on the left; head, vest and hands on the right.
	Content->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 6.f)[MakeSectionTitle(LOCTEXT("EquipmentTitle", "Ausrüstung"))];
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
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)
			[
				MakeIcon(IconForSkill(Skill), 14.f, MutedColor)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(88.f)
				[
					SNew(STextBlock).Text(DCSkills::Name(Skill)).Font(Font(12, !bUnused)).ColorAndOpacity(bUnused ? MutedColor : TextColor)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(4.f)
				[
					SNew(SProgressBar).Style(&BarStyle()).Percent(Value / 10.f).FillColorAndOpacity(bUnused ? MutedColor : TitleColor)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(18.f)
				[
					SNew(STextBlock).Text(FText::AsNumber(Value)).Font(Font(12, true)).ColorAndOpacity(bUnused ? MutedColor : TextColor).Justification(ETextJustify::Right)
				]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			MakeSectionTitle(LOCTEXT("SkillsTitle", "Fähigkeiten"))
		]
		+ SVerticalBox::Slot().AutoHeight()[Rows];
}

TSharedRef<SWidget> UDCInventoryPanel::MakeSlotCard(EDCEquipSlot EquipSlot)
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	UDCInventoryComponent* Inventory = GetShownInventory();
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Inventory;
	UDCItem* Item = Inventory->GetSlotItem(EquipSlot);
	const TSharedRef<FDCItemTreeContext> Ctx = Context.ToSharedRef();

	// The slot itself takes things to wear; its grid (clothes, vest, backpack) takes things to pack.
	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeDropZone(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeMuted(DCItems::DescribeSlot(EquipSlot), 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
				[
					Item ? MakeHeader(Item, Ctx, true, false) : MakeMuted(LOCTEXT("EmptySlot", "–"))
				],
				[WeakThis, EquipSlot](UDCItem* Dragged)
				{
					if (!WeakThis.IsValid() || !WeakThis->Crew.IsValid())
					{
						return FText::GetEmpty();
					}
					return WeakThis->IsPlanView() ? LOCTEXT("PlanViewOnlySlot", "Das ist der Stand nach dem Plan \u2013 zum Umpacken oben auf \u201eJetzt\u201c schalten")
						: WeakThis->Crew->GetInventory()->CheckSlot(Dragged, EquipSlot);
				},
				[WeakThis, EquipSlot](UDCItem* Dragged)
				{
					if (WeakThis.IsValid())
					{
						WeakThis->MoveItem(Dragged, EquipSlot, nullptr);
					}
				},
				ZoneColor, FMargin(7.f, 4.f), DCItems::DescribeSlot(EquipSlot))
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
	UDCInventoryComponent* Inventory = GetShownInventory();
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Inventory;
	const TSharedRef<FDCItemTreeContext> Ctx = Context.ToSharedRef();

	// One big place per hand; something large fills both.
	const auto Hand = [&](EDCEquipSlot EquipSlot, const FText& Label, UDCItem* Item)
	{
		TSharedRef<SWidget> Filling = MakeMuted(LOCTEXT("EmptyHand", "frei"), 12);
		if (Item)
		{
			Filling = MakeTile(Item, Ctx,
				SNew(SBorder)
				.BorderImage_Lambda([Ctx, Weak = TWeakObjectPtr<UDCItem>(Item)]() { return Ctx->GetSelected && Ctx->GetSelected() == Weak.Get() ? FocusFramedBrush() : SoftFramedBrush(); })
				.BorderBackgroundColor(TileColor)
				.Padding(FMargin(5.f, 3.f))
				.Cursor(EMouseCursor::GrabHand)
				.ToolTipText(FText::Format(LOCTEXT("HandTip", "{0} · {1}"), Item->GetDisplayName(), DCItems::FormatKg(Item->GetTotalMass())))
				[
					SNew(STextBlock)
					.Text(PlannedOnly.Contains(Item) ? FText::Format(LOCTEXT("HandPlanned", "{0} \u00b7 geplant"), Item->GetDisplayName()) : Item->GetDisplayName())
					.Font(Font(11, true))
					.ColorAndOpacity(PlannedOnly.Contains(Item) ? CoordinationColor : (Item->GetDef().Value > 0 ? LootColor : TextColor))
					.AutoWrapText(true)
				]);
		}
		return MakeDropZone(
			SNew(SBox)
			.MinDesiredHeight(HandHeight)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeMuted(Label, 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Filling]
			],
			[WeakThis, EquipSlot](UDCItem* Dragged)
				{
					if (!WeakThis.IsValid() || !WeakThis->Crew.IsValid())
					{
						return FText::GetEmpty();
					}
					return WeakThis->IsPlanView() ? LOCTEXT("PlanViewOnlySlot", "Das ist der Stand nach dem Plan \u2013 zum Umpacken oben auf \u201eJetzt\u201c schalten")
						: WeakThis->Crew->GetInventory()->CheckSlot(Dragged, EquipSlot);
				},
			[WeakThis, EquipSlot](UDCItem* Dragged)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->MoveItem(Dragged, EquipSlot, nullptr);
				}
			},
			ZoneColor, FMargin(7.f, 4.f), Label);
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
