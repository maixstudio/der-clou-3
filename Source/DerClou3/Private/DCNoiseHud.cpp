#include "DCNoiseHud.h"

#include "DCItemWidgets.h"
#include "DCNoise.h"
#include "DCNoiseListener.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCNoiseHud"

namespace
{
	constexpr double ShowNoiseSeconds = 3.0;
	constexpr int32 SuspicionSegments = 5;

	/** Calm in the given colour, amber from a third, red from two thirds. */
	FLinearColor SuspicionColor(float Suspicion, const FLinearColor& Calm)
	{
		return Suspicion >= 67.f ? DCItemWidgets::MessageColor : Suspicion >= 34.f ? DCItemWidgets::TitleColor : Calm;
	}

	float HighestSuspicion(const UWorld* World)
	{
		float Highest = 0.f;
		if (!World)
		{
			return Highest;
		}
		for (TActorIterator<ADCNoiseListener> It(World); It; ++It)
		{
			Highest = FMath::Max(Highest, It->GetSuspicion());
		}
		return Highest;
	}

	/** Five small bars, lit from the left by the highest suspicion. */
	TSharedRef<SWidget> MakeSuspicionMeter(TWeakObjectPtr<UDCNoiseHud> WeakThis)
	{
		using namespace DCItemWidgets;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < SuspicionSegments; ++Index)
		{
			Row->AddSlot().AutoWidth().Padding(Index > 0 ? 3.f : 0.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(16.f).HeightOverride(6.f)
				[
					SNew(SBorder)
					.BorderImage(RoundBrush())
					.BorderBackgroundColor_Lambda([WeakThis, Index]() -> FSlateColor
					{
						const float Highest = HighestSuspicion(WeakThis.IsValid() ? WeakThis->GetWorld() : nullptr);
						const bool bLit = Highest > Index * (100.f / SuspicionSegments);
						return bLit ? SuspicionColor(Highest, OkColor) : BrassDimColor;
					})
				]
			];
		}
		return Row;
	}
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
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(SBorder)
				.BorderImage(FramedBrush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(14.f, 3.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f, 0.f)
					[
						MakeIcon(TEXT("ear"), 17.f, TAttribute<FSlateColor>::CreateLambda([WeakThis]() -> FSlateColor
						{
							return SuspicionColor(HighestSuspicion(WeakThis.IsValid() ? WeakThis->GetWorld() : nullptr), MutedColor);
						}))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
					SNew(STextBlock)
					.Font(Font(14, true))
					.Text_Lambda([WeakThis]()
					{
						TArray<FString> Parts;
						for (TActorIterator<ADCNoiseListener> It(WeakThis.IsValid() ? WeakThis->GetWorld() : nullptr); It; ++It)
						{
							Parts.Add(FString::Printf(TEXT("%s: Misstrauen %d %%"), *It->GetInteractableName().ToString(), FMath::RoundToInt(It->GetSuspicion())));
						}
						return FText::FromString(FString::Join(Parts, TEXT("  ·  ")));
					})
					.ColorAndOpacity_Lambda([WeakThis]() -> FSlateColor
					{
						float Highest = 0.f;
						for (TActorIterator<ADCNoiseListener> It(WeakThis.IsValid() ? WeakThis->GetWorld() : nullptr); It; ++It)
						{
							Highest = FMath::Max(Highest, It->GetSuspicion());
						}
						return SuspicionColor(Highest, TextColor);
					})
					]
					// The highest suspicion as five segments.
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
					[
						MakeSuspicionMeter(WeakThis)
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(RoundBrush())
				.BorderBackgroundColor(FLinearColor(0.55f, 0.03f, 0.02f, 0.95f))
				.Padding(FMargin(20.f, 6.f))
				.Visibility_Lambda([GetNoise]()
				{
					const UDCNoiseSubsystem* Noise = GetNoise();
					return Noise && Noise->IsAlarm() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						MakeIcon(TEXT("siren"), 24.f, FLinearColor::White)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([GetNoise]()
						{
							const UDCNoiseSubsystem* Noise = GetNoise();
							return Noise ? FText::Format(LOCTEXT("Alarm", "ALARM – {0}"), Noise->GetAlarmText()) : FText::GetEmpty();
						})
						.Font(HeadFont(22))
						.TransformPolicy(ETextTransformPolicy::ToUpper)
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FramedBrush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(14.f, 5.f))
				.Visibility_Lambda([IsRecent]() { return IsRecent() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f, 0.f)
					[
						MakeIcon(TEXT("volume-2"), 16.f, TAttribute<FSlateColor>::CreateLambda([GetNoise]() -> FSlateColor
						{
							const UDCNoiseSubsystem* Noise = GetNoise();
							return FSlateColor(Noise ? UDCNoiseSubsystem::ColorFor(Noise->GetLastLoudNoise().Range) : FLinearColor::White);
						}))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
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
