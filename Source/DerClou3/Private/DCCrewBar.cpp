#include "DCCrewBar.h"

#include "DCCrewCharacter.h"
#include "DCHeistState.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCCrewBar"

namespace
{
	constexpr float CardWidth = 252.f;
	constexpr float PortraitSize = 46.f;
	const FLinearColor CardColor(0.05f, 0.055f, 0.075f, 0.92f);
	const FLinearColor SelectedCardColor(0.12f, 0.13f, 0.17f, 0.96f);
	const FLinearColor CardProblemColor(1.f, 0.42f, 0.35f);

	ADCPlayerController* BarController(const UUserWidget* Widget)
	{
		return Widget ? Cast<ADCPlayerController>(Widget->GetOwningPlayer()) : nullptr;
	}

	/** "Schlösser 6 · Kraft 6": the two best skills. */
	FText BestSkills(const FDCSkills& Skills)
	{
		TArray<EDCSkill> Order;
		for (int32 Index = 0; Index < static_cast<int32>(EDCSkill::Count); ++Index)
		{
			Order.Add(static_cast<EDCSkill>(Index));
		}
		Order.StableSort([&Skills](EDCSkill A, EDCSkill B) { return Skills.Get(A) > Skills.Get(B); });
		return FText::Format(LOCTEXT("BestSkills", "{0} {1} · {2} {3}"), DCSkills::Name(Order[0]), FText::AsNumber(Skills.Get(Order[0])),
			DCSkills::Name(Order[1]), FText::AsNumber(Skills.Get(Order[1])));
	}
}

TSharedRef<SWidget> UDCCrewBar::RebuildWidget()
{
	Super::RebuildWidget();
	Root = SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(12.f, 12.f))
		[
			SAssignNew(Cards, SVerticalBox)
		];
	CardCount = -1;
	return Root.ToSharedRef();
}

void UDCCrewBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	Cards.Reset();
}

void UDCCrewBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	int32 Count = 0;
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		++Count;
	}
	if (Cards.IsValid() && Count != CardCount)
	{
		CardCount = Count;
		RebuildCards();
	}
}

void UDCCrewBar::RebuildCards()
{
	Cards->ClearChildren();
	int32 Number = 0;
	for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
	{
		Cards->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeCard(*It, ++Number)];
	}
}

