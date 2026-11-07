#include "DCInventoryPanel.h"

#include "DCCrewBar.h"

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
	constexpr float SlotLabelWidth = 90.f;
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
	Context->CheckInto = [WeakThis](UDCItem* Item, UDCItem* Into)
	{
		const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
		return Member ? Member->GetInventory()->CheckInto(Item, Into) : FText::GetEmpty();
	};
	Context->MoveInto = [WeakThis](UDCItem* Item, UDCItem* Into)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->MoveItem(Item, EDCEquipSlot::None, Into);
		}
	};
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
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(UDCCrewBar::Width + 8.f, 12.f, 20.f, 20.f))
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
									SNew(STextBlock).Text(FText::FromString(TEXT("×"))).Font(Font(13, true))
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
									DCItems::FormatKg(Inventory->GetTotalMass()), DCItems::FormatKg(Inventory->GetMaxCarryMass()),
									FText::AsNumber(FMath::RoundToInt(Inventory->GetSpeedFactor() * 100.f)), DCItems::FormatEuro(Inventory->GetTotalValue()));
							})
							.Font(Font(11))
							.ColorAndOpacity(MutedColor)
						]
						+ SVerticalBox::Slot().MaxHeight(430.f)
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
							MakeMuted(LOCTEXT("Hint", "Ziehen zum Umpacken · auf einen Behälter ziehen = hineinlegen · aus dem Fenster ziehen = auf den Boden legen"), 10)
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
	const bool bMoved = Container ? Inventory->MoveInto(Item, Container, Reason) : Inventory->MoveToSlot(Item, EquipSlot, Reason);
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
	if (!Member || !Member->GetInventory() || !Context.IsValid())
	{
		Content->AddSlot().AutoHeight()[DCItemWidgets::MakeMuted(LOCTEXT("Nobody", "Niemand ausgewählt."), 12)];
		return;
	}
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[MakeSkills()];
	for (const EDCEquipSlot EquipSlot : { EDCEquipSlot::Head, EDCEquipSlot::Body, EDCEquipSlot::Vest, EDCEquipSlot::Back, EDCEquipSlot::LeftHand, EDCEquipSlot::RightHand })
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)[MakeSlotRow(EquipSlot)];
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

TSharedRef<SWidget> UDCInventoryPanel::MakeSlotRow(EDCEquipSlot EquipSlot)
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCInventoryPanel> WeakThis = this;
	UDCInventoryComponent* Inventory = Crew->GetInventory();
	TWeakObjectPtr<UDCInventoryComponent> WeakInventory = Inventory;
	UDCItem* Item = Inventory->GetSlotItem(EquipSlot);

	TSharedRef<SWidget> Filling = MakeMuted(LOCTEXT("EmptySlot", "–"));
	if (Item && EquipSlot == EDCEquipSlot::LeftHand && Inventory->HasBothHandsFull())
	{
		Filling = MakeMuted(FText::Format(LOCTEXT("BothHands", "trägt {0} mit beiden Händen"), Item->GetDisplayName()));
	}
	else if (Item)
	{
		Filling = MakeItem(Item, Context.ToSharedRef());
	}

	return MakeDropZone(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(SBox).WidthOverride(SlotLabelWidth)[MakeMuted(DCItems::DescribeSlot(EquipSlot))]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			Filling
		],
		[WeakInventory, EquipSlot](UDCItem* Dragged) { return WeakInventory.IsValid() ? WeakInventory->CheckSlot(Dragged, EquipSlot) : FText::GetEmpty(); },
		[WeakThis, EquipSlot](UDCItem* Dragged)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->MoveItem(Dragged, EquipSlot, nullptr);
			}
		},
		ZoneColor, FMargin(6.f, 3.f));
}

#undef LOCTEXT_NAMESPACE
