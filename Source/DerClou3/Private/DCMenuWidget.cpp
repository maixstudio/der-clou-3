#include "DCMenuWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "DCItemWidgets.h"
#include "DerClou3.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCMenuWidget"

namespace
{
	using namespace DCItemWidgets;

	constexpr float MenuColumnWidth = 440.f;
	constexpr float MenuPageWidth = 700.f;
	/** Labels in the options are this wide, so the choices line up. */
	constexpr float MenuLabelWidth = 150.f;
	constexpr double MenuMessageSeconds = 5.0;
	const FLinearColor MenuColumnColor(0.02f, 0.024f, 0.035f, 0.88f);
	/** Over a heist the column hides the crew cards behind it. */
	const FLinearColor MenuColumnGameColor(0.02f, 0.024f, 0.035f, 0.97f);
	const FLinearColor MenuDimColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor MenuDisabledText(0.42f, 0.44f, 0.48f);
	const FLinearColor MenuRowSelectedColor(0.05f, 0.045f, 0.028f);

	/** A narrow bar in the focus colour on the left of the chosen entry (transparent otherwise). */
	TSharedRef<SWidget> MenuAccent(bool bOn)
	{
		return SNew(SBox)
			.WidthOverride(4.f)
			[
				SNew(SBorder).BorderImage(RoundBrush()).BorderBackgroundColor(bOn ? TitleColor : FLinearColor::Transparent)
			];
	}

	/** An entry of the menu column: large label, optional line below; no box of its own (the column is the surface). */
	TSharedRef<SWidget> MenuColumnButton(const FText& Label, const FText& Sub, TFunction<void()> OnClick, bool bEnabled, bool bPrimary, bool bActive,
		const FText& Tip = FText::GetEmpty())
	{
		const bool bFilled = bPrimary && bEnabled;
		const FLinearColor LabelColor = bFilled ? DarkTextColor : !bEnabled ? MenuDisabledText : bActive ? TitleColor : TextColor;
		const FLinearColor SubColor = bFilled ? FLinearColor(0.22f, 0.18f, 0.06f) : MutedColor;
		return SNew(SButton)
			.ButtonStyle(bFilled ? &PrimaryButtonStyle() : &FlatButtonStyle())
			.IsFocusable(false)
			.IsEnabled(bEnabled)
			.ToolTipText(Tip)
			.ContentPadding(FMargin(0.f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[MenuAccent(bActive && !bFilled)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(18.f, 9.f, 16.f, 9.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Label.ToUpper()).Font(HeadFont(22)).ColorAndOpacity(LabelColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Visibility(Sub.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
						.Text(Sub)
						.Font(Font(12))
						.ColorAndOpacity(SubColor)
					]
				]
			];
	}

	enum class EMenuButtonKind : uint8
	{
		Normal,
		Primary,
		Danger
	};

	/** A button on a page (Zurück, Laden, Löschen) in one of the three styles of the UI guide. */
	TSharedRef<SWidget> MenuPageButton(const FText& Label, TFunction<void()> OnClick, EMenuButtonKind Kind = EMenuButtonKind::Normal, bool bEnabled = true,
		const FText& Tip = FText::GetEmpty(), int32 Size = 14)
	{
		const bool bFilled = Kind == EMenuButtonKind::Primary && bEnabled;
		const FLinearColor Color = !bEnabled ? MenuDisabledText : bFilled ? DarkTextColor : Kind == EMenuButtonKind::Danger ? MessageColor : TextColor;
		return SNew(SButton)
			.ButtonStyle(bFilled ? &PrimaryButtonStyle() : Kind == EMenuButtonKind::Danger ? &DangerButtonStyle() : &NormalButtonStyle())
			.IsFocusable(false)
			.IsEnabled(bEnabled)
			.ToolTipText(Tip)
			.ContentPadding(FMargin(16.f, 6.f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(Font(Size, true)).ColorAndOpacity(Color)
			];
	}

	/** Several segments, the chosen one filled (the shared switch). */
	TSharedRef<SWidget> MenuChoice(const TArray<FText>& Labels, int32 Chosen, TFunction<void(int32)> OnChoose)
	{
		return MakeSegments(Labels, TArray<FText>(), [Chosen](int32 Index) { return Index == Chosen; }, OnChoose);
	}

	/** Label on the left, the choice next to it, one sentence below. */
	TSharedRef<SWidget> MenuOptionRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Sentence = FText::GetEmpty())
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(MenuLabelWidth)[SNew(STextBlock).Text(Label).Font(Font(13, true)).ColorAndOpacity(TextColor)]
				]
				+ SHorizontalBox::Slot().AutoWidth()[Content]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(MenuLabelWidth, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Visibility(Sentence.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				.Text(Sentence)
				.Font(Font(12))
				.ColorAndOpacity(MutedColor)
				.AutoWrapText(true)
			];
	}

	/** A row of a list (heists, saves): faint outline, the chosen one with the focus outline. */
	TSharedRef<SWidget> MenuListRow(const FText& Title, const FText& Line, const FText& Extra, bool bSelected, TFunction<void()> OnClick)
	{
		return SNew(SBorder)
			.BorderImage(bSelected ? FocusFramedBrush() : SoftFramedBrush())
			.BorderBackgroundColor(bSelected ? MenuRowSelectedColor : ZoneColor)
			.Padding(0.f)
			[
				SNew(SButton)
				.ButtonStyle(&FlatButtonStyle())
				.IsFocusable(false)
				.ContentPadding(FMargin(14.f, 9.f))
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Title).Font(Font(15, true)).ColorAndOpacity(bSelected ? TitleColor : TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(Line).Font(Font(12)).ColorAndOpacity(MutedColor).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Visibility(Extra.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
						.Text(Extra)
						.Font(Font(12))
						.ColorAndOpacity(TextColor)
					]
				]
			];
	}
}

