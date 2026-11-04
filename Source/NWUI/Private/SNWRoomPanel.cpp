#include "SNWRoomPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "NWRoomSession.h"
#include "SNWRoomTimeline.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NWRoomPanel"

namespace NWRoomPanelStyle
{
	const FLinearColor PanelColor(0.03f, 0.035f, 0.05f, 0.9f);
	const FLinearColor TitleColor(0.95f, 0.95f, 0.97f, 1.f);
	const FLinearColor InfoColor(0.85f, 0.87f, 0.9f, 1.f);
	const FLinearColor HintColor(0.6f, 0.63f, 0.68f, 1.f);
	const FLinearColor ActiveColor(1.f, 0.82f, 0.2f, 1.f);
	const FLinearColor FailColor(1.f, 0.4f, 0.35f, 1.f);
	const FLinearColor RadialColor(0.05f, 0.06f, 0.09f, 0.95f);

	constexpr float RadialRadius = 105.f;
	constexpr float RadialButtonWidth = 170.f;
	constexpr float RadialButtonHeight = 38.f;

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

void SNWRoomPanel::Construct(const FArguments& InArgs)
{
	Session = InArgs._Session;
	SetVisibility(EVisibility::SelfHitTestInvisible);

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16.f)
		[
			MakeHeader()
		]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(NWRoomPanelStyle::White())
			.BorderBackgroundColor(NWRoomPanelStyle::PanelColor)
			.Padding(FMargin(12.f, 10.f))
			[
				SAssignNew(Timeline, SNWRoomTimeline).Session(InArgs._Session)
			]
		]
		+ SOverlay::Slot()
		[
			SAssignNew(RadialLayer, SCanvas)
			.Visibility(EVisibility::SelfHitTestInvisible)
		]
	];
}

void SNWRoomPanel::SetSession(UNWRoomSession* InSession)
{
	Session = InSession;
	if (Timeline.IsValid())
	{
		Timeline->SetSession(InSession);
	}
}

void SNWRoomPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	CachedGeometry = AllottedGeometry;
	if (bRadialClearPending && RadialLayer.IsValid())
	{
		RadialLayer->ClearChildren();
		bRadialClearPending = false;
	}
}

TSharedRef<SWidget> SNWRoomPanel::MakeButton(const FText& Label, TFunction<void()> OnClick, TAttribute<bool> IsEnabled)
{
	return SNew(SButton)
		.IsEnabled(IsEnabled)
		.ContentPadding(FMargin(10.f, 4.f))
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(Label).Font(NWRoomPanelStyle::Font(11))
		];
}

