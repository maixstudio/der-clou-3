#include "DCTimelinePanel.h"

#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCTimelinePanel"

namespace
{
	constexpr float NameWidth = 110.f;
	constexpr float TrackWidth = 1000.f;
	constexpr float RowHeight = 24.f;
	constexpr float MinVisibleSeconds = 30.f;
	const FLinearColor WaitColor(0.32f, 0.33f, 0.36f);
	const FLinearColor BlockedColor(0.75f, 0.1f, 0.06f);

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
}

TSharedRef<SWidget> UDCTimelinePanel::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCTimelinePanel> WeakThis = this;

	const auto Mode = [WeakThis]()
	{
		const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
		return Plans ? Plans->GetMode() : EDCPlanMode::Planning;
	};
	const auto ShowIn = [Mode](TArray<EDCPlanMode> Modes)
	{
		return TAttribute<EVisibility>::CreateLambda([Mode, Modes]() { return Modes.Contains(Mode()) ? EVisibility::Visible : EVisibility::Collapsed; });
	};
	// A small button that calls the controller.
	const auto Button = [WeakThis](const TAttribute<FText>& Label, const FText& Tip, TFunction<void(ADCPlayerController*)> OnClick)
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(Tip)
			.ContentPadding(FMargin(8.f, 2.f))
			.OnClicked_Lambda([WeakThis, OnClick]()
			{
				if (ADCPlayerController* Controller = GetController(WeakThis.Get()))
				{
					OnClick(Controller);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(Font(11, true))
			];
	};

	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(12.f)
		[
			SNew(SBox)
			.WidthOverride(NameWidth + TrackWidth + 40.f)
			[
				MakeSwallowZone(
					SNew(SBorder)
					.BorderImage(Brush())
					.BorderBackgroundColor(PanelColor)
					.Padding(FMargin(12.f, 8.f))
					.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
							[
								SNew(STextBlock).Text(LOCTEXT("Title", "Zeitleiste")).Font(Font(14, true)).ColorAndOpacity(TitleColor)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[
								SNew(SSpacer)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
								[
									Button(LOCTEXT("Play", "Abspielen"), LOCTEXT("PlayTip", "Plan abspielen (Leertaste)"), [](ADCPlayerController* C) { C->StartPlayback(); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Playing }))
								[
									Button(TAttribute<FText>::CreateLambda([WeakThis]()
									{
										const ADCPlayerController* Controller = GetController(WeakThis.Get());
										return Controller && Controller->IsPlaybackPaused() ? LOCTEXT("Resume", "Weiter") : LOCTEXT("Pause", "Pause");
									}), LOCTEXT("PauseTip", "Anhalten / weiter (Leertaste)"), [](ADCPlayerController* C) { C->TogglePause(); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Playing, EDCPlanMode::Direct }))
								[
									Button(LOCTEXT("Back", "Zurück zur Planung"), LOCTEXT("BackTip", "Raum zurücksetzen, Plan bleibt erhalten"), [](ADCPlayerController* C) { C->ReturnToPlanning(); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning, EDCPlanMode::Playing }))
								[
									Button(TAttribute<FText>::CreateLambda([WeakThis]()
									{
										const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
										return FText::Format(LOCTEXT("Speed", "Tempo {0}×"), FText::AsNumber(Plans ? FMath::RoundToInt(Plans->GetSpeed()) : 1));
									}), LOCTEXT("SpeedTip", "Abspieltempo 1×, 2×, 4×"), [](ADCPlayerController* C) { C->CycleSpeed(); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
								[
									Button(LOCTEXT("Wait", "Warten +5 s"), LOCTEXT("WaitTip", "Die ausgewählte Figur wartet 5 Sekunden"), [](ADCPlayerController* C) { C->AddWait(5.f); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
								[
									Button(LOCTEXT("Undo", "Schritt zurück"), LOCTEXT("UndoTip", "Letzten Schritt der ausgewählten Figur löschen (Rücktaste)"), [](ADCPlayerController* C) { C->RemoveLastStep(); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
								[
									Button(LOCTEXT("Clear", "Plan leeren"), LOCTEXT("ClearTip", "Alle Schritte aller Figuren löschen"), [](ADCPlayerController* C) { C->ClearPlan(); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								SNew(SBox).Visibility(ShowIn({ EDCPlanMode::Planning }))
								[
									Button(LOCTEXT("DirectMode", "Direkt steuern"), LOCTEXT("DirectTip", "Ohne Plan ausprobieren (Raum wird später zurückgesetzt)"), [](ADCPlayerController* C) { C->SetDirectMode(true); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
							[
								Button(LOCTEXT("Inventory", "Inventar (I)"), LOCTEXT("InventoryTip", "Inventar der ausgewählten Figur"), [](ADCPlayerController* C) { C->ToggleInventory(); })
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(Rows, SVerticalBox)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 4.f, 0.f, 0.f)
						[
							SNew(STextBlock)
								.Font(Font(11, true))
								.ColorAndOpacity(TextColor)
								.Text_Lambda([WeakThis, Mode]()
								{
									const UDCHeistStateSubsystem* State = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
									const ADCPlayerController* Controller = GetController(WeakThis.Get());
									const ADCCrewCharacter* Crew = Controller ? Controller->GetSelectedCrew() : nullptr;
									const float Duration = State ? State->GetSchedule().Duration : 0.f;
									switch (Mode())
									{
									case EDCPlanMode::Planning:
										return FText::Format(LOCTEXT("Planning", "Planung für {0} · dauert {1}"),
											Crew ? Crew->GetCrewName() : FText::GetEmpty(), FormatTime(Duration));
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
						+ SVerticalBox::Slot().AutoHeight().Padding(NameWidth, 4.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Font(Font(10))
							.ColorAndOpacity(MutedColor)
							.Text_Lambda([Mode]()
							{
								switch (Mode())
								{
								case EDCPlanMode::Planning:
									return LOCTEXT("HintPlanning", "Figur anklicken = auswählen · Klick auf Boden, Tür oder Gegenstand = Schritt anhängen · Leertaste = abspielen · Rücktaste = Schritt zurück");
								case EDCPlanMode::Playing:
									return LOCTEXT("HintPlaying", "grau = wartet auf etwas, das jemand anderes erst erledigt · Leertaste = Pause");
								case EDCPlanMode::Direct:
									return LOCTEXT("HintDirect", "Klicks wirken sofort · „Zurück zur Planung“ setzt den Raum zurück");
								}
								return FText::GetEmpty();
							})
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
	Rows.Reset();
}

void UDCTimelinePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const FString Now = MakeSignature();
	if (Rows.IsValid() && Now != Signature)
	{
		Signature = Now;
		RebuildRows();
	}
}

FString UDCTimelinePanel::MakeSignature() const
{
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	const ADCPlayerController* Controller = GetController(this);
	FString Result = FString::Printf(TEXT("%d|%d|%s|"), State ? State->GetScheduleVersion() : 0, Plans ? static_cast<int32>(Plans->GetMode()) : 0,
		Controller && Controller->GetSelectedCrew() ? *Controller->GetSelectedCrew()->GetName() : TEXT(""));
	if (State && State->IsPlaying())
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
	using namespace DCItemWidgets;
	Rows->ClearChildren();
	const UDCHeistStateSubsystem* State = UDCHeistStateSubsystem::Get(this);
	if (!State)
	{
		return;
	}
	const float Scale = GetScale();
	const float VisibleSeconds = TrackWidth / Scale;
	const ADCPlayerController* Controller = GetController(this);
	const ADCCrewCharacter* SelectedCrew = Controller ? Controller->GetSelectedCrew() : nullptr;
	TWeakObjectPtr<UDCTimelinePanel> WeakThis = this;

	// The playback cursor: a thin line at the current time, in every row.
	const auto AddCursor = [WeakThis, Scale](const TSharedRef<SCanvas>& Canvas, float Height)
	{
		Canvas->AddSlot()
			.Position(TAttribute<FVector2D>::CreateLambda([WeakThis, Scale]()
			{
				const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
				return FVector2D(FMath::Min(TrackWidth - 2.f, (Heist ? Heist->GetPlaybackTime() : 0.f) * Scale), 0.0);
			}))
			.Size(FVector2D(2.0, Height))
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(FLinearColor::White)
				.Visibility_Lambda([WeakThis]()
				{
					const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
					return Heist && Heist->IsPlaying() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
			];
	};

	// Ruler: a mark every 5 s, a number every 10 s.
	{
		TSharedRef<SCanvas> Ruler = SNew(SCanvas);
		for (int32 Second = 0; Second <= FMath::FloorToInt(VisibleSeconds); Second += 5)
		{
			const bool bMajor = Second % 10 == 0;
			Ruler->AddSlot().Position(FVector2D(Second * Scale, bMajor ? 0.0 : 8.0)).Size(FVector2D(1.0, bMajor ? 16.0 : 8.0))
			[
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(MutedColor)
			];
			if (bMajor)
			{
				Ruler->AddSlot().Position(FVector2D(Second * Scale + 3.0, 0.0)).Size(FVector2D(40.0, 14.0))
				[
					SNew(STextBlock).Text(FormatTime(static_cast<float>(Second))).Font(Font(9)).ColorAndOpacity(MutedColor)
				];
			}
		}
		Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(NameWidth)]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(TrackWidth).HeightOverride(16.f)[Ruler]]
		];
	}

	for (const FDCCrewSchedule& Crew : State->GetSchedule().Crews)
	{
		const ADCCrewCharacter* Member = State->FindCrew(Crew.Crew);
		const UDCPlanRunnerComponent* Runner = Member ? Member->GetPlanRunner() : nullptr;
		const bool bPlaying = State->IsPlaying();
		TSharedRef<SCanvas> Track = SNew(SCanvas);

		for (const FDCScheduledTask& Task : Crew.Tasks)
		{
			const EDCTaskStatus Status = bPlaying && Runner ? Runner->GetStatus(Task.Index) : EDCTaskStatus::Planned;
			const bool bCurrent = bPlaying && Runner && Runner->IsRunning() && Runner->GetCurrentIndex() == Task.Index;
			const float Fade = Status == EDCTaskStatus::Done ? 0.55f : 1.f;

			TArray<FString> TipLines;
			TipLines.Add(FString::Printf(TEXT("%d. %s"), Task.Index + 1, *Task.Label.ToString()));
			if (!Task.Detail.IsEmpty())
			{
				TipLines.Add(Task.Detail.ToString());
			}
			if (!Task.WaitReason.IsEmpty())
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

			const auto AddBlock = [&Track, Scale, &Tip](float From, float To, const FLinearColor& Color, const FText& Text, bool bOutline)
			{
				const float Width = FMath::Max(0.f, (To - From) * Scale);
				if (Width < 1.5f)
				{
					return;
				}
				Track->AddSlot().Position(FVector2D(From * Scale, 2.0)).Size(FVector2D(Width - 1.0, RowHeight - 4.0))
				[
					SNew(SBorder)
					.BorderImage(Brush())
					.BorderBackgroundColor(bOutline ? FLinearColor::White : Color)
					.Padding(bOutline ? 1.f : 0.f)
					.ToolTipText(Tip)
					[
						SNew(SBorder)
						.BorderImage(Brush())
						.BorderBackgroundColor(Color)
						.Padding(FMargin(3.f, 0.f))
						.VAlign(VAlign_Center)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(STextBlock).Text(Text).Font(Font(9, true)).ColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.06f))
						]
					]
				];
			};

			if (Task.bBlocked)
			{
				// Will not happen: a short red block where it would start.
				const float From = Task.Ready;
				AddBlock(From, From + 64.f / Scale, BlockedColor, FText::Format(LOCTEXT("BlockedLabel", "{0}: geht nicht"), Task.Label), false);
				continue;
			}
			const FLinearColor Base = Crew.Color;
			AddBlock(Task.Start, Task.Ready, FLinearColor(Base.R * 0.45f, Base.G * 0.45f, Base.B * 0.45f, Fade), FText::GetEmpty(), false);
			AddBlock(Task.Ready, Task.WorkStart, FLinearColor(WaitColor.R, WaitColor.G, WaitColor.B, Fade), LOCTEXT("WaitLabel", "wartet"), false);
			const FLinearColor Work = Task.bLoud ? FLinearColor(1.f, 0.35f, 0.15f, Fade) : FLinearColor(Base.R, Base.G, Base.B, Fade);
			AddBlock(Task.WorkStart, FMath::Max(Task.End, Task.WorkStart + 0.4f), Status == EDCTaskStatus::Failed ? BlockedColor : Work, Task.Label, bCurrent);
		}
		AddCursor(Track, RowHeight);

		const bool bSelectedRow = SelectedCrew && SelectedCrew->GetFName() == Crew.Crew;
		Rows->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(NameWidth)
				[
					SNew(STextBlock)
					.Text(bSelectedRow ? FText::Format(LOCTEXT("SelectedName", "> {0}"), Crew.Name) : Crew.Name)
					.Font(Font(12, true))
					.ColorAndOpacity(Crew.Color)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(TrackWidth).HeightOverride(RowHeight)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(ZoneColor)]
					+ SOverlay::Slot()[Track]
				]
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
