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
	const FVector2D TitleSize(260.0, 50.0);

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
	if (bSubPending)
	{
		bSubPending = false;
		Build(PendingTitle, MoveTemp(PendingOptions), LastCentre);
	}
}

void UDCRadialMenu::Open(const FText& Title, TArray<FDCRadialOption> Options)
{
	bSubPending = false;
	const FVector2D Centre = bOpenAtCentreForTests ? FVector2D(LastGeometry.GetLocalSize()) * 0.5
		: LastGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	Build(Title, MoveTemp(Options), Centre);
}

void UDCRadialMenu::OpenSub(const FText& Title, TArray<FDCRadialOption> Options)
{
	if (Options.IsEmpty())
	{
		return;
	}
	ShownTitle = Title;
	Current = Options;
	PendingTitle = Title;
	PendingOptions = MoveTemp(Options);
	bSubPending = true;
	bOpen = true;
}

void UDCRadialMenu::Build(const FText& Title, TArray<FDCRadialOption> Options, const FVector2D& Centre)
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
	LastCentre = Centre;

	// The title has the object on the first line and who plans on the second ("Wandtresor" / "plant: Rosa").
	const FString FullTitle = Title.ToString();
	FString TitleName = FullTitle;
	FString TitleRest;
	FullTitle.Split(TEXT("\n"), &TitleName, &TitleRest);
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
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(DCItemWidgets::Upper(FText::FromString(TitleName))).Font(DCItemWidgets::HeadFont(15)).ColorAndOpacity(DCItemWidgets::TextColor)
					.Justification(ETextJustify::Center)
				]
				// "plant: Rosa" smaller underneath.
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TitleRest)).Font(DCItemWidgets::Font(11)).ColorAndOpacity(DCItemWidgets::MutedColor)
					.Justification(ETextJustify::Center)
					.Visibility(TitleRest.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
			]
		];

	// Options on an ellipse around the cursor, starting at the top.
	const bool bDetails = Options.ContainsByPredicate([](const FDCRadialOption& Option) { return !Option.Detail.IsEmpty(); });
	const float Grow = FMath::Max(0, Options.Num() - 4) * GrowPerOption + (bDetails ? 60.f : 0.f);
	// Wide enough that the options left and right of the title never cover it.
	const FVector2D OptionSize = bDetails ? DetailButtonSize : ButtonSize;
	const float SpreadX = FMath::Max(RadiusX + Grow, static_cast<float>(TitleSize.X + OptionSize.X) * 0.5f + 12.f);
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const float Angle = -UE_HALF_PI + UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Options.Num());
		const FVector2D Position = Centre + FVector2D(FMath::Cos(Angle) * SpreadX, FMath::Sin(Angle) * (RadiusY + Grow * 0.6f));
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
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, Option.Icon.IsNone() ? 0.f : 7.f, 1.f)
							[
								DCItemWidgets::MakeIcon(Option.Icon, 18.f, Option.bEnabled ? DCItemWidgets::TextColor : DCItemWidgets::MutedColor)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(DCItemWidgets::Upper(Option.Label)).Font(DCItemWidgets::HeadFont(16))
								.ColorAndOpacity(Option.bEnabled ? DCItemWidgets::TextColor : DCItemWidgets::MutedColor)
							]
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
			// Not inside a click of the menu: a ring the entry opens can be built at once.
			if (bSubPending)
			{
				bSubPending = false;
				Build(PendingTitle, MoveTemp(PendingOptions), LastCentre);
			}
			return true;
		}
	}
	return false;
}

void UDCRadialMenu::Close()
{
	bClearPending = bClearPending || bOpen;
	bOpen = false;
	bSubPending = false;
}

bool UDCRadialMenu::HasOption(const FString& LabelPart, bool* bOutEnabled) const
{
	for (const FDCRadialOption& Option : Current)
	{
		if (bOpen && Option.Label.ToString().Contains(LabelPart))
		{
			if (bOutEnabled)
			{
				*bOutEnabled = Option.bEnabled;
			}
			return true;
		}
	}
	return false;
}