UDCGameFlowSubsystem* UDCMenuWidget::Flow() const
{
	return UDCGameFlowSubsystem::Get(GetOwningPlayer());
}

void UDCMenuWidget::Setup(bool bInInGame)
{
	bInGame = bInInGame;
	Refresh();
}

void UDCMenuWidget::Refresh()
{
	ReadSaves();
	Page = EDCMenuPage::None;
	bConfirmDelete = false;
	bDirty = true;
}

void UDCMenuWidget::ReadSaves()
{
	const UDCGameFlowSubsystem* GameFlow = Flow();
	Saves = GameFlow ? GameFlow->ListSaves() : TArray<FDCSaveInfo>();
}

void UDCMenuWidget::ShowPage(EDCMenuPage InPage)
{
	Page = InPage;
	bConfirmDelete = false;
	switch (Page)
	{
	case EDCMenuPage::Heists:
	case EDCMenuPage::Load:
		ReadSaves();
		break;
	case EDCMenuPage::Save:
		SaveName = Flow() ? Flow()->DefaultSaveName() : FString();
		break;
	case EDCMenuPage::Options:
		PendingOptions = DCOptions::GetCurrent(GetOwningPlayer());
		break;
	case EDCMenuPage::None:
		break;
	}
	bDirty = true;
}

bool UDCMenuWidget::Back()
{
	if (Page == EDCMenuPage::None)
	{
		return false;
	}
	ShowPage(EDCMenuPage::None);
	return true;
}

void UDCMenuWidget::SelectHeist(FName Id)
{
	SelectedHeist = Id;
	bDirty = true;
}

void UDCMenuWidget::SelectSave(const FString& SaveSlot)
{
	SelectedSave = SaveSlot;
	bConfirmDelete = false;
	bDirty = true;
}

