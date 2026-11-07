#include "DCContainerPanel.h"

#include "DCFrame.h"
#include "DCCrewCharacter.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCItemWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCContainerPanel"

namespace
{
	constexpr float ContainerWidth = 430.f;
	/** The window closes once the crew member is this far from the edge of the container. */
	constexpr float ReachDistance = 150.f;

	void CollectTree(UDCItem* Item, TArray<UDCItem*>& Out)
	{
		Out.Add(Item);
		for (UDCItem* Child : Item->GetContents())
		{
			CollectTree(Child, Out);
		}
	}

	FText JoinItemNames(const TArray<const UDCItem*>& Items)
	{
		TArray<FString> Names;
		for (const UDCItem* Item : Items)
		{
			Names.Add(Item->GetDisplayName().ToString());
		}
		return Names.IsEmpty() ? FText::FromString(TEXT("\u2013")) : FText::FromString(FString::Join(Names, TEXT(", ")));
	}
}

void UDCContainerPanel::NativeConstruct()
{
	Super::NativeConstruct();
	ItemsChangedHandle = UDCItem::OnAnyChanged().AddWeakLambda(this, [this]() { bDirty = true; });
}

void UDCContainerPanel::NativeDestruct()
{
	UDCItem::OnAnyChanged().Remove(ItemsChangedHandle);
	Super::NativeDestruct();
}

TSharedRef<SWidget> UDCContainerPanel::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCContainerPanel> WeakThis = this;

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
			WeakThis->MoveInto(Item, Into, Cell);
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
		TArray<EDCItemAction> Actions = DefaultActions(Item, true, false);
		// While the game is paused for a find, only quick handling: nothing that takes a while.
		if (WeakThis.IsValid() && WeakThis->IsFindMode())
		{
			Actions.RemoveAll([](EDCItemAction Action) { return Action != EDCItemAction::Take && Action != EDCItemAction::Open && Action != EDCItemAction::Close; });
		}
		return Actions;
	};
	Context->CheckAction = [WeakThis](UDCItem* Item, EDCItemAction Action)
	{
		const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
		return Member ? Member->CheckItemAction(Item, Action) : FDCTaskCheck();
	};
	Context->DoAction = [WeakThis](UDCItem* Item, EDCItemAction Action)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->DoAction(Item, Action);
		}
	};
	// Things from the world dropped next to every window simply stay where they are.

	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, UDCFrame::TopHeight + 12.f, UDCFrame::RightWidth + 12.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(ContainerWidth)
			.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				MakeSwallowZone(
					SNew(SBorder)
					.BorderImage(Brush())
					.BorderBackgroundColor(PanelColor)
					.Padding(FMargin(14.f, 12.f))
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
									const ADCItemPickup* Container = WeakThis.IsValid() ? WeakThis->Pickup.Get() : nullptr;
									if (!Container)
									{
										return FText::GetEmpty();
									}
									return WeakThis->IsFindMode() ? FText::Format(LOCTEXT("FindTitle", "Fund – {0}"), Container->GetInteractableName())
										: FText::Format(LOCTEXT("Title", "Durchsuchen – {0}"), Container->GetInteractableName());
								})
								.Font(Font(15, true))
								.ColorAndOpacity(TitleColor)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.IsFocusable(false)
								.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsFindMode() ? EVisibility::Collapsed : EVisibility::Visible; })
								.ToolTipText(LOCTEXT("CloseTip", "Schließen"))
								.OnClicked_Lambda([WeakThis]()
								{
									if (WeakThis.IsValid())
									{
										WeakThis->Close();
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
								const ADCItemPickup* Container = WeakThis.IsValid() ? WeakThis->Pickup.Get() : nullptr;
								if (!Container)
								{
									return FText::GetEmpty();
								}
								if (WeakThis->IsFindMode())
								{
									const ADCCrewCharacter* Member = WeakThis->Crew.Get();
									return FText::Format(LOCTEXT("FindSubtitle", "{0} hat gefunden: {1}"), Member ? Member->GetCrewName() : FText::GetEmpty(), WeakThis->DescribeFound());
								}
								return Container->GetInteractableKind();
							})
							.AutoWrapText(true)
							.Font_Lambda([WeakThis]() { return Font(WeakThis.IsValid() && WeakThis->IsFindMode() ? 12 : 11, WeakThis.IsValid() && WeakThis->IsFindMode()); })
							.ColorAndOpacity_Lambda([WeakThis]() { return FSlateColor(WeakThis.IsValid() && WeakThis->IsFindMode() ? LootColor : MutedColor); })
						]
						+ SVerticalBox::Slot().MaxHeight(560.f)
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
							SNew(STextBlock)
							.Text_Lambda([WeakThis]()
							{
								return WeakThis.IsValid() && WeakThis->IsFindMode()
									? LOCTEXT("FindHint", "Das Spiel wartet. Ins Profil ziehen oder „nehmen“ = mitnehmen · drin lassen = liegen lassen · Eigenes hierher ziehen = dalassen · jeder Handgriff kostet Spielzeit (nehmen 0,6 s, umpacken 1 s)")
									: LOCTEXT("Hint", "Anklicken = Knöpfe · ins Profil ziehen oder „nehmen“ · Dinge aus dem Profil hierher ziehen = verstecken");
							})
							.Font(Font(10))
							.ColorAndOpacity(MutedColor)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
						[
							MakeFindBar()
						]
					])
			]
		];
	bDirty = true;
	return Root.ToSharedRef();
}

void UDCContainerPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	Content.Reset();
}

void UDCContainerPanel::Open(ADCCrewCharacter* InCrew, ADCItemPickup* InPickup)
{
	if (Pickup.Get() != InPickup)
	{
		Selected.Reset();
	}
	Crew = InCrew;
	Pickup = InPickup;
	Message = FText::GetEmpty();
	MessageTime = 0.f;
	bDirty = true;
}

void UDCContainerPanel::Close()
{
	Pickup.Reset();
	Crew.Reset();
	bFindMode = false;
	OnContinue = nullptr;
}

void UDCContainerPanel::OpenFind(ADCCrewCharacter* InCrew, ADCItemPickup* InPickup, TFunction<void()> InOnContinue)
{
	Open(InCrew, InPickup);
	bFindMode = true;
	OnContinue = MoveTemp(InOnContinue);
	HandlingSeconds = 0.f;
	HandlingCount = 0;
	Found.Reset();
	StartTree.Reset();
	Selected.Reset();
	UDCItem* Box = InPickup ? InPickup->GetItem() : nullptr;
	if (!Box)
	{
		return;
	}
	TArray<UDCItem*> All;
	CollectTree(Box, All);
	TArray<UDCItem*> Takeable;
	for (UDCItem* Item : All)
	{
		StartTree.Add(Item);
		if (Item != Box && !Item->IsAttached() && Item->GetDef().bMovable && Item->GetParent() && Item->GetParent()->IsReachable())
		{
			Takeable.Add(Item);
		}
	}
	Takeable.Sort([](const UDCItem& A, const UDCItem& B) { return A.GetDef().Value > B.GetDef().Value; });
	for (UDCItem* Item : Takeable)
	{
		Found.Add(Item);
	}
}

bool UDCContainerPanel::Owns(const UDCItem* Item) const
{
	const ADCCrewCharacter* Member = Crew.Get();
	return Member && Item && Member->GetInventory()->Owns(Item);
}

void UDCContainerPanel::AddHandling(float Seconds)
{
	if (!bFindMode)
	{
		return;
	}
	HandlingSeconds += Seconds;
	++HandlingCount;
}

