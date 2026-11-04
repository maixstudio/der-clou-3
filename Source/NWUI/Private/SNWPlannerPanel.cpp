#include "SNWPlannerPanel.h"

#include "NWPlanEditor.h"
#include "Sim/SimTimeline.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NWPlanner"

namespace NWPlannerStyle
{
	constexpr float NameColumnWidth = 110.f;
	constexpr float TrackHeight = 30.f;
	constexpr float RulerHeight = 18.f;
	constexpr float MinBlockWidth = 10.f;
	constexpr float UnreachedBlockWidth = 70.f;

	const FLinearColor Panel(0.02f, 0.025f, 0.04f, 0.88f);
	const FLinearColor TrackBackground(0.07f, 0.08f, 0.11f, 1.f);
	const FLinearColor SelectedTrack(0.12f, 0.15f, 0.22f, 1.f);
	const FLinearColor Text(0.85f, 0.87f, 0.9f, 1.f);
	const FLinearColor Dim(0.5f, 0.53f, 0.58f, 1.f);
	const FLinearColor Good(0.35f, 0.85f, 0.45f, 1.f);
	const FLinearColor Bad(0.95f, 0.3f, 0.25f, 1.f);
	const FLinearColor Warn(0.95f, 0.65f, 0.2f, 1.f);
	const FLinearColor Pick(0.f, 0.9f, 0.9f, 1.f);

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FLinearColor ActionColor(ENWPlanActionType Type)
	{
		switch (Type)
		{
		case ENWPlanActionType::MoveTo: return FLinearColor(0.22f, 0.42f, 0.72f);
		case ENWPlanActionType::Wait: return FLinearColor(0.35f, 0.36f, 0.4f);
		case ENWPlanActionType::Signal: return FLinearColor(0.58f, 0.38f, 0.78f);
		case ENWPlanActionType::WaitForSignal: return FLinearColor(0.38f, 0.3f, 0.52f);
		case ENWPlanActionType::PickLock: return FLinearColor(0.78f, 0.56f, 0.18f);
		case ENWPlanActionType::DisableAlarm: return FLinearColor(0.16f, 0.62f, 0.55f);
		case ENWPlanActionType::CrackSafe: return FLinearColor(0.82f, 0.72f, 0.16f);
		}
		return FLinearColor::Gray;
	}

	FText PickName(ENWPlanActionType Type)
	{
		switch (Type)
		{
		case ENWPlanActionType::MoveTo: return LOCTEXT("PickMoveTo", "Gehe zu");
		case ENWPlanActionType::PickLock: return LOCTEXT("PickPickLock", "Schloss knacken");
		case ENWPlanActionType::DisableAlarm: return LOCTEXT("PickAlarm", "Alarm aus");
		case ENWPlanActionType::CrackSafe: return LOCTEXT("PickSafe", "Tresor knacken");
		default: return FText::GetEmpty();
		}
	}

	FText Clock(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
	}

	float TicksToSeconds(nw::sim::Tick Ticks)
	{
		return static_cast<float>(Ticks) / static_cast<float>(nw::sim::TicksPerSecond);
	}
}

void SNWPlannerPanel::Construct(const FArguments& InArgs)
{
	SetEditor(InArgs._Editor);
}

SNWPlannerPanel::~SNWPlannerPanel()
{
	if (UNWPlanEditor* Current = Editor.Get())
	{
		Current->OnChanged.Remove(ChangedHandle);
	}
}

void SNWPlannerPanel::SetEditor(UNWPlanEditor* InEditor)
{
	if (UNWPlanEditor* Current = Editor.Get())
	{
		Current->OnChanged.Remove(ChangedHandle);
	}
	Editor = InEditor;
	if (InEditor)
	{
		ChangedHandle = InEditor->OnChanged.AddSP(this, &SNWPlannerPanel::Rebuild);
	}
	// Rebuild on the next tick, never inside the click handler that caused the change.
	bNeedsRebuild = true;
}

void SNWPlannerPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (!bNeedsRebuild)
	{
		return;
	}
	bNeedsRebuild = false;

	if (TimelineScroll.IsValid())
	{
		ScrollOffset = TimelineScroll->GetScrollOffset();
	}
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16.f)
		[
			MakeHeader()
		]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(NWPlannerStyle::White())
			.BorderBackgroundColor(NWPlannerStyle::Panel)
			.Padding(FMargin(12.f, 8.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					MakePalette()
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeTimeline()
				]
			]
		]
	];
	if (TimelineScroll.IsValid())
	{
		TimelineScroll->SetScrollOffset(ScrollOffset);
	}
}

void SNWPlannerPanel::Rebuild()
{
	bNeedsRebuild = true;
}

bool SNWPlannerPanel::IsExecuting() const
{
	const UNWPlanEditor* Current = GetEditor();
	return Current && Current->GetExecution().bActive;
}

float SNWPlannerPanel::GetTimelineSeconds() const
{
	const UNWPlanEditor* Current = GetEditor();
	float Seconds = 300.f;
	if (Current)
	{
		Seconds = FMath::Max(Seconds, NWPlannerStyle::TicksToSeconds(Current->GetTimeline().Result.EndTick) + 30.f);
	}
	return Seconds;
}

TSharedRef<SWidget> SNWPlannerPanel::MakeButton(const FText& Label, TFunction<void()> OnClick, TAttribute<bool> IsEnabled, const FText& ToolTip)
{
	return SNew(SButton)
		.IsEnabled(IsEnabled)
		.ToolTipText(ToolTip)
		.ContentPadding(FMargin(8.f, 2.f))
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(Label).Font(NWPlannerStyle::Font(10))
		];
}

