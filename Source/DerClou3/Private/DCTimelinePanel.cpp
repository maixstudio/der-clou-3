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
	constexpr float ListHeight = 150.f;
	constexpr float NameWidth = 64.f;
	constexpr float TrackWidth = 640.f;
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
	const FLinearColor SelectedTrackColor(0.11f, 0.115f, 0.145f);
	const FLinearColor DarkText(0.05f, 0.05f, 0.06f);
	const FLinearColor SignalColor(1.f, 0.92f, 0.35f);
	const FLinearColor PickTrackColor(0.26f, 0.23f, 0.08f);

	FText FormatTime(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
	}

	FText FormatSeconds(float Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), Seconds).Replace(TEXT("."), TEXT(",")));
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

	/** A small flat button that calls the controller. */
	TSharedRef<SWidget> PanelButton(TWeakObjectPtr<UDCTimelinePanel> Panel, const TAttribute<FText>& Label, const FText& Tip,
		TFunction<void(ADCPlayerController*)> OnClick, int32 FontSize = 11, const FMargin& Padding = FMargin(8.f, 2.f))
	{
		using namespace DCItemWidgets;
		return SNew(SButton)
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
				SNew(STextBlock).Text(Label).Font(Font(FontSize, true))
			];
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
	const auto HeaderButton = [WeakThis, ShowIn](TArray<EDCPlanMode> Modes, const TAttribute<FText>& Label, const FText& Tip, TFunction<void(ADCPlayerController*)> OnClick)
	{
		return SNew(SBox).Visibility(ShowIn(Modes))[PanelButton(WeakThis, Label, Tip, MoveTemp(OnClick))];
	};
	const auto PreviewOn = [WeakThis]()
	{
		const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
		return Heist && Heist->GetPreviewTime() >= 0.f && GetPlanMode(WeakThis.Get()) == EDCPlanMode::Planning;
	};

	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(12.f)
		[
			MakeSwallowZone(
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(12.f, 8.f))
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					// Title and buttons.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
						[
							SNew(STextBlock).Text(LOCTEXT("Title", "Plan & Zeitleiste")).Font(Font(14, true)).ColorAndOpacity(TitleColor)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning }, LOCTEXT("Play", "Abspielen"), LOCTEXT("PlayTip", "Plan abspielen (Leertaste)"), [](ADCPlayerController* C) { C->StartPlayback(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Playing }, TAttribute<FText>::CreateLambda([WeakThis]()
							{
								const ADCPlayerController* Controller = GetController(WeakThis.Get());
								return Controller && Controller->IsPlaybackPaused() ? LOCTEXT("Resume", "Weiter") : LOCTEXT("Pause", "Pause");
							}), LOCTEXT("PauseTip", "Anhalten / weiter (Leertaste)"), [](ADCPlayerController* C) { C->TogglePause(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Playing, EDCPlanMode::Direct }, LOCTEXT("Back", "Zurück zur Planung"), LOCTEXT("BackTip", "Raum zurücksetzen, Plan bleibt erhalten"), [](ADCPlayerController* C) { C->ReturnToPlanning(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning, EDCPlanMode::Playing }, TAttribute<FText>::CreateLambda([WeakThis]()
							{
								const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
								return FText::Format(LOCTEXT("Speed", "Tempo {0}×"), FText::AsNumber(Plans ? FMath::RoundToInt(Plans->GetSpeed()) : 1));
							}), LOCTEXT("SpeedTip", "Abspieltempo 1×, 2×, 4×"), [](ADCPlayerController* C) { C->CycleSpeed(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning }, LOCTEXT("Wait", "Warten +5 s"), LOCTEXT("WaitTip", "Die ausgewählte Figur wartet 5 Sekunden"), [](ADCPlayerController* C) { C->AddWait(5.f); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning }, LOCTEXT("WaitForButton", "Warten auf …"),
								LOCTEXT("WaitForTip", "Die ausgewählte Figur wartet, bis eine andere einen Schritt erledigt oder ein Signal gibt – danach in der Zeitleiste auf diesen Schritt klicken (oder auf den Namen / die Figur: bis sie ganz fertig ist)"),
								[](ADCPlayerController* C) { C->StartPickWait(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning }, LOCTEXT("SignalButton", "Signal geben"),
								LOCTEXT("SignalTip", "Die ausgewählte Figur gibt ein Signal; andere können mit „Warten auf …“ darauf warten"),
								[](ADCPlayerController* C) { C->AddSignal(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning }, LOCTEXT("Clear", "Plan leeren"), LOCTEXT("ClearTip", "Alle Schritte aller Figuren löschen"), [](ADCPlayerController* C) { C->ClearPlan(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							HeaderButton({ EDCPlanMode::Planning }, LOCTEXT("DirectMode", "Direkt steuern"), LOCTEXT("DirectTip", "Ohne Plan ausprobieren (Raum wird später zurückgesetzt)"), [](ADCPlayerController* C) { C->SetDirectMode(true); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
						[
							PanelButton(WeakThis, LOCTEXT("Inventory", "Inventar (I)"), LOCTEXT("InventoryTip", "Inventar der ausgewählten Figur"), [](ADCPlayerController* C) { C->ToggleInventory(); })
						]
					]
					// Left the plan as a list, right the timeline.
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
							// Choosing what to wait for.
							+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 4.f, 0.f, 0.f)
							[
								SNew(SBox)
								.WidthOverride(TrackWidth)
								.Visibility_Lambda([WeakThis]()
								{
									const ADCPlayerController* Controller = GetController(WeakThis.Get());
									return Controller && Controller->IsPickingWait() ? EVisibility::Visible : EVisibility::Collapsed;
								})
								[
									SNew(STextBlock)
									.Font(Font(11, true))
									.ColorAndOpacity(SignalColor)
									.AutoWrapText(true)
									.Text_Lambda([WeakThis]()
									{
										const ADCPlayerController* Controller = GetController(WeakThis.Get());
										const ADCCrewCharacter* Crew = Controller ? Controller->GetSelectedCrew() : nullptr;
										return FText::Format(LOCTEXT("PickPrompt", "Worauf soll {0} warten? Klick auf einen Schritt oder ein Signal einer anderen Figur – auf ihren Namen oder sie selbst im Raum: bis sie ganz fertig ist. Esc = abbrechen"),
											Crew ? Crew->GetCrewName() : FText::GetEmpty());
									})
								]
							]
							// What everyone does at the preview time.
							+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 4.f, 0.f, 0.f)
							[
								SNew(SBox)
								.WidthOverride(TrackWidth)
								[
									SNew(STextBlock)
									.Font(Font(10, true))
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
									.Font(Font(11, true))
									.ColorAndOpacity(TextColor)
									.Text_Lambda([WeakThis]()
									{
										const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
										const float Duration = State ? State->GetSchedule().Duration : 0.f;
										switch (GetPlanMode(WeakThis.Get()))
										{
										case EDCPlanMode::Planning:
											return FText::Format(LOCTEXT("Planning", "Planung · der ganze Plan dauert {0}"), FormatTime(Duration));
										case EDCPlanMode::Playing:
											return State && State->IsPlaybackFinished()
												? FText::Format(LOCTEXT("Finished", "Plan abgespielt ({0})"), FormatTime(State->GetPlaybackTime()))
												: FText::Format(LOCTEXT("Playing", "läuft {0} / geplant {1}"), FormatTime(State ? State->GetPlaybackTime() : 0.f), FormatTime(Duration));
										case EDCPlanMode::Direct:
											return LOCTEXT("Direct", "Direkt steuern (zum Ausprobieren)");
										}
										return FText::GetEmpty();
									})
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox)
									.Visibility_Lambda([PreviewOn]() { return PreviewOn() ? EVisibility::Visible : EVisibility::Collapsed; })
									[
										PanelButton(WeakThis, LOCTEXT("PreviewOff", "Vorschau aus"), LOCTEXT("PreviewOffTip", "Die Stecknadeln im Raum ausblenden (Esc)"),
											[](ADCPlayerController* C) { C->SetPreviewTime(-1.f); }, 10, FMargin(6.f, 1.f))
									]
								]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(Font(10))
						.ColorAndOpacity(MutedColor)
						.Text_Lambda([Mode]()
						{
							switch (Mode())
							{
							case EDCPlanMode::Planning:
								return LOCTEXT("HintPlanning", "Klick auf Boden, Tür oder Gegenstand = Schritt anhängen · Figur anklicken = ihren Plan bearbeiten · Klick in die Zeitleiste = sehen, wo alle dann sind · „Warten auf …“ / „Signal geben“ = aufeinander abstimmen · Leertaste = abspielen");
							case EDCPlanMode::Playing:
								return LOCTEXT("HintPlaying", "grau = wartet auf etwas, das jemand anderes erst erledigt · Leertaste = Pause");
							case EDCPlanMode::Direct:
								return LOCTEXT("HintDirect", "Klicks wirken sofort · „Zurück zur Planung“ setzt den Raum zurück");
							}
							return FText::GetEmpty();
						})
					]
				])
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
	FString Result = FString::Printf(TEXT("%d|%d|%s|%d|%d|"), State ? State->GetScheduleVersion() : 0, static_cast<int32>(GetPlanMode(this)),
		Selected ? *Selected->GetName() : TEXT(""), State ? State->GetFocusStep() : INDEX_NONE, Controller && Controller->IsPickingWait() ? 1 : 0);
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
		ListHeader->AddSlot().AutoHeight()[MakeMuted(LOCTEXT("NoCrew", "Figur anklicken, um ihren Plan zu sehen"))];
		return;
	}
	const EDCPlanMode Mode = GetPlanMode(this);
	const bool bPlanning = Mode == EDCPlanMode::Planning;
	const bool bPlaying = State->IsPlaying();
	const ADCCrewCharacter* Member = State->FindCrew(Focus);
	const UDCPlanRunnerComponent* Runner = Member ? Member->GetPlanRunner() : nullptr;
	TWeakObjectPtr<UDCTimelinePanel> WeakThis = this;

	// Title: whose plan, how many steps, done when.
	ListHeader->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
		[
			SNew(STextBlock).Text(FText::Format(LOCTEXT("PlanOf", "Plan von {0}"), Crew->Name)).Font(Font(13, true)).ColorAndOpacity(Crew->Color)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom).Padding(8.f, 0.f, 0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(Crew->Tasks.IsEmpty() ? LOCTEXT("NoSteps", "noch leer")
				: FText::Format(Crew->Tasks.Num() == 1 ? LOCTEXT("PlanSummaryOne", "1 Schritt · fertig nach {1}") : LOCTEXT("PlanSummary", "{0} Schritte · fertig nach {1}"),
					FText::AsNumber(Crew->Tasks.Num()), FormatTime(Crew->End)))
			.Font(Font(10))
			.ColorAndOpacity(MutedColor)
		]
	];

	// What the hands hold once the plan has run – and putting it down, as a planned step.
	if (const UDCInventoryComponent* Carried = State->GetPlannedInventory(Focus))
	{
		TSharedRef<SHorizontalBox> Hands = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("HandsAfter", "Hände danach:")).Font(Font(10)).ColorAndOpacity(MutedColor)
			];
		const UDCItem* Right = Carried->GetSlotItem(EDCEquipSlot::RightHand);
		const UDCItem* Left = Carried->GetSlotItem(EDCEquipSlot::LeftHand);
		const auto AddHand = [&Hands, bPlanning, WeakThis](const FText& Which, const UDCItem* Item)
		{
			Hands->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("HandItem", "{0} {1}"), Which, Item ? Item->GetDisplayName() : LOCTEXT("HandFree", "frei")))
				.Font(Font(10, Item != nullptr))
				.ColorAndOpacity(Item ? TextColor : MutedColor)
			];
			if (Item && bPlanning)
			{
				const FName ItemId = Item->GetDefId();
				Hands->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					PanelButton(WeakThis, LOCTEXT("DropButton", "ablegen"),
						FText::Format(LOCTEXT("DropTip", "Als nächsten Schritt: {0} auf den Boden legen (macht die Hand frei)"), Item->GetDisplayName()),
						[ItemId](ADCPlayerController* C) { C->PlanDrop(ItemId); }, 9, FMargin(5.f, 0.f))
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
		ListHeader->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Hands];
	}

	if (Crew->Tasks.IsEmpty())
	{
		ListRows->AddSlot().AutoHeight().Padding(6.f, 6.f)
		[
			MakeMuted(bPlanning ? LOCTEXT("EmptyPlan", "Noch keine Schritte. Ein Klick auf Boden, Tür oder Gegenstand hängt den ersten an.")
				: LOCTEXT("EmptyPlanPlaying", "Kein Plan."), 10)
		];
		return;
	}

	for (const FDCScheduledTask& Task : Crew->Tasks)
	{
		const int32 Index = Task.Index;

		// Waiting for someone else: its own line, saying for whom and what.
		const float Waited = Task.WorkStart - Task.Ready;
		if (!Task.bBlocked && Waited > 0.25f && Task.Type != EDCPlanTaskType::WaitFor)
		{
			FText WaitText;
			if (!Task.WaitFor.IsNone())
			{
				const FDCCrewSchedule* Other = Schedule.Find(Task.WaitFor);
				const FDCScheduledTask* Theirs = Other ? Other->Tasks.FindByPredicate([&Task](const FDCScheduledTask& Entry) { return Entry.Index == Task.WaitForIndex; }) : nullptr;
				WaitText = FText::Format(LOCTEXT("WaitsFor", "wartet {0} s – bis {1} mit „{2}“ fertig ist ({1}s Schritt {3})"),
					FormatSeconds(Waited), Task.WaitForName, Theirs ? Theirs->Label : FText::GetEmpty(), FText::AsNumber(Task.WaitForIndex + 1));
			}
			else
			{
				WaitText = FText::Format(LOCTEXT("WaitsPlain", "wartet {0} s – {1}"), FormatSeconds(Waited), Task.WaitReason);
			}
			ListRows->AddSlot().AutoHeight().Padding(30.f, 1.f, 4.f, 1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(34.f)[SNew(STextBlock).Text(FormatTime(Task.Ready)).Font(Font(9)).ColorAndOpacity(MutedColor)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(WaitText).Font(Font(10)).ColorAndOpacity(WaitTextColor).AutoWrapText(true)
				]
			];
		}

		// The step itself.
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

		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Task.Label).Font(Font(11, true)).AutoWrapText(true)
				.ColorAndOpacity(Task.bBlocked ? StepBlockedTextColor : Task.Type == EDCPlanTaskType::Signal ? SignalColor : TextColor)
			];
		if (!Task.bBlocked && !Task.Detail.IsEmpty())
		{
			Lines->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(Task.Detail).Font(Font(9)).ColorAndOpacity(MutedColor).AutoWrapText(true)
			];
		}
		if (Task.bBlocked)
		{
			Lines->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::Format(LOCTEXT("BlockedLine", "geht nicht: {0}"), Task.WaitReason)).Font(Font(10)).ColorAndOpacity(StepBlockedTextColor).AutoWrapText(true)
			];
		}
		if (!Task.Carry.IsEmpty() && !Task.bBlocked)
		{
			Lines->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(Task.Carry).Font(Font(10)).ColorAndOpacity(LootColor).AutoWrapText(true)
			];
		}
		// Who waits for this step.
		for (const FDCCrewSchedule& Other : Schedule.Crews)
		{
			for (const FDCScheduledTask& Theirs : Other.Tasks)
			{
				if (Theirs.WaitFor == Focus && Theirs.WaitForIndex == Index && Other.Crew != Focus)
				{
					Lines->AddSlot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Dependent", "→ {0} wartet darauf ({1} s, dort Schritt {2})"), Other.Name, FormatSeconds(Theirs.WorkStart - Theirs.Ready), FText::AsNumber(Theirs.Index + 1)))
						.Font(Font(10, true))
						.AutoWrapText(true)
						.ColorAndOpacity(Other.Color)
					];
				}
			}
		}

		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(22.f).HeightOverride(18.f)
				[
					SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(Badge).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
					[
						SNew(STextBlock).Text(FText::AsNumber(Index + 1)).Font(Font(10, true)).ColorAndOpacity(DarkText)
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(34.f)[SNew(STextBlock).Text(FormatTime(Task.bBlocked || Task.Type == EDCPlanTaskType::WaitFor ? Task.Ready : Task.WorkStart)).Font(Font(10)).ColorAndOpacity(MutedColor)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				Lines
			];
		if (bPlanning)
		{
			const int32 Count = Crew->Tasks.Num();
			const auto SmallButton = [WeakThis](const FText& Label, const FText& Tip, bool bEnabled, TFunction<void(ADCPlayerController*)> OnClick)
			{
				return SNew(SBox)
					.IsEnabled(bEnabled)
					[
						PanelButton(WeakThis, Label, Tip, MoveTemp(OnClick), 10, FMargin(5.f, 0.f))
					];
			};
			Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.f, 0.f)
				[
					SmallButton(LOCTEXT("Up", "↑"), LOCTEXT("UpTip", "Schritt früher machen"), Index > 0, [Index](ADCPlayerController* C) { C->MoveStep(Index, -1); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.f, 0.f)
				[
					SmallButton(LOCTEXT("Down", "↓"), LOCTEXT("DownTip", "Schritt später machen"), Index < Count - 1, [Index](ADCPlayerController* C) { C->MoveStep(Index, 1); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.f, 0.f)
				[
					SmallButton(LOCTEXT("Delete", "×"), LOCTEXT("DeleteTip", "Schritt löschen"), true, [Index](ADCPlayerController* C) { C->RemoveStep(Index); })
				]
			];
		}

		const float StepTime = Task.WorkStart;
		ListRows->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.Padding(FMargin(4.f, 2.f))
			.BorderBackgroundColor_Lambda([WeakThis, Focus, Index]() -> FSlateColor
			{
				const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
				if (!Heist)
				{
					return FLinearColor::Transparent;
				}
				bool bHighlight = Heist->GetFocusCrew() == Focus && Heist->GetFocusStep() == Index;
				if (!bHighlight && Heist->GetPreviewTime() >= 0.f)
				{
					const FDCCrewSchedule* Own = Heist->GetSchedule().Find(Focus);
					bHighlight = Own && UDCHeistStateSubsystem::GetStepAt(*Own, Heist->GetPreviewTime()) == Index;
				}
				return bHighlight ? HighlightRowColor : FLinearColor::Transparent;
			})
			.OnMouseButtonDown_Lambda([WeakThis, Focus, Index, StepTime](const FGeometry&, const FPointerEvent&)
			{
				if (ADCPlayerController* Controller = GetController(WeakThis.Get()))
				{
					Controller->FocusStep(Focus, Index);
					Controller->SetPreviewTime(StepTime);
				}
				return FReply::Handled();
			})
			[
				Row
			]
		];
	}
}

void UDCTimelinePanel::OnTrackMouse(const FGeometry& Geometry, const FPointerEvent& Event, float Scale, bool bPress)
{
	ADCPlayerController* Controller = GetController(this);
	if (!Controller)
	{
		return;
	}
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	if (Controller->IsPickingWait())
	{
		// Choosing what to wait for: the row's step at that time; on the name or after the end = until done.
		const int32 Row = FMath::FloorToInt((Local.Y - RulerHeight) / RowPitch);
		if (bPress && State && State->GetSchedule().Crews.IsValidIndex(Row))
		{
			const FDCCrewSchedule& Crew = State->GetSchedule().Crews[Row];
			const int32 Step = Local.X < NameWidth ? INDEX_NONE : UDCHeistStateSubsystem::GetStepAt(Crew, static_cast<float>(Local.X - NameWidth) / Scale);
			Controller->PickWaitTarget(Crew.Crew, Step);
		}
		return;
	}
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
	const ADCPlayerController* PickController = GetController(this);
	const bool bPicking = PickController && PickController->IsPickingWait();
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
				SNew(STextBlock).Text(FormatTime(static_cast<float>(Second))).Font(Font(9)).ColorAndOpacity(MutedColor).Visibility(EVisibility::HitTestInvisible)
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
				.Font(Font(bSelectedRow ? 12 : 11, true))
				.ColorAndOpacity(bSelectedRow ? Crew.Color : Dim(Crew.Color, 0.75f))
			]
		];
		const bool bPickable = bPicking && !bSelectedRow;
		Canvas->AddSlot().Position(FVector2D(NameWidth, Top)).Size(FVector2D(TrackWidth, RowHeight))
		[
			SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(bPickable ? PickTrackColor : bSelectedRow ? SelectedTrackColor : ZoneColor).Visibility(EVisibility::HitTestInvisible)
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
							if (Controller->IsPickingWait())
							{
								Controller->PickWaitTarget(CrewName, TaskIndex);
							}
							else
							{
								Controller->FocusStep(CrewName, TaskIndex);
								Controller->SetPreviewTime(StepTime);
							}
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
							SNew(STextBlock).Text(Width >= 12.f ? Text : FText::GetEmpty()).Font(Font(9, true)).ColorAndOpacity(DarkText)
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
			const FLinearColor Work = bSignal ? FLinearColor(SignalColor.R, SignalColor.G, SignalColor.B, Fade)
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
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(FLinearColor::White).Visibility(EVisibility::HitTestInvisible)
			];
			const float DotY = RowTop(Source) + RowHeight * 0.5f - 5.f;
			Canvas->AddSlot().Position(FVector2D(X - 4.0, DotY)).Size(FVector2D(10.0, 10.0))
			[
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(FLinearColor::White).Padding(2.f).Visibility(EVisibility::HitTestInvisible)
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