bool UDCMenuWidget::SaveNow()
{
	UDCGameFlowSubsystem* GameFlow = Flow();
	const FString SaveSlot = GameFlow ? GameFlow->SavePlan(SaveName) : FString();
	if (SaveSlot.IsEmpty())
	{
		SetMessage(LOCTEXT("SaveFailed", "Speichern ging nicht."));
		return false;
	}
	ReadSaves();
	const FDCSaveInfo* Saved = Saves.FindByPredicate([&SaveSlot](const FDCSaveInfo& Save) { return Save.Slot == SaveSlot; });
	SetMessage(FText::Format(LOCTEXT("Saved", "Gespeichert: {0}"), Saved ? UDCGameFlowSubsystem::SaveTitle(*Saved) : FText::FromString(SaveName)));
	ShowPage(EDCMenuPage::None);
	return true;
}

void UDCMenuWidget::SetMessage(const FText& Text)
{
	Message = Text;
	MessageTime = FPlatformTime::Seconds();
}

TSharedRef<SWidget> UDCMenuWidget::RebuildWidget()
{
	Super::RebuildWidget();
	Root = SNew(SOverlay)
		// Over a heist the scene goes dark and nothing behind the menu takes a click.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(DCItemWidgets::Brush())
			.BorderBackgroundColor(bInGame ? MenuDimColor : FLinearColor::Transparent)
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		]
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SAssignNew(ColumnHost, SBox).WidthOverride(MenuColumnWidth)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeRule(false, true)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(40.f, 48.f))
			[
				SAssignNew(PageHost, SBox).WidthOverride(MenuPageWidth)
			]
		];
	bDirty = true;
	return Root.ToSharedRef();
}

void UDCMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	ColumnHost.Reset();
	PageHost.Reset();
	NameBox.Reset();
}

void UDCMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bDirty)
	{
		return;
	}
	bDirty = false;
	NameBox.Reset();
	if (ColumnHost.IsValid())
	{
		ColumnHost->SetContent(MakeColumn());
	}
	if (PageHost.IsValid())
	{
		PageHost->SetContent(MakePage());
	}
	// Typing the name right away.
	if (NameBox.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(NameBox, EFocusCause::SetDirectly);
	}
}

