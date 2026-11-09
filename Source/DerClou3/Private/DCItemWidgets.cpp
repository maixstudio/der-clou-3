#include "DCItemWidgets.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "DCDoor.h"
#include "DCItem.h"
#include "DCPlan.h"
#include "DCSkills.h"
#include "DerClou3.h"
#include "Fonts/CompositeFont.h"
#include "ImageUtils.h"
#include "Input/DragAndDrop.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DCItemWidgets"

namespace
{
	/**
	 * A font family from Content/Slate/Fonts (read at runtime, see Config/DefaultGame.ini for packaging).
	 * Glyphs Barlow lacks (arrows) come from the engine's fallback font, as with the default font.
	 */
	TSharedPtr<const FCompositeFont> LoadUIFont(const TCHAR* RegularFile, const TCHAR* BoldFile)
	{
		const FString Dir = FPaths::ProjectContentDir() / TEXT("Slate/Fonts");
		const FString Regular = Dir / RegularFile;
		const FString Bold = Dir / BoldFile;
		if (!FPaths::FileExists(Regular) || !FPaths::FileExists(Bold))
		{
			UE_LOG(LogDerClou, Warning, TEXT("UI font %s missing in %s - using the engine font"), RegularFile, *Dir);
			return nullptr;
		}
		TSharedRef<FCompositeFont> Family = MakeShared<FCompositeFont>();
		Family->DefaultTypeface.AppendFont(TEXT("Regular"), Regular, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		Family->DefaultTypeface.AppendFont(TEXT("Bold"), Bold, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		Family->FallbackTypeface.Typeface.AppendFont(TEXT("Regular"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
			EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		return Family;
	}

	FSlateFontInfo FamilyFont(const TSharedPtr<const FCompositeFont>& Family, int32 Size, bool bBold)
	{
		return Family.IsValid() ? FSlateFontInfo(Family, Size, bBold ? TEXT("Bold") : TEXT("Regular"))
			: FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

namespace DCItemWidgets
{
	// Palette of Docs/UI-Mockup/planung_v1.webp (sRGB hex in the comments, values are linear).
	const FLinearColor TitleColor(0.8879f, 0.3663f, 0.0423f);            // #F2A33A amber: focus, main action
	const FLinearColor MutedColor(0.2664f, 0.2918f, 0.3278f);            // #8D939B
	const FLinearColor TextColor(0.8388f, 0.8148f, 0.7682f);             // #ECE9E3 warm white
	const FLinearColor LootColor(0.8469f, 0.5457f, 0.1046f);             // #EDC35B gold
	const FLinearColor PanelColor(0.0091f, 0.0137f, 0.0203f, 0.97f);     // #181F27
	const FLinearColor ZoneColor(0.0137f, 0.0194f, 0.0273f);             // #1F262E
	const FLinearColor MessageColor(0.8550f, 0.1441f, 0.1095f);          // #EE6A5D
	const FLinearColor CoordinationColor(0.0595f, 0.6724f, 0.7305f);     // #45D6DE cyan
	const FLinearColor DarkTextColor(0.0103f, 0.0065f, 0.0027f);         // #1A1309
	const FLinearColor TileColor(0.0194f, 0.0262f, 0.0369f);             // #262D36
	const FLinearColor BackdropColor(0.0060f, 0.0086f, 0.0123f);         // #12171D
	const FLinearColor BrassColor(0.2051f, 0.1356f, 0.0578f);            // #7D6744
	const FLinearColor BrassDimColor(0.0423f, 0.0369f, 0.0273f);         // #3A362E
	const FLinearColor OkColor(0.1981f, 0.5520f, 0.2122f);               // #7BC47F
	const FLinearColor TileHoverColor(0.0296f, 0.0395f, 0.0561f);        // #303843
	const FLinearColor ItemTileColor(0.0343f, 0.0467f, 0.0648f);         // #343D48
	const FLinearColor ItemEdgeColor(0.0802f, 0.0931f, 0.1144f);         // #50565F
	const FLinearColor SelectedTileColor(0.0704f, 0.0423f, 0.0160f);     // #4B3A22
	const FLinearColor AcceptColor(0.0176f, 0.0802f, 0.0252f);           // #24502C
	const FLinearColor RejectColor(0.1022f, 0.0168f, 0.0130f);           // #5A231E
	const FLinearColor LockedColor(0.8550f, 0.1441f, 0.1095f);           // = MessageColor
	const FLinearColor CellColor(0.0144f, 0.0203f, 0.0284f);             // #20272F
	const FLinearColor HintOkColor(0.2747f, 0.6939f, 0.3050f);           // #8FD996
	const FLinearColor DangerEdgeColor(0.1559f, 0.0356f, 0.0296f);       // #6E3530
	constexpr float Indent = 14.f;
	constexpr float Corner = 3.f;

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		static const TSharedPtr<const FCompositeFont> Family = LoadUIFont(TEXT("Barlow-Regular.ttf"), TEXT("Barlow-Bold.ttf"));
		return FamilyFont(Family, Size, bBold);
	}

	FSlateFontInfo HeadFont(int32 Size, bool bBold)
	{
		static const TSharedPtr<const FCompositeFont> Family = LoadUIFont(TEXT("BarlowCondensed-Regular.ttf"), TEXT("BarlowCondensed-Bold.ttf"));
		FSlateFontInfo Info = FamilyFont(Family, Size, bBold);
		Info.LetterSpacing = 30;
		return Info;
	}

	const FSlateBrush* Brush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	const FSlateBrush* RoundBrush()
	{
		static const FSlateRoundedBoxBrush Box(FLinearColor::White, Corner);
		return &Box;
	}

	const FSlateBrush* FramedBrush()
	{
		static const FSlateRoundedBoxBrush Box(FLinearColor::White, Corner, BrassColor, 1.f);
		return &Box;
	}

	const FSlateBrush* SoftFramedBrush()
	{
		static const FSlateRoundedBoxBrush Box(FLinearColor::White, Corner, BrassDimColor, 1.f);
		return &Box;
	}

	const FSlateBrush* FocusFramedBrush()
	{
		static const FSlateRoundedBoxBrush Box(FLinearColor::White, Corner, TitleColor, 2.f);
		return &Box;
	}

	const FSlateBrush* CoordinationFramedBrush()
	{
		static const FSlateRoundedBoxBrush Box(FLinearColor::White, Corner, CoordinationColor, 1.f);
		return &Box;
	}

	const FSlateBrush* ProblemFramedBrush()
	{
		static const FSlateRoundedBoxBrush Box(FLinearColor::White, Corner, MessageColor, 1.f);
		return &Box;
	}

	const FProgressBarStyle& BarStyle()
	{
		// Plain flat bar; the default style tints the fill blue.
		static const FProgressBarStyle Style = FProgressBarStyle()
			.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.5f)))
			.SetFillImage(FSlateColorBrush(FLinearColor::White))
			.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		return Style;
	}

	const FButtonStyle& PrimaryButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(TitleColor, Corner))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.4851f, 0.0953f), Corner))     // #FFB957
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.6376f, 0.2542f, 0.0232f), Corner)) // #D18A2A
			.SetDisabled(FSlateRoundedBoxBrush(TileColor, Corner, BrassDimColor, 1.f))
			.SetNormalForeground(DarkTextColor)
			.SetHoveredForeground(DarkTextColor)
			.SetPressedForeground(DarkTextColor)
			.SetDisabledForeground(MutedColor)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	namespace
	{
		FButtonStyle OutlinedButtonStyle(const FLinearColor& Edge, const FLinearColor& HoverEdge)
		{
			return FButtonStyle()
				.SetNormal(FSlateRoundedBoxBrush(TileColor, Corner, Edge, 1.f))
				.SetHovered(FSlateRoundedBoxBrush(TileHoverColor, Corner, HoverEdge, 1.f))
				.SetPressed(FSlateRoundedBoxBrush(ZoneColor, Corner, TitleColor, 1.f))
				.SetDisabled(FSlateRoundedBoxBrush(ZoneColor, Corner, BrassDimColor, 1.f))
				.SetNormalForeground(TextColor)
				.SetHoveredForeground(FLinearColor::White)
				.SetPressedForeground(TextColor)
				.SetDisabledForeground(MutedColor)
				.SetNormalPadding(FMargin(2.f))
				.SetPressedPadding(FMargin(2.f, 3.f, 2.f, 1.f));
		}
	}

	const FButtonStyle& NormalButtonStyle()
	{
		static const FButtonStyle Style = OutlinedButtonStyle(FLinearColor(0.0582f, 0.0545f, 0.0477f), BrassColor); // #45423D
		return Style;
	}

	const FButtonStyle& DangerButtonStyle()
	{
		static const FButtonStyle Style = OutlinedButtonStyle(DangerEdgeColor, MessageColor);
		return Style;
	}

	const FButtonStyle& FlatButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f), Corner))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.15f), Corner))
			.SetDisabled(FSlateNoResource())
			.SetNormalForeground(FSlateColor::UseForeground())
			.SetHoveredForeground(FSlateColor::UseForeground())
			.SetPressedForeground(FSlateColor::UseForeground())
			.SetDisabledForeground(MutedColor)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	TSharedRef<SWidget> MakeSegments(const TArray<FText>& Labels, const TArray<FText>& Tips, TFunction<bool(int32 Index)> IsActive, TFunction<void(int32 Index)> OnClick)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < Labels.Num(); ++Index)
		{
			Row->AddSlot().AutoWidth().Padding(Index > 0 ? 2.f : 0.f, 0.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(RoundBrush())
				.Padding(0.f)
				.BorderBackgroundColor_Lambda([IsActive, Index]() -> FSlateColor { return IsActive(Index) ? TitleColor : FLinearColor::Transparent; })
				[
					SNew(SButton)
					.IsFocusable(false)
					.ButtonStyle(&FlatButtonStyle())
					.ToolTipText(Tips.IsValidIndex(Index) ? Tips[Index] : FText::GetEmpty())
					.ContentPadding(FMargin(10.f, 2.f))
					.OnClicked_Lambda([OnClick, Index]()
					{
						OnClick(Index);
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(Labels[Index]).Font(Font(12, true))
						.ColorAndOpacity_Lambda([IsActive, Index]() -> FSlateColor { return IsActive(Index) ? DarkTextColor : MutedColor; })
					]
				]
			];
		}
		return SNew(SBorder).BorderImage(SoftFramedBrush()).BorderBackgroundColor(ZoneColor).Padding(FMargin(2.f))[Row];
	}

	TSharedRef<SWidget> MakeMuted(const FText& Text, int32 Size)
	{
		return SNew(STextBlock).Text(Text).Font(Font(Size)).ColorAndOpacity(MutedColor).AutoWrapText(true);
	}

	FText Upper(const FText& Text)
	{
		return Text.ToUpper();
	}

	TAttribute<FText> Upper(const TAttribute<FText>& Text)
	{
		if (!Text.IsBound())
		{
			return Text.Get().ToUpper();
		}
		return TAttribute<FText>::CreateLambda([Text]() { return Text.Get().ToUpper(); });
	}

	TSharedRef<SWidget> MakeTitle(const TAttribute<FText>& Text, int32 Size, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.Text(Upper(Text))
			.Font(HeadFont(Size))
			.ColorAndOpacity(Color)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
	}

	TSharedRef<SWidget> MakeRule(bool bHorizontal, bool bDim)
	{
		return SNew(SBox)
			.HeightOverride(bHorizontal ? FOptionalSize(1.f) : FOptionalSize())
			.WidthOverride(bHorizontal ? FOptionalSize() : FOptionalSize(1.f))
			[
				SNew(SBorder).BorderImage(Brush()).BorderBackgroundColor(bDim ? BrassDimColor : BrassColor).Padding(0.f)
			];
	}

	TSharedRef<SWidget> MakeSectionTitle(const FText& Text)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Upper(Text))
				.Font(HeadFont(14))
				.ColorAndOpacity(TextColor)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 1.f, 0.f, 0.f)
			[
				MakeRule(true, true)
			];
	}

	const FSlateBrush* Icon(FName Name)
	{
		// One brush per icon; Slate rasterises the SVG anew for every size it is drawn at.
		static TMap<FName, TUniquePtr<FSlateVectorImageBrush>> Cache;
		if (Name.IsNone())
		{
			return nullptr;
		}
		if (const TUniquePtr<FSlateVectorImageBrush>* Found = Cache.Find(Name))
		{
			return Found->Get();
		}
		const FString Path = FPaths::ProjectContentDir() / TEXT("Slate/Icons") / (Name.ToString() + TEXT(".svg"));
		TUniquePtr<FSlateVectorImageBrush>& Entry = Cache.Add(Name);
		if (FPaths::FileExists(Path))
		{
			Entry = MakeUnique<FSlateVectorImageBrush>(Path, FVector2f(24.f, 24.f));
		}
		else
		{
			UE_LOG(LogDerClou, Warning, TEXT("UI icon %s missing (%s)"), *Name.ToString(), *Path);
		}
		return Entry.Get();
	}

	TSharedRef<SWidget> MakeIcon(FName Name, float Size, const TAttribute<FSlateColor>& Color)
	{
		const FSlateBrush* IconBrush = Icon(Name);
		if (!IconBrush)
		{
			return SNullWidget::NullWidget;
		}
		return SNew(SBox)
			.WidthOverride(Size)
			.HeightOverride(Size)
			[
				SNew(SImage)
				.Image(IconBrush)
				.ColorAndOpacity(Color)
			];
	}

	const FSlateBrush* Portrait(const FString& CrewName)
	{
		// Dynamic brushes: a plain image brush is only loaded when it belongs to a registered style.
		static TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> Cache;
		const FString Key = CrewName.ToLower();
		if (Key.IsEmpty())
		{
			return nullptr;
		}
		if (const TSharedPtr<FSlateDynamicImageBrush>* Found = Cache.Find(Key))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateDynamicImageBrush>& Entry = Cache.Add(Key);
		for (const TCHAR* Extension : { TEXT(".png"), TEXT(".jpg") })
		{
			const FString Path = FPaths::ProjectContentDir() / TEXT("Slate/Portraits") / (Key + Extension);
			FImage Picture;
			if (!FPaths::FileExists(Path) || !FImageUtils::LoadImage(*Path, Picture) || Picture.SizeX <= 0 || Picture.SizeY <= 0)
			{
				continue;
			}
			// Cut to 3:4: trim the sides of wider pictures, keep the upper part (the head) of taller ones.
			constexpr float Aspect = 3.f / 4.f;
			const float Own = static_cast<float>(Picture.SizeX) / static_cast<float>(Picture.SizeY);
			FBox2f Region(FVector2f(0.f, 0.f), FVector2f(1.f, 1.f));
			if (Own > Aspect)
			{
				const float Keep = Aspect / Own;
				Region = FBox2f(FVector2f((1.f - Keep) * 0.5f, 0.f), FVector2f((1.f + Keep) * 0.5f, 1.f));
			}
			else if (Own < Aspect)
			{
				const float Keep = Own / Aspect;
				const float Top = (1.f - Keep) * 0.25f;
				Region = FBox2f(FVector2f(0.f, Top), FVector2f(1.f, Top + Keep));
			}
			Entry = MakeShared<FSlateDynamicImageBrush>(FName(*Path), FVector2f(static_cast<float>(Picture.SizeX), static_cast<float>(Picture.SizeY)));
			Entry->SetUVRegion(Region);
			UE_LOG(LogDerClou, Display, TEXT("Portrait of %s: %s (%d x %d)"), *CrewName, *Path, Picture.SizeX, Picture.SizeY);
			break;
		}
		return Entry.Get();
	}

	TSharedRef<SWidget> MakePortrait(const FString& CrewName, const FLinearColor& CrewColor, float Width)
	{
		const float Height = FMath::RoundToFloat(Width * 4.f / 3.f);
		TSharedRef<SWidget> Inside = SNullWidget::NullWidget;
		if (const FSlateBrush* Picture = Portrait(CrewName))
		{
			Inside = SNew(SImage).Image(Picture);
		}
		else
		{
			Inside = SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(CrewName.Left(1).ToUpper()))
					.Font(HeadFont(FMath::RoundToInt(Width * 0.55f)))
					.ColorAndOpacity(CrewColor)
				];
		}
		// Brass line around it; the 1.5 padding keeps the square picture inside the rounded corners.
		return SNew(SBox)
			.WidthOverride(Width)
			.HeightOverride(Height)
			[
				SNew(SBorder)
				.BorderImage(FramedBrush())
				.BorderBackgroundColor(TileColor)
				.Padding(1.5f)
				[
					Inside
				]
			];
	}

	float IconSizeFor(int32 FontSize)
	{
		// Lucide draws inside 20 of its 24 units, so this lines up with the cap height of Barlow.
		return FMath::RoundToFloat(FontSize * 1.2f);
	}

	TSharedRef<SWidget> MakeIconText(FName IconName, const TAttribute<FText>& Text, const FSlateFontInfo& InFont, const TAttribute<FSlateColor>& Color, float Gap)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		if (Icon(IconName))
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, Gap, 0.f)
			[
				MakeIcon(IconName, IconSizeFor(InFont.Size), Color)
			];
		}
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Text)
			.Font(InFont)
			.ColorAndOpacity(Color)
		];
		return Row;
	}

	FName IconForTask(const FDCPlanTask& Task)
	{
		switch (Task.Type)
		{
		case EDCPlanTaskType::Move:
			return TEXT("footprints");
		case EDCPlanTaskType::Wait:
		case EDCPlanTaskType::WaitFor:
			return TEXT("hourglass");
		case EDCPlanTaskType::Signal:
			return TEXT("radio");
		case EDCPlanTaskType::PickUp:
			return TEXT("hand-grab");
		case EDCPlanTaskType::Loot:
			return TEXT("search");
		case EDCPlanTaskType::Drop:
			return TEXT("arrow-down-to-line");
		case EDCPlanTaskType::Door:
			switch (static_cast<EDCDoorAction>(Task.Action))
			{
			case EDCDoorAction::Open:
				return TEXT("door-open");
			case EDCDoorAction::Close:
				return TEXT("door-closed");
			case EDCDoorAction::PickLock:
				return TEXT("key-round");
			default:
				return TEXT("footprints");
			}
		case EDCPlanTaskType::Item:
			switch (static_cast<EDCItemAction>(Task.Action))
			{
			case EDCItemAction::PickLock:
				return TEXT("key-round");
			case EDCItemAction::Force:
			case EDCItemAction::Smash:
				return TEXT("hammer");
			case EDCItemAction::Take:
				return TEXT("hand-grab");
			case EDCItemAction::Drop:
				return TEXT("arrow-down-to-line");
			case EDCItemAction::Close:
				return TEXT("door-closed");
			default:
				return TEXT("door-open");
			}
		default:
			return NAME_None;
		}
	}

	FName IconForSkill(EDCSkill Skill)
	{
		switch (Skill)
		{
		case EDCSkill::Locks:
			return TEXT("key-round");
		case EDCSkill::Safes:
			return TEXT("vault");
		case EDCSkill::Electronics:
			return TEXT("cpu");
		case EDCSkill::Explosives:
			return TEXT("bomb");
		case EDCSkill::Driving:
			return TEXT("car");
		case EDCSkill::Fighting:
			return TEXT("swords");
		case EDCSkill::Lookout:
			return TEXT("eye");
		case EDCSkill::Strength:
			return TEXT("biceps-flexed");
		default:
			return NAME_None;
		}
	}
}

