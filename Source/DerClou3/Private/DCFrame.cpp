#include "DCFrame.h"

#include "DCFloors.h"
#include "DCGameViewportClient.h"
#include "DCHeistState.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "DCPlayerController.h"
#include "Engine/World.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCFrame"

namespace
{
	const FLinearColor FramePlanColor(0.2623f, 0.1714f, 0.0648f);   // #8C7348 brass
	const FLinearColor FramePlayColor(0.1095f, 0.4508f, 0.1441f);   // #5DB36A
	const FLinearColor FrameDirectColor(0.8714f, 0.1946f, 0.0232f); // #F07A2A
	constexpr float FrameLine = 2.f;

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

	/** The mode name top left: amber while planning (the focus colour), else the line colour. */
	FLinearColor ModeTextColor(EDCPlanMode Mode)
	{
		return Mode == EDCPlanMode::Planning ? DCItemWidgets::TitleColor : ModeColor(Mode);
	}
}

TSharedRef<SWidget> UDCFrame::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCFrame> WeakThis = this;
	const auto LineColor = [WeakThis]() -> FSlateColor { return ModeColor(FrameMode(WeakThis.Get())); };
	// Up / down one storey; greyed out at the top and bottom storey.
	const auto FloorButton = [WeakThis](int32 Delta, const FText& Label, const FText& Tip) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ButtonStyle(&NormalButtonStyle())
			.ToolTipText(Tip)
			.ContentPadding(FMargin(9.f, 1.f))
			.IsEnabled_Lambda([WeakThis, Delta]()
			{
				const UDCFloorSubsystem* Floors = WeakThis.IsValid() ? UDCFloorSubsystem::Get(WeakThis.Get()) : nullptr;
				return Floors && Floors->CanStep(Delta);
			})
			.OnClicked_Lambda([WeakThis, Delta]()
			{
				if (UDCFloorSubsystem* Floors = WeakThis.IsValid() ? UDCFloorSubsystem::Get(WeakThis.Get()) : nullptr)
				{
					Floors->StepFloor(Delta);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(Font(14, true)).Text(Label)
			];
	};

	// A frame zone: solid, and it keeps clicks from reaching the scene behind it.
	const auto Zone = [](float Width, float Height)
	{
		return SNew(SBox)
			.WidthOverride(Width > 0.f ? FOptionalSize(Width) : FOptionalSize())
			.HeightOverride(Height > 0.f ? FOptionalSize(Height) : FOptionalSize())
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(BackdropColor)
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
		// A hairline under the top bar, over the full width.
		+ SOverlay::Slot().VAlign(VAlign_Top).Padding(FMargin(0.f, TopHeight - 1.f, 0.f, 0.f))
		[
			SNew(SBox).Visibility(EVisibility::HitTestInvisible)[MakeRule(true, true)]
		]
		// What mode the game is in, top left – and while planning, which moment of the plan the room
		// shows (doors as the plan leaves them then), after a separator.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(18.f, 0.f))
		[
			SNew(SBox)
			.HeightOverride(TopHeight)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(HeadFont(22))
					.TransformPolicy(ETextTransformPolicy::ToUpper)
					.ColorAndOpacity_Lambda([WeakThis]() -> FSlateColor { return ModeTextColor(FrameMode(WeakThis.Get())); })
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
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f)
				[
					SNew(SBox)
					.HeightOverride(20.f)
					.Visibility_Lambda([WeakThis]() { return FrameMode(WeakThis.Get()) == EDCPlanMode::Planning ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						MakeRule(false, false)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility_Lambda([WeakThis]() { return FrameMode(WeakThis.Get()) == EDCPlanMode::Planning ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.Font(Font(13))
					.ColorAndOpacity(MutedColor)
					.Text_Lambda([WeakThis]()
					{
						const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
						return Heist ? Heist->DescribeViewMoment() : FText::GetEmpty();
					})
				]
			]
		]
		// The game menu (pause, save, load, options), top right; Esc opens it too.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(16.f, 7.f))
		[
			SNew(SButton)
			.IsFocusable(false)
			.ButtonStyle(&NormalButtonStyle())
			.ToolTipText(LOCTEXT("MenuTip", "Pause, Plan speichern, Laden, Optionen, Hauptmenü (Esc)"))
			.ContentPadding(FMargin(14.f, 1.f))
			.OnClicked_Lambda([WeakThis]()
			{
				if (ADCPlayerController* Controller = WeakThis.IsValid() ? Cast<ADCPlayerController>(WeakThis->GetOwningPlayer()) : nullptr)
				{
					Controller->OpenPauseMenu();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(Font(13, true)).Text(LOCTEXT("Menu", "Menü"))
			]
		]
		// Storeys (only where the building has several): which one the camera shows, and arrows to change it.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(16.f, 7.f, 108.f, 7.f))
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([WeakThis]()
			{
				const UDCFloorSubsystem* Floors = WeakThis.IsValid() ? UDCFloorSubsystem::Get(WeakThis.Get()) : nullptr;
				return Floors && Floors->HasSeveralFloors() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Font(HeadFont(16, false)).ColorAndOpacity(MutedColor).TransformPolicy(ETextTransformPolicy::ToUpper)
				.Text(LOCTEXT("FloorLabel", "Ebene"))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(STextBlock)
				.Font(HeadFont(18))
				.ColorAndOpacity(TextColor)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
				.Text_Lambda([WeakThis]()
				{
					const UDCFloorSubsystem* Floors = WeakThis.IsValid() ? UDCFloorSubsystem::Get(WeakThis.Get()) : nullptr;
					return UDCFloorSubsystem::FloorName(Floors ? Floors->GetShownFloor() : 0);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)
			[
				FloorButton(1, LOCTEXT("FloorUp", "↑"), LOCTEXT("FloorUpTip", "Eine Ebene höher zeigen (Bild ↑)"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)
			[
				FloorButton(-1, LOCTEXT("FloorDown", "↓"), LOCTEXT("FloorDownTip", "Eine Ebene tiefer zeigen (Bild ↓)"))
			]
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
