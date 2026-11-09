#include "DCTimelinePanel.h"

#include "DCFrame.h"
#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCTimelinePanel"

namespace
{
	constexpr float ListWidth = 440.f;
	constexpr float ListHeight = 160.f;
	constexpr float NameWidth = 64.f;
	/** The timeline gets whatever width is left; at least this much. */
	constexpr float MinTrackWidth = 640.f;
	/** The bar keeps this width in every mode, so nothing jumps. */
	/** Everything of the bar that is not the timeline itself. */
	constexpr float PanelFixedWidth = ListWidth + 14.f + NameWidth + 28.f;
	/** Messages appear this far above the bottom edge, just over the bar. */
	constexpr float ToastBottom = UDCFrame::BottomHeight + 12.f;
	/** Times on top, the measuring marks below them. */
	constexpr float RulerHeight = 24.f;
	constexpr float RulerLabelWidth = 44.f;
	/** Times on the ruler are at least this far apart. */
	constexpr float RulerLabelSpacing = 46.f;
	/** The tab with the time on top of the time marker. */
	constexpr float MarkerLabelWidth = 38.f;
	constexpr float RowHeight = 24.f;
	/** Room between the lanes for the arrows of who waits for whom. */
	constexpr float RowPitch = 32.f;
	constexpr float MinVisibleSeconds = 30.f;
	// Linear values, sRGB hex of the mockup palette in the comments (see Docs/UI-Leitfaden.md).
	const FLinearColor WaitTextColor(0.4851f, 0.5089f, 0.5457f);        // #B8BCC3
	/** Waiting in the timeline: grey ground with lighter stripes. */
	const FLinearColor WaitGroundColor(0.0545f, 0.0586f, 0.0686f);      // #424449
	const FLinearColor WaitStripeColor(0.4851f, 0.5089f, 0.5457f, 0.32f); // #B8BCC3, faint
	/** Faint stripes on the lanes: nothing planned there. */
	const FLinearColor LaneStripeColor(1.f, 1.f, 1.f, 0.045f);
	const FLinearColor StepBlockedColor(0.4793f, 0.0356f, 0.0242f);     // #B8352B
	const FLinearColor StepBlockedTextColor(0.8550f, 0.1441f, 0.1095f); // #EE6A5D = MessageColor
	const FLinearColor DoneColor(0.1981f, 0.5520f, 0.2122f);            // #7BC47F = OkColor
	const FLinearColor HighlightRowColor(0.0296f, 0.0395f, 0.0561f);    // #303843
	const FLinearColor PickedRowColor(0.0704f, 0.0423f, 0.0160f);       // #4B3A22 amber-dark
	const FLinearColor SelectedTrackColor(0.0212f, 0.0296f, 0.0409f);   // #283039
	const FLinearColor ToastProblemColor(0.1946f, 0.0232f, 0.0185f, 0.97f); // #7A2A25
	/** Frame around the bar while trying things out (clicks act at once). */
	const FLinearColor DirectFrameColor(1.f, 0.5f, 0.1f);

	FText FormatTime(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
	}

	/** Durations in whole seconds ("19"). */
	FText FormatSeconds(float Seconds)
	{
		return FText::AsNumber(FMath::Max(0, FMath::RoundToInt(Seconds)));
	}

	ADCPlayerController* GetController(const UUserWidget* Widget)
	{
		return Widget ? Cast<ADCPlayerController>(Widget->GetOwningPlayer()) : nullptr;
	}

	EDCPlanMode GetPlanMode(const UObject* Context)
	{
		const UDCPlanSubsystem* Plans = Context ? UDCPlanSubsystem::Get(Context) : nullptr;
		return Plans ? Plans->GetMode() : EDCPlanMode::Planning;
	}

	/**
	 * Where the time marker of the timeline stands: the playback time while the plan runs, otherwise the preview time while
	 * planning. False if there is none to show.
	 */
	bool GetMarkerTime(const UObject* Context, float& OutTime, bool& bOutRunning)
	{
		const UDCHeistStateSubsystem* Heist = Context ? UDCHeistStateSubsystem::Get(Context) : nullptr;
		OutTime = 0.f;
		bOutRunning = false;
		if (!Heist)
		{
			return false;
		}
		if (Heist->IsPlaying())
		{
			OutTime = Heist->GetPlaybackTime();
			bOutRunning = true;
			return true;
		}
		OutTime = FMath::Max(0.f, Heist->GetPreviewTime());
		return Heist->GetPreviewTime() >= 0.f && GetPlanMode(Context) == EDCPlanMode::Planning;
	}

