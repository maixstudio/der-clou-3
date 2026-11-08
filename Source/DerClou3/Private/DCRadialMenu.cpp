#include "DCRadialMenu.h"

#include "DerClou3.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "DCItemWidgets.h"

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
	const FVector2D ButtonSize(170.0, 44.0);
	const FVector2D DetailButtonSize(260.0, 56.0);
	const FLinearColor DetailColor(0.4851f, 0.5089f, 0.5457f); // #B8BCC3
	const FVector2D TitleSize(220.0, 38.0);

	FSlateFontInfo RadialFont(int32 Size, bool bBold)
	{
		return DCItemWidgets::Font(Size, bBold);
	}

	/** Pill-shaped option: dark with a brass outline, amber outline under the mouse; dimmer when not possible. */
	const FButtonStyle& RadialButtonStyle(bool bPossible)
	{
		using namespace DCItemWidgets;
		const auto Make = [](const FLinearColor& Edge, float Width)
		{
			return FButtonStyle()
				.SetNormal(FSlateRoundedBoxBrush(PanelColor, Edge, Width))
				.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.0296f, 0.0395f, 0.0561f, 0.98f), TitleColor, 2.f))
				.SetPressed(FSlateRoundedBoxBrush(ZoneColor, TitleColor, 2.f))
				.SetDisabled(FSlateRoundedBoxBrush(PanelColor, BrassDimColor, 1.f))
				.SetNormalForeground(TextColor)
				.SetHoveredForeground(FLinearColor::White)
				.SetPressedForeground(TextColor)
				.SetDisabledForeground(MutedColor)
				.SetNormalPadding(FMargin(0.f))
				.SetPressedPadding(FMargin(0.f));
		};
		static const FButtonStyle Possible = Make(BrassColor, 1.5f);
		static const FButtonStyle Impossible = Make(BrassDimColor, 1.f);
		return bPossible ? Possible : Impossible;
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
			.BorderImage(DCItemWidgets::FramedBrush())
			.BorderBackgroundColor(DCItemWidgets::PanelColor)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10.f, 2.f))
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(STextBlock).Text(Title).Font(DCItemWidgets::HeadFont(15)).ColorAndOpacity(DCItemWidgets::TextColor)
				.TransformPolicy(ETextTransformPolicy::ToUpper).Justification(ETextJustify::Center)
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
				.ButtonStyle(&RadialButtonStyle(Option.bEnabled))
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
							SNew(STextBlock).Text(Option.Label).Font(DCItemWidgets::HeadFont(16))
							.TransformPolicy(ETextTransformPolicy::ToUpper)
							.ColorAndOpacity(Option.bEnabled ? DCItemWidgets::TextColor : DCItemWidgets::MutedColor)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock).Text(Option.Detail).Font(RadialFont(11, false))
							.ColorAndOpacity(!Option.bEnabled ? DCItemWidgets::MutedColor
								: Option.Tone == EDCOptionTone::Problem ? DCItemWidgets::MessageColor
								: Option.Tone == EDCOptionTone::Waits ? DCItemWidgets::CoordinationColor
								: DetailColor)
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