void UDCContainerPanel::TakeAllValuable()
{
	ADCCrewCharacter* Member = Crew.Get();
	ADCItemPickup* Container = Pickup.Get();
	UDCItem* Box = Container ? Container->GetItem() : nullptr;
	if (!Member || !Box)
	{
		return;
	}
	TArray<const UDCItem*> NoRoom;
	for (const TWeakObjectPtr<UDCItem>& Weak : Found)
	{
		UDCItem* Item = Weak.Get();
		// Still in the container and not already taken along with something else.
		if (!Item || Item->GetDef().Value <= 0 || Owns(Item) || !Item->IsInside(Box))
		{
			continue;
		}
		const FText Reason = Member->PerformItemAction(Item, EDCItemAction::Take);
		if (Reason.IsEmpty())
		{
			AddHandling(DCItems::FindTakeSeconds);
		}
		else
		{
			NoRoom.Add(Item);
		}
	}
	if (!NoRoom.IsEmpty())
	{
		ShowMessage(FText::Format(LOCTEXT("NoRoomLeft", "Kein Platz mehr für: {0}"), JoinItemNames(NoRoom)));
	}
}

void UDCContainerPanel::ContinueFind()
{
	// The callback closes this window, which clears OnContinue: keep it alive while it runs.
	TFunction<void()> Callback = MoveTemp(OnContinue);
	OnContinue = nullptr;
	if (Callback)
	{
		Callback();
	}
}

FText UDCContainerPanel::DescribeFound() const
{
	TArray<const UDCItem*> Items;
	for (const TWeakObjectPtr<UDCItem>& Weak : Found)
	{
		if (const UDCItem* Item = Weak.Get())
		{
			Items.Add(Item);
		}
	}
	return JoinItemNames(Items);
}

FText UDCContainerPanel::DescribeFindResult() const
{
	TArray<const UDCItem*> Taken;
	TArray<const UDCItem*> Left;
	for (const TWeakObjectPtr<UDCItem>& Weak : Found)
	{
		if (const UDCItem* Item = Weak.Get())
		{
			(Owns(Item) ? Taken : Left).Add(Item);
		}
	}
	TArray<const UDCItem*> Deposited;
	ADCItemPickup* Container = Pickup.Get();
	if (UDCItem* Box = Container ? Container->GetItem() : nullptr)
	{
		TArray<UDCItem*> All;
		CollectTree(Box, All);
		for (const UDCItem* Item : All)
		{
			if (!StartTree.Contains(Item))
			{
				Deposited.Add(Item);
			}
		}
	}
	FText Result = FText::Format(LOCTEXT("FindResult", "mitgenommen: {0} · liegen gelassen: {1}"), JoinItemNames(Taken), JoinItemNames(Left));
	if (!Deposited.IsEmpty())
	{
		Result = FText::Format(LOCTEXT("FindResultLeft", "{0} · dagelassen: {1}"), Result, JoinItemNames(Deposited));
	}
	return Result;
}

TSharedRef<SWidget> UDCContainerPanel::MakeFindBar()
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCContainerPanel> WeakThis = this;
	return SNew(SVerticalBox)
		.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsFindMode() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Text_Lambda([WeakThis]()
			{
				if (!WeakThis.IsValid())
				{
					return FText::GetEmpty();
				}
				const FString Seconds = FString::Printf(TEXT("%.1f"), WeakThis->HandlingSeconds).Replace(TEXT("."), TEXT(","));
				return FText::Format(LOCTEXT("Handling", "{0} Handgriffe \u00b7 kosten {1} s Spielzeit"), FText::AsNumber(WeakThis->HandlingCount), FText::FromString(Seconds));
			})
			.Font(Font(11))
			.ColorAndOpacity(MutedColor)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("TakeAllTip", "Nimmt alles mit Wert, das noch Platz hat – das Wertvollste zuerst"))
			.ContentPadding(FMargin(8.f, 3.f))
			.OnClicked_Lambda([WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->TakeAllValuable();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("TakeAll", "Alles Wertvolle nehmen")).Font(Font(11))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ButtonStyle(&PrimaryButtonStyle())
			.ToolTipText(LOCTEXT("ContinueTip", "Entscheidung fertig – der Einbruch geht weiter (Enter)"))
			.ContentPadding(FMargin(14.f, 4.f))
			.OnClicked_Lambda([WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->ContinueFind();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("Continue", "Weiter")).Font(Font(12, true)).ColorAndOpacity(DarkTextColor)
			]
		]
		];
}

