#include "DCTimelinePanel.h"

#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCInventoryComponent.h"
#include "DCItem.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
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
	constexpr float TrackWidth = 640.f;
	/** The bar keeps this width in every mode, so nothing jumps. */
	constexpr float PanelWidth = ListWidth + 14.f + NameWidth + TrackWidth + 28.f;
	/** Messages appear this far above the bottom edge, just over the bar. */
	constexpr float ToastBottom = 318.f;
	constexpr float RulerHeight = 16.f;
	constexpr float RowHeight = 24.f;
	constexpr float RowPitch = 30.f;
	constexpr float MinVisibleSeconds = 30.f;
	const FLinearColor WaitBlockColor(0.32f, 0.33f, 0.36f);
	const FLinearColor WaitTextColor(0.72f, 0.74f, 0.78f);
	const FLinearColor StepBlockedColor(0.75f, 0.1f, 0.06f);
	const FLinearColor StepBlockedTextColor(1.f, 0.42f, 0.35f);
	const FLinearColor DoneColor(0.35f, 0.78f, 0.4f);
	const FLinearColor HighlightRowColor(1.f, 1.f, 1.f, 0.09f);
	const FLinearColor PickedRowColor(1.f, 0.85f, 0.25f, 0.14f);
	const FLinearColor SelectedTrackColor(0.11f, 0.115f, 0.145f);
	const FLinearColor InactiveSegmentColor(0.14f, 0.15f, 0.19f);
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
		Danger
	};

	/** A flat button that calls the controller. */
	TSharedRef<SWidget> PanelButton(TWeakObjectPtr<UDCTimelinePanel> Panel, const TAttribute<FText>& Label, const FText& Tip,
		TFunction<void(ADCPlayerController*)> OnClick, int32 FontSize = 12, const FMargin& Padding = FMargin(10.f, 3.f), EPanelButton Kind = EPanelButton::Normal)
	{
		using namespace DCItemWidgets;
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
				SNew(STextBlock)
				.Text(Label)
				.Font(Font(FontSize, true))
				.ColorAndOpacity(Kind == EPanelButton::Primary ? DarkTextColor : Kind == EPanelButton::Danger ? StepBlockedTextColor : TextColor)
			];
		if (Kind == EPanelButton::Primary)
		{
			Button->SetButtonStyle(&PrimaryButtonStyle());
		}
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
	const auto PreviewOn = [WeakThis]()
	{
		const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
		return Heist && Heist->GetPreviewTime() >= 0.f && GetPlanMode(WeakThis.Get()) == EDCPlanMode::Planning;
	};
	// One half of the mode switch "Planen | Ausprobieren": filled while that mode is on.
	const auto Segment = [WeakThis, Mode](const FText& Label, const FText& Tip, EDCPlanMode Own, TFunction<void(ADCPlayerController*)> OnClick)
	{
		return SNew(SBorder)
			.BorderImage(Brush())
			.Padding(FMargin(12.f, 4.f))
			.Cursor(EMouseCursor::Hand)
			.ToolTipText(Tip)
			.BorderBackgroundColor_Lambda([Mode, Own]() -> FSlateColor { return Mode() == Own ? TitleColor : InactiveSegmentColor; })
			.OnMouseButtonDown_Lambda([WeakThis, OnClick](const FGeometry&, const FPointerEvent&)
			{
				if (ADCPlayerController* Controller = GetController(WeakThis.Get()))
				{
					OnClick(Controller);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Font(12, true))
				.ColorAndOpacity_Lambda([Mode, Own]() -> FSlateColor { return Mode() == Own ? DarkTextColor : TextColor; })
			];
	};

	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		// Short messages ("Vic, Schritt 3: Schublade knacken", "Geht nicht: …") just above the bar.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, ToastBottom))
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.Padding(FMargin(16.f, 7.f))
			.Visibility_Lambda([WeakThis]()
			{
				const ADCPlayerController* Controller = GetController(WeakThis.Get());
				bool bProblem = false;
				return Controller && !Controller->GetToast(bProblem).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.BorderBackgroundColor_Lambda([WeakThis]() -> FSlateColor
			{
				const ADCPlayerController* Controller = GetController(WeakThis.Get());
				bool bProblem = false;
				if (Controller)
				{
					Controller->GetToast(bProblem);
				}
				return bProblem ? FLinearColor(0.45f, 0.06f, 0.04f, 0.95f) : FLinearColor(0.03f, 0.035f, 0.05f, 0.95f);
			})
			[
				SNew(STextBlock)
				.Font(Font(13, true))
				.ColorAndOpacity(TextColor)
				.Text_Lambda([WeakThis]()
				{
					const ADCPlayerController* Controller = GetController(WeakThis.Get());
					bool bProblem = false;
					return Controller ? Controller->GetToast(bProblem) : FText::GetEmpty();
				})
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(12.f)
		[
			SNew(SBox)
			.WidthOverride(PanelWidth)
			[
				MakeSwallowZone(
					// Orange frame while trying things out: clicks act at once there.
					SNew(SBorder)
					.BorderImage(Brush())
					.Padding(FMargin(2.f))
					.BorderBackgroundColor_Lambda([Mode]() -> FSlateColor { return Mode() == EDCPlanMode::Direct ? DirectFrameColor : PanelColor; })
					.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					[
						SNew(SBorder)
						.BorderImage(Brush())
						.BorderBackgroundColor(PanelColor)
						.Padding(FMargin(12.f, 8.f))
						[
							SNew(SVerticalBox)
							// Mode, state, the main action, the rest small.
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									Segment(LOCTEXT("ModePlan", "Planen"), LOCTEXT("ModePlanTip", "Schritte planen; Klicks hängen Schritte an den Plan der gewählten Figur"), EDCPlanMode::Planning,
										[](ADCPlayerController* C) { C->ReturnToPlanningIfNeeded(); })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f, 0.f, 0.f)
								[
									Segment(LOCTEXT("ModeTry", "Ausprobieren"), LOCTEXT("ModeTryTip", "Ohne Plan ausprobieren: Klicks wirken sofort (der Raum wird beim Abspielen zurückgesetzt)"), EDCPlanMode::Direct,
										[](ADCPlayerController* C) { C->SetDirectMode(true); })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Font(Font(12, true))
									.ColorAndOpacity(TextColor)
									.Text_Lambda([WeakThis]()
									{
										const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
										const float Duration = State ? State->GetSchedule().Duration : 0.f;
										switch (GetPlanMode(WeakThis.Get()))
										{
										case EDCPlanMode::Planning:
											return FText::Format(LOCTEXT("Planning", "Plan dauert {0}"), FormatTime(Duration));
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
								+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
									[
										PanelButton(WeakThis, LOCTEXT("Play", "Abspielen"), LOCTEXT("PlayTip", "Den ganzen Plan abspielen (Leertaste)"),
											[](ADCPlayerController* C) { C->StartPlayback(); }, 13, FMargin(20.f, 3.f), EPanelButton::Primary)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Playing }))
									[
										PanelButton(WeakThis, TAttribute<FText>::CreateLambda([WeakThis]()
										{
											const ADCPlayerController* Controller = GetController(WeakThis.Get());
											return Controller && Controller->IsPlaybackPaused() ? LOCTEXT("Resume", "Weiter") : LOCTEXT("Pause", "Pause");
										}), LOCTEXT("PauseTip", "Anhalten / weiter (Leertaste)"), [](ADCPlayerController* C) { C->TogglePause(); }, 13, FMargin(20.f, 3.f), EPanelButton::Primary)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Playing }))
									[
										PanelButton(WeakThis, LOCTEXT("Back", "Zurück zur Planung"), LOCTEXT("BackTip", "Raum zurücksetzen, der Plan bleibt"), [](ADCPlayerController* C) { C->ReturnToPlanning(); })
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
								// Deleting everything asks once more: a second click within a few seconds.
								+ SHorizontalBox::Slot().AutoWidth().Padding(16.f, 0.f, 0.f, 0.f)
								[
									SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
									[
										PanelButton(WeakThis, TAttribute<FText>::CreateLambda([WeakThis]()
										{
											return WeakThis.IsValid() && WeakThis->IsClearArmed() ? LOCTEXT("ClearSure", "Wirklich alles löschen?") : LOCTEXT("Clear", "Plan leeren");
										}), LOCTEXT("ClearTip", "Alle Schritte aller Figuren löschen (zweimal klicken)"), [WeakThis](ADCPlayerController* C)
										{
											if (WeakThis.IsValid() && WeakThis->IsClearArmed())
											{
												WeakThis->ClearArmedUntil = 0.0;
												C->ClearPlan();
											}
											else if (WeakThis.IsValid())
											{
												WeakThis->ClearArmedUntil = FPlatformTime::Seconds() + 4.0;
											}
										}, 12, FMargin(10.f, 3.f), EPanelButton::Danger)
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
												.BorderImage(Brush())
												.BorderBackgroundColor(ZoneColor)
												.Padding(2.f)
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
										SAssignNew(Tracks, SBox).WidthOverride(NameWidth + TrackWidth)
									]
									// What everyone does at the preview time.
									+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 6.f, 0.f, 0.f)
									[
										SNew(SBox)
										.WidthOverride(TrackWidth)
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
										return LOCTEXT("HintPlanning", "Klick auf Boden, Tür oder Gegenstand = nächster Schritt · Karte links, Taste 1–9 oder Figur anklicken = andere Figur · Leertaste = abspielen");
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
	FString Result = FString::Printf(TEXT("%d|%d|%s|%d|"), State ? State->GetScheduleVersion() : 0, static_cast<int32>(GetPlanMode(this)),
		Selected ? *Selected->GetName() : TEXT(""), State ? State->GetFocusStep() : INDEX_NONE);
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
	return TrackWidth / Visible;
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
			SNew(STextBlock).Text(FText::Format(LOCTEXT("PlanOf", "Plan von {0}"), Crew->Name)).Font(Font(14, true)).ColorAndOpacity(Crew->Color)
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
					[](ADCPlayerController* C) { C->OpenWaitDialog(); }, 12, FMargin(10.f, 2.f))
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
						[ItemId](ADCPlayerController* C) { C->PlanDrop(ItemId); }, 11, FMargin(6.f, 0.f))
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

	for (const FDCScheduledTask& Task : Crew->Tasks)
	{
		const int32 Index = Task.Index;
		const bool bPicked = State->GetFocusCrew() == Focus && State->GetFocusStep() == Index;
		const bool bCoordination = Task.Type == EDCPlanTaskType::WaitFor || Task.Type == EDCPlanTaskType::Signal;

		// Waiting for someone else without being asked to: one compact line.
		const float Waited = Task.WorkStart - Task.Ready;
		if (!Task.bBlocked && Waited > 0.5f && Task.Type != EDCPlanTaskType::WaitFor)
		{
			const FText WaitText = Task.WaitFor.IsNone()
				? FText::Format(LOCTEXT("WaitsPlain", "wartet {0} s – {1}"), FormatSeconds(Waited), Task.WaitReason)
				: FText::Format(LOCTEXT("WaitsFor", "wartet {0} s, bis {1} Schritt {2} erledigt hat"), FormatSeconds(Waited), Task.WaitForName, FText::AsNumber(Task.WaitForIndex + 1));
			ListRows->AddSlot().AutoHeight().Padding(30.f, 1.f, 4.f, 1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(38.f)[SNew(STextBlock).Text(FormatTime(Task.Ready)).Font(Font(11)).ColorAndOpacity(MutedColor)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(WaitText).Font(Font(11)).ColorAndOpacity(Task.WaitFor.IsNone() ? WaitTextColor : CoordinationColor).AutoWrapText(true)
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
		if (Task.bBlocked || Status == EDCTaskStatus::Failed)
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
		if (Task.bBlocked)
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
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Task.Label)
					.Font(Font(12, true))
					.ColorAndOpacity(Task.bBlocked ? StepBlockedTextColor : bCoordination ? CoordinationColor : TextColor)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ToolTipText(Tip)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock).Text(Info).Font(Font(11, InfoColor != MutedColor)).ColorAndOpacity(InfoColor)
				]
			];
		if (Task.bBlocked)
		{
			Lines->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(Task.WaitReason).Font(Font(11)).ColorAndOpacity(StepBlockedTextColor).AutoWrapText(true)
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
							bCoordination ? LOCTEXT("DeleteLinkedTip", "Schritt löschen – Warten und Zeichen gehören zusammen und gehen gemeinsam") : LOCTEXT("DeleteTip", "Schritt löschen"),
							true, [Index](ADCPlayerController* C) { C->RemoveStep(Index); }, EPanelButton::Danger)
					]
				];
			}
		}

		const float StepTime = Task.WorkStart;
		ListRows->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.Padding(FMargin(4.f, 3.f))
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
				return FLinearColor::Transparent;
			})
			// Click picks the step (details and buttons open), a second click closes it again.
			.OnMouseButtonDown_Lambda([WeakThis, Focus, Index, StepTime, bPicked](const FGeometry&, const FPointerEvent&)
			{
				if (ADCPlayerController* Controller = GetController(WeakThis.Get()))
				{
					Controller->FocusStep(Focus, bPicked ? INDEX_NONE : Index);
					if (!bPicked)
					{
						Controller->SetPreviewTime(StepTime);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(24.f).HeightOverride(20.f)
					[
						SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(Badge).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
						[
							SNew(STextBlock).Text(FText::AsNumber(Index + 1)).Font(Font(11, true)).ColorAndOpacity(DarkTextColor)
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(38.f)
					[
						SNew(STextBlock).Text(FormatTime(Task.bBlocked || Task.Type == EDCPlanTaskType::WaitFor ? Task.Ready : Task.WorkStart)).Font(Font(11)).ColorAndOpacity(MutedColor)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					Lines
				]
			]
		];
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

	// Ruler: a mark every 5 s, a number every 10 s.
	for (int32 Second = 0; Second <= FMath::FloorToInt(VisibleSeconds); Second += 5)
	{
		const bool bMajor = Second % 10 == 0;
		Canvas->AddSlot().Position(FVector2D(NameWidth + Second * Scale, bMajor ? 0.0 : 8.0)).Size(FVector2D(1.0, bMajor ? RulerHeight : 8.0))
		[
			SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(MutedColor).Visibility(EVisibility::HitTestInvisible)
		];
		if (bMajor)
		{
			Canvas->AddSlot().Position(FVector2D(NameWidth + Second * Scale + 3.0, 0.0)).Size(FVector2D(40.0, 14.0))
			[
				SNew(STextBlock).Text(FormatTime(static_cast<float>(Second))).Font(Font(10)).ColorAndOpacity(MutedColor).Visibility(EVisibility::HitTestInvisible)
			];
		}
	}

	const auto RowTop = [](int32 Row) { return RulerHeight + Row * RowPitch; };
	for (int32 Row = 0; Row < Schedule.Crews.Num(); ++Row)
	{
		const FDCCrewSchedule& Crew = Schedule.Crews[Row];
		const bool bSelectedRow = Crew.Crew == Focus;
		const float Top = RowTop(Row);
		const ADCCrewCharacter* Member = State ? State->FindCrew(Crew.Crew) : nullptr;
		const UDCPlanRunnerComponent* Runner = Member ? Member->GetPlanRunner() : nullptr;

		Canvas->AddSlot().Position(FVector2D(0.0, Top)).Size(FVector2D(NameWidth - 6.0, RowHeight))
		[
			SNew(SBox).VAlign(VAlign_Center).Visibility(EVisibility::HitTestInvisible)
			[
				SNew(STextBlock)
				.Text(bSelectedRow ? FText::Format(LOCTEXT("SelectedName", "> {0}"), Crew.Name) : Crew.Name)
				.Font(Font(bSelectedRow ? 13 : 12, true))
				.ColorAndOpacity(bSelectedRow ? Crew.Color : Dim(Crew.Color, 0.75f))
			]
		];
		Canvas->AddSlot().Position(FVector2D(NameWidth, Top)).Size(FVector2D(TrackWidth, RowHeight))
		[
			SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(bSelectedRow ? SelectedTrackColor : ZoneColor).Visibility(EVisibility::HitTestInvisible)
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

			const auto AddBlock = [&Canvas, Scale, Top, &Tip, WeakThis, CrewName, TaskIndex, StepTime](float From, float To, const FLinearColor& Color, const FText& Text, bool bOutline)
			{
				const float Width = FMath::Max(0.f, (To - From) * Scale);
				if (Width < 1.5f)
				{
					return;
				}
				Canvas->AddSlot().Position(FVector2D(NameWidth + From * Scale, Top + 2.0)).Size(FVector2D(Width - 1.0, RowHeight - 4.0))
				[
					SNew(SBorder)
					.BorderImage(Brush())
					.BorderBackgroundColor(bOutline ? FLinearColor::White : Color)
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
						SNew(SBorder)
						.BorderImage(Brush())
						.BorderBackgroundColor(Color)
						.Padding(FMargin(3.f, 0.f))
						.VAlign(VAlign_Center)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(STextBlock).Text(Width >= 12.f ? Text : FText::GetEmpty()).Font(Font(10, true)).ColorAndOpacity(DarkTextColor)
						]
					]
				];
			};

			const FText Number = FText::AsNumber(Task.Index + 1);
			if (Task.bBlocked)
			{
				// Will not happen: a short red block where it would start.
				AddBlock(Task.Ready, Task.Ready + 56.f / Scale, StepBlockedColor, FText::Format(LOCTEXT("BlockedLabel", "{0} geht nicht"), Number), bPicked);
				continue;
			}
			const FLinearColor Base = Crew.Color;
			AddBlock(Task.Start, Task.Ready, Dim(Base, 0.42f, Fade), FText::GetEmpty(), false);
			AddBlock(Task.Ready, Task.WorkStart, FLinearColor(WaitBlockColor.R, WaitBlockColor.G, WaitBlockColor.B, Fade), LOCTEXT("WaitLabel", "wartet"), false);
			const bool bSignal = Task.Type == EDCPlanTaskType::Signal;
			const FLinearColor Work = bSignal ? FLinearColor(CoordinationColor.R, CoordinationColor.G, CoordinationColor.B, Fade)
				: Task.bLoud ? FLinearColor(1.f, 0.35f, 0.15f, Fade) : FLinearColor(Base.R, Base.G, Base.B, Fade);
			const float WorkWidth = (FMath::Max(Task.End, Task.WorkStart + (bSignal ? 1.2f : 0.4f)) - Task.WorkStart) * Scale;
			const FText WorkText = WorkWidth >= 80.f ? FText::Format(LOCTEXT("WorkLabel", "{0} {1}"), Number, Task.Label) : Number;
			AddBlock(Task.WorkStart, Task.WorkStart + WorkWidth / Scale, Status == EDCTaskStatus::Failed ? StepBlockedColor : Work, WorkText, bCurrent || bPicked);
		}
	}

	// Who waits for whom: a line from the step that frees the way down (or up) to the one waiting.
	for (int32 Row = 0; Row < Schedule.Crews.Num(); ++Row)
	{
		for (const FDCScheduledTask& Task : Schedule.Crews[Row].Tasks)
		{
			if (Task.WaitFor.IsNone() || Task.bBlocked)
			{
				continue;
			}
			const int32 Source = Schedule.Crews.IndexOfByPredicate([&Task](const FDCCrewSchedule& Entry) { return Entry.Crew == Task.WaitFor; });
			if (Source == INDEX_NONE)
			{
				continue;
			}
			const float X = NameWidth + Task.WorkStart * Scale - 1.f;
			const float From = RowTop(FMath::Min(Row, Source)) + RowHeight * 0.5f;
			const float To = RowTop(FMath::Max(Row, Source)) + RowHeight * 0.5f;
			const FLinearColor Color = Schedule.Crews[Source].Color;
			Canvas->AddSlot().Position(FVector2D(X, From)).Size(FVector2D(3.0, To - From))
			[
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(CoordinationColor).Visibility(EVisibility::HitTestInvisible)
			];
			const float DotY = RowTop(Source) + RowHeight * 0.5f - 5.f;
			Canvas->AddSlot().Position(FVector2D(X - 4.0, DotY)).Size(FVector2D(10.0, 10.0))
			[
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(CoordinationColor).Padding(2.f).Visibility(EVisibility::HitTestInvisible)
				[
					SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(Color)
				]
			];
		}
	}

	// Preview time (planning, yellow) and playback time (white).
	Canvas->AddSlot()
		.Position(TAttribute<FVector2D>::CreateLambda([WeakThis, Scale]()
		{
			const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
			const float Time = Heist ? (Heist->IsPlaying() ? Heist->GetPlaybackTime() : Heist->GetPreviewTime()) : 0.f;
			return FVector2D(NameWidth + FMath::Clamp(Time * Scale, 0.f, TrackWidth - 2.f), 0.0);
		}))
		.Size(FVector2D(2.0, Height))
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.BorderBackgroundColor_Lambda([WeakThis]() -> FSlateColor
			{
				const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
				return Heist && Heist->IsPlaying() ? FLinearColor::White : TitleColor;
			})
			.Visibility_Lambda([WeakThis]()
			{
				const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
				const bool bShow = Heist && (Heist->IsPlaying() || (Heist->GetPreviewTime() >= 0.f && GetPlanMode(WeakThis.Get()) == EDCPlanMode::Planning));
				return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
		];

	Tracks->SetContent(SNew(SBox).WidthOverride(NameWidth + TrackWidth).HeightOverride(Height + 2.f)[Canvas]);
}

#undef LOCTEXT_NAMESPACE
