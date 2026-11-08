#include "DCLoadingScreen.h"

#include "Brushes/SlateColorBrush.h"

#include "DCGameFlow.h"
#include "DCItemWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCLoadingScreen"

namespace
{
	const FLinearColor LoadingBackColor(0.012f, 0.015f, 0.022f, 1.f);

	/** A thin bar on a faint track, so it shows even near zero. */
	const FProgressBarStyle& LoadingBarStyle()
	{
		static const FProgressBarStyle Style = FProgressBarStyle()
			.SetBackgroundImage(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f)))
			.SetFillImage(FSlateColorBrush(FLinearColor::White))
			.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		return Style;
	}
}

void SDCLoadingScreen::Construct(const FArguments& InArgs)
{
	using namespace DCItemWidgets;
	State = InArgs._State.IsValid() ? InArgs._State : MakeShared<FDCLoadingState>();
	TSharedPtr<FDCLoadingState> Shown = State;
	FSlateFontInfo TitleFont = HeadFont(22);
	TitleFont.LetterSpacing = 360;
	FSlateFontInfo KindFont = HeadFont(16);
	KindFont.LetterSpacing = 200;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(Brush())
		.BorderBackgroundColor(LoadingBackColor)
		.Padding(0.f)
		// Nothing behind the loading screen can be clicked.
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(64.f, 48.f))
			[
				SNew(STextBlock)
				.Font(TitleFont)
				.ColorAndOpacity(MutedColor)
				.Text(UDCGameFlowSubsystem::GetGameTitle().ToUpper())
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(64.f, 0.f, 64.f, 64.f))
			[
				SNew(SBox)
				.WidthOverride(760.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(KindFont)
						.ColorAndOpacity(TitleColor)
						.Text_Lambda([Shown]() { return Shown->Kind.ToUpper(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 22.f)
					[
						SNew(STextBlock)
						.Font(HeadFont(52))
						.ColorAndOpacity(TextColor)
						.Text_Lambda([Shown]() { return Shown->Title.ToUpper(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
					[
						SNew(STextBlock)
						.Font(Font(14))
						.ColorAndOpacity(TextColor)
						.Text_Lambda([Shown]() { return Shown->Status; })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(520.f)
						.HeightOverride(4.f)
						[
							SNew(SProgressBar)
							.Style(&LoadingBarStyle())
							.FillColorAndOpacity(TitleColor)
							.Percent_Lambda([this]() { return TOptional<float>(ShownProgress); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 30.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(Font(13))
						.ColorAndOpacity(MutedColor)
						.AutoWrapText(true)
						.Text_Lambda([Shown]() { return Shown->Tip; })
					]
				]
			]
		]
	];
}

void SDCLoadingScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// The bar glides towards the real value, never back.
	const float Want = FMath::Clamp(State->Progress, 0.f, 1.f);
	ShownProgress = Want < ShownProgress ? Want : FMath::FInterpTo(ShownProgress, Want, InDeltaTime, 6.f);
}

FText SDCLoadingScreen::NextTip()
{
	static int32 Next = FMath::Rand();
	static const TArray<FText> Tips = {
		LOCTEXT("Tip1", "Tipp: Einen Schritt in der Liste wählen – neue Schritte kommen direkt dahinter."),
		LOCTEXT("Tip2", "Tipp: Wer durch eine geschlossene Tür geht, öffnet sie unterwegs selbst. Verschlossene muss vorher jemand knacken."),
		LOCTEXT("Tip3", "Tipp: Hängt der Plan, zeigt „Lösen …“ den Schritt, der nicht geht, und wer helfen könnte."),
		LOCTEXT("Tip4", "Tipp: Bild ↑ und Bild ↓ wechseln die Ebene; sonst folgt die Kamera der gewählten Figur."),
		LOCTEXT("Tip5", "Tipp: Aufbrechen ist laut, Knacken leise – der Nachbar hört mit."),
		LOCTEXT("Tip6", "Tipp: Leertaste spielt den Plan ab, Esc öffnet das Menü, Strg+Z nimmt die letzte Änderung zurück."),
		LOCTEXT("Tip7", "Tipp: „Rosa wartet hierauf“ am gewählten Schritt stimmt zwei Figuren mit einem Klick ab."),
		LOCTEXT("Tip8", "Tipp: Was im Schrank liegt, zeigt erst der Einbruch – dann hält das Spiel an und fragt.")
	};
	return Tips[(Next++) % Tips.Num()];
}

#undef LOCTEXT_NAMESPACE