TSharedRef<SWidget> UDCCrewBar::MakeCard(ADCCrewCharacter* Member, int32 Number)
{
	using namespace DCItemWidgets;
	TWeakObjectPtr<UDCCrewBar> WeakThis = this;
	TWeakObjectPtr<ADCCrewCharacter> WeakMember = Member;
	const FLinearColor Color = Member->GetBodyColor();
	const FString Name = Member->GetCrewName().ToString();
	const FText Initial = FText::FromString(Name.Left(1).ToUpper());

	const auto IsSelected = [WeakThis, WeakMember]()
	{
		const ADCPlayerController* Controller = BarController(WeakThis.Get());
		return Controller && WeakMember.IsValid() && Controller->GetSelectedCrew() == WeakMember.Get();
	};
	// What the crew member plans (planning) or does (playing, direct control) – and whether something is wrong.
	const auto Status = [WeakThis, WeakMember](bool& bProblem) -> FText
	{
		bProblem = false;
		const ADCCrewCharacter* Crew = WeakMember.Get();
		const UDCPlanSubsystem* Plans = WeakThis.IsValid() ? UDCPlanSubsystem::Get(WeakThis.Get()) : nullptr;
		const UDCHeistStateSubsystem* Heist = WeakThis.IsValid() ? UDCHeistStateSubsystem::Get(WeakThis.Get()) : nullptr;
		if (!Crew || !Plans || !Heist)
		{
			return FText::GetEmpty();
		}
		if (Plans->GetMode() == EDCPlanMode::Planning)
		{
			const FDCCrewSchedule* Own = Heist->GetSchedule().Find(Crew->GetFName());
			if (!Own || Own->Tasks.IsEmpty())
			{
				return LOCTEXT("NoPlan", "noch kein Plan");
			}
			if (const FDCScheduledTask* Blocked = Own->Tasks.FindByPredicate([](const FDCScheduledTask& Task) { return Task.bBlocked; }))
			{
				bProblem = true;
				return FText::Format(LOCTEXT("StepBlocked", "Schritt {0} geht nicht"), FText::AsNumber(Blocked->Index + 1));
			}
			const int32 Whole = FMath::RoundToInt(Own->End);
			const FText Until = FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
			return Own->Tasks.Num() == 1 ? FText::Format(LOCTEXT("OneStep", "1 Schritt · bis {0}"), Until)
				: FText::Format(LOCTEXT("Steps", "{0} Schritte · bis {1}"), FText::AsNumber(Own->Tasks.Num()), Until);
		}
		const FText Activity = Crew->GetActivityText();
		if (!Activity.IsEmpty())
		{
			return Activity;
		}
		return Plans->GetMode() == EDCPlanMode::Playing && Crew->GetPlanRunner()->IsFinished() ? LOCTEXT("Done", "fertig") : LOCTEXT("Ready", "bereit");
	};

	return SNew(SBox)
		.WidthOverride(CardWidth)
		[
			MakeSwallowZone(
				SNew(SBorder)
				.BorderImage(Brush())
				.Padding(FMargin(2.f))
				.Cursor(EMouseCursor::Hand)
				.ToolTipText(FText::Format(LOCTEXT("CardTip", "{0} auswählen und die Kamera hinbewegen (Taste {1})"), Member->GetCrewName(), FText::AsNumber(Number)))
				.BorderBackgroundColor_Lambda([IsSelected, Color]() -> FSlateColor { return IsSelected() ? Color : FLinearColor(0.f, 0.f, 0.f, 0.f); })
				.OnMouseButtonDown_Lambda([WeakThis, WeakMember](const FGeometry&, const FPointerEvent&)
				{
					ADCPlayerController* Controller = BarController(WeakThis.Get());
					if (Controller && WeakMember.IsValid())
					{
						Controller->SelectFromBar(WeakMember.Get());
					}
					return FReply::Handled();
				})
				[
					// A coloured frame (2 px) while selected.
					SNew(SBorder)
					.BorderImage(Brush())
					.Padding(FMargin(8.f, 7.f))
					.BorderBackgroundColor_Lambda([IsSelected]() -> FSlateColor { return IsSelected() ? SelectedCardColor : CardColor; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 9.f, 0.f)
						[
							SNew(SBox).WidthOverride(PortraitSize).HeightOverride(PortraitSize)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(Color).HAlign(HAlign_Center).VAlign(VAlign_Center)
									[
										SNew(STextBlock).Text(Initial).Font(Font(22, true)).ColorAndOpacity(DarkTextColor)
									]
								]
								// The number key that selects this crew member.
								+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 3.f, 1.f)
								[
									SNew(STextBlock).Text(FText::AsNumber(Number)).Font(Font(10, true)).ColorAndOpacity(DarkTextColor)
								]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(Member->GetCrewName()).Font(Font(14, true))
									.ColorAndOpacity_Lambda([IsSelected, Color]() -> FSlateColor { return IsSelected() ? FLinearColor::White : Color; })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SButton)
									.IsFocusable(false)
									.ContentPadding(FMargin(5.f, 0.f))
									.ToolTipText(LOCTEXT("ProfileTip", "Profil: Fähigkeiten und Inventar (I)"))
									.OnClicked_Lambda([WeakThis, WeakMember]()
									{
										ADCPlayerController* Controller = BarController(WeakThis.Get());
										if (Controller && WeakMember.IsValid())
										{
											Controller->OpenProfile(WeakMember.Get());
										}
										return FReply::Handled();
									})
									[
										SNew(STextBlock).Text(LOCTEXT("Profile", "Profil")).Font(Font(11, true))
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
							[
								SNew(STextBlock).Text(BestSkills(Member->GetSkills())).Font(Font(11)).ColorAndOpacity(MutedColor)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Font(Font(12))
								.AutoWrapText(true)
								.Text_Lambda([Status]() { bool bProblem = false; return Status(bProblem); })
								.ColorAndOpacity_Lambda([Status]() -> FSlateColor { bool bProblem = false; Status(bProblem); return bProblem ? CardProblemColor : TextColor; })
							]
						]
					]
				])
		];
}

#undef LOCTEXT_NAMESPACE