TSharedRef<SWidget> UDCMenuWidget::MakeColumn()
{
	TWeakObjectPtr<UDCMenuWidget> WeakThis = this;
	const auto Do = [WeakThis](TFunction<void(UDCMenuWidget&)> What)
	{
		return [WeakThis, What]()
		{
			if (UDCMenuWidget* Menu = WeakThis.Get())
			{
				What(*Menu);
			}
		};
	};
	const auto Open = [Do](EDCMenuPage Target) { return Do([Target](UDCMenuWidget& Menu) { Menu.ShowPage(Target); }); };
	const UDCGameFlowSubsystem* GameFlow = Flow();
	const bool bHasSaves = !Saves.IsEmpty();
	const FText NoSaves = LOCTEXT("NoSavesTip", "Noch kein Spielstand – „Neues Spiel“ beginnt einen Plan.");

	TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
	const auto Add = [&Buttons](TSharedRef<SWidget> Button) { Buttons->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[Button]; };
	FText Title;
	FText Subtitle;
	if (!bInGame)
	{
		Title = UDCGameFlowSubsystem::GetGameTitle();
		Subtitle = LOCTEXT("Subtitle", "Ein Einbruch, Schritt für Schritt geplant.");
		const FString Latest = bHasSaves ? Saves[0].Slot : FString();
		Add(MenuColumnButton(LOCTEXT("Continue", "Fortsetzen"),
			bHasSaves ? UDCGameFlowSubsystem::DescribeSave(Saves[0]) : LOCTEXT("ContinueNone", "Noch nichts gespeichert"),
			Do([Latest](UDCMenuWidget& Menu) { if (Menu.Flow()) { Menu.Flow()->LoadSave(Latest); } }), bHasSaves, bHasSaves, false,
			LOCTEXT("ContinueTip", "Den zuletzt gespeicherten Plan laden")));
		Add(MenuColumnButton(LOCTEXT("NewGame", "Neues Spiel"), FText::GetEmpty(), Open(EDCMenuPage::Heists), true, !bHasSaves, Page == EDCMenuPage::Heists,
			LOCTEXT("NewGameTip", "Einen Einbruch wählen und neu planen")));
		Add(MenuColumnButton(LOCTEXT("Load", "Laden"), FText::GetEmpty(), Open(EDCMenuPage::Load), bHasSaves, false, Page == EDCMenuPage::Load,
			bHasSaves ? LOCTEXT("LoadTip", "Einen gespeicherten Plan wählen") : NoSaves));
		Add(MenuColumnButton(LOCTEXT("Options", "Optionen"), FText::GetEmpty(), Open(EDCMenuPage::Options), true, false, Page == EDCMenuPage::Options));
		Add(MenuColumnButton(LOCTEXT("Quit", "Beenden"), FText::GetEmpty(), Do([](UDCMenuWidget& Menu) { if (Menu.Flow()) { Menu.Flow()->QuitGame(&Menu); } }),
			true, false, false));
	}
	else
	{
		Title = LOCTEXT("PauseTitle", "Pause");
		const FDCHeistInfo* Heist = GameFlow ? UDCGameFlowSubsystem::FindHeist(GameFlow->GetCurrentHeist()) : nullptr;
		int32 Steps = 0;
		if (const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(GetOwningPlayer()))
		{
			for (const FDCCrewPlan& Plan : Plans->GetPlans())
			{
				Steps += Plan.Tasks.Num();
			}
		}
		Subtitle = FText::Format(LOCTEXT("PauseSubtitle", "{0} · {1} {1}|plural(one=Schritt,other=Schritte) im Plan"),
			Heist ? Heist->Name : FText::GetEmpty(), Steps);
		const bool bCanSave = Heist != nullptr;
		Add(MenuColumnButton(LOCTEXT("Resume", "Weiter"), FText::GetEmpty(), Do([](UDCMenuWidget& Menu) { if (Menu.OnCloseRequested) { Menu.OnCloseRequested(); } }),
			true, true, false, LOCTEXT("ResumeTip", "Zurück ins Spiel (Esc)")));
		Add(MenuColumnButton(LOCTEXT("SavePlan", "Plan speichern"), FText::GetEmpty(), Open(EDCMenuPage::Save), bCanSave, false, Page == EDCMenuPage::Save,
			LOCTEXT("SavePlanTip", "Den Plan unter einem eigenen Namen speichern (automatisch gespeichert wird er ohnehin)")));
		Add(MenuColumnButton(LOCTEXT("Load", "Laden"), FText::GetEmpty(), Open(EDCMenuPage::Load), bHasSaves, false, Page == EDCMenuPage::Load,
			bHasSaves ? LOCTEXT("LoadTip", "Einen gespeicherten Plan wählen") : NoSaves));
		Add(MenuColumnButton(LOCTEXT("Options", "Optionen"), FText::GetEmpty(), Open(EDCMenuPage::Options), true, false, Page == EDCMenuPage::Options));
		Add(MenuColumnButton(LOCTEXT("MainMenu", "Hauptmenü"), FText::GetEmpty(), Do([](UDCMenuWidget& Menu) { if (Menu.Flow()) { Menu.Flow()->OpenMainMenu(); } }),
			true, false, false, LOCTEXT("MainMenuTip", "Zum Startmenü – der Plan ist automatisch gespeichert")));
		Add(MenuColumnButton(LOCTEXT("QuitGame", "Spiel beenden"), FText::GetEmpty(), Do([](UDCMenuWidget& Menu) { if (Menu.Flow()) { Menu.Flow()->QuitGame(&Menu); } }),
			true, false, false, LOCTEXT("QuitGameTip", "Der Plan ist automatisch gespeichert")));
	}

	FSlateFontInfo TitleFont = HeadFont(50);
	TitleFont.LetterSpacing = 110;
	return SNew(SBorder)
		.BorderImage(Brush())
		.BorderBackgroundColor(bInGame ? MenuColumnGameColor : MenuColumnColor)
		.Padding(FMargin(40.f, 64.f, 28.f, 40.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(18.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Title.ToUpper()).Font(TitleFont).ColorAndOpacity(TextColor)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(18.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Subtitle).Font(Font(13)).ColorAndOpacity(MutedColor).AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 48.f, 0.f, 0.f)
			[
				Buttons
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SSpacer)
			]
			// What just happened ("Gespeichert: …"), for a few seconds.
			+ SVerticalBox::Slot().AutoHeight().Padding(18.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(Font(13))
				.ColorAndOpacity(TextColor)
				.AutoWrapText(true)
				.Text_Lambda([WeakThis]()
				{
					const UDCMenuWidget* Menu = WeakThis.Get();
					return Menu && FPlatformTime::Seconds() - Menu->MessageTime < MenuMessageSeconds ? Menu->Message : FText::GetEmpty();
				})
			]
		];
}