namespace
{
	using namespace DCItemWidgets;

	/** What the place under the dragged item says: "→ Rucksack" in green, or the reason in red. */
	struct FDCDragHint
	{
		FText Text;
		bool bOk = true;
	};

	/** An item being dragged. Dropped where nothing takes it (the game world), DropOutside decides. */
	class FDCItemDragOp : public FDragDropOperation
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FDCItemDragOp, FDragDropOperation)

		TWeakObjectPtr<UDCItem> Item;
		TFunction<void(UDCItem*)> OnDroppedOutside;
		/** Which cell of the item the cursor holds (its footprint follows the cursor from there). */
		FIntPoint GrabCell = FIntPoint::ZeroValue;
		TSharedRef<FDCDragHint> Hint = MakeShared<FDCDragHint>();

		static TSharedRef<FDCItemDragOp> New(UDCItem* InItem, TFunction<void(UDCItem*)> InOnDroppedOutside, FIntPoint InGrabCell)
		{
			TSharedRef<FDCItemDragOp> Operation = MakeShared<FDCItemDragOp>();
			Operation->Item = InItem;
			Operation->OnDroppedOutside = MoveTemp(InOnDroppedOutside);
			Operation->GrabCell = InGrabCell;
			Operation->Construct();
			return Operation;
		}

		void SetHint(const FText& Text, bool bOk)
		{
			Hint->Text = Text;
			Hint->bOk = bOk;
		}

		virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
		{
			const TSharedRef<FDCDragHint> HintRef = Hint;
			return SNew(SBorder)
				.BorderImage(FramedBrush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(10.f, 6.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Item.IsValid() ? FText::Format(LOCTEXT("DragName", "{0}  ({1})"), Item->GetDisplayName(),
							Item->GetDef().FitsInGrid() ? DCItems::FormatCells(Item->GetGridSize()) : LOCTEXT("DragHands", "nur Hände")) : FText::GetEmpty())
						.Font(Font(12, true))
						.ColorAndOpacity(TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(SBox)
						.MaxDesiredWidth(320.f)
						.Visibility_Lambda([HintRef]() { return HintRef->Text.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							SNew(STextBlock)
							.Text_Lambda([HintRef]() { return HintRef->Text; })
							.ColorAndOpacity_Lambda([HintRef]() { return FSlateColor(HintRef->bOk ? HintOkColor : MessageColor); })
							.Font(Font(11, true))
							.AutoWrapText(true)
						]
					]
				];
		}

		virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override
		{
			if (!bDropWasHandled && OnDroppedOutside && Item.IsValid())
			{
				OnDroppedOutside(Item.Get());
			}
			FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
		}
	};

	/** Makes its content draggable as an item; a click without dragging selects it. */
	class SDCItemTile : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCItemTile)
			: _Draggable(true)
			, _CellSize(0.f)
		{}
			SLATE_ARGUMENT(TWeakObjectPtr<UDCItem>, Item)
			SLATE_ARGUMENT(TSharedPtr<FDCItemTreeContext>, Context)
			SLATE_ARGUMENT(bool, Draggable)
			/** > 0: a rectangle in a grid; the cell under the click becomes the grab point. */
			SLATE_ARGUMENT(float, CellSize)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Item = InArgs._Item;
			Context = InArgs._Context;
			bDraggable = InArgs._Draggable;
			GridCellSize = InArgs._CellSize;
			ChildSlot[InArgs._Content.Widget];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			PressPoint = FVector2D(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
			return bDraggable ? FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton) : FReply::Handled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Item.IsValid() && Context.IsValid() && Context->Select)
			{
				Context->Select(Item.Get());
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (!Item.IsValid() || !bDraggable)
			{
				return FReply::Unhandled();
			}
			const FIntPoint Size = Item->GetGridSize();
			FIntPoint Grab(FMath::Max(0, Size.X - 1) / 2, FMath::Max(0, Size.Y - 1) / 2);
			if (GridCellSize > 0.f)
			{
				Grab.X = FMath::Clamp(FMath::FloorToInt(PressPoint.X / GridCellSize), 0, FMath::Max(0, Size.X - 1));
				Grab.Y = FMath::Clamp(FMath::FloorToInt(PressPoint.Y / GridCellSize), 0, FMath::Max(0, Size.Y - 1));
			}
			TFunction<void(UDCItem*)> Outside = Context.IsValid() ? Context->DropOutside : nullptr;
			return FReply::Handled().BeginDragDrop(FDCItemDragOp::New(Item.Get(), MoveTemp(Outside), Grab));
		}

	private:
		TWeakObjectPtr<UDCItem> Item;
		TSharedPtr<FDCItemTreeContext> Context;
		bool bDraggable = true;
		float GridCellSize = 0.f;
		FVector2D PressPoint = FVector2D::ZeroVector;
	};

	/**
	 * Takes dropped items. While an item hovers over it, it turns green (fits) or red (does not).
	 * Without OnDropped it just swallows the drop.
	 */
	class SDCDropZone : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCDropZone) {}
			SLATE_ARGUMENT(TFunction<FText(UDCItem*)>, Check)
			SLATE_ARGUMENT(TFunction<void(UDCItem*)>, OnDropped)
			SLATE_ARGUMENT(FMargin, Padding)
			SLATE_ARGUMENT(FLinearColor, Color)
			SLATE_ARGUMENT(FText, Target)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Check = InArgs._Check;
			OnDropped = InArgs._OnDropped;
			Color = InArgs._Color;
			Target = InArgs._Target;
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(RoundBrush())
				.BorderBackgroundColor_Lambda([this]() { return !bHovered ? Color : (bAccepts ? AcceptColor : RejectColor); })
				.Padding(InArgs._Padding)
				[
					InArgs._Content.Widget
				]
			];
		}

		virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			bHovered = Operation.IsValid() && OnDropped;
			if (!bHovered)
			{
				return;
			}
			const FText Reason = Check ? Check(Operation->Item.Get()) : FText::GetEmpty();
			bAccepts = Reason.IsEmpty();
			Operation->SetHint(bAccepts ? (Target.IsEmpty() ? LOCTEXT("Fits", "passt") : FText::Format(LOCTEXT("FitsTarget", "→ {0}"), Target)) : Reason, bAccepts);
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			if (bHovered)
			{
				if (const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>())
				{
					Operation->SetHint(FText::GetEmpty(), true);
				}
			}
			bHovered = false;
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			bHovered = false;
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			if (OnDropped && Operation->Item.IsValid())
			{
				OnDropped(Operation->Item.Get());
			}
			return FReply::Handled();
		}

	private:
		TFunction<FText(UDCItem*)> Check;
		TFunction<void(UDCItem*)> OnDropped;
		FLinearColor Color = FLinearColor::Transparent;
		FText Target;
		bool bHovered = false;
		bool bAccepts = false;
	};

	FText Tooltip(const UDCItem* Item)
	{
		TArray<FDCInfoLine> Lines;
		Item->GetInfoLines(Lines);
		TArray<FString> Rows;
		Rows.Add(Item->GetDisplayName().ToString());
		for (const FDCInfoLine& Line : Lines)
		{
			Rows.Add(Line.Label.ToString() + TEXT(": ") + Line.Value.ToString());
		}
		if (!Item->GetDef().Description.IsEmpty())
		{
			Rows.Add(Item->GetDef().Description.ToString());
		}
		return FText::FromString(FString::Join(Rows, TEXT("\n")));
	}

	FLinearColor NameColor(const UDCItem* Item)
	{
		return Item->IsLocked() ? LockedColor : (Item->GetDef().Value > 0 ? LootColor : TextColor);
	}

	bool IsSelected(const TSharedPtr<FDCItemTreeContext>& Context, const UDCItem* Item)
	{
		return Context.IsValid() && Context->GetSelected && Context->GetSelected() == Item;
	}

	bool IsPlannedItem(const TSharedPtr<FDCItemTreeContext>& Context, const UDCItem* Item)
	{
		return Context.IsValid() && Context->IsPlanned && Context->IsPlanned(Item);
	}

	/**
	 * A container's grid: cells underneath, the items on them as rectangles. While an item is dragged
	 * over it, its footprint shows where it would land, green or red; over a container lying in the
	 * grid it goes into that container instead.
	 */
	class SDCItemGrid : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCItemGrid) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UDCItem>, Container)
			SLATE_ARGUMENT(TSharedPtr<FDCItemTreeContext>, Context)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Container = InArgs._Container;
			Context = InArgs._Context;
			const UDCItem* Holder = Container.Get();
			Grid = Holder ? Holder->GetDef().Grid : FIntPoint(1, 1);

			TSharedRef<SCanvas> Canvas = SNew(SCanvas);
			if (Holder && Context.IsValid())
			{
				for (UDCItem* Item : Holder->GetContents())
				{
					if (Item->HasGridPos())
					{
						AddTile(Canvas, Item);
					}
				}
			}
			ChildSlot
			[
				SNew(SBox)
				.WidthOverride(Grid.X * CellSize)
				.HeightOverride(Grid.Y * CellSize)
				[
					Canvas
				]
			];
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			for (int32 Y = 0; Y < Grid.Y; ++Y)
			{
				for (int32 X = 0; X < Grid.X; ++X)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(FVector2f(CellSize - 2.f, CellSize - 2.f), FSlateLayoutTransform(FVector2f(X * CellSize + 1.f, Y * CellSize + 1.f))),
						RoundBrush(), ESlateDrawEffect::None, CellColor);
				}
			}
			int32 Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
			if (bPreview)
			{
				FLinearColor Fill = bPreviewOk ? FLinearColor(0.2f, 0.85f, 0.3f, 0.45f) : FLinearColor(0.95f, 0.2f, 0.15f, 0.45f);
				// Only the part inside the grid.
				const FIntPoint Min(FMath::Max(0, PreviewMin.X), FMath::Max(0, PreviewMin.Y));
				const FIntPoint Max(FMath::Min(Grid.X, PreviewMin.X + PreviewSize.X), FMath::Min(Grid.Y, PreviewMin.Y + PreviewSize.Y));
				if (Max.X > Min.X && Max.Y > Min.Y)
				{
					++Layer;
					FSlateDrawElement::MakeBox(OutDrawElements, Layer,
						AllottedGeometry.ToPaintGeometry(FVector2f((Max.X - Min.X) * CellSize, (Max.Y - Min.Y) * CellSize), FSlateLayoutTransform(FVector2f(Min.X * CellSize, Min.Y * CellSize))),
						Brush(), ESlateDrawEffect::None, Fill);
				}
			}
			return Layer;
		}

		virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			UpdatePreview(MyGeometry, DragDropEvent, *Operation);
			return FReply::Handled();
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			bPreview = false;
			if (const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>())
			{
				Operation->SetHint(FText::GetEmpty(), true);
			}
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			const TSharedPtr<FDCItemDragOp> Operation = DragDropEvent.GetOperationAs<FDCItemDragOp>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			UpdatePreview(MyGeometry, DragDropEvent, *Operation);
			bPreview = false;
			if (Operation->Item.IsValid() && Target.IsValid() && Context.IsValid() && Context->MoveInto)
			{
				Context->MoveInto(Operation->Item.Get(), Target.Get(), TargetCell);
			}
			return FReply::Handled();
		}

	private:
		void AddTile(const TSharedRef<SCanvas>& Canvas, UDCItem* Item)
		{
			const FIntPoint Pos = Item->GetGridPos();
			const FIntPoint Size = Item->GetGridSize();
			const FVector2D Pixels(Size.X * CellSize - 2.f, Size.Y * CellSize - 2.f);
			const TSharedRef<FDCItemTreeContext> Ctx = Context.ToSharedRef();
			TWeakObjectPtr<UDCItem> WeakItem = Item;
			TSharedPtr<FDCItemTreeContext> CtxPtr = Context;
			const FDCItemDef& Def = Item->GetDef();

			// Upright long things (crowbar, flashlight) get their label turned sideways.
			const bool bUpright = Size.X == 1 && Size.Y >= 2;
			const FVector2D LabelBox = bUpright ? FVector2D(Pixels.Y - 4.f, Pixels.X - 4.f) : FVector2D(Pixels.X - 4.f, Pixels.Y - 4.f);
			TSharedRef<SCanvas> Label = SNew(SCanvas).Visibility(EVisibility::HitTestInvisible);
			Label->AddSlot()
				.Position(FVector2D((Pixels.X - 4.f) * 0.5f, (Pixels.Y - 4.f) * 0.5f))
				.Size(LabelBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.RenderTransform(bUpright ? FSlateRenderTransform(FQuat2D(-UE_HALF_PI)) : FSlateRenderTransform())
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					[
						SNew(STextBlock)
						.Text(Def.GetShortName())
						.Font(Font(Size.X == 1 && Size.Y == 1 ? 8 : 10, true))
						.ColorAndOpacity(NameColor(Item))
						.Justification(ETextJustify::Center)
						.AutoWrapText(!bUpright && Size.X > 1)
					]
				];

			const FLinearColor Fill = FMath::Lerp(ItemTileColor, Def.Color.CopyWithNewOpacity(1.f), 0.25f);
			const bool bPlanned = IsPlannedItem(Context, Item);
			TSharedRef<SWidget> Look = SNew(SBorder)
				.BorderImage(RoundBrush())
				.BorderBackgroundColor_Lambda([CtxPtr, WeakItem, bPlanned]() { return IsSelected(CtxPtr, WeakItem.Get()) ? TitleColor : (bPlanned ? CoordinationColor : ItemEdgeColor); })
				.Padding(FMargin(bPlanned ? 2.f : 1.f))
				.ToolTipText(bPlanned ? FText::Format(LOCTEXT("PlannedTip", "geplant \u2013 kommt erst mit dem Plan\n{0}"), Tooltip(Item)) : Tooltip(Item))
				.Cursor(EMouseCursor::GrabHand)
				[
					SNew(SBorder)
					.BorderImage(RoundBrush())
					.BorderBackgroundColor(Fill)
					.Padding(FMargin(1.f))
					[
						Label
					]
				];

			Canvas->AddSlot()
				.Position(FVector2D(Pos.X * CellSize + 1.f, Pos.Y * CellSize + 1.f))
				.Size(Pixels)
				[
					SNew(SDCItemTile).Item(WeakItem).Context(CtxPtr).Draggable(!Item->IsAttached()).CellSize(CellSize)
					[
						Look
					]
				];
		}

		void UpdatePreview(const FGeometry& MyGeometry, const FDragDropEvent& Event, FDCItemDragOp& Operation)
		{
			UDCItem* Holder = Container.Get();
			UDCItem* Item = Operation.Item.Get();
			bPreview = Holder && Item && Context.IsValid() && Context->CheckInto;
			if (!bPreview)
			{
				return;
			}
			const FVector2D Local(MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
			const FIntPoint Cursor(FMath::FloorToInt(Local.X / CellSize), FMath::FloorToInt(Local.Y / CellSize));

			// Over a container lying here: into that one.
			UDCItem* Under = Holder->GetItemAt(Cursor);
			if (Under && Under->IsContainer() && !Under->IsInside(Item))
			{
				Target = Under;
				TargetCell = DCItems::AnyCell;
				PreviewMin = Under->GetGridPos();
				PreviewSize = Under->GetGridSize();
				const FText Reason = Context->CheckInto(Item, Under, DCItems::AnyCell);
				bPreviewOk = Reason.IsEmpty();
				Operation.SetHint(bPreviewOk ? FText::Format(LOCTEXT("IntoItem", "→ in {0}"), Under->GetDisplayName()) : Reason, bPreviewOk);
				return;
			}

			const FIntPoint Size = Item->GetDef().FitsInGrid() ? Item->GetGridSize() : FIntPoint(1, 1);
			FIntPoint Cell = Cursor - Operation.GrabCell;
			// Keep the footprint inside the grid where it can be (friendlier near the edges).
			if (Size.X <= Grid.X && Size.Y <= Grid.Y)
			{
				Cell.X = FMath::Clamp(Cell.X, 0, Grid.X - Size.X);
				Cell.Y = FMath::Clamp(Cell.Y, 0, Grid.Y - Size.Y);
			}
			Target = Holder;
			TargetCell = Cell;
			PreviewMin = Cell;
			PreviewSize = Size;
			const FText Reason = Context->CheckInto(Item, Holder, Cell);
			bPreviewOk = Reason.IsEmpty();
			Operation.SetHint(bPreviewOk ? FText::Format(LOCTEXT("IntoGrid", "→ {0}"), Holder->GetDisplayName()) : Reason, bPreviewOk);
		}

		TWeakObjectPtr<UDCItem> Container;
		TSharedPtr<FDCItemTreeContext> Context;
		FIntPoint Grid = FIntPoint(1, 1);

		bool bPreview = false;
		bool bPreviewOk = false;
		FIntPoint PreviewMin = FIntPoint::ZeroValue;
		FIntPoint PreviewSize = FIntPoint(1, 1);
		TWeakObjectPtr<UDCItem> Target;
		FIntPoint TargetCell = DCItems::AnyCell;
	};
}