	FLinearColor Dim(const FLinearColor& Color, float Factor, float Alpha = 1.f)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, Alpha);
	}

	enum class EPanelButton : uint8
	{
		Normal,
		/** The one main action: filled in the focus colour. */
		Primary,
		/** Destroys something: red text. */
		Danger,
		/** Working together (waits, signals): cyan text. */
		Coordination
	};

	/** The icon in front of the plan status: works out, hangs, runs, done. */
	FName StatusIcon(const UDCTimelinePanel* Panel)
	{
		const UDCHeistStateSubsystem* State = Panel ? UDCHeistStateSubsystem::Get(Panel) : nullptr;
		if (!State)
		{
			return NAME_None;
		}
		switch (GetPlanMode(Panel))
		{
		case EDCPlanMode::Planning:
			if (!State->GetSchedule().FindProblems().IsEmpty())
			{
				return TEXT("triangle-exclamation-point");
			}
			return State->GetSchedule().Duration > 0.f ? FName(TEXT("circle-check")) : NAME_None;
		case EDCPlanMode::Playing:
			return State->IsPlaybackFinished() ? TEXT("circle-check") : TEXT("play");
		default:
			return NAME_None;
		}
	}

	/** A flat button that calls the controller. */
	TSharedRef<SWidget> PanelButton(TWeakObjectPtr<UDCTimelinePanel> Panel, const TAttribute<FText>& Label, const TAttribute<FText>& Tip,
		TFunction<void(ADCPlayerController*)> OnClick, int32 FontSize = 12, const FMargin& Padding = FMargin(10.f, 3.f), EPanelButton Kind = EPanelButton::Normal,
		const TAttribute<FName>& IconName = TAttribute<FName>())
	{
		using namespace DCItemWidgets;
		const FLinearColor Ink = Kind == EPanelButton::Primary ? DarkTextColor : Kind == EPanelButton::Danger ? StepBlockedTextColor : Kind == EPanelButton::Coordination ? CoordinationColor : TextColor;
		const float IconSize = IconSizeFor(FontSize);
		TSharedRef<SButton> Button = SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(Tip)
			.ContentPadding(Padding)
			.OnClicked_Lambda([Panel, OnClick]()
			{
				if (ADCPlayerController* Controller = GetController(Panel.Get()))
				{
					OnClick(Controller);
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
					.Visibility_Lambda([IconName]() { return Icon(IconName.Get()) ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SBox).WidthOverride(IconSize).HeightOverride(IconSize)
						[
							SNew(SImage).Image_Lambda([IconName]() { return Icon(IconName.Get()); }).ColorAndOpacity(Ink)
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(Font(FontSize, true))
					.ColorAndOpacity(Ink)
				]
			];
		Button->SetButtonStyle(Kind == EPanelButton::Primary ? &PrimaryButtonStyle() : Kind == EPanelButton::Danger ? &DangerButtonStyle() : &NormalButtonStyle());
		return Button;
	}
}

TSharedRef<SWidget> UDCTimelinePanel::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCTimelinePanel> WeakThis = this;

	const auto Mode = [WeakThis]() { return GetPlanMode(WeakThis.Get()); };
	const auto ShowIn = [Mode](TArray<EDCPlanMode> Modes)
	{
		return TAttribute<EVisibility>::CreateLambda([Mode, Modes]() { return Modes.Contains(Mode()) ? EVisibility::Visible : EVisibility::Collapsed; });
	};
	// Planning, and the plan hangs (bHangs) / works out (!bHangs).
	const auto PlanHangs = [WeakThis](bool bHangs)
	{
		return TAttribute<EVisibility>::CreateLambda([WeakThis, bHangs]()
		{
			const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
			if (!State || GetPlanMode(WeakThis.Get()) != EDCPlanMode::Planning)
			{
				return EVisibility::Collapsed;
			}
			return State->GetSchedule().FindProblems().IsEmpty() != bHangs ? EVisibility::Visible : EVisibility::Collapsed;
		});
	};
	const auto PreviewOn = [WeakThis]()
	{
		const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
		return Heist && Heist->GetPreviewTime() >= 0.f && GetPlanMode(WeakThis.Get()) == EDCPlanMode::Planning;
	};
	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		// Short messages ("Vic, Schritt 3: Schublade knacken", "Geht nicht: …") just above the bar.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, ToastBottom))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
			SNew(SBorder)
			.Padding(FMargin(0.f))
			.Visibility_Lambda([WeakThis]()
			{
				const ADCPlayerController* Controller = GetController(WeakThis.Get());
				bool bProblem = false;
				return Controller && !Controller->GetToast(bProblem).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.BorderImage_Lambda([WeakThis]()
			{
				const ADCPlayerController* Controller = GetController(WeakThis.Get());
				bool bProblem = false;
				if (Controller)
				{
					Controller->GetToast(bProblem);
				}
				return bProblem ? ProblemFramedBrush() : FramedBrush();
			})
			.BorderBackgroundColor_Lambda([WeakThis]() -> FSlateColor
			{
				const ADCPlayerController* Controller = GetController(WeakThis.Get());
				bool bProblem = false;
				if (Controller)
				{
					Controller->GetToast(bProblem);
				}
				return bProblem ? ToastProblemColor : PanelColor;
			})
			[
				SNew(SHorizontalBox)
				// An amber (red: problem) bar on the left edge.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(4.f)
					[
						SNew(SBorder)
						.BorderImage(RoundBrush())
						.BorderBackgroundColor_Lambda([WeakThis]() -> FSlateColor
						{
							const ADCPlayerController* Controller = GetController(WeakThis.Get());
							bool bProblem = false;
							if (Controller)
							{
								Controller->GetToast(bProblem);
							}
							return bProblem ? MessageColor : TitleColor;
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)
					[
						SNew(SImage)
						.Image_Lambda([WeakThis]()
						{
							const ADCPlayerController* Controller = GetController(WeakThis.Get());
							bool bProblem = false;
							if (Controller)
							{
								Controller->GetToast(bProblem);
							}
							return Icon(bProblem ? TEXT("triangle-exclamation-point") : TEXT("crosshair"));
						})
						.ColorAndOpacity_Lambda([WeakThis]() -> FSlateColor
						{
							const ADCPlayerController* Controller = GetController(WeakThis.Get());
							bool bProblem = false;
							if (Controller)
							{
								Controller->GetToast(bProblem);
							}
							return bProblem ? MessageColor : TitleColor;
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 7.f, 18.f, 7.f))
				[
					SNew(STextBlock)
					.Font(Font(14, true))
					.ColorAndOpacity(TextColor)
					.Text_Lambda([WeakThis]()
					{
						const ADCPlayerController* Controller = GetController(WeakThis.Get());
						bool bProblem = false;
						return Controller ? Controller->GetToast(bProblem) : FText::GetEmpty();
					})
				]
			]
			]
			// A click made the plan hang: the help stays until the player reacts.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(ProblemFramedBrush())
				.Padding(FMargin(14.f, 8.f))
				.BorderBackgroundColor(ToastProblemColor)
				.Visibility_Lambda([WeakThis]()
				{
					const ADCPlayerController* Controller = GetController(WeakThis.Get());
					return Controller && !Controller->GetPlanHelp().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						MakeIcon(TEXT("triangle-exclamation-point"), 18.f, MessageColor)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
					[
						SNew(SBox)
						.MaxDesiredWidth(780.f)
						[
							SNew(STextBlock)
							.Font(Font(13, true))
							.ColorAndOpacity(TextColor)
							.AutoWrapText(true)
							.Text_Lambda([WeakThis]()
							{
								const ADCPlayerController* Controller = GetController(WeakThis.Get());
								return Controller ? Controller->GetPlanHelp() : FText::GetEmpty();
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
					[
						PanelButton(WeakThis, LOCTEXT("HelpUndo", "Rückgängig"), LOCTEXT("HelpUndoTip", "Die letzte Änderung zurücknehmen (Strg+Z)"),
							[](ADCPlayerController* C) { C->DismissPlanHelp(); C->UndoPlan(); }, 12, FMargin(10.f, 3.f), EPanelButton::Normal, FName(TEXT("undo-2")))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
					[
						PanelButton(WeakThis, LOCTEXT("HelpLater", "Später"), LOCTEXT("HelpLaterTip", "Hinweis ausblenden – oben in der Leiste steht weiter, was hängt"),
							[](ADCPlayerController* C) { C->DismissPlanHelp(); }, 12, FMargin(10.f, 3.f))
					]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(12.f)
		[
			SNew(SBox)
			.WidthOverride_Lambda([WeakThis]() { return FOptionalSize(PanelFixedWidth + (WeakThis.IsValid() ? WeakThis->TrackWidthNow : MinTrackWidth)); })
			[
				MakeSwallowZone(
					// Orange frame while trying things out: clicks act at once there.
					SNew(SBorder)
					.BorderImage(RoundBrush())
					.Padding(FMargin(2.f))
					.BorderBackgroundColor_Lambda([Mode]() -> FSlateColor { return Mode() == EDCPlanMode::Direct ? DirectFrameColor : FLinearColor::Transparent; })
					.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					[
						SNew(SBorder)
						.BorderImage(FramedBrush())
						.BorderBackgroundColor(PanelColor)
						.Padding(FMargin(14.f, 10.f))
						[
							SNew(SVerticalBox)
							// Mode, state, the main action, the rest small.
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SNew(SHorizontalBox)
								// One plain way to take a step away: the picked one, else the last one (Entf does the same).
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SBox)
									.Visibility(ShowIn({ EDCPlanMode::Planning }))
									.IsEnabled(TAttribute<bool>::CreateLambda([WeakThis]()
									{
										const ADCPlayerController* Controller = GetController(WeakThis.Get());
										return Controller && !Controller->DescribeDeleteStep().IsEmpty();
									}))
									[
										PanelButton(WeakThis, TAttribute<FText>::CreateLambda([WeakThis]()
										{
											const ADCPlayerController* Controller = GetController(WeakThis.Get());
											const FText Label = Controller ? Controller->DescribeDeleteStep() : FText::GetEmpty();
											return Label.IsEmpty() ? LOCTEXT("DeleteNone", "Letzten Schritt löschen") : Label;
										}), LOCTEXT("DeleteTip", "Löscht den gewählten Schritt der Figur, sonst ihren letzten (Entf)"),
										[](ADCPlayerController* C) { C->DeleteStep(); }, 12, FMargin(10.f, 3.f), EPanelButton::Normal, FName(TEXT("trash")))
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
								[
									SNew(SBox)
									.WidthOverride(17.f)
									.HeightOverride(17.f)
									.Visibility_Lambda([WeakThis]() { return Icon(StatusIcon(WeakThis.Get())) ? EVisibility::Visible : EVisibility::Collapsed; })
									[
										SNew(SImage)
										.Image_Lambda([WeakThis]() { return Icon(StatusIcon(WeakThis.Get())); })
										.ColorAndOpacity_Lambda([WeakThis]() -> FSlateColor
										{
											const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
											if (State && GetPlanMode(WeakThis.Get()) == EDCPlanMode::Planning && !State->GetSchedule().FindProblems().IsEmpty())
											{
												return StepBlockedTextColor;
											}
											return GetPlanMode(WeakThis.Get()) == EDCPlanMode::Playing && !(State && State->IsPlaybackFinished()) ? TextColor : OkColor;
										})
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f, 0.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Font(Font(14, true))
									.ColorAndOpacity_Lambda([WeakThis]() -> FSlateColor
									{
										const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
										if (!State || GetPlanMode(WeakThis.Get()) != EDCPlanMode::Planning)
										{
											return TextColor;
										}
										if (!State->GetSchedule().FindProblems().IsEmpty())
										{
											return StepBlockedTextColor;
										}
										return State->GetSchedule().Duration > 0.f ? OkColor : MutedColor;
									})
									.Text_Lambda([WeakThis]()
									{
										const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
										const float Duration = State ? State->GetSchedule().Duration : 0.f;
										switch (GetPlanMode(WeakThis.Get()))
										{
										case EDCPlanMode::Planning:
										{
											// Does the plan work out? Said at a fixed place, all the time.
											const TArray<FDCPlanProblem> Problems = State ? State->GetSchedule().FindProblems() : TArray<FDCPlanProblem>();
											if (!Problems.IsEmpty())
											{
												return FText::Format(LOCTEXT("PlanHangs", "Plan hängt: {0} ab Schritt {1} – {2}"), Problems[0].CrewName, FText::AsNumber(Problems[0].Index + 1), Problems[0].ShortReason);
											}
											return Duration > 0.f ? FText::Format(LOCTEXT("PlanWorks", "Plan geht auf · fertig {0}"), FormatTime(Duration)) : LOCTEXT("PlanEmptyStatus", "Noch kein Plan");
										}
										case EDCPlanMode::Playing:
											return State && State->IsPlaybackFinished()
												? FText::Format(LOCTEXT("Finished", "abgespielt in {0}"), FormatTime(State->GetPlaybackTime()))
												: FText::Format(LOCTEXT("Playing", "Plan läuft: {0} von {1}"), FormatTime(State ? State->GetPlaybackTime() : 0.f), FormatTime(Duration));
										case EDCPlanMode::Direct:
											return LOCTEXT("Direct", "Klicks wirken sofort");
										}
										return FText::GetEmpty();
									})
								]
								// The plan hangs: one click shows where.
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
								[
									SNew(SBox).Visibility(PlanHangs(true))
									[
										PanelButton(WeakThis, LOCTEXT("Solve", "Lösen …"), LOCTEXT("SolveTip", "Den Schritt zeigen, der nicht geht, und sagen, warum"),
											[](ADCPlayerController* C) { C->ShowPlanProblem(); }, 11, FMargin(8.f, 2.f), EPanelButton::Normal, FName(TEXT("crosshair")))
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(PlanHangs(false))
									[
										PanelButton(WeakThis, LOCTEXT("Play", "Abspielen"), LOCTEXT("PlayTip", "Den ganzen Plan abspielen (Leertaste)"),
											[](ADCPlayerController* C) { C->StartPlayback(); }, 14, FMargin(20.f, 4.f), EPanelButton::Primary, FName(TEXT("play")))
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(PlanHangs(true))
									[
										PanelButton(WeakThis, LOCTEXT("PlayAnyway", "Trotzdem abspielen"), LOCTEXT("PlayAnywayTip", "Der Plan hängt irgendwo – trotzdem abspielen (Leertaste)"),
											[](ADCPlayerController* C) { C->StartPlayback(); }, 12, FMargin(14.f, 3.f), EPanelButton::Normal, FName(TEXT("play")))
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Playing }))
									[
										PanelButton(WeakThis, TAttribute<FText>::CreateLambda([WeakThis]()
										{
											const ADCPlayerController* Controller = GetController(WeakThis.Get());
											if (Controller && Controller->IsDecidingFind())
											{
												return LOCTEXT("FindOpen", "Fund offen");
											}
											return Controller && Controller->IsPlaybackPaused() ? LOCTEXT("Resume", "Weiter") : LOCTEXT("Pause", "Pause");
										}), LOCTEXT("PauseTip", "Anhalten / weiter (Leertaste)"), [](ADCPlayerController* C) { C->TogglePause(); }, 13, FMargin(20.f, 3.f), EPanelButton::Primary,
											TAttribute<FName>::CreateLambda([WeakThis]()
											{
												const ADCPlayerController* Controller = GetController(WeakThis.Get());
												if (Controller && Controller->IsDecidingFind())
												{
													return FName(TEXT("search"));
												}
												return FName(Controller && Controller->IsPlaybackPaused() ? TEXT("play") : TEXT("pause"));
											}))
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Playing }))
									[
										PanelButton(WeakThis, LOCTEXT("Back", "Zurück zur Planung"), LOCTEXT("BackTip", "Raum zurücksetzen, der Plan bleibt"), [](ADCPlayerController* C) { C->ReturnToPlanning(); }, 12, FMargin(10.f, 3.f), EPanelButton::Normal, FName(TEXT("undo-2")))
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning, EDCPlanMode::Playing }))
									[
										PanelButton(WeakThis, TAttribute<FText>::CreateLambda([WeakThis]()
										{
											const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
											return FText::Format(LOCTEXT("Speed", "Tempo {0}×"), FText::AsNumber(Plans ? FMath::RoundToInt(Plans->GetSpeed()) : 1));
										}), LOCTEXT("SpeedTip", "Abspieltempo 1×, 2×, 4×"), [](ADCPlayerController* C) { C->CycleSpeed(); })
									]
								]
								// Deleting everything at once.
								+ SHorizontalBox::Slot().AutoWidth().Padding(16.f, 0.f, 0.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
									[
										PanelButton(WeakThis, LOCTEXT("Clear", "Plan leeren"), LOCTEXT("ClearTip", "Alle Schritte aller Figuren löschen"),
											[](ADCPlayerController* C) { C->ClearPlan(); }, 12, FMargin(10.f, 3.f), EPanelButton::Danger)
									]
								]
							]
							// Left the plan of the selected crew member, right the timeline of all.
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox)
									.WidthOverride(ListWidth)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
										[
											SAssignNew(ListHeader, SVerticalBox)
										]
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(ListHeight)
											[
												SNew(SBorder)
												.BorderImage(SoftFramedBrush())
												.BorderBackgroundColor(ZoneColor)
												.Padding(4.f)
												[
													SNew(SScrollBox)
													+ SScrollBox::Slot()
													[
														SAssignNew(ListRows, SVerticalBox)
													]
												]
											]
										]
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SAssignNew(Tracks, SBox).WidthOverride_Lambda([WeakThis]() { return FOptionalSize(NameWidth + (WeakThis.IsValid() ? WeakThis->TrackWidthNow : MinTrackWidth)); })
									]
									// What everyone does at the preview time.
									+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 6.f, 0.f, 0.f)
									[
										SNew(SBox)
										.WidthOverride_Lambda([WeakThis]() { return FOptionalSize(WeakThis.IsValid() ? WeakThis->TrackWidthNow : MinTrackWidth); })
										[
											SNew(STextBlock)
											.Font(Font(12, true))
											.ColorAndOpacity(TitleColor)
											.AutoWrapText(true)
											.Visibility_Lambda([PreviewOn]() { return PreviewOn() ? EVisibility::Visible : EVisibility::Collapsed; })
											.Text_Lambda([WeakThis]()
											{
												const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
												if (!Heist)
												{
													return FText::GetEmpty();
												}
												const float Time = Heist->GetPreviewTime();
												TArray<FString> Parts;
												for (const FDCCrewSchedule& Crew : Heist->GetSchedule().Crews)
												{
													Parts.Add(FString::Printf(TEXT("%s: %s"), *Crew.Name.ToString(), *UDCHeistStateSubsystem::DescribeActivityAt(Crew, Time).ToString()));
												}
												return FText::Format(LOCTEXT("PreviewLine", "Bei {0} – {1}"), FormatTime(Time), FText::FromString(FString::Join(Parts, TEXT("  ·  "))));
											})
										]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 4.f, 0.f, 0.f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Font(Font(11))
											.ColorAndOpacity(MutedColor)
											.AutoWrapText(true)
											.Visibility_Lambda([PreviewOn, Mode]() { return !PreviewOn() && Mode() == EDCPlanMode::Planning ? EVisibility::Visible : EVisibility::Collapsed; })
											.Text(LOCTEXT("PreviewHint", "In die Zeitleiste klicken: zeigt im Raum, wo alle zu diesem Zeitpunkt sind."))
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SBox)
											.Visibility_Lambda([PreviewOn]() { return PreviewOn() ? EVisibility::Visible : EVisibility::Collapsed; })
											[
												PanelButton(WeakThis, LOCTEXT("PreviewOff", "Vorschau aus"), LOCTEXT("PreviewOffTip", "Die Stecknadeln im Raum ausblenden (Esc)"),
													[](ADCPlayerController* C) { C->SetPreviewTime(-1.f); }, 11, FMargin(8.f, 1.f))
											]
										]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Font(Font(11))
								.ColorAndOpacity(MutedColor)
								.Text_Lambda([Mode]()
								{
									switch (Mode())
									{
									case EDCPlanMode::Planning:
										return LOCTEXT("HintPlanning", "Klick auf Boden, Tür oder Gegenstand = nächster Schritt · Schritt anklicken = dahinter einfügen (Esc = ans Ende) · Taste 1–9 = andere Figur · Entf = Schritt löschen · Leertaste = abspielen");
									case EDCPlanMode::Playing:
										return LOCTEXT("HintPlaying", "grau = wartet auf etwas, das jemand anderes erst erledigt · Leertaste = Pause");
									case EDCPlanMode::Direct:
										return LOCTEXT("HintDirect", "Klicks wirken sofort · „Planen“ setzt den Raum zurück");
									}
									return FText::GetEmpty();
								})
							]
						]
					])
			]
		];
	Signature.Reset();
	return Root.ToSharedRef();
}

void UDCTimelinePanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	ListHeader.Reset();
	ListRows.Reset();
	Tracks.Reset();
}

void UDCTimelinePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Track = FMath::Max(MinTrackWidth, static_cast<float>(MyGeometry.GetLocalSize().X) - 24.f - PanelFixedWidth);
	if (FMath::Abs(Track - TrackWidthNow) > 1.f)
	{
		TrackWidthNow = Track;
		Signature.Reset();
	}
	const FString Now = MakeSignature();
	if (ListRows.IsValid() && Now != Signature)
	{
		Signature = Now;
		RebuildRows();
	}
}