TSharedRef<SWidget> SNWPlannerPanel::MakePickButton(const FText& Label, ENWPlanActionType Type)
{
	TWeakObjectPtr<UNWPlanEditor> WeakEditor = Editor;
	return SNew(SButton)
		.ContentPadding(FMargin(8.f, 2.f))
		.ButtonColorAndOpacity_Lambda([WeakEditor, Type]()
		{
			const UNWPlanEditor* Current = WeakEditor.Get();
			const bool bActive = Current && Current->IsPicking() && Current->GetPendingType() == Type;
			return FSlateColor(bActive ? NWPlannerStyle::Pick : FLinearColor::White);
		})
		.ToolTipText(LOCTEXT("PickTip", "Danach das Ziel in der Welt anklicken"))
		.OnClicked_Lambda([WeakEditor, Type]()
		{
			if (UNWPlanEditor* Current = WeakEditor.Get())
			{
				Current->BeginPick(Type);
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(Label).Font(NWPlannerStyle::Font(10))
		];
}

TSharedRef<SWidget> SNWPlannerPanel::MakeHeader()
{
	UNWPlanEditor* Current = GetEditor();
	if (!Current)
	{
		return SNullWidget::NullWidget;
	}
	TWeakObjectPtr<UNWPlanEditor> WeakEditor = Editor;
	const nw::sim::SimResult& Ghost = Current->GetTimeline().Result;
	const bool bGhostClean = Ghost.Result == nw::sim::Outcome::CleanGetaway;

	const FText GhostText = FText::Format(LOCTEXT("Ghost", "Geisterlauf: {0} · {1} · Beute {2}"),
		UNWPlanEditor::DescribeOutcome(Ghost.Result), NWPlannerStyle::Clock(NWPlannerStyle::TicksToSeconds(Ghost.EndTick)), FText::AsNumber(Ghost.LootValue));

	auto PlannerVisibility = [this]() { return IsExecuting() ? EVisibility::Collapsed : EVisibility::Visible; };
	auto ExecutionVisibility = [this]() { return IsExecuting() ? EVisibility::Visible : EVisibility::Collapsed; };

	return SNew(SBorder)
		.BorderImage(NWPlannerStyle::White())
		.BorderBackgroundColor(NWPlannerStyle::Panel)
		.Padding(FMargin(14.f, 10.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("Title", "PLANER · {0}"), Current->GetObjectName()))
				.Font(NWPlannerStyle::Font(14, true))
				.ColorAndOpacity(NWPlannerStyle::Text)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(GhostText)
				.Font(NWPlannerStyle::Font(11))
				.ColorAndOpacity(bGhostClean ? NWPlannerStyle::Good : NWPlannerStyle::Bad)
			]
			// Planning controls.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda(PlannerVisibility)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					MakeButton(LOCTEXT("Undo", "Rückgängig"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->Undo(); } },
						TAttribute<bool>::CreateLambda([WeakEditor]() { return WeakEditor.IsValid() && WeakEditor->CanUndo(); }))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
				[
					MakeButton(LOCTEXT("Redo", "Wiederholen"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->Redo(); } },
						TAttribute<bool>::CreateLambda([WeakEditor]() { return WeakEditor.IsValid() && WeakEditor->CanRedo(); }))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeButton(LOCTEXT("Execute", "Ausführen"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->CancelPick(); WeakEditor->RequestExecute(); } },
						true, LOCTEXT("ExecuteTip", "Plan in Echtzeit abspielen"))
				]
			]
			// Execution controls.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda(ExecutionVisibility)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock)
					.Font(NWPlannerStyle::Font(11, true))
					.ColorAndOpacity(NWPlannerStyle::Warn)
					.Text_Lambda([WeakEditor]()
					{
						if (!WeakEditor.IsValid())
						{
							return FText::GetEmpty();
						}
						const FNWExecutionState& State = WeakEditor->GetExecution();
						const FText Time = NWPlannerStyle::Clock(State.SimulatedSeconds);
						if (State.bFinished)
						{
							return FText::Format(LOCTEXT("ExecDone", "Ausführung beendet: {0} · {1} · Beute {2}"),
								UNWPlanEditor::DescribeOutcome(State.Result.Result), Time, FText::AsNumber(State.Result.LootValue));
						}
						return FText::Format(LOCTEXT("ExecRunning", "Ausführung läuft · {0} · Tempo {1}×"), Time, FText::AsNumber(FMath::RoundToInt(State.PlaybackSpeed)));
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						MakeButton(LOCTEXT("Speed1", "1×"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->RequestPlaybackSpeed(1.f); } })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						MakeButton(LOCTEXT("Speed4", "4×"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->RequestPlaybackSpeed(4.f); } })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
					[
						MakeButton(LOCTEXT("Speed16", "16×"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->RequestPlaybackSpeed(16.f); } })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						MakeButton(LOCTEXT("Back", "Zurück zum Planer"), [WeakEditor]() { if (WeakEditor.IsValid()) { WeakEditor->RequestStop(); } })
					]
				]
			]
		];
}