namespace DCItemWidgets
{
	TSharedRef<SWidget> MakeDropZone(TSharedRef<SWidget> Content, TFunction<FText(UDCItem*)> Check, TFunction<void(UDCItem*)> OnDropped,
		const FLinearColor& Color, const FMargin& Padding, const FText& Target)
	{
		return SNew(SDCDropZone).Check(MoveTemp(Check)).OnDropped(MoveTemp(OnDropped)).Color(Color).Padding(Padding).Target(Target)[Content];
	}

	TSharedRef<SWidget> MakeSwallowZone(TSharedRef<SWidget> Content)
	{
		return SNew(SDCDropZone).Padding(FMargin(0.f))[Content];
	}

	TSharedRef<SWidget> MakeTile(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, TSharedRef<SWidget> Content, bool bDraggable)
	{
		return SNew(SDCItemTile).Item(Item).Context(Context).Draggable(bDraggable && Item && !Item->IsAttached())[Content];
	}

	TArray<EDCItemAction> DefaultActions(const UDCItem* Item, bool bTake, bool bDrop)
	{
		TArray<EDCItemAction> Actions;
		const FDCItemDef& Def = Item->GetDef();
		if (Item->IsLocked())
		{
			Actions.Add(EDCItemAction::PickLock);
			if (Def.bForceable)
			{
				Actions.Add(EDCItemAction::Force);
			}
		}
		else if (Def.bOpenable && !Item->IsDestroyed())
		{
			Actions.Add(Item->IsOpen() ? EDCItemAction::Close : EDCItemAction::Open);
		}
		if (Def.bBreakable && !Item->IsDestroyed() && !Item->IsOpen())
		{
			Actions.Add(EDCItemAction::Smash);
		}
		if (!Item->IsAttached() && Def.bMovable)
		{
			if (bTake)
			{
				Actions.Add(EDCItemAction::Take);
			}
			if (bDrop)
			{
				Actions.Add(EDCItemAction::Drop);
			}
		}
		return Actions;
	}