FString UDCTimelinePanel::MakeSignature() const
{
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	const ADCPlayerController* Controller = GetController(this);
	const ADCCrewCharacter* Selected = Controller ? Controller->GetSelectedCrew() : nullptr;
	FString Result = FString::Printf(TEXT("%d|%d|%s|%d|%d|"), State ? State->GetScheduleVersion() : 0, static_cast<int32>(GetPlanMode(this)),
		Selected ? *Selected->GetName() : TEXT(""), State ? State->GetFocusStep() : INDEX_NONE, Controller ? Controller->GetInsertIndex() : INDEX_NONE);
	if (!State)
	{
		return Result;
	}
	// What the hands hold after the plan.
	if (const UDCInventoryComponent* Carried = Selected ? State->GetPlannedInventory(Selected->GetFName()) : nullptr)
	{
		for (const EDCEquipSlot Hand : { EDCEquipSlot::RightHand, EDCEquipSlot::LeftHand })
		{
			const UDCItem* Held = Carried->GetSlotItem(Hand);
			Result += Held ? Held->GetDefId().ToString() : TEXT("-");
		}
		Result += TEXT("|");
	}
	if (State->IsPlaying())
	{
		for (const FDCCrewSchedule& Crew : State->GetSchedule().Crews)
		{
			if (const ADCCrewCharacter* Member = State->FindCrew(Crew.Crew))
			{
				const UDCPlanRunnerComponent* Runner = Member->GetPlanRunner();
				Result += FString::Printf(TEXT("%d:"), Runner->GetCurrentIndex());
				for (const FDCScheduledTask& Task : Crew.Tasks)
				{
					Result += FString::FromInt(static_cast<int32>(Runner->GetStatus(Task.Index)));
				}
				Result += TEXT(";");
			}
		}
	}
	return Result;
}

