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
		return DefaultActions(Item, true, false);
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
									return Container ? FText::Format(LOCTEXT("Title", "Durchsuchen – {0}"), Container->GetInteractableName()) : FText::GetEmpty();
								})
								.Font(Font(15, true))
								.ColorAndOpacity(TitleColor)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.IsFocusable(false)
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
								return Container ? Container->GetInteractableKind() : FText::GetEmpty();
							})
							.Font(Font(11))
							.ColorAndOpacity(MutedColor)
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
							MakeMuted(LOCTEXT("Hint", "Anklicken = Knöpfe · ins Profil ziehen oder „nehmen“ · Dinge aus dem Profil hierher ziehen = verstecken"), 10)
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
	if (!Member->GetInventory()->MoveInto(Item, Into, Reason, Cell))
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
}

void UDCContainerPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	MessageTime = FMath::Max(0.f, MessageTime - InDeltaTime);

	// Only while the crew member stands next to it (and it still exists).
	const ADCCrewCharacter* Member = Crew.Get();
	const ADCItemPickup* Container = Pickup.Get();
	if (Pickup.IsValid() && (!Member || !Container || Container->IsActorBeingDestroyed()
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