TSharedRef<SWidget> SNWPlannerPanel::MakePalette()
{
	UNWPlanEditor* Current = GetEditor();
	if (!Current)
	{
		return SNullWidget::NullWidget;
	}
	TWeakObjectPtr<UNWPlanEditor> WeakEditor = Editor;
	const int32 Crew = Current->GetSelectedCrew();

	auto AddDirect = [WeakEditor](ENWPlanActionType Type, int32 Target, float Seconds)
	{
		return [WeakEditor, Type, Target, Seconds]()
		{
			if (UNWPlanEditor* E = WeakEditor.Get())
			{
				FNWPlanAction Action;
				Action.Type = Type;
				Action.Target = Target;
				Action.DurationSeconds = Seconds;
				E->CancelPick();
				E->AddAction(E->GetSelectedCrew(), Action);
			}
		};
	};

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		.Visibility_Lambda([this]() { return IsExecuting() ? EVisibility::Collapsed : EVisibility::Visible; });

	auto AddToRow = [&Row](TSharedRef<SWidget> Widget, float RightPadding = 4.f)
	{
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, RightPadding, 0.f)[Widget];
	};
	auto Label = [](const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(NWPlannerStyle::Font(10)).ColorAndOpacity(NWPlannerStyle::Dim);
	};

	AddToRow(SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ActionFor", "Aktion für {0}:"), Current->GetCrewName(Crew)))
		.Font(NWPlannerStyle::Font(11, true))
		.ColorAndOpacity(NWPlannerStyle::Text), 10.f);
	AddToRow(MakePickButton(LOCTEXT("MoveTo", "Gehe zu"), ENWPlanActionType::MoveTo));
	AddToRow(MakePickButton(LOCTEXT("PickLock", "Schloss knacken"), ENWPlanActionType::PickLock));
	AddToRow(MakePickButton(LOCTEXT("DisableAlarm", "Alarm aus"), ENWPlanActionType::DisableAlarm));
	AddToRow(MakePickButton(LOCTEXT("CrackSafe", "Tresor knacken"), ENWPlanActionType::CrackSafe), 14.f);

	AddToRow(Label(LOCTEXT("SignalLabel", "Signal senden")));
	for (int32 Signal = 0; Signal < 3; ++Signal)
	{
		AddToRow(MakeButton(FText::AsNumber(Signal), AddDirect(ENWPlanActionType::Signal, Signal, 0.f)), Signal == 2 ? 14.f : 2.f);
	}
	AddToRow(Label(LOCTEXT("WaitSignalLabel", "Warte auf Signal")));
	for (int32 Signal = 0; Signal < 3; ++Signal)
	{
		AddToRow(MakeButton(FText::AsNumber(Signal), AddDirect(ENWPlanActionType::WaitForSignal, Signal, 0.f)), Signal == 2 ? 14.f : 2.f);
	}
	AddToRow(Label(LOCTEXT("WaitLabel", "Warten")));
	AddToRow(MakeButton(LOCTEXT("Wait10", "10 s"), AddDirect(ENWPlanActionType::Wait, INDEX_NONE, 10.f)), 2.f);
	AddToRow(MakeButton(LOCTEXT("Wait30", "30 s"), AddDirect(ENWPlanActionType::Wait, INDEX_NONE, 30.f)), 14.f);
	AddToRow(MakeButton(LOCTEXT("ClearTrack", "Spur leeren"), [WeakEditor]() { if (UNWPlanEditor* E = WeakEditor.Get()) { E->CancelPick(); E->ClearCrew(E->GetSelectedCrew()); } }), 14.f);
	AddToRow(MakeButton(LOCTEXT("ZoomOut", "−"), [this]() { PixelsPerSecond = FMath::Max(1.f, PixelsPerSecond / 1.5f); bNeedsRebuild = true; }, true, LOCTEXT("ZoomOutTip", "Zeitleiste verkleinern")), 2.f);
	AddToRow(MakeButton(LOCTEXT("ZoomIn", "+"), [this]() { PixelsPerSecond = FMath::Min(40.f, PixelsPerSecond * 1.5f); bNeedsRebuild = true; }, true, LOCTEXT("ZoomInTip", "Zeitleiste vergrößern")));

	FText Hint = LOCTEXT("HintIdle", "Spur links wählen, Aktion wählen. Rechtsklick auf einen Block löscht ihn.");
	FLinearColor HintColor = NWPlannerStyle::Dim;
	if (Current->IsPicking())
	{
		Hint = FText::Format(LOCTEXT("HintPick", "Ziel für „{0}“ in der Welt anklicken (türkis markiert). Rechtsklick in die Welt bricht ab."),
			NWPlannerStyle::PickName(Current->GetPendingType()));
		HintColor = NWPlannerStyle::Pick;
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Row
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return IsExecuting() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text(Hint)
			.Font(NWPlannerStyle::Font(10))
			.ColorAndOpacity(HintColor)
		];
}