float UDCTimelinePanel::GetScale() const
{
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	const float Duration = State ? State->GetSchedule().Duration : 0.f;
	const float Visible = FMath::Max(MinVisibleSeconds, FMath::CeilToFloat((Duration + 5.f) / 10.f) * 10.f);
	return TrackWidthNow / Visible;
}

void UDCTimelinePanel::RebuildRows()
{
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	if (!State || !ListRows.IsValid() || !ListHeader.IsValid() || !Tracks.IsValid())
	{
		return;
	}
	const ADCPlayerController* Controller = GetController(this);
	const ADCCrewCharacter* Selected = Controller ? Controller->GetSelectedCrew() : nullptr;
	const FName Focus = Selected ? Selected->GetFName() : NAME_None;
	RebuildList(State->GetSchedule(), Focus);
	RebuildTracks(State->GetSchedule(), Focus);
}

void UDCTimelinePanel::RebuildList(const FDCSchedule& Schedule, FName Focus)
{
	using namespace DCItemWidgets;
	ListHeader->ClearChildren();
	ListRows->ClearChildren();
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	const FDCCrewSchedule* Crew = Schedule.Find(Focus);
	if (!State || !Crew)
	{
		ListHeader->AddSlot().AutoHeight()[MakeMuted(LOCTEXT("NoCrew", "Links eine Figur wählen, um ihren Plan zu sehen"), 12)];
		return;
	}
	const bool bPlanning = GetPlanMode(this) == EDCPlanMode::Planning;
	const bool bPlaying = State->IsPlaying();
	const ADCCrewCharacter* Member = State->FindCrew(Focus);
	const UDCPlanRunnerComponent* Runner = Member ? Member->GetPlanRunner() : nullptr;
	TWeakObjectPtr<UDCTimelinePanel> WeakThis = this;

	// Whose plan, how long – and "Warten …" right here, for this crew member.
	ListHeader->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeTitle(FText::Format(LOCTEXT("PlanOf", "Plan von {0}"), Crew->Name), 19)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(Crew->Tasks.IsEmpty() ? LOCTEXT("NoSteps", "noch leer")
				: FText::Format(Crew->Tasks.Num() == 1 ? LOCTEXT("PlanSummaryOne", "1 Schritt · fertig {1}") : LOCTEXT("PlanSummary", "{0} Schritte · fertig {1}"),
					FText::AsNumber(Crew->Tasks.Num()), FormatTime(Crew->End)))
			.Font(Font(12))
			.ColorAndOpacity(MutedColor)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility(bPlanning ? EVisibility::Visible : EVisibility::Collapsed)
			[
				PanelButton(WeakThis, LOCTEXT("WaitDialogButton", "Warten …"),
					FText::Format(LOCTEXT("WaitDialogTip", "{0} warten lassen: eine feste Zeit, bis eine andere Figur einen Schritt erledigt hat, oder bis sie fertig ist"), Crew->Name),
					[](ADCPlayerController* C) { C->OpenWaitDialog(); }, 12, FMargin(10.f, 2.f), EPanelButton::Normal, FName(TEXT("hourglass")))
			]
		]
	];

	// What the hands hold once the plan has run – and putting it down, as a planned step.
	if (const UDCInventoryComponent* Carried = State->GetPlannedInventory(Focus))
	{
		TSharedRef<SHorizontalBox> Hands = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("HandsAfter", "Hände danach:")).Font(Font(12)).ColorAndOpacity(MutedColor)
			];
		const UDCItem* Right = Carried->GetSlotItem(EDCEquipSlot::RightHand);
		const UDCItem* Left = Carried->GetSlotItem(EDCEquipSlot::LeftHand);
		const auto AddHand = [&Hands, bPlanning, WeakThis](const FText& Which, const UDCItem* Item)
		{
			Hands->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("HandItem", "{0} {1}"), Which, Item ? Item->GetDisplayName() : LOCTEXT("HandFree", "frei")))
				.Font(Font(12, Item != nullptr))
				.ColorAndOpacity(Item ? TextColor : MutedColor)
			];
			if (Item && bPlanning)
			{
				const FName ItemId = Item->GetDefId();
				Hands->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					PanelButton(WeakThis, LOCTEXT("DropButton", "ablegen"),
						FText::Format(LOCTEXT("DropTip", "Als nächsten Schritt: {0} auf den Boden legen (macht die Hand frei)"), Item->GetDisplayName()),
						[ItemId](ADCPlayerController* C) { C->PlanDrop(ItemId); }, 11, FMargin(6.f, 0.f), EPanelButton::Normal, FName(TEXT("arrow-down-to-line")))
				];
			}
			else
			{
				Hands->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(8.f)];
			}
		};
		if (Right && Right == Left)
		{
			AddHand(LOCTEXT("BothHands", "beide:"), Right);
		}
		else
		{
			AddHand(LOCTEXT("RightHand", "rechts:"), Right);
			AddHand(LOCTEXT("LeftHand", "links:"), Left);
		}
		ListHeader->AddSlot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[Hands];
	}

	if (Crew->Tasks.IsEmpty())
	{
		ListRows->AddSlot().AutoHeight().Padding(8.f, 8.f)
		[
			MakeMuted(bPlanning ? LOCTEXT("EmptyPlan", "Noch keine Schritte. Boden, Tür oder Gegenstand anklicken.") : LOCTEXT("EmptyPlanPlaying", "Kein Plan."), 12)
		];
		return;
	}

	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	const TArray<FDCPlanTask>* PlanTasks = Plans ? &Plans->GetTasks(Focus) : nullptr;
	for (const FDCScheduledTask& Task : Crew->Tasks)
	{
		const int32 Index = Task.Index;
		const FName StepIcon = PlanTasks && PlanTasks->IsValidIndex(Index) ? IconForTask((*PlanTasks)[Index]) : NAME_None;
		const bool bPicked = State->GetFocusCrew() == Focus && State->GetFocusStep() == Index;
		const bool bCoordination = Task.Type == EDCPlanTaskType::WaitFor || Task.Type == EDCPlanTaskType::Signal;

		// Waiting for someone else without being asked to: one compact line.
		const float Waited = Task.WorkStart - Task.Ready + Task.WaitedOnTheWay();
		if (!Task.bBlocked && Waited > 0.5f && Task.Type != EDCPlanTaskType::WaitFor)
		{
			const FText WaitText = UDCHeistStateSubsystem::DescribeWaited(Task, Waited);
			ListRows->AddSlot().AutoHeight().Padding(30.f, 1.f, 4.f, 1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(38.f)[SNew(STextBlock).Text(FormatTime(Task.Ready)).Font(Font(11)).ColorAndOpacity(MutedColor)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(WaitText).Font(Font(12)).ColorAndOpacity(Task.WaitFor.IsNone() ? WaitTextColor : CoordinationColor).AutoWrapText(true)
				]
			];
		}

		// Who waits for this step.
		TArray<FString> Waiting;
		for (const FDCCrewSchedule& Other : Schedule.Crews)
		{
			for (const FDCScheduledTask& Theirs : Other.Tasks)
			{
				if (Theirs.WaitFor == Focus && Theirs.WaitForIndex == Index && Other.Crew != Focus)
				{
					Waiting.AddUnique(Other.Name.ToString());
				}
			}
		}

		const EDCTaskStatus Status = bPlaying && Runner ? Runner->GetStatus(Index) : EDCTaskStatus::Planned;
		FLinearColor Badge = Crew->Color;
		if (Task.bAfterBlocked && Status == EDCTaskStatus::Planned)
		{
			Badge = MutedColor;
		}
		else if (Task.bBlocked || Status == EDCTaskStatus::Failed)
		{
			Badge = StepBlockedColor;
		}
		else if (Status == EDCTaskStatus::Done)
		{
			Badge = DoneColor;
		}
		else if (Status == EDCTaskStatus::Running || Status == EDCTaskStatus::Waiting)
		{
			Badge = FLinearColor::White;
		}

		// On the right of the line: what matters most about the step.
		FText Info;
		FLinearColor InfoColor = MutedColor;
		const float WorkSeconds = Task.End - Task.WorkStart;
		if (Task.bAfterBlocked)
		{
			Info = LOCTEXT("InfoAfterBlocked", "kommt nicht dran");
		}
		else if (Task.bBlocked)
		{
			Info = LOCTEXT("InfoBlocked", "geht nicht");
			InfoColor = StepBlockedTextColor;
		}
		else if (!Waiting.IsEmpty())
		{
			Info = FText::Format(LOCTEXT("InfoWaiting", "→ {0} wartet"), FText::FromString(FString::Join(Waiting, TEXT(", "))));
			InfoColor = CoordinationColor;
		}
		else if (Task.Type == EDCPlanTaskType::WaitFor)
		{
			Info = Waited > 0.5f ? FText::Format(LOCTEXT("InfoWaited", "wartet {0} s"), FormatSeconds(Waited)) : LOCTEXT("InfoNoWait", "kein Warten");
			InfoColor = CoordinationColor;
		}
		else if (Task.bHasChance)
		{
			Info = FText::Format(LOCTEXT("InfoChance", "{0} s · {1} %"), FormatSeconds(WorkSeconds), FText::AsNumber(FMath::RoundToInt((1.f - Task.FailChance) * 100.f)));
		}
		else if (WorkSeconds >= 0.5f)
		{
			Info = FText::Format(LOCTEXT("InfoSeconds", "{0} s"), FormatSeconds(WorkSeconds));
		}

		TArray<FString> TipLines = { Task.Label.ToString() };
		for (const FText& Line : { Task.Detail, Task.Carry, Task.bBlocked ? Task.WaitReason : FText::GetEmpty() })
		{
			if (!Line.IsEmpty())
			{
				TipLines.Add(Line.ToString());
			}
		}
		const FText Tip = FText::FromString(FString::Join(TipLines, TEXT("\n")));

		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, StepIcon.IsNone() ? 0.f : 6.f, 0.f)
				[
					MakeIcon(StepIcon, 14.f, Task.bAfterBlocked ? MutedColor : Task.bBlocked ? StepBlockedTextColor : bCoordination ? CoordinationColor : MutedColor)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Task.Label)
					.Font(Font(12, true))
					.ColorAndOpacity(Task.bAfterBlocked ? MutedColor : Task.bBlocked ? StepBlockedTextColor : bCoordination ? CoordinationColor : TextColor)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ToolTipText(Tip)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock).Text(Info).Font(Font(11, InfoColor != MutedColor)).ColorAndOpacity(InfoColor)
				]
			];
		if (Task.bBlocked && !Task.bAfterBlocked)
		{
			Lines->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(Task.WaitReason).Font(Font(12)).ColorAndOpacity(StepBlockedTextColor).AutoWrapText(true)
			];
		}
		// The picked step shows everything about it, and the buttons to move or delete it.
		if (bPicked)
		{
			for (const TPair<FText, FLinearColor>& Line : TArray<TPair<FText, FLinearColor>>{ { Task.Detail, MutedColor }, { Task.Carry, LootColor } })
			{
				if (!Line.Key.IsEmpty())
				{
					Lines->AddSlot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(Line.Key).Font(Font(11)).ColorAndOpacity(Line.Value).AutoWrapText(true)
					];
				}
			}
			if (bPlanning)
			{
				const int32 Count = Crew->Tasks.Num();
				const auto RowButton = [WeakThis](const FText& Label, const FText& ButtonTip, bool bEnabled, TFunction<void(ADCPlayerController*)> OnClick, EPanelButton Kind)
				{
					return SNew(SBox).IsEnabled(bEnabled)[PanelButton(WeakThis, Label, ButtonTip, MoveTemp(OnClick), 11, FMargin(8.f, 1.f), Kind)];
				};
				Lines->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 2.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						RowButton(LOCTEXT("Up", "↑ früher"), LOCTEXT("UpTip", "Schritt eins nach vorn"), Index > 0, [Index](ADCPlayerController* C) { C->MoveStep(Index, -1); }, EPanelButton::Normal)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						RowButton(LOCTEXT("Down", "↓ später"), LOCTEXT("DownTip", "Schritt eins nach hinten"), Index < Count - 1, [Index](ADCPlayerController* C) { C->MoveStep(Index, 1); }, EPanelButton::Normal)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						RowButton(LOCTEXT("Delete", "× löschen"),
							bCoordination ? LOCTEXT("DeleteLinkedTip", "Schritt löschen – Warten und Zeichen gehören zusammen und gehen gemeinsam") : LOCTEXT("DeleteTip", "Schritt löschen (Entf)"),
							true, [Index](ADCPlayerController* C) { C->RemoveStep(Index); }, EPanelButton::Danger)
					]
				];
				// Working together: one click lets another crew member wait for this step.
				const UDCPlanSubsystem* PlanSource = UDCPlanSubsystem::Get(WeakThis.Get());
				const TArray<FDCPlanTask>& OwnSteps = PlanSource ? PlanSource->GetTasks(Focus) : TArray<FDCPlanTask>();
				if (!bCoordination && OwnSteps.IsValidIndex(Index))
				{
					const FGuid StepId = OwnSteps[Index].Id;
					TSharedRef<SHorizontalBox> WaitButtons = SNew(SHorizontalBox);
					for (const FDCCrewSchedule& Other : Schedule.Crews)
					{
						if (Other.Crew == Focus)
						{
							continue;
						}
						const bool bWaits = PlanSource->GetTasks(Other.Crew).ContainsByPredicate([&StepId](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::WaitFor && Task.Ref == StepId; });
						const FName OtherCrew = Other.Crew;
						WaitButtons->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
						[
							RowButton(FText::Format(bWaits ? LOCTEXT("WaitsNoMore", "{0} wartet nicht mehr") : LOCTEXT("WaitsOnThis", "{0} wartet hierauf"), Other.Name),
								FText::Format(bWaits ? LOCTEXT("WaitsNoMoreTip", "{0} soll nicht mehr auf diesen Schritt warten") : LOCTEXT("WaitsOnThisTip", "{0} wartet, bis dieser Schritt erledigt ist – vor ihrem ersten Schritt, der sonst zu früh anfangen würde"), Other.Name),
								true, [Focus, Index, OtherCrew](ADCPlayerController* C) { C->ToggleWaitOn(Focus, Index, OtherCrew); }, EPanelButton::Coordination)
						];
					}
					Lines->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 2.f)[WaitButtons];
				}
			}
		}

		const float StepTime = Task.WorkStart;
		ListRows->AddSlot().AutoHeight().Padding(0.f, 1.f, 0.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(bPicked ? FocusFramedBrush() : SoftFramedBrush())
			.Padding(FMargin(5.f, 4.f))
			.Cursor(EMouseCursor::Hand)
			.BorderBackgroundColor_Lambda([WeakThis, Focus, Index, bPicked]() -> FSlateColor
			{
				if (bPicked)
				{
					return PickedRowColor;
				}
				const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
				if (Heist && Heist->GetPreviewTime() >= 0.f)
				{
					const FDCCrewSchedule* Own = Heist->GetSchedule().Find(Focus);
					if (Own && UDCHeistStateSubsystem::GetStepAt(*Own, Heist->GetPreviewTime()) == Index)
					{
						return HighlightRowColor;
					}
				}
				return PanelColor;
			})
			// Click picks the step (details and buttons open), a second click closes it again.
			.OnMouseButtonDown_Lambda([WeakThis, Focus, Index, StepTime, bPicked](const FGeometry&, const FPointerEvent&)
			{
				if (ADCPlayerController* Controller = GetController(WeakThis.Get()))
				{
					Controller->FocusStep(Focus, bPicked ? INDEX_NONE : Index);
					// The room shows the moment after the step (doors, ghost) - no preview pins.
					Controller->SetPreviewTime(-1.f);
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(24.f).HeightOverride(20.f)
					[
						SNew(SBorder).BorderImage(RoundBrush()).BorderBackgroundColor(Badge).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
						[
							SNew(STextBlock).Text(FText::AsNumber(Index + 1)).Font(Font(12, true)).ColorAndOpacity(DarkTextColor)
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(38.f)
					[
						SNew(STextBlock).Text(FormatTime(Task.bBlocked || Task.Type == EDCPlanTaskType::WaitFor ? Task.Ready : Task.WorkStart)).Font(Font(12)).ColorAndOpacity(MutedColor)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					Lines
				]
			]
		];
		// The picked step: new steps come right after it (variant: only after "+ danach einfügen").
		const ADCPlayerController* InsertOwner = GetController(WeakThis.Get());
		if (bPicked && bPlanning && InsertOwner && InsertOwner->GetInsertIndex() != Index + 1)
		{
			ListRows->AddSlot().AutoHeight().Padding(30.f, 2.f, 4.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					PanelButton(WeakThis, LOCTEXT("InsertAfter", "+ danach einfügen"), LOCTEXT("InsertAfterTip", "Neue Schritte kommen direkt hinter diesen Schritt"),
						[Index](ADCPlayerController* C) { C->InsertAfterStep(Index); }, 11, FMargin(8.f, 1.f))
				]
			];
		}
		else if (bPicked && bPlanning)
		{
			ListRows->AddSlot().AutoHeight().Padding(30.f, 2.f, 4.f, 4.f)
			[
				SNew(SBorder)
				.BorderImage(FocusFramedBrush())
				.BorderBackgroundColor(PickedRowColor)
				.Padding(FMargin(8.f, 3.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("InsertHere", "+ Neue Schritte kommen hierhin")).Font(Font(12, true)).ColorAndOpacity(TitleColor)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						PanelButton(WeakThis, LOCTEXT("ToEnd", "ans Ende"), LOCTEXT("ToEndTip", "Neue Schritte wieder hinten anhängen (Esc)"),
							[](ADCPlayerController* C) { C->InsertAtEnd(); }, 11, FMargin(8.f, 1.f))
					]
				]
			];
		}
	}
}