TSharedRef<SWidget> UDCMenuWidget::MakePage()
{
	switch (Page)
	{
	case EDCMenuPage::Heists: return MakeHeistsPage();
	case EDCMenuPage::Load: return MakeLoadPage();
	case EDCMenuPage::Save: return MakeSavePage();
	case EDCMenuPage::Options: return MakeOptionsPage();
	case EDCMenuPage::None: break;
	}
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> UDCMenuWidget::MakeWindow(const FText& Title, TSharedRef<SWidget> Body, TSharedRef<SWidget> Buttons)
{
	return SNew(SBorder)
		.BorderImage(FramedBrush())
		.BorderBackgroundColor(PanelColor)
		.Padding(FMargin(28.f, 24.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
			[
				MakeTitle(Title, 22)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Body
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 20.f, 0.f, 0.f)
			[
				Buttons
			]
		];
}

TSharedRef<SWidget> UDCMenuWidget::MakeHeistsPage()
{
	TWeakObjectPtr<UDCMenuWidget> WeakThis = this;
	const TArray<FDCHeistInfo>& Heists = UDCGameFlowSubsystem::GetHeists();
	if (!UDCGameFlowSubsystem::FindHeist(SelectedHeist) && !Heists.IsEmpty())
	{
		SelectedHeist = Heists[0].Id;
	}
	const auto AutosaveOf = [this](FName Id) -> const FDCSaveInfo*
	{
		return Saves.FindByPredicate([Id](const FDCSaveInfo& Save) { return Save.bAuto && Save.Heist == Id; });
	};

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	bool bPracticeTitle = false;
	for (const FDCHeistInfo& Heist : Heists)
	{
		if (Heist.bPractice && !bPracticeTitle)
		{
			bPracticeTitle = true;
			List->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[MakeSectionTitle(LOCTEXT("Practice", "Übung"))];
		}
		const FDCSaveInfo* Auto = AutosaveOf(Heist.Id);
		const FName Id = Heist.Id;
		List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MenuListRow(Heist.Name, Heist.Description,
				Auto ? FText::Format(LOCTEXT("HeistSaved", "Gespeicherter Plan: {0}"), UDCGameFlowSubsystem::DescribeSave(*Auto)) : FText::GetEmpty(),
				Id == SelectedHeist, [WeakThis, Id]() { if (WeakThis.IsValid()) { WeakThis->SelectHeist(Id); } })
		];
	}

	const FDCHeistInfo* Selected = UDCGameFlowSubsystem::FindHeist(SelectedHeist);
	const FDCSaveInfo* Auto = Selected ? AutosaveOf(Selected->Id) : nullptr;
	const FString AutoSlot = Auto ? Auto->Slot : FString();
	List->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Visibility(Auto ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
		.Text(Selected ? FText::Format(LOCTEXT("HeistSavedNote", "Für {0} gibt es einen gespeicherten Plan. „Neu beginnen“ ersetzt ihn beim ersten neuen Schritt."),
			Selected->Name) : FText::GetEmpty())
		.Font(Font(12))
		.ColorAndOpacity(MutedColor)
		.AutoWrapText(true)
	];

	const FName StartId = SelectedHeist;
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("Back", "Zurück"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Back(); } })
		];
	if (Auto)
	{
		Buttons->AddSlot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("ContinuePlan", "Plan fortsetzen"), [WeakThis, AutoSlot]()
			{
				if (WeakThis.IsValid() && WeakThis->Flow())
				{
					WeakThis->Flow()->LoadSave(AutoSlot);
				}
			})
		];
	}
	Buttons->AddSlot().AutoWidth().Padding(4.f, 0.f)
	[
		MenuPageButton(Auto ? LOCTEXT("StartOver", "Neu beginnen") : LOCTEXT("Start", "Starten"), [WeakThis, StartId]()
		{
			if (WeakThis.IsValid() && WeakThis->Flow())
			{
				WeakThis->Flow()->StartHeist(StartId);
			}
		}, EMenuButtonKind::Primary, Selected != nullptr)
	];
	return MakeWindow(LOCTEXT("HeistsTitle", "Welcher Einbruch?"), List, Buttons);
}