void UDCContainerPanel::ShowMessage(const FText& Text)
{
	Message = Text;
	MessageTime = 4.f;
}

void UDCContainerPanel::SelectItem(UDCItem* Item)
{
	Selected = Selected.Get() == Item ? nullptr : Item;
	bDirty = true;
}

void UDCContainerPanel::MoveInto(UDCItem* Item, UDCItem* Into, FIntPoint Cell)
{
	ADCCrewCharacter* Member = Crew.Get();
	if (!Member || !Item || !Into)
	{
		return;
	}
	FText Reason;
	const bool bWasOwned = Owns(Item);
	const FIntPoint OldCell = Item->GetGridPos();
	const UDCItem* OldParent = Item->GetParent();
	if (Member->GetInventory()->MoveInto(Item, Into, Reason, Cell))
	{
		if (Item->GetParent() != OldParent || Item->GetGridPos() != OldCell)
		{
			AddHandling(!bWasOwned && Owns(Item) ? DCItems::FindTakeSeconds : DCItems::FindRepackSeconds);
		}
	}
	else
	{
		ShowMessage(Reason);
	}
}

void UDCContainerPanel::DoAction(UDCItem* Item, EDCItemAction Action)
{
	ADCCrewCharacter* Member = Crew.Get();
	ADCItemPickup* Container = Pickup.Get();
	if (!Member || !Container || !Item)
	{
		return;
	}
	const FText Reason = Member->PerformItemAction(Item, Action, Item == Container->GetItem() ? Container : nullptr);
	if (!Reason.IsEmpty())
	{
		ShowMessage(Reason);
	}
	else if (Action == EDCItemAction::Take || Action == EDCItemAction::Open || Action == EDCItemAction::Close)
	{
		AddHandling(DCItems::FindTakeSeconds);
	}
}

void UDCContainerPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	MessageTime = FMath::Max(0.f, MessageTime - InDeltaTime);

	// Only while the crew member stands next to it (and it still exists).
	const ADCCrewCharacter* Member = Crew.Get();
	ADCItemPickup* Container = Pickup.Get();
	if (Pickup.IsValid() && !bFindMode && (!Member || !Container || Container->IsActorBeingDestroyed()
		|| FVector::Dist2D(Member->GetActorLocation(), Container->GetActorLocation()) > Container->GetFootprintRadius() + ReachDistance))
	{
		Close();
	}
	if (bDirty && Content.IsValid())
	{
		bDirty = false;
		RebuildContent();
	}
}

void UDCContainerPanel::RebuildContent()
{
	Content->ClearChildren();
	ADCItemPickup* Container = Pickup.Get();
	UDCItem* Item = Container ? Container->GetItem() : nullptr;
	if (!Item || !Context.IsValid())
	{
		return;
	}
	// The container itself is not dragged; it can be taken with its button if it is portable.
	Content->AddSlot().AutoHeight()[DCItemWidgets::MakeSection(Item, Context.ToSharedRef(), false)];

	// What the window shows anyway: the container and its fixed parts. Below: the selected item.
	TArray<UDCItem*> Shown = { Item };
	for (UDCItem* Part : Item->GetContents())
	{
		if (Part->IsAttached())
		{
			Shown.Add(Part);
		}
	}
	UDCItem* Pick = Selected.Get();
	if (Pick && !Pick->IsInside(Item))
	{
		Selected.Reset();
		Pick = nullptr;
	}
	if (Pick && !Shown.Contains(Pick))
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[DCItemWidgets::MakeSelection(Pick, Shown, Context.ToSharedRef())];
	}
	if (!Item->IsContainer() && Item->GetContents().IsEmpty())
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			DCItemWidgets::MakeMuted(FText::Format(LOCTEXT("NoSpace", "{0} hat keinen Platz für etwas anderes."), Item->GetDisplayName()))
		];
	}
}

#undef LOCTEXT_NAMESPACE