void UDCTimelinePanel::OnTrackMouse(const FGeometry& Geometry, const FPointerEvent& Event, float Scale, bool /*bPress*/)
{
	ADCPlayerController* Controller = GetController(this);
	if (!Controller)
	{
		return;
	}
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	if (Local.X < NameWidth)
	{
		// On a name: that crew member's plan.
		const int32 Row = FMath::FloorToInt((Local.Y - RulerHeight) / RowPitch);
		if (State && State->GetSchedule().Crews.IsValidIndex(Row))
		{
			Controller->FocusStep(State->GetSchedule().Crews[Row].Crew, INDEX_NONE);
		}
		return;
	}
	Controller->SetPreviewTime(FMath::Max(0.f, static_cast<float>(Local.X - NameWidth) / Scale));
}

void UDCTimelinePanel::RebuildTracks(const FDCSchedule& Schedule, FName Focus)
{
	using namespace DCItemWidgets;
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	const float Scale = GetScale();
	const float TrackWidth = TrackWidthNow;
	const float VisibleSeconds = TrackWidth / Scale;
	const bool bPlaying = State && State->IsPlaying();
	const float Height = RulerHeight + Schedule.Crews.Num() * RowPitch;
	TWeakObjectPtr<UDCTimelinePanel> WeakThis = this;
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);

	// Underneath everything: clicking or dragging sets the preview time.
	Canvas->AddSlot().Position(FVector2D::ZeroVector).Size(FVector2D(NameWidth + TrackWidth, Height))
	[
		SNew(SBorder)
		.BorderImage(Brush())
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Cursor(EMouseCursor::Hand)
		.ToolTipText(LOCTEXT("TrackTip", "Klicken oder ziehen: im Raum zeigen, wo alle zu diesem Zeitpunkt sind"))
		.OnMouseButtonDown_Lambda([WeakThis, Scale](const FGeometry& Geometry, const FPointerEvent& Event)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTrackMouse(Geometry, Event, Scale, true);
			}
			return FReply::Handled();
		})
		.OnMouseMove_Lambda([WeakThis, Scale](const FGeometry& Geometry, const FPointerEvent& Event)
		{
			if (WeakThis.IsValid() && Event.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				WeakThis->OnTrackMouse(Geometry, Event, Scale, false);
			}
			return FReply::Handled();
		})
	];

	// Ruler like a measuring tape: a hairline along the bottom, a short mark every second (when there is room), a longer one
	// every 5 s and the time above every 5 s – or every 10, 15, 30 s when the plan is long and the marks get crowded.
	int32 LabelEvery = 5;
	for (const int32 Candidate : { 5, 10, 15, 30, 60 })
	{
		LabelEvery = Candidate;
		if (Candidate * Scale >= RulerLabelSpacing)
		{
			break;
		}
	}
	const int32 TickEvery = Scale >= 5.f ? 1 : Scale * 5.f >= 5.f ? 5 : LabelEvery;
	const int32 LastSecond = FMath::FloorToInt(VisibleSeconds + 0.01f);
	Canvas->AddSlot().Position(FVector2D(NameWidth, 0.0)).Size(FVector2D(TrackWidth, RulerHeight))
	[
		MakePainter([Scale, LastSecond, LabelEvery, TickEvery](const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer)
		{
			const float Bottom = RulerHeight - 1.5f;
			const float Width = static_cast<float>(Geometry.GetLocalSize().X);
			FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), TArray<FVector2f>{ FVector2f(0.f, Bottom), FVector2f(Width, Bottom) },
				ESlateDrawEffect::None, BrassDimColor, false, 1.f);
			for (int32 Second = 0; Second <= LastSecond; Second += TickEvery)
			{
				const float X = FMath::Min(FMath::RoundToFloat(Second * Scale), Width - 1.f) + 0.5f;
				const bool bLabel = Second % LabelEvery == 0;
				const float Length = bLabel ? 7.f : Second % 5 == 0 ? 5.f : 3.f;
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), TArray<FVector2f>{ FVector2f(X, Bottom - Length), FVector2f(X, Bottom) },
					ESlateDrawEffect::None, bLabel ? MutedColor : BrassDimColor, false, 1.f);
			}
			return Layer;
		})
	];
	for (int32 Second = 0; Second <= LastSecond; Second += LabelEvery)
	{
		Canvas->AddSlot().Position(FVector2D(NameWidth + Second * Scale - RulerLabelWidth * 0.5f, 0.0)).Size(FVector2D(RulerLabelWidth, 14.0))
		[
			SNew(STextBlock)
			.Text(FormatTime(static_cast<float>(Second)))
			.Justification(ETextJustify::Center)
			.Font(Font(10))
			.ColorAndOpacity(MutedColor)
			.Visibility(EVisibility::HitTestInvisible)
		];
	}

	const auto RowTop = [](int32 Row) { return RulerHeight + Row * RowPitch; };
	for (int32 Row = 0; Row < Schedule.Crews.Num(); ++Row)
	{
		const FDCCrewSchedule& Crew = Schedule.Crews[Row];
		const bool bSelectedRow = Crew.Crew == Focus;
		const float Top = RowTop(Row);
		const ADCCrewCharacter* Member = State ? State->FindCrew(Crew.Crew) : nullptr;
		const UDCPlanRunnerComponent* Runner = Member ? Member->GetPlanRunner() : nullptr;

		// Name with a bar in the crew colour, as on the profile cards.
		Canvas->AddSlot().Position(FVector2D(0.0, Top)).Size(FVector2D(NameWidth - 6.0, RowHeight))
		[
			SNew(SHorizontalBox)
			.Visibility(EVisibility::HitTestInvisible)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 2.f, 7.f, 2.f))
			[
				SNew(SBox).WidthOverride(4.f)
				[
					SNew(SBorder).BorderImage(RoundBrush()).BorderBackgroundColor(bSelectedRow ? Crew.Color : Dim(Crew.Color, 0.55f))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Crew.Name)
				.Font(Font(bSelectedRow ? 14 : 13, true))
				.ColorAndOpacity(bSelectedRow ? Crew.Color : Dim(Crew.Color, 0.75f))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
		];
		// The lane: faint stripes wherever nothing is planned; the selected crew member's lane has an outline.
		Canvas->AddSlot().Position(FVector2D(NameWidth, Top)).Size(FVector2D(TrackWidth, RowHeight))
		[
			SNew(SBorder)
			.BorderImage(bSelectedRow ? SoftFramedBrush() : RoundBrush())
			.BorderBackgroundColor(bSelectedRow ? SelectedTrackColor : ZoneColor)
			.Padding(0.f)
			.Visibility(EVisibility::HitTestInvisible)
			[
				MakeHatch(FLinearColor::Transparent, LaneStripeColor)
			]
		];

		for (const FDCScheduledTask& Task : Crew.Tasks)
		{
			const EDCTaskStatus Status = bPlaying && Runner ? Runner->GetStatus(Task.Index) : EDCTaskStatus::Planned;
			const bool bCurrent = bPlaying && Runner && Runner->IsRunning() && Runner->GetCurrentIndex() == Task.Index;
			const bool bPicked = State && State->GetFocusCrew() == Crew.Crew && State->GetFocusStep() == Task.Index;
			const float Fade = Status == EDCTaskStatus::Done ? 0.55f : 1.f;

			TArray<FString> TipLines;
			TipLines.Add(FString::Printf(TEXT("%s, Schritt %d: %s"), *Crew.Name.ToString(), Task.Index + 1, *Task.Label.ToString()));
			if (!Task.Detail.IsEmpty())
			{
				TipLines.Add(Task.Detail.ToString());
			}
			if (!Task.Carry.IsEmpty())
			{
				TipLines.Add(Task.Carry.ToString());
			}
			if (!Task.WaitFor.IsNone())
			{
				TipLines.Add(FText::Format(LOCTEXT("TipWaitsFor", "wartet {0} s auf {1} (Schritt {2})"), FormatSeconds(Task.WorkStart - Task.Ready), Task.WaitForName, FText::AsNumber(Task.WaitForIndex + 1)).ToString());
			}
			else if (!Task.WaitReason.IsEmpty())
			{
				TipLines.Add(Task.WaitReason.ToString());
			}
			if (!Task.bBlocked)
			{
				TipLines.Add(FString::Printf(TEXT("%s – %s s"), *FormatSeconds(Task.Start).ToString(), *FormatSeconds(Task.End).ToString()));
			}
			if (Status == EDCTaskStatus::Failed)
			{
				TipLines.Add(LOCTEXT("Failed", "nicht geschafft").ToString());
			}
			const FText Tip = FText::FromString(FString::Join(TipLines, TEXT("\n")));
			const FName CrewName = Crew.Crew;
			const int32 TaskIndex = Task.Index;
			const float StepTime = Task.WorkStart;

			// One block: solid in Color with the number (and the label if given), or hatched grey while waiting.
			const auto AddBlock = [&Canvas, Scale, Top, &Tip, WeakThis, CrewName, TaskIndex, StepTime, Fade]
				(float From, float To, const FLinearColor& Color, const FText& Number, const FText& Label, bool bOutline, bool bWaiting)
			{
				const float Width = FMath::Max(0.f, (To - From) * Scale);
				if (Width < 1.5f)
				{
					return;
				}
				TSharedPtr<SWidget> Content;
				if (bWaiting)
				{
					const FLinearColor Ground(WaitGroundColor.R, WaitGroundColor.G, WaitGroundColor.B, Fade);
					Content = SNew(SOverlay)
						+ SOverlay::Slot()
						[
							MakeHatch(Ground, FLinearColor(WaitStripeColor.R, WaitStripeColor.G, WaitStripeColor.B, WaitStripeColor.A * Fade))
						]
						// The word sits on a gap in the stripes, so it stays readable.
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f))
						[
							SNew(SBorder)
							.BorderImage(RoundBrush())
							.BorderBackgroundColor(Ground)
							.Padding(FMargin(3.f, 0.f))
							.Visibility(Width >= 52.f && !Label.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
							[
								SNew(STextBlock).Text(Label).Font(Font(10, true)).ColorAndOpacity(FLinearColor(WaitTextColor.R, WaitTextColor.G, WaitTextColor.B, Fade))
							]
						];
				}
				else
				{
					Content = SNew(SBorder)
						.BorderImage(RoundBrush())
						.BorderBackgroundColor(Color)
						.Padding(FMargin(5.f, 0.f))
						.HAlign(Label.IsEmpty() ? HAlign_Center : HAlign_Fill)
						.VAlign(VAlign_Center)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(Width >= 12.f ? Number : FText::GetEmpty()).Font(Font(11, true)).ColorAndOpacity(DarkTextColor)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(5.f, 0.f, 0.f, 0.f))
							[
								SNew(STextBlock)
								.Text(Label)
								.Font(Font(11))
								.ColorAndOpacity(DarkTextColor)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Visibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							]
						];
				}
				Canvas->AddSlot().Position(FVector2D(NameWidth + From * Scale, Top + 2.0)).Size(FVector2D(Width - 1.0, RowHeight - 4.0))
				[
					SNew(SBorder)
					.BorderImage(RoundBrush())
					.BorderBackgroundColor(bOutline ? FLinearColor::White : FLinearColor::Transparent)
					.Padding(bOutline ? 2.f : 0.f)
					.ToolTipText(Tip)
					.Cursor(EMouseCursor::Hand)
					.OnMouseButtonDown_Lambda([WeakThis, CrewName, TaskIndex, StepTime](const FGeometry&, const FPointerEvent&)
					{
						if (ADCPlayerController* Controller = GetController(WeakThis.Get()))
						{
							Controller->FocusStep(CrewName, TaskIndex);
							Controller->SetPreviewTime(StepTime);
						}
						return FReply::Handled();
					})
					[
						Content.ToSharedRef()
					]
				];
			};

			const FText Number = FText::AsNumber(Task.Index + 1);
			const FText WaitWord = LOCTEXT("WaitLabel", "wartet");
			if (Task.bBlocked)
			{
				// Will not happen: a short red block where it would start.
				AddBlock(Task.Ready, Task.Ready + 96.f / Scale, StepBlockedColor, Number, LOCTEXT("BlockedLabel", "geht nicht"), bPicked, false);
				continue;
			}
			const FLinearColor Base = Crew.Color;
			// Walking in stretches; a wait on the way (at a locked door) sits between them.
			float Previous = Task.Start;
			for (const FDCWalkLeg& Leg : Task.Legs)
			{
				AddBlock(Previous, Leg.Start, FLinearColor::Transparent, FText::GetEmpty(), WaitWord, false, true);
				AddBlock(Leg.Start, Leg.End, Dim(Base, 0.42f, Fade), FText::GetEmpty(), FText::GetEmpty(), false, false);
				Previous = Leg.End;
			}
			if (Task.Legs.IsEmpty())
			{
				AddBlock(Task.Start, Task.Ready, Dim(Base, 0.42f, Fade), FText::GetEmpty(), FText::GetEmpty(), false, false);
			}
			AddBlock(Task.Ready, Task.WorkStart, FLinearColor::Transparent, FText::GetEmpty(), WaitWord, false, true);
			const bool bSignal = Task.Type == EDCPlanTaskType::Signal;
			const FLinearColor Work = bSignal ? FLinearColor(CoordinationColor.R, CoordinationColor.G, CoordinationColor.B, Fade)
				: Task.bLoud ? FLinearColor(1.f, 0.35f, 0.15f, Fade) : FLinearColor(Base.R, Base.G, Base.B, Fade);
			const float WorkWidth = (FMath::Max(Task.End, Task.WorkStart + (bSignal ? 1.2f : 0.4f)) - Task.WorkStart) * Scale;
			AddBlock(Task.WorkStart, Task.WorkStart + WorkWidth / Scale, Status == EDCTaskStatus::Failed ? StepBlockedColor : Work, Number,
				WorkWidth >= 80.f ? Task.Label : FText::GetEmpty(), bCurrent || bPicked, false);
		}
	}

	// Who waits for whom: an arrow from the step that frees the way (a dot in that crew member's colour on its edge) to the
	// start of the step that waits for it.
	struct FLink
	{
		float X = 0.f;
		float SourceY = 0.f;
		float TargetY = 0.f;
		FLinearColor SourceColor;
	};
	TArray<FLink> Links;
	for (int32 Row = 0; Row < Schedule.Crews.Num(); ++Row)
	{
		for (const FDCScheduledTask& Task : Schedule.Crews[Row].Tasks)
		{
			if (Task.WaitFor.IsNone() || Task.bBlocked)
			{
				continue;
			}
			const int32 Source = Schedule.Crews.IndexOfByPredicate([&Task](const FDCCrewSchedule& Entry) { return Entry.Crew == Task.WaitFor; });
			if (Source == INDEX_NONE || Source == Row)
			{
				continue;
			}
			FLink& Link = Links.AddDefaulted_GetRef();
			Link.X = NameWidth + Task.WorkStart * Scale;
			// From the edge of the step that frees the way, so the dot does not cover its number.
			Link.SourceY = Row > Source ? RowTop(Source) + RowHeight - 2.f : RowTop(Source) + 2.f;
			Link.TargetY = Row > Source ? RowTop(Row) + 2.f : RowTop(Row) + RowHeight - 2.f;
			Link.SourceColor = Schedule.Crews[Source].Color;
		}
	}
	if (!Links.IsEmpty())
	{
		Canvas->AddSlot().Position(FVector2D::ZeroVector).Size(FVector2D(NameWidth + TrackWidth, Height))
		[
			MakePainter([Links](const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer)
			{
				for (const FLink& Link : Links)
				{
					const float Toward = Link.TargetY > Link.SourceY ? 1.f : -1.f;
					const float TipY = Link.TargetY - Toward;
					FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(),
						TArray<FVector2f>{ FVector2f(Link.X, Link.SourceY), FVector2f(Link.X, TipY) }, ESlateDrawEffect::None, CoordinationColor, true, 2.f);
					FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(),
						TArray<FVector2f>{ FVector2f(Link.X - 4.5f, TipY - Toward * 5.f), FVector2f(Link.X, TipY), FVector2f(Link.X + 4.5f, TipY - Toward * 5.f) },
						ESlateDrawEffect::None, CoordinationColor, true, 2.f);
					FSlateDrawElement::MakeBox(Out, Layer + 1,
						Geometry.ToPaintGeometry(FVector2f(10.f, 10.f), FSlateLayoutTransform(FVector2f(Link.X - 5.f, Link.SourceY - 5.f))),
						RoundBrush(), ESlateDrawEffect::None, CoordinationColor);
					FSlateDrawElement::MakeBox(Out, Layer + 2,
						Geometry.ToPaintGeometry(FVector2f(6.f, 6.f), FSlateLayoutTransform(FVector2f(Link.X - 3.f, Link.SourceY - 3.f))),
						RoundBrush(), ESlateDrawEffect::None, Link.SourceColor);
				}
				return Layer + 2;
			})
		];
	}

	// The time marker: a line through all lanes with the time on a tab above it ("0:21"), amber for the preview while
	// planning, white while the plan runs.
	const auto MarkerX = [WeakThis, Scale, TrackWidth]()
	{
		float Time = 0.f;
		bool bRunning = false;
		GetMarkerTime(WeakThis.Get(), Time, bRunning);
		return NameWidth + FMath::Clamp(Time * Scale, 0.f, TrackWidth);
	};
	const auto MarkerColor = [WeakThis]() -> FSlateColor
	{
		float Time = 0.f;
		bool bRunning = false;
		GetMarkerTime(WeakThis.Get(), Time, bRunning);
		return bRunning ? FLinearColor::White : TitleColor;
	};
	const auto MarkerVisibility = [WeakThis]()
	{
		float Time = 0.f;
		bool bRunning = false;
		return GetMarkerTime(WeakThis.Get(), Time, bRunning) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
	};
	Canvas->AddSlot()
		.Position(TAttribute<FVector2D>::CreateLambda([MarkerX]() { return FVector2D(MarkerX() - 1.f, RulerHeight - 9.f); }))
		.Size(FVector2D(2.0, Height - RulerHeight + 9.f))
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.BorderBackgroundColor_Lambda(MarkerColor)
			.Visibility_Lambda(MarkerVisibility)
		];
	Canvas->AddSlot()
		.Position(TAttribute<FVector2D>::CreateLambda([MarkerX]() { return FVector2D(MarkerX() - MarkerLabelWidth * 0.5f, 0.0); }))
		.Size(FVector2D(MarkerLabelWidth, 15.0))
		[
			SNew(SBorder)
			.BorderImage(RoundBrush())
			.BorderBackgroundColor_Lambda(MarkerColor)
			.Padding(0.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Visibility_Lambda(MarkerVisibility)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakThis]()
				{
					float Time = 0.f;
					bool bRunning = false;
					GetMarkerTime(WeakThis.Get(), Time, bRunning);
					return FormatTime(Time);
				})
				.Font(Font(11, true))
				.ColorAndOpacity(DarkTextColor)
			]
		];

	Tracks->SetContent(SNew(SBox).WidthOverride(NameWidth + TrackWidth).HeightOverride(Height + 2.f)[Canvas]);
}

#undef LOCTEXT_NAMESPACE