TSharedRef<SWidget> UDCMenuWidget::MakeLoadPage()
{
	TWeakObjectPtr<UDCMenuWidget> WeakThis = this;
	if (!Saves.ContainsByPredicate([this](const FDCSaveInfo& Save) { return Save.Slot == SelectedSave; }))
	{
		SelectedSave = Saves.IsEmpty() ? FString() : Saves[0].Slot;
	}
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const FDCSaveInfo& Save : Saves)
	{
		const FString SaveSlot = Save.Slot;
		Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			MenuListRow(UDCGameFlowSubsystem::SaveTitle(Save), UDCGameFlowSubsystem::DescribeSave(Save), FText::GetEmpty(), SaveSlot == SelectedSave,
				[WeakThis, SaveSlot]() { if (WeakThis.IsValid()) { WeakThis->SelectSave(SaveSlot); } })
		];
	}
	if (Saves.IsEmpty())
	{
		Rows->AddSlot().AutoHeight()[MakeMuted(LOCTEXT("NoSaves", "Noch kein Spielstand – „Neues Spiel“ beginnt einen Plan."), 13)];
	}
	const bool bSelected = !SelectedSave.IsEmpty();
	const FString SaveSlot = SelectedSave;
	TSharedRef<SWidget> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("Back", "Zurück"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Back(); } })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(bConfirmDelete ? LOCTEXT("DeleteSure", "Wirklich löschen?") : LOCTEXT("Delete", "Löschen"), [WeakThis, SaveSlot]()
			{
				UDCMenuWidget* Menu = WeakThis.Get();
				if (!Menu)
				{
					return;
				}
				if (!Menu->bConfirmDelete)
				{
					Menu->bConfirmDelete = true;
				}
				else
				{
					Menu->bConfirmDelete = false;
					if (Menu->Flow() && Menu->Flow()->DeleteSave(SaveSlot))
					{
						Menu->SetMessage(LOCTEXT("Deleted", "Spielstand gelöscht."));
					}
					Menu->ReadSaves();
				}
				Menu->bDirty = true;
			}, EMenuButtonKind::Danger, bSelected, LOCTEXT("DeleteTip", "Löscht den gewählten Spielstand für immer (zweiter Klick bestätigt)"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("LoadIt", "Laden"), [WeakThis, SaveSlot]()
			{
				if (WeakThis.IsValid() && WeakThis->Flow())
				{
					WeakThis->Flow()->LoadSave(SaveSlot);
				}
			}, EMenuButtonKind::Primary, bSelected)
		];
	return MakeWindow(LOCTEXT("LoadTitle", "Welchen Stand laden?"),
		SNew(SBox).MaxDesiredHeight(460.f)[SNew(SScrollBox) + SScrollBox::Slot()[Rows]],
		Buttons);
}

