#include "DCRadialMenu.h"

#include "DerClou3.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float RadiusX = 125.f;
	constexpr float RadiusY = 85.f;
	/** Each option beyond four pushes the circle a bit further out. */
	constexpr float GrowPerOption = 22.f;
	const FVector2D ButtonSize(150.0, 40.0);
	const FVector2D DetailButtonSize(250.0, 50.0);
	const FLinearColor DisabledColor(0.55f, 0.57f, 0.6f);
	const FLinearColor DetailColor(0.75f, 0.78f, 0.82f);
	const FVector2D TitleSize(200.0, 44.0);

	FSlateFontInfo RadialFont(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

TSharedRef<SWidget> UDCRadialMenu::RebuildWidget()
{
	Super::RebuildWidget();
	SAssignNew(Canvas, SCanvas).Visibility(EVisibility::SelfHitTestInvisible);
	return Canvas.ToSharedRef();
}

void UDCRadialMenu::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Canvas.Reset();
}

void UDCRadialMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastGeometry = MyGeometry;
	if (bClearPending && Canvas.IsValid())
	{
		Canvas->ClearChildren();
		bClearPending = false;
	}
}

void UDCRadialMenu::Open(const FText& Title, TArray<FDCRadialOption> Options)
{
	ShownTitle = Title;
	if (!Canvas.IsValid() || Options.IsEmpty())
	{
		return;
	}
	Canvas->ClearChildren();
	bClearPending = false;
	bOpen = true;
	Current = Options;

	const FVector2D Centre = LastGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());

	Canvas->AddSlot()
		.Position(Centre)
		.Size(TitleSize)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.92f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(STextBlock).Text(Title).Font(RadialFont(12, true)).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.25f)).Justification(ETextJustify::Center)
			]
		];

	// Options on an ellipse around the cursor, starting at the top.
	const bool bDetails = Options.ContainsByPredicate([](const FDCRadialOption& Option) { return !Option.Detail.IsEmpty(); });
	const float Grow = FMath::Max(0, Options.Num() - 4) * GrowPerOption + (bDetails ? 60.f : 0.f);
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const float Angle = -UE_HALF_PI + UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Options.Num());
		const FVector2D Position = Centre + FVector2D(FMath::Cos(Angle) * (RadiusX + Grow), FMath::Sin(Angle) * (RadiusY + Grow * 0.6f));
		TFunction<void()> OnSelected = MoveTemp(Options[Index].OnSelected);
		const FDCRadialOption& Option = Options[Index];
		Canvas->AddSlot()
			.Position(Position)
			.Size(bDetails ? DetailButtonSize : ButtonSize)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				// Never take keyboard focus: the camera keys must keep reaching the game.
				.IsFocusable(false)
				.ToolTipText(Option.Tooltip)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([this, OnSelected, Label = Option.Label, Detail = Option.Detail, bEnabled = Option.bEnabled]()
				{
					UE_LOG(LogDerClou, Display, TEXT("Menu: %s (%s)%s"), *Label.ToString(), *Detail.ToString(), bEnabled ? TEXT("") : TEXT(" [not possible]"));
					Close();
					if (OnSelected)
					{
						OnSelected();
					}
					return FReply::Handled();
				})
				[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Text(Option.Label).Font(RadialFont(13, true))
							.ColorAndOpacity(Option.bEnabled ? FLinearColor::White : DisabledColor)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Text(Option.Detail).Font(RadialFont(9, false))
							.ColorAndOpacity(Option.bEnabled ? DetailColor : DisabledColor)
							.Visibility(Option.Detail.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						]
					]
			];
	}
}

bool UDCRadialMenu::ChooseOption(const FString& LabelPart)
{
	for (const FDCRadialOption& Option : Current)
	{
		if (bOpen && Option.Label.ToString().Contains(LabelPart) && Option.OnSelected)
		{
			const TFunction<void()> Selected = Option.OnSelected;
			Close();
			Selected();
			return true;
		}
	}
	return false;
}

void UDCRadialMenu::Close()
{
	bClearPending = bClearPending || bOpen;
	bOpen = false;
}
