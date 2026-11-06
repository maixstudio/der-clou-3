#include "DCNoiseHud.h"

#include "DCItemWidgets.h"
#include "DCNoise.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCNoiseHud"

namespace
{
	constexpr double ShowNoiseSeconds = 3.0;
}

TSharedRef<SWidget> UDCNoiseHud::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCNoiseHud> WeakThis = this;
	const auto GetNoise = [WeakThis]() -> const UDCNoiseSubsystem*
	{
		return WeakThis.IsValid() ? UDCNoiseSubsystem::Get(WeakThis.Get()) : nullptr;
	};
	const auto IsRecent = [WeakThis, GetNoise]()
	{
		const UDCNoiseSubsystem* Noise = GetNoise();
		return Noise && WeakThis->GetWorld() && WeakThis->GetWorld()->GetTimeSeconds() - Noise->GetLastLoudNoiseTime() < ShowNoiseSeconds;
	};

	Root = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 18.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(FLinearColor(0.55f, 0.03f, 0.02f, 0.95f))
				.Padding(FMargin(18.f, 8.f))
				.Visibility_Lambda([GetNoise]()
				{
					const UDCNoiseSubsystem* Noise = GetNoise();
					return Noise && Noise->IsAlarm() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(STextBlock)
					.Text_Lambda([GetNoise]()
					{
						const UDCNoiseSubsystem* Noise = GetNoise();
						return Noise ? FText::Format(LOCTEXT("Alarm", "ALARM – {0}"), Noise->GetAlarmText()) : FText::GetEmpty();
					})
					.Font(Font(16, true))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(14.f, 5.f))
				.Visibility_Lambda([IsRecent]() { return IsRecent() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(STextBlock)
					.Text_Lambda([GetNoise]()
					{
						const UDCNoiseSubsystem* Noise = GetNoise();
						if (!Noise)
						{
							return FText::GetEmpty();
						}
						const FDCNoiseEvent& Last = Noise->GetLastLoudNoise();
						return FText::Format(LOCTEXT("Noise", "Lärm: {0} – {1}"), Last.What, UDCNoiseSubsystem::DescribeLoudness(Last.Range));
					})
					.ColorAndOpacity_Lambda([GetNoise]()
					{
						const UDCNoiseSubsystem* Noise = GetNoise();
						return FSlateColor(Noise ? UDCNoiseSubsystem::ColorFor(Noise->GetLastLoudNoise().Range) : FLinearColor::White);
					})
					.Font(Font(13, true))
				]
			]
		];
	return Root.ToSharedRef();
}

void UDCNoiseHud::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
}

#undef LOCTEXT_NAMESPACE