TSharedRef<SWidget> UDCMenuWidget::MakeSavePage()
{
	TWeakObjectPtr<UDCMenuWidget> WeakThis = this;
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
		[
			MakeMuted(LOCTEXT("SaveNote", "Gespeichert werden die Schritte aller Figuren – ein laufender Einbruch nicht."), 12)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(NameBox, SEditableTextBox)
			.Text(FText::FromString(SaveName))
			.Font(Font(14))
			.SelectAllTextWhenFocused(true)
			.HintText(LOCTEXT("SaveHint", "Name des Spielstands"))
			.OnTextChanged_Lambda([WeakThis](const FText& Text) { if (WeakThis.IsValid()) { WeakThis->SaveName = Text.ToString(); } })
			.OnTextCommitted_Lambda([WeakThis](const FText& Text, ETextCommit::Type How)
			{
				if (WeakThis.IsValid() && How == ETextCommit::OnEnter)
				{
					WeakThis->SaveName = Text.ToString();
					WeakThis->SaveNow();
				}
			})
		];
	TSharedRef<SWidget> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("Back", "Zurück"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Back(); } })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("SaveIt", "Speichern"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->SaveNow(); } }, EMenuButtonKind::Primary, true,
				LOCTEXT("SaveItTip", "Speichern (Enter)"))
		];
	return MakeWindow(LOCTEXT("SaveTitle", "Plan speichern"), Body, Buttons);
}

