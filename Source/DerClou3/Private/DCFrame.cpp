#include "DCFrame.h"

#include "DCGameViewportClient.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCFrame"

namespace
{
	const FLinearColor FrameColor(0.035f, 0.04f, 0.055f, 1.f);
	const FLinearColor FramePlanColor(0.62f, 0.5f, 0.28f);
	const FLinearColor FramePlayColor(0.35f, 0.75f, 0.42f);
	const FLinearColor FrameDirectColor(1.f, 0.5f, 0.1f);
	constexpr float FrameLine = 3.f;

	EDCPlanMode FrameMode(const UObject* Context)
	{
		const UDCPlanSubsystem* Plans = Context ? UDCPlanSubsystem::Get(Context) : nullptr;
		return Plans ? Plans->GetMode() : EDCPlanMode::Planning;
	}

	FLinearColor ModeColor(EDCPlanMode Mode)
	{
		switch (Mode)
		{
		case EDCPlanMode::Playing: return FramePlayColor;
		case EDCPlanMode::Direct: return FrameDirectColor;
		case EDCPlanMode::Planning: break;
		}
		return FramePlanColor;
	}
}

TSharedRef<SWidget> UDCFrame::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCFrame> WeakThis = this;
	const auto LineColor = [WeakThis]() -> FSlateColor { return ModeColor(FrameMode(WeakThis.Get())); };

	// A frame zone: solid, and it keeps clicks from reaching the scene behind it.
	const auto Zone = [](float Width, float Height)
	{
		return SNew(SBox)
			.WidthOverride(Width > 0.f ? FOptionalSize(Width) : FOptionalSize())
			.HeightOverride(Height > 0.f ? FOptionalSize(Height) : FOptionalSize())
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(FrameColor)
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			];
	};
	// One edge of the line around the scene.
	const auto Edge = [LineColor](bool bHorizontal)
	{
		return SNew(SBox)
			.WidthOverride(bHorizontal ? FOptionalSize() : FOptionalSize(FrameLine))
			.HeightOverride(bHorizontal ? FOptionalSize(FrameLine) : FOptionalSize())
			[
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor_Lambda(LineColor)
			];
	};

	Root = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Zone(0.f, TopHeight)]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Zone(LeftWidth, 0.f)]
				// The scene: nothing here takes clicks, they go to the game.
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SOverlay)
					.Visibility(EVisibility::HitTestInvisible)
					+ SOverlay::Slot().VAlign(VAlign_Top)[Edge(true)]
					+ SOverlay::Slot().VAlign(VAlign_Bottom)[Edge(true)]
					+ SOverlay::Slot().HAlign(HAlign_Left)[Edge(false)]
					+ SOverlay::Slot().HAlign(HAlign_Right)[Edge(false)]
				]
				+ SHorizontalBox::Slot().AutoWidth()[Zone(RightWidth, 0.f)]
			]
			+ SVerticalBox::Slot().AutoHeight()[Zone(0.f, BottomHeight)]
		]
		// What mode the game is in, top left, in the colour of the line.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(16.f, 9.f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Font(Font(16, true))
			.ColorAndOpacity_Lambda(LineColor)
			.Text_Lambda([WeakThis]()
			{
				switch (FrameMode(WeakThis.Get()))
				{
				case EDCPlanMode::Playing: return LOCTEXT("ModePlaying", "Plan läuft");
				case EDCPlanMode::Direct: return LOCTEXT("ModeDirect", "Ausprobieren");
				case EDCPlanMode::Planning: break;
				}
				return LOCTEXT("ModePlanning", "Planung");
			})
		];
	return Root.ToSharedRef();
}

void UDCFrame::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
}

void UDCFrame::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Tell the viewport which part of the window is left for the scene (in 0..1 of the window).
	const FVector2D Size = MyGeometry.GetLocalSize();
	UWorld* World = GetWorld();
	UDCGameViewportClient* Client = World ? Cast<UDCGameViewportClient>(World->GetGameViewport()) : nullptr;
	if (!Client || Size.X < 1.0 || Size.Y < 1.0)
	{
		return;
	}
	const FVector2D Origin(LeftWidth / Size.X, TopHeight / Size.Y);
	const FVector2D SceneSize(FMath::Max(0.1, 1.0 - (LeftWidth + RightWidth) / Size.X), FMath::Max(0.1, 1.0 - (TopHeight + BottomHeight) / Size.Y));
	Client->SetSceneRect(Origin, SceneSize);
}

#undef LOCTEXT_NAMESPACE
