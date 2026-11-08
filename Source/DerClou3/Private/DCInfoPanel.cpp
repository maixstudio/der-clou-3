#include "DCInfoPanel.h"

#include "DCFrame.h"
#include "DCItemWidgets.h"
#include "DCInteractable.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCInfoPanel"

namespace
{
	FSlateFontInfo InfoFont(int32 Size, bool bBold = false)
	{
		return DCItemWidgets::Font(Size, bBold);
	}

	/** Labels or values of all lines, one per row (two text columns stay aligned). */
	FText JoinColumn(const TArray<FDCInfoLine>& Lines, bool bLabels)
	{
		TArray<FString> Rows;
		for (const FDCInfoLine& Line : Lines)
		{
			Rows.Add((bLabels ? Line.Label : Line.Value).ToString());
		}
		return FText::FromString(FString::Join(Rows, TEXT("\n")));
	}
}

TSharedRef<SWidget> UDCInfoPanel::RebuildWidget()
{
	Super::RebuildWidget();
	TWeakObjectPtr<UDCInfoPanel> WeakThis = this;

	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, UDCFrame::TopHeight + 12.f, UDCFrame::RightWidth + 12.f, 0.f))
		[
			SNew(SBox)
			.MinDesiredWidth(340.f)
			.MaxDesiredWidth(560.f)
			.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsShown() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(DCItemWidgets::FramedBrush())
				.BorderBackgroundColor(DCItemWidgets::PanelColor)
				.Padding(FMargin(16.f, 12.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							DCItemWidgets::MakeTitle(TAttribute<FText>::CreateLambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetTitle() : FText::GetEmpty(); }), 20)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.IsFocusable(false)
							.ButtonStyle(&DCItemWidgets::NormalButtonStyle())
							.ContentPadding(FMargin(7.f, 0.f))
							.ToolTipText(LOCTEXT("CloseTip", "Schließen"))
							.OnClicked_Lambda([WeakThis]()
							{
								if (WeakThis.IsValid())
								{
									WeakThis->Hide();
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("×"))).Font(InfoFont(13, true))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(STextBlock)
						.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetKind() : FText::GetEmpty(); })
						.Font(InfoFont(13))
						.ColorAndOpacity(DCItemWidgets::MutedColor)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetLabels() : FText::GetEmpty(); })
							.Font(InfoFont(13))
							.ColorAndOpacity(DCItemWidgets::MutedColor)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetValues() : FText::GetEmpty(); })
							.Font(InfoFont(13, true))
							.ColorAndOpacity(DCItemWidgets::TextColor)
						]
					]
				]
			]
		];
	return Root.ToSharedRef();
}

void UDCInfoPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
}

void UDCInfoPanel::ShowFor(UObject* Interactable)
{
	Target = Cast<IDCInteractable>(Interactable) ? Interactable : nullptr;
}

void UDCInfoPanel::Hide()
{
	Target.Reset();
}

FText UDCInfoPanel::GetTitle() const
{
	const IDCInteractable* Object = Cast<IDCInteractable>(Target.Get());
	return Object ? Object->GetInteractableName() : FText::GetEmpty();
}

FText UDCInfoPanel::GetKind() const
{
	const IDCInteractable* Object = Cast<IDCInteractable>(Target.Get());
	return Object ? Object->GetInteractableKind() : FText::GetEmpty();
}

FText UDCInfoPanel::GetLabels() const
{
	TArray<FDCInfoLine> Lines;
	if (const IDCInteractable* Object = Cast<IDCInteractable>(Target.Get()))
	{
		Object->GetInfoLines(Lines);
	}
	return JoinColumn(Lines, true);
}

FText UDCInfoPanel::GetValues() const
{
	TArray<FDCInfoLine> Lines;
	if (const IDCInteractable* Object = Cast<IDCInteractable>(Target.Get()))
	{
		Object->GetInfoLines(Lines);
	}
	return JoinColumn(Lines, false);
}

#undef LOCTEXT_NAMESPACE