TSharedRef<SWidget> UDCMenuWidget::MakeOptionsPage()
{
	TWeakObjectPtr<UDCMenuWidget> WeakThis = this;
	const auto Edit = [WeakThis](TFunction<void(FDCOptionValues&)> Change)
	{
		if (UDCMenuWidget* Menu = WeakThis.Get())
		{
			Change(Menu->PendingOptions);
			Menu->bDirty = true;
		}
	};
	const FDCOptionValues& P = PendingOptions;
	const TArray<FIntPoint> Sizes = DCOptions::GetResolutions(P.WindowMode, P.Resolution);
	const int32 SizeIndex = Sizes.IndexOfByKey(P.Resolution);
	const auto StepSize = [Edit, Sizes, SizeIndex](int32 Delta)
	{
		const int32 Next = SizeIndex + Delta;
		if (Sizes.IsValidIndex(Next))
		{
			const FIntPoint Size = Sizes[Next];
			Edit([Size](FDCOptionValues& Values) { Values.Resolution = Size; });
		}
	};

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[MakeSectionTitle(LOCTEXT("Graphics", "Grafik"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Quality", "Qualität"),
				MenuChoice({ DCOptions::QualityName(0), DCOptions::QualityName(1), DCOptions::QualityName(2), DCOptions::QualityName(3) }, P.Quality,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.Quality = Index; }); }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Display", "Anzeige"),
				MenuChoice({ DCOptions::WindowModeName(0), DCOptions::WindowModeName(1), DCOptions::WindowModeName(2) }, P.WindowMode,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.WindowMode = Index; }); }),
				LOCTEXT("DisplaySentence", "„Randlos“ füllt den Bildschirm in seiner eigenen Auflösung."))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Resolution", "Auflösung"),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[MenuPageButton(LOCTEXT("Smaller", "◀"), [StepSize]() { StepSize(-1); }, EMenuButtonKind::Normal, SizeIndex > 0)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(130.f).HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(14, true)).ColorAndOpacity(P.WindowMode == 1 ? MenuDisabledText : TextColor)
						.Text(FText::Format(LOCTEXT("Size", "{0} × {1}"), FText::AsNumber(P.Resolution.X, &FNumberFormattingOptions::DefaultNoGrouping()),
							FText::AsNumber(P.Resolution.Y, &FNumberFormattingOptions::DefaultNoGrouping())))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()[MenuPageButton(LOCTEXT("Larger", "▶"), [StepSize]() { StepSize(1); }, EMenuButtonKind::Normal, SizeIndex + 1 < Sizes.Num())])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 4.f)[MakeSectionTitle(LOCTEXT("Controls", "Steuerung"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Camera", "Kameratempo"),
				MenuChoice({ DCOptions::CameraSpeedName(0), DCOptions::CameraSpeedName(1), DCOptions::CameraSpeedName(2) }, P.CameraSpeed,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.CameraSpeed = Index; }); }),
				LOCTEXT("CameraSentence", "Wie schnell die Planungskamera mit WASD fährt und mit Q/E dreht."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 4.f)[MakeSectionTitle(LOCTEXT("Planning", "Planung"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Ghost", "Geist"),
				MenuChoice({ LOCTEXT("GhostFigure", "Figur"), LOCTEXT("GhostPin", "Stecknadel") }, P.bGhostAsPin ? 1 : 0,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.bGhostAsPin = Index == 1; }); }),
				LOCTEXT("GhostSentence", "Wo der nächste Schritt der gewählten Figur anfängt: als abgedunkelte Figur oder als Stecknadel wie in der Vorschau."))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Help", "Hinweis"),
				MenuChoice({ LOCTEXT("HelpBar", "Leiste"), LOCTEXT("HelpMenu", "Menü an der Maus") }, P.bHelpAsMenu ? 1 : 0,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.bHelpAsMenu = Index == 1; }); }),
				LOCTEXT("HelpSentence", "Wenn ein Klick den Plan hängen lässt: ruhige Leiste über dem Plan oder ein Menü genau an der Stelle des Klicks."))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Insert", "Einfügen"),
				MenuChoice({ LOCTEXT("InsertPicked", "hinter Gewähltem"), LOCTEXT("InsertButton", "nur per Knopf") }, P.bInsertOnlyWithButton ? 1 : 0,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.bInsertOnlyWithButton = Index == 1; }); }),
				LOCTEXT("InsertSentence", "Neue Schritte kommen sofort hinter den gewählten Schritt – oder erst nach „+ danach einfügen“."))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MenuOptionRow(LOCTEXT("Finds", "Funde"),
				MenuChoice({ LOCTEXT("FindsAlone", "selbst einpacken"), LOCTEXT("FindsAsk", "immer fragen") }, P.bPauseOnEveryFind ? 1 : 0,
					[Edit](int32 Index) { Edit([Index](FDCOptionValues& Values) { Values.bPauseOnEveryFind = Index == 1; }); }),
				LOCTEXT("FindsSentence", "Beim Einbruch packt die Figur Wertvolles selbst ein und fragt nur, wenn nicht alles passt oder etwas anderes dabei ist – oder das Spiel hält bei jedem Fund an."))
		];

	const bool bChanged = PendingOptions != DCOptions::GetCurrent(GetOwningPlayer());
	TSharedRef<SWidget> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(bChanged ? LOCTEXT("Discard", "Verwerfen") : LOCTEXT("Back", "Zurück"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Back(); } })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			MenuPageButton(LOCTEXT("Apply", "Übernehmen"), [WeakThis]()
			{
				if (UDCMenuWidget* Menu = WeakThis.Get())
				{
					DCOptions::Apply(Menu->GetOwningPlayer(), Menu->PendingOptions);
					Menu->PendingOptions = DCOptions::GetCurrent(Menu->GetOwningPlayer());
					Menu->SetMessage(LOCTEXT("Applied", "Optionen übernommen."));
					Menu->bDirty = true;
				}
			}, EMenuButtonKind::Primary, bChanged)
		];
	return MakeWindow(LOCTEXT("OptionsTitle", "Optionen"), Body, Buttons);
}

#undef LOCTEXT_NAMESPACE