	TSharedRef<SWidget> MakeHeader(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable, bool bButtons)
	{
		TWeakObjectPtr<UDCItem> WeakItem = Item;
		TSharedPtr<FDCItemTreeContext> CtxPtr = Context;
		const FDCItemDef& Def = Item->GetDef();

		// Buttons for what can be done with it.
		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		if (bButtons && Context->GetActions)
		{
			for (const EDCItemAction Action : Context->GetActions(Item))
			{
				// Greyed out when not possible; the tooltip says why or how long and how likely.
				const FDCTaskCheck Check = Context->CheckAction ? Context->CheckAction(Item, Action) : FDCTaskCheck();
				Buttons->AddSlot().AutoWidth().Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.IsFocusable(false)
					.ButtonStyle(&NormalButtonStyle())
					.ToolTipText(Check.Describe())
					.ForegroundColor(Check.bPossible ? TextColor : MutedColor)
					.ContentPadding(FMargin(5.f, 0.f))
					.OnClicked_Lambda([CtxPtr, WeakItem, Action]()
					{
						if (WeakItem.IsValid() && CtxPtr->DoAction)
						{
							CtxPtr->DoAction(WeakItem.Get(), Action);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(DCItems::DescribeAction(Action)).Font(Font(11, true))
						.ColorAndOpacity(Check.bPossible ? TextColor : MutedColor)
					]
				];
			}
		}

		const FText State = Item->DescribeState();
		const bool bCanDrag = bDraggable && !Item->IsAttached() && Def.bMovable;
		const bool bPlanned = IsPlannedItem(Context, Item);
		FText Name = State.IsEmpty() ? Def.Name : FText::Format(LOCTEXT("NameState", "{0}  ({1})"), Def.Name, State);
		if (bPlanned)
		{
			Name = FText::Format(LOCTEXT("NamePlanned", "{0}  \u00b7 geplant"), Name);
		}
		TSharedRef<SWidget> Look = SNew(SBorder)
			.BorderImage_Lambda([CtxPtr, WeakItem]() { return IsSelected(CtxPtr, WeakItem.Get()) ? FocusFramedBrush() : SoftFramedBrush(); })
			.BorderBackgroundColor_Lambda([CtxPtr, WeakItem]() { return IsSelected(CtxPtr, WeakItem.Get()) ? SelectedTileColor : TileColor; })
			.Padding(FMargin(8.f, 3.f, 4.f, 3.f))
			.ToolTipText(Tooltip(Item))
			.Cursor(bCanDrag ? EMouseCursor::GrabHand : EMouseCursor::Default)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Name)
					.Font(Font(11, true))
					.ColorAndOpacity(bPlanned ? CoordinationColor : NameColor(Item))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 2.f, 0.f)
				[
					MakeMuted(DCItems::FormatKg(Item->GetTotalMass()), 10)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Buttons
				]
			];
		return SNew(SDCItemTile).Item(WeakItem).Context(CtxPtr).Draggable(bCanDrag)[Look];
	}

	TSharedRef<SWidget> MakeGrid(UDCItem* Container, const TSharedRef<FDCItemTreeContext>& Context)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HAlign(HAlign_Left)[SNew(SDCItemGrid).Container(Container).Context(Context)]
			];
		// Things without a cell (put there by the level, more than fits): still reachable.
		for (UDCItem* Loose : Container->GetContents())
		{
			if (!Loose->HasGridPos() && !Loose->IsAttached())
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[MakeHeader(Loose, Context, true, false)];
			}
		}
		return Box;
	}

	TSharedRef<SWidget> MakeSection(UDCItem* Item, const TSharedRef<FDCItemTreeContext>& Context, bool bDraggable, bool bGrid)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[MakeHeader(Item, Context, bDraggable, true)];
		if (Item->IsContainer() && bGrid)
		{
			const FDCItemDef& Def = Item->GetDef();
			if (Item->IsReachable())
			{
				Box->AddSlot().AutoHeight().Padding(Indent, 3.f, 0.f, 0.f)[MakeGrid(Item, Context)];
				Box->AddSlot().AutoHeight().Padding(Indent, 1.f, 0.f, 0.f)
				[
					MakeMuted(FText::Format(LOCTEXT("GridInfo", "Raster {0} · {1} von {2} Feldern belegt · bis „{3}“"), DCItems::FormatCells(Def.Grid),
						FText::AsNumber(Item->GetUsedCells()), FText::AsNumber(Def.GetCellCount()), DCItems::DescribeSize(Def.MaxContentSize)), 9)
				];
			}
			else
			{
				Box->AddSlot().AutoHeight().Padding(Indent, 3.f, 0.f, 0.f)
				[
					MakeMuted(Item->IsLocked() ? LOCTEXT("GridLocked", "versperrt – was drin ist, sieht man erst offen") : LOCTEXT("GridClosed", "zu – öffnen, um hineinzusehen"), 10)
				];
			}
		}
		// Fixed parts (the desk's drawer) each with their own grid.
		for (UDCItem* Part : Item->GetContents())
		{
			if (Part->IsAttached())
			{
				Box->AddSlot().AutoHeight().Padding(Indent, 4.f, 0.f, 0.f)[MakeSection(Part, Context, false, true)];
			}
		}
		return Box;
	}

	TSharedRef<SWidget> MakeSelection(UDCItem* Selected, const TArray<UDCItem*>& Shown, const TSharedRef<FDCItemTreeContext>& Context)
	{
		if (!Selected)
		{
			return SNullWidget::NullWidget;
		}
		// The selected item and every container it lies in, up to one the window shows anyway.
		TArray<UDCItem*> Chain;
		for (UDCItem* Item = Selected; Item && !Shown.Contains(Item); Item = Item->GetParent())
		{
			Chain.Insert(Item, 0);
		}
		const bool bAlreadyShown = Chain.IsEmpty();
		if (bAlreadyShown)
		{
			Chain.Add(Selected);
		}
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (UDCItem* Item : Chain)
		{
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[MakeSection(Item, Context, true, !bAlreadyShown)];
		}
		return SNew(SBorder)
			.BorderImage(RoundBrush())
			.BorderBackgroundColor(ZoneColor)
			.Padding(FMargin(8.f, 6.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 5.f)
				[
					MakeSectionTitle(LOCTEXT("SelectionTitle", "Ausgewählt"))
				]
				+ SVerticalBox::Slot().AutoHeight()[Box]
			];
	}
}

#undef LOCTEXT_NAMESPACE