TSharedRef<SWidget> SNWPlannerPanel::MakeTimeline()
{
	UNWPlanEditor* Current = GetEditor();
	if (!Current)
	{
		return SNullWidget::NullWidget;
	}
	TWeakObjectPtr<UNWPlanEditor> WeakEditor = Editor;
	const float Seconds = GetTimelineSeconds();
	const float Width = Seconds * PixelsPerSecond;

	TSharedRef<SVerticalBox> Names = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(NWPlannerStyle::RulerHeight)
		];
	TSharedRef<SVerticalBox> Tracks = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeRuler(Width, Seconds)
		];

	for (int32 CrewIndex = 0; CrewIndex < Current->GetCrewCount(); ++CrewIndex)
	{
		const bool bSelected = CrewIndex == Current->GetSelectedCrew();
		Names->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SBox)
			.HeightOverride(NWPlannerStyle::TrackHeight)
			.WidthOverride(NWPlannerStyle::NameColumnWidth)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FSlateColor(bSelected ? NWPlannerStyle::Pick : FLinearColor::White))
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("SelectCrewTip", "Spur auswählen"))
				.OnClicked_Lambda([WeakEditor, CrewIndex]()
				{
					if (UNWPlanEditor* E = WeakEditor.Get())
					{
						E->CancelPick();
						E->SelectCrew(CrewIndex);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Current->GetCrewName(CrewIndex)).Font(NWPlannerStyle::Font(11, bSelected))
				]
			]
		];
		Tracks->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			MakeTrack(CrewIndex, Width)
		];
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
		[
			Names
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SAssignNew(TimelineScroll, SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SNew(SBox).WidthOverride(Width)
				[
					Tracks
				]
			]
		];
}

TSharedRef<SWidget> SNWPlannerPanel::MakeRuler(float TimelineWidth, float Seconds)
{
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	const float Step = PixelsPerSecond >= 8.f ? 10.f : (PixelsPerSecond >= 3.f ? 30.f : 60.f);
	for (float T = 0.f; T <= Seconds; T += Step)
	{
		Canvas->AddSlot()
			.Position(FVector2D(T * PixelsPerSecond, 0.f))
			.Size(FVector2D(60.f, NWPlannerStyle::RulerHeight))
			[
				SNew(STextBlock).Text(NWPlannerStyle::Clock(T)).Font(NWPlannerStyle::Font(8)).ColorAndOpacity(NWPlannerStyle::Dim)
			];
	}
	return SNew(SBox).HeightOverride(NWPlannerStyle::RulerHeight).WidthOverride(TimelineWidth)[Canvas];
}

