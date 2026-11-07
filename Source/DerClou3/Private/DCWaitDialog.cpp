#include "DCWaitDialog.h"

#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCItemWidgets.h"
#include "DCPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCWaitDialog"

namespace
{
	constexpr float DialogWidth = 560.f;
	const FLinearColor DimBackground(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor OptionColor(0.11f, 0.115f, 0.145f);
	const FLinearColor ChosenOptionColor(0.2f, 0.32f, 0.22f);

	FText ClockText(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
	}

	/** Steps one can wait for (not the waits and signals themselves). */
	bool IsWaitableStep(const FDCPlanTask& Task)
	{
		return Task.Type != EDCPlanTaskType::WaitFor && Task.Type != EDCPlanTaskType::Signal;
	}
}

void UDCWaitDialog::Open(ADCCrewCharacter* InCrew)
{
	Crew = InCrew;
	Choice.Reset();
	bOpen = InCrew != nullptr;
	bDirty = true;
}

void UDCWaitDialog::Close()
{
	bOpen = false;
	Choice.Reset();
}

void UDCWaitDialog::Choose(const FDCWaitChoice& InChoice)
{
	Choice = InChoice;
}

bool UDCWaitDialog::IsChosen(const FDCWaitChoice& Option) const
{
	return Choice.IsSet() && Choice.GetValue() == Option;
}

bool UDCWaitDialog::Confirm()
{
	ADCPlayerController* Controller = Cast<ADCPlayerController>(GetOwningPlayer());
	if (!bOpen || !Choice.IsSet() || !Controller)
	{
		return false;
	}
	Controller->ApplyWaitChoice(Choice.GetValue());
	Close();
	return true;
}

FText UDCWaitDialog::DescribeChoice() const
{
	const ADCCrewCharacter* Member = Crew.Get();
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	if (!Member || !Heist)
	{
		return FText::GetEmpty();
	}
	if (!Choice.IsSet())
	{
		return LOCTEXT("ChooseFirst", "Oben eine Möglichkeit wählen.");
	}
	const FDCWaitChoice& Picked = Choice.GetValue();
	const FText Name = Member->GetCrewName();
	if (Picked.Kind == FDCWaitChoice::EKind::Seconds)
	{
		return FText::Format(LOCTEXT("SayWaitSeconds", "{0} wartet {1} Sekunden und macht dann mit dem nächsten Schritt weiter."), Name, FText::AsNumber(FMath::RoundToInt(Picked.Seconds)));
	}

	// When will the other one be that far, and when is this one there?
	const FDCSchedule& Schedule = Heist->GetSchedule();
	const FDCCrewSchedule* Own = Schedule.Find(Member->GetFName());
	const FDCCrewSchedule* Other = Schedule.Find(Picked.Crew);
	const ADCCrewCharacter* OtherMember = Heist->FindCrew(Picked.Crew);
	const FText OtherName = OtherMember ? OtherMember->GetCrewName() : FText::FromName(Picked.Crew);
	const float Arrives = Own ? Own->End : 0.f;
	float Ready = Other ? Other->End : 0.f;
	FText What;
	if (Picked.Kind == FDCWaitChoice::EKind::AfterStep)
	{
		const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		const TArray<FDCPlanTask>& Theirs = Plans->GetTasks(Picked.Crew);
		const FDCScheduledTask* Step = Other ? Other->Tasks.FindByPredicate([&Picked](const FDCScheduledTask& Task) { return Task.Index == Picked.Step; }) : nullptr;
		Ready = Step ? Step->End : Ready;
		What = FText::Format(LOCTEXT("SayAfterStep", "{0} gibt {1} ein Zeichen, sobald „{2}“ erledigt ist (ca. {3}). {1} wartet so lange."),
			OtherName, Name, Theirs.IsValidIndex(Picked.Step) ? Heist->DescribeTask(Theirs[Picked.Step]) : FText::GetEmpty(), ClockText(Ready));
	}
	else
	{
		What = FText::Format(LOCTEXT("SayAllDone", "{0} wartet, bis {1} mit allem fertig ist (ca. {2})."), Name, OtherName, ClockText(Ready));
	}
	const FText When = Ready > Arrives + 0.5f
		? FText::Format(LOCTEXT("SayWaits", "{0} ist um {1} so weit und wartet dann ca. {2} s."), Name, ClockText(Arrives), FText::AsNumber(FMath::RoundToInt(Ready - Arrives)))
		: FText::Format(LOCTEXT("SayNoWait", "{0} ist erst um {1} so weit – dann ist das schon erledigt, es geht ohne Warten weiter."), Name, ClockText(Arrives));
	return FText::Format(LOCTEXT("SayBoth", "{0}\n{1}"), What, When);
}

TSharedRef<SWidget> UDCWaitDialog::RebuildWidget()
{
	Super::RebuildWidget();
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCWaitDialog> WeakThis = this;

	Root = SNew(SOverlay)
		.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->IsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
		// The game behind it stays visible but takes no clicks while the question is open.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.BorderBackgroundColor(DimBackground)
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(DialogWidth)
			[
				SNew(SBorder)
				.BorderImage(Brush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(18.f, 14.f))
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(Font(16, true))
						.ColorAndOpacity(TitleColor)
						.Text_Lambda([WeakThis]()
						{
							const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
							return FText::Format(LOCTEXT("Title", "Worauf soll {0} warten?"), Member ? Member->GetCrewName() : FText::GetEmpty());
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 10.f)
					[
						SNew(STextBlock)
						.Font(Font(12))
						.ColorAndOpacity(MutedColor)
						.AutoWrapText(true)
						.Text_Lambda([WeakThis]()
						{
							const ADCCrewCharacter* Member = WeakThis.IsValid() ? WeakThis->Crew.Get() : nullptr;
							const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
							if (!Member || !Plans)
							{
								return FText::GetEmpty();
							}
							const int32 Count = Plans->GetTasks(Member->GetFName()).Num();
							return Count == 0
								? FText::Format(LOCTEXT("WhereStart", "Das wird der erste Schritt: {0} wartet gleich am Anfang, dort wo die Figur steht."), Member->GetCrewName())
								: FText::Format(LOCTEXT("WhereEnd", "Das wird Schritt {1}: {0} wartet dort, wo der bisherige Plan endet, und macht danach weiter."), Member->GetCrewName(), FText::AsNumber(Count + 1));
						})
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.MaxDesiredHeight(330.f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(Options, SVerticalBox)
							]
						]
					]
					// What will happen, in one or two sentences.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(Brush())
						.BorderBackgroundColor(ZoneColor)
						.Padding(FMargin(10.f, 8.f))
						[
							SNew(STextBlock)
							.Font(Font(12))
							.ColorAndOpacity(TextColor)
							.AutoWrapText(true)
							.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->DescribeChoice() : FText::GetEmpty(); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 12.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SButton)
							.IsFocusable(false)
							.ContentPadding(FMargin(14.f, 4.f))
							.ToolTipText(LOCTEXT("CancelTip", "Nichts ändern (Esc)"))
							.OnClicked_Lambda([WeakThis]()
							{
								if (WeakThis.IsValid())
								{
									WeakThis->Close();
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(LOCTEXT("Cancel", "Abbrechen")).Font(Font(13, true))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.IsFocusable(false)
							.ContentPadding(FMargin(18.f, 4.f))
							.ButtonStyle(&PrimaryButtonStyle())
							.ToolTipText(LOCTEXT("ConfirmTip", "In den Plan übernehmen (Eingabetaste)"))
							.IsEnabled_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->HasChoice(); })
							.OnClicked_Lambda([WeakThis]()
							{
								if (WeakThis.IsValid())
								{
									WeakThis->Confirm();
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(LOCTEXT("Confirm", "Übernehmen")).Font(Font(13, true)).ColorAndOpacity(DarkTextColor)
							]
						]
					]
				]
			]
		];
	bDirty = true;
	return Root.ToSharedRef();
}

