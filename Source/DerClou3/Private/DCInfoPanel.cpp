#include "DCInfoPanel.h"

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
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
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
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(20.f)
		[
			SNew(SBox)
			.MinDesiredWidth(340.f)
			.MaxDesiredWidth(560.f)
			.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsShown() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.03f, 0.035f, 0.05f, 0.92f))
				.Padding(FMargin(16.f, 12.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetTitle() : FText::GetEmpty(); })
							.Font(InfoFont(15, true))
							.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.25f))
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
						.Font(InfoFont(11))
						.ColorAndOpacity(FLinearColor(0.6f, 0.63f, 0.68f))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetLabels() : FText::GetEmpty(); })
							.Font(InfoFont(12))
							.ColorAndOpacity(FLinearColor(0.65f, 0.68f, 0.73f))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetValues() : FText::GetEmpty(); })
							.Font(InfoFont(12, true))
							.ColorAndOpacity(FLinearColor(0.93f, 0.94f, 0.96f))
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