TSharedRef<SWidget> SNWPlannerPanel::MakeTrack(int32 CrewIndex, float TimelineWidth)
{
	UNWPlanEditor* Current = GetEditor();
	TWeakObjectPtr<UNWPlanEditor> WeakEditor = Editor;
	const nw::sim::PlanTimeline& Timeline = Current->GetTimeline();
	const TArray<FNWPlanAction>& Actions = Current->GetPlan()[CrewIndex].Actions;
	const nw::sim::CrewTimeline* Track = static_cast<size_t>(CrewIndex) < Timeline.Crew.size() ? &Timeline.Crew[static_cast<size_t>(CrewIndex)] : nullptr;
	const float H = NWPlannerStyle::TrackHeight;

	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	float NextUnreachedX = 0.f;

	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		const FNWPlanAction& Action = Actions[Index];
		const nw::sim::ActionSpan* Span = Track && static_cast<size_t>(Index) < Track->Actions.size() ? &Track->Actions[static_cast<size_t>(Index)] : nullptr;
		const bool bReached = Span && Span->Start != nw::sim::NoTick;

		float X = NextUnreachedX;
		float W = NWPlannerStyle::UnreachedBlockWidth;
		FLinearColor Color = NWPlannerStyle::ActionColor(Action.Type);
		FText Detail = LOCTEXT("NotReached", "Wird nicht erreicht");
		if (bReached)
		{
			const float Start = NWPlannerStyle::TicksToSeconds(Span->Start);
			const float End = NWPlannerStyle::TicksToSeconds(Span->End);
			X = Start * PixelsPerSecond;
			W = FMath::Max(NWPlannerStyle::MinBlockWidth, (End - Start) * PixelsPerSecond - 1.f);
			Detail = FText::Format(LOCTEXT("Span", "{0} – {1}"), NWPlannerStyle::Clock(Start), NWPlannerStyle::Clock(End));
			if (Span->bFailed)
			{
				Color = NWPlannerStyle::Bad;
				Detail = FText::Format(LOCTEXT("Failed", "{0} · gescheitert"), Detail);
			}
			else if (Span->bUnfinished)
			{
				Color = NWPlannerStyle::Warn;
				Detail = FText::Format(LOCTEXT("Unfinished", "{0} · nicht abgeschlossen"), Detail);
			}
		}
		else
		{
			Color = Color.Desaturate(0.7f) * 0.6f;
			Color.A = 1.f;
		}
		NextUnreachedX = FMath::Max(NextUnreachedX, X + W + 2.f);

		Canvas->AddSlot()
			.Position(FVector2D(X, 0.f))
			.Size(FVector2D(W, H))
			[
				SNew(SBorder)
				.BorderImage(NWPlannerStyle::White())
				.BorderBackgroundColor(Color)
				.Padding(FMargin(4.f, 2.f))
				.VAlign(VAlign_Center)
				.Clipping(EWidgetClipping::ClipToBounds)
				.ToolTipText(FText::Format(LOCTEXT("BlockTip", "{0}\n{1}\nRechtsklick: löschen"), Current->DescribeAction(Action), Detail))
				.OnMouseButtonDown_Lambda([WeakEditor, CrewIndex, Index](const FGeometry&, const FPointerEvent& Event)
				{
					UNWPlanEditor* E = WeakEditor.Get();
					if (!E)
					{
						return FReply::Unhandled();
					}
					E->CancelPick();
					if (Event.GetEffectingButton() == EKeys::RightMouseButton)
					{
						E->RemoveAction(CrewIndex, Index);
					}
					else
					{
						E->SelectCrew(CrewIndex);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Current->DescribeAction(Action, true)).Font(NWPlannerStyle::Font(9)).ColorAndOpacity(FLinearColor::White)
				]
			];
	}

	// Conflict markers: failures, alarms and sightings of this crew member.
	for (const nw::sim::SimEvent& Event : Timeline.Events)
	{
		const bool bCrewAgent = Event.Agent == CrewIndex && (Event.Type == nw::sim::EventType::ActionFailed || Event.Type == nw::sim::EventType::AlarmTriggered);
		const bool bSpotted = Event.Type == nw::sim::EventType::GuardSpotted && Event.Target == CrewIndex;
		if (!bCrewAgent && !bSpotted)
		{
			continue;
		}
		Canvas->AddSlot()
			.Position(FVector2D(NWPlannerStyle::TicksToSeconds(Event.At) * PixelsPerSecond - 2.f, 0.f))
			.Size(FVector2D(4.f, H))
			[
				SNew(SBorder)
				.BorderImage(NWPlannerStyle::White())
				.BorderBackgroundColor(NWPlannerStyle::Bad)
				.ToolTipText(FText::Format(LOCTEXT("MarkerTip", "{0} · {1}"), Current->DescribeEvent(Event), NWPlannerStyle::Clock(NWPlannerStyle::TicksToSeconds(Event.At))))
			];
	}

	// Playhead while executing.
	Canvas->AddSlot()
		.Position(TAttribute<FVector2D>::CreateLambda([WeakEditor, this]()
		{
			const float T = WeakEditor.IsValid() ? WeakEditor->GetExecution().SimulatedSeconds : 0.f;
			return FVector2D(T * PixelsPerSecond - 1.f, 0.f);
		}))
		.Size(FVector2D(2.f, H))
		[
			SNew(SBorder)
			.Visibility_Lambda([this]() { return IsExecuting() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderImage(NWPlannerStyle::White())
			.BorderBackgroundColor(FLinearColor::White)
		];

	const bool bSelected = CrewIndex == Current->GetSelectedCrew();
	return SNew(SBox)
		.HeightOverride(H)
		.WidthOverride(TimelineWidth)
		[
			SNew(SBorder)
			.BorderImage(NWPlannerStyle::White())
			.BorderBackgroundColor(bSelected ? NWPlannerStyle::SelectedTrack : NWPlannerStyle::TrackBackground)
			.Padding(0.f)
			[
				Canvas
			]
		];
}

#undef LOCTEXT_NAMESPACE