void UDCWaitDialog::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	Options.Reset();
}

void UDCWaitDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bDirty && Options.IsValid())
	{
		bDirty = false;
		RebuildOptions();
	}
}

void UDCWaitDialog::RebuildOptions()
{
	using namespace DCItemWidgets;
	Options->ClearChildren();
	const ADCCrewCharacter* Member = Crew.Get();
	const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
	const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
	if (!Member || !Heist || !Plans)
	{
		return;
	}
	TWeakObjectPtr<UDCWaitDialog> WeakThis = this;

	// One selectable line: highlighted while chosen.
	const auto AddOption = [WeakThis](const TSharedRef<SVerticalBox>& Into, const FDCWaitChoice& Option, const FText& Text, const FText& Right, const FLinearColor& Accent)
	{
		Into->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SBorder)
			.BorderImage(Brush())
			.Padding(FMargin(0.f))
			.Cursor(EMouseCursor::Hand)
			.BorderBackgroundColor_Lambda([WeakThis, Option]() -> FSlateColor
			{
				return WeakThis.IsValid() && WeakThis->IsChosen(Option) ? ChosenOptionColor : OptionColor;
			})
			.OnMouseButtonDown_Lambda([WeakThis, Option](const FGeometry&, const FPointerEvent&)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->Choose(Option);
				}
				return FReply::Handled();
			})
			.OnMouseDoubleClick_Lambda([WeakThis, Option](const FGeometry&, const FPointerEvent&)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->Choose(Option);
					WeakThis->Confirm();
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				// A bar on the left: the colour of whoever it is about; bright while chosen.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(5.f)
					[
						SNew(SBorder)
						.BorderImage(Brush())
						.BorderBackgroundColor_Lambda([WeakThis, Option, Accent]() -> FSlateColor
						{
							return WeakThis.IsValid() && WeakThis->IsChosen(Option) ? FLinearColor::White : Accent;
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 6.f)
				[
					SNew(STextBlock).Text(Text).Font(Font(12, true)).ColorAndOpacity(TextColor).AutoWrapText(true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock).Text(Right).Font(Font(11)).ColorAndOpacity(MutedColor)
				]
			]
		];
	};
	const auto AddHeading = [this](const FText& Text, const FLinearColor& Color)
	{
		Options->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 3.f)
		[
			SNew(STextBlock).Text(Text).Font(Font(14, true)).ColorAndOpacity(Color)
		];
	};

	// A fixed time.
	AddHeading(LOCTEXT("FixedTime", "Eine feste Zeit"), TextColor);
	TSharedRef<SHorizontalBox> Times = SNew(SHorizontalBox);
	for (const float Seconds : { 5.f, 10.f, 20.f, 30.f })
	{
		FDCWaitChoice Option;
		Option.Kind = FDCWaitChoice::EKind::Seconds;
		Option.Seconds = Seconds;
		TSharedRef<SVerticalBox> Cell = SNew(SVerticalBox);
		AddOption(Cell, Option, FText::Format(LOCTEXT("SecondsOption", "{0} s"), FText::AsNumber(FMath::RoundToInt(Seconds))), FText::GetEmpty(), MutedColor);
		Times->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)[Cell];
	}
	Options->AddSlot().AutoHeight()[Times];

	// The others: one of their steps (they give a signal then), or until they are done.
	const FDCSchedule& Schedule = Heist->GetSchedule();
	for (const FDCCrewSchedule& Other : Schedule.Crews)
	{
		if (Other.Crew == Member->GetFName())
		{
			continue;
		}
		AddHeading(FText::Format(LOCTEXT("OnOther", "Auf {0} warten"), Other.Name), Other.Color);
		const TArray<FDCPlanTask>& Theirs = Plans->GetTasks(Other.Crew);
		TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
		bool bAnyStep = false;
		for (int32 Index = 0; Index < Theirs.Num(); ++Index)
		{
			if (!IsWaitableStep(Theirs[Index]))
			{
				continue;
			}
			const FDCScheduledTask* Step = Other.Tasks.FindByPredicate([Index](const FDCScheduledTask& Task) { return Task.Index == Index; });
			FDCWaitChoice Option;
			Option.Kind = FDCWaitChoice::EKind::AfterStep;
			Option.Crew = Other.Crew;
			Option.Step = Index;
			const bool bBlocked = Step && Step->bBlocked;
			AddOption(List, Option,
				FText::Format(LOCTEXT("AfterStepOption", "bis {0} Schritt {1} erledigt hat: {2}"), Other.Name, FText::AsNumber(Index + 1), Heist->DescribeTask(Theirs[Index])),
				bBlocked ? LOCTEXT("NeverOption", "geht nicht") : FText::Format(LOCTEXT("AtTime", "ca. {0}"), ClockText(Step ? Step->End : 0.f)), Other.Color);
			bAnyStep = true;
		}
		FDCWaitChoice Done;
		Done.Kind = FDCWaitChoice::EKind::AllDone;
		Done.Crew = Other.Crew;
		AddOption(List, Done, FText::Format(LOCTEXT("AllDoneOption", "bis {0} mit allem fertig ist"), Other.Name), FText::Format(LOCTEXT("AtTime", "ca. {0}"), ClockText(Other.End)), Other.Color);
		if (!bAnyStep)
		{
			List->AddSlot().AutoHeight().Padding(10.f, 2.f)
			[
				MakeMuted(FText::Format(LOCTEXT("NoPlanYet", "{0} hat noch keine Schritte – erst den Plan von {0} anlegen, dann kann hier auf einen Schritt gewartet werden."), Other.Name), 10)
			];
		}
		Options->AddSlot().AutoHeight()[List];
	}
}

#undef LOCTEXT_NAMESPACE