TSharedRef<SWidget> SNWRoomPanel::MakeSpeedButton(float Speed)
{
	TWeakObjectPtr<UNWRoomSession> WeakSession = Session;
	return SNew(SButton)
		.ContentPadding(FMargin(8.f, 4.f))
		.ButtonColorAndOpacity_Lambda([WeakSession, Speed]()
		{
			const bool bActive = WeakSession.IsValid() && FMath::IsNearlyEqual(WeakSession->GetPlaybackSpeed(), Speed);
			return FSlateColor(bActive ? NWRoomPanelStyle::ActiveColor : FLinearColor::White);
		})
		.OnClicked_Lambda([WeakSession, Speed]()
		{
			if (WeakSession.IsValid())
			{
				WeakSession->SetPlaybackSpeed(Speed);
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(FText::Format(LOCTEXT("Speed", "{0}×"), FText::AsNumber(FMath::RoundToInt(Speed)))).Font(NWRoomPanelStyle::Font(11))
		];
}

TSharedRef<SWidget> SNWRoomPanel::MakeHeader()
{
	TWeakObjectPtr<UNWRoomSession> WeakSession = Session;

	auto Status = [WeakSession]()
	{
		const UNWRoomSession* S = WeakSession.Get();
		if (!S || !S->IsValid())
		{
			return LOCTEXT("NoSession", "Kein Raum geladen");
		}
		const int32 Agent = S->GetSelectedAgent();
		return FText::Format(LOCTEXT("Status", "{0}: {1}"), S->GetAgentName(Agent), S->DescribeActivity(Agent));
	};
	auto Clock = [WeakSession]()
	{
		const UNWRoomSession* S = WeakSession.Get();
		if (!S || !S->IsValid())
		{
			return FText::GetEmpty();
		}
		const int32 Whole = FMath::FloorToInt(S->GetSeconds());
		const FText Time = FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
		return S->IsReplaying()
			? FText::Format(LOCTEXT("ClockReplay", "Planzeit {0}  ·  Wiedergabe läuft"), Time)
			: FText::Format(LOCTEXT("Clock", "Planzeit {0}  ·  {1} Befehle"), Time, FText::AsNumber(S->GetCommandCount()));
	};
	auto LastFailure = [WeakSession]()
	{
		const UNWRoomSession* S = WeakSession.Get();
		if (!S || !S->IsValid() || S->GetSim().GetRecords().empty())
		{
			return FText::GetEmpty();
		}
		const nw::room::CommandRecord& Last = S->GetSim().GetRecords().back();
		if (!Last.bFailed)
		{
			return FText::GetEmpty();
		}
		return FText::Format(LOCTEXT("Failed", "Geht nicht: {0} – {1}"), S->DescribeCommand(Last.Cmd), FText::FromString(UTF8_TO_TCHAR(Last.FailReason.c_str())));
	};

	return SNew(SBorder)
		.BorderImage(NWRoomPanelStyle::White())
		.BorderBackgroundColor(NWRoomPanelStyle::PanelColor)
		.Padding(FMargin(16.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "PROTOTYP · Planen durch Vormachen"))
				.Font(NWRoomPanelStyle::Font(15, true))
				.ColorAndOpacity(NWRoomPanelStyle::TitleColor)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda(Status).Font(NWRoomPanelStyle::Font(13, true)).ColorAndOpacity(NWRoomPanelStyle::ActiveColor)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda(Clock).Font(NWRoomPanelStyle::Font(12)).ColorAndOpacity(NWRoomPanelStyle::InfoColor)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda(LastFailure).Font(NWRoomPanelStyle::Font(12, true)).ColorAndOpacity(NWRoomPanelStyle::FailColor)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					MakeButton(LOCTEXT("Undo", "Rückgängig"), [WeakSession]() { if (WeakSession.IsValid()) { WeakSession->Undo(); } },
						TAttribute<bool>::CreateLambda([WeakSession]() { return WeakSession.IsValid() && WeakSession->CanUndo(); }))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					MakeButton(LOCTEXT("Clear", "Alles löschen"), [WeakSession]() { if (WeakSession.IsValid()) { WeakSession->ClearAll(); } },
						TAttribute<bool>::CreateLambda([WeakSession]() { return WeakSession.IsValid() && WeakSession->CanUndo(); }))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)
				[
					MakeButton(LOCTEXT("Replay", "Von vorn abspielen"), [WeakSession]() { if (WeakSession.IsValid()) { WeakSession->Replay(); } },
						TAttribute<bool>::CreateLambda([WeakSession]() { return WeakSession.IsValid() && WeakSession->CanUndo(); }))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(STextBlock).Text(LOCTEXT("Tempo", "Tempo")).Font(NWRoomPanelStyle::Font(11)).ColorAndOpacity(NWRoomPanelStyle::HintColor)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)[MakeSpeedButton(1.f)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)[MakeSpeedButton(2.f)]
				+ SHorizontalBox::Slot().AutoWidth()[MakeSpeedButton(4.f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Hint", "Boden anklicken: hingehen   ·   Tür / Fenster anklicken: Aktionen\nFigur anklicken: auswählen, nochmal: warten   ·   Rücktaste: rückgängig"))
				.Font(NWRoomPanelStyle::Font(10))
				.ColorAndOpacity(NWRoomPanelStyle::HintColor)
			]
		];
}

void SNWRoomPanel::OpenRadialMenu(const FText& Title, TArray<FNWRadialOption> Options)
{
	if (!RadialLayer.IsValid() || Options.IsEmpty())
	{
		return;
	}
	RadialLayer->ClearChildren();
	bRadialClearPending = false;
	bRadialOpen = true;
	const FVector2D Centre = CachedGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());

	// Title in the middle.
	RadialLayer->AddSlot()
		.Position(Centre)
		.Size(FVector2D(160.f, 30.f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(NWRoomPanelStyle::White())
			.BorderBackgroundColor(NWRoomPanelStyle::RadialColor)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Title).Font(NWRoomPanelStyle::Font(11, true)).ColorAndOpacity(NWRoomPanelStyle::ActiveColor)
			]
		];

	// Options on a circle, starting at the top.
	const int32 Count = Options.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = -UE_HALF_PI + UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Count);
		const FVector2D Position = Centre + FVector2D(FMath::Cos(Angle) * NWRoomPanelStyle::RadialRadius * 1.3f, FMath::Sin(Angle) * NWRoomPanelStyle::RadialRadius);
		TFunction<void()> OnSelected = MoveTemp(Options[Index].OnSelected);
		RadialLayer->AddSlot()
			.Position(Position)
			.Size(FVector2D(NWRoomPanelStyle::RadialButtonWidth, NWRoomPanelStyle::RadialButtonHeight))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([this, OnSelected]()
				{
					CloseRadialMenu();
					if (OnSelected)
					{
						OnSelected();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Options[Index].Label).Font(NWRoomPanelStyle::Font(12, true))
				]
			];
	}
}

void SNWRoomPanel::CloseRadialMenu()
{
	bRadialClearPending = bRadialOpen || bRadialClearPending;
	bRadialOpen = false;
}

#undef LOCTEXT_NAMESPACE
