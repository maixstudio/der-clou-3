#include "DCRadialMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float RadiusX = 125.f;
	constexpr float RadiusY = 85.f;
	const FVector2D ButtonSize(150.0, 40.0);
	const FVector2D TitleSize(130.0, 30.0);

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

TSharedRef<SWidget> UDCRadialMenu::RebuildWidget()
{
	Super::RebuildWidget();
	SAssignNew(Canvas, SCanvas).Visibility(EVisibility::SelfHitTestInvisible);
	return Canvas.ToSharedRef();
}

void UDCRadialMenu::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Canvas.Reset();
}

void UDCRadialMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastGeometry = MyGeometry;
	if (bClearPending && Canvas.IsValid())
	{
		Canvas->ClearChildren();
		bClearPending = false;
	}
}

void UDCRadialMenu::Open(const FText& Title, TArray<FDCRadialOption> Options)
{
	if (!Canvas.IsValid() || Options.IsEmpty())
	{
		return;
	}
	Canvas->ClearChildren();
	bClearPending = false;
	bOpen = true;

	const FVector2D Centre = LastGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());

	Canvas->AddSlot()
		.Position(Centre)
		.Size(TitleSize)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.92f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(STextBlock).Text(Title).Font(Font(12, true)).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.25f))
			]
		];

	// Options on an ellipse around the cursor, starting at the top.
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const float Angle = -UE_HALF_PI + UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Options.Num());
		const FVector2D Position = Centre + FVector2D(FMath::Cos(Angle) * RadiusX, FMath::Sin(Angle) * RadiusY);
		TFunction<void()> OnSelected = MoveTemp(Options[Index].OnSelected);
		Canvas->AddSlot()
			.Position(Position)
			.Size(ButtonSize)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				// Never take keyboard focus: the camera keys must keep reaching the game.
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([this, OnSelected]()
				{
					Close();
					if (OnSelected)
					{
						OnSelected();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Options[Index].Label).Font(Font(13, true))
				]
			];
	}
}

void UDCRadialMenu::Close()
{
	bClearPending = bClearPending || bOpen;
	bOpen = false;
}
