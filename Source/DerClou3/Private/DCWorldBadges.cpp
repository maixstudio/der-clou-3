#include "DCWorldBadges.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "DCFloors.h"
#include "DCHeistState.h"
#include "DCItemWidgets.h"
#include "DCPlan.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	/** Sizes of a sign: the selected crew member's are large and carry verbs, the others' small. */
	struct FBadgeStyle
	{
		float Height;
		float Circle;
		float IconSize;
		int32 NumberSize;
		int32 TextSize;
		float Stem;
	};
	const FBadgeStyle BadgeFocusStyle{ 26.f, 20.f, 15.f, 12, 12, 24.f };
	const FBadgeStyle BadgeQuietStyle{ 20.f, 15.f, 12.f, 10, 11, 14.f };
	const FBadgeStyle BadgeLabelStyle{ 22.f, 0.f, 14.f, 11, 11, 18.f };

	/** Pills and circles: a rounded box whose corners are half its height (one brush per radius). */
	const FSlateBrush* BadgePill(float Radius)
	{
		static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Brushes;
		const int32 Key = FMath::RoundToInt(Radius * 2.f);
		TSharedPtr<FSlateRoundedBoxBrush>& Brush = Brushes.FindOrAdd(Key);
		if (!Brush.IsValid())
		{
			Brush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, Key * 0.5f);
		}
		return Brush.Get();
	}

	/** Dark or white, whatever reads on this colour. */
	FLinearColor BadgeInk(const FLinearColor& Ground)
	{
		using namespace DCItemWidgets;
		const float Luminance = 0.2126f * Ground.R + 0.7152f * Ground.G + 0.0722f * Ground.B;
		return Luminance > 0.25f ? DarkTextColor : FLinearColor::White;
	}

	FVector2D MeasureBadgeText(const FText& Text, const FSlateFontInfo& Font)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	const FBadgeStyle& BadgeStyleOf(const FDCWorldBadge& Badge)
	{
		if (Badge.Kind != EDCBadgeKind::Step)
		{
			return BadgeLabelStyle;
		}
		return Badge.bFocus ? BadgeFocusStyle : BadgeQuietStyle;
	}

	/** What draws on the sign; with Out = nullptr it only measures. Returns the width. */
	struct FBadgeCanvas
	{
		const FGeometry* Geometry = nullptr;
		FSlateWindowElementList* Out = nullptr;
		int32 Layer = 0;
		float Alpha = 1.f;

		void Box(const FVector2D& Pos, const FVector2D& Size, const FSlateBrush* Brush, const FLinearColor& Color) const
		{
			if (Out && Brush)
			{
				FSlateDrawElement::MakeBox(*Out, Layer, Geometry->ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos))), Brush,
					ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * Alpha));
			}
		}
		void Text(const FVector2D& Pos, const FText& String, const FSlateFontInfo& Font, const FLinearColor& Color) const
		{
			if (Out)
			{
				const FVector2D Size = MeasureBadgeText(String, Font);
				FSlateDrawElement::MakeText(*Out, Layer + 1, Geometry->ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos))), String, Font,
					ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * Alpha));
			}
		}
		void IconAt(const FVector2D& Pos, float Size, FName Name, const FLinearColor& Color) const
		{
			if (Out && !Name.IsNone())
			{
				FSlateDrawElement::MakeBox(*Out, Layer + 1, Geometry->ToPaintGeometry(FVector2f(Size, Size), FSlateLayoutTransform(FVector2f(Pos))),
					DCItemWidgets::Icon(Name), ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * Alpha));
			}
		}
	};

	/** Lays out (and with a canvas that has Out, draws) one sign with its top left at Pos. Returns its size. */
	FVector2D LayBadge(const FDCWorldBadge& Badge, const FVector2D& Pos, const FBadgeCanvas& Canvas)
	{
		using namespace DCItemWidgets;
		const FBadgeStyle& Style = BadgeStyleOf(Badge);
		const float H = Style.Height;
		const FSlateFontInfo NumberFont = Font(Style.NumberSize, true);
		const FSlateFontInfo TextFont = Font(Style.TextSize, Badge.Kind == EDCBadgeKind::Step);
		const FVector2D Top = Pos;

		if (Badge.Kind != EDCBadgeKind::Step)
		{
			// Door and moment: dark pill with a rim in the crew colour, icon and text.
			const FVector2D TextSize = MeasureBadgeText(Badge.Text, TextFont);
			const float W = 8.f + (Badge.Icon.IsNone() ? 0.f : Style.IconSize + 5.f) + TextSize.X + 10.f;
			const FLinearColor Rim = Badge.Color.CopyWithNewOpacity(Badge.bFocus ? 1.f : 0.7f);
			Canvas.Box(Top, FVector2D(W, H), BadgePill(H * 0.5f), Rim);
			Canvas.Box(Top + FVector2D(1.5f, 1.5f), FVector2D(W - 3.f, H - 3.f), BadgePill(H * 0.5f - 1.5f), BackdropColor.CopyWithNewOpacity(0.92f));
			float X = Top.X + 8.f;
			if (!Badge.Icon.IsNone())
			{
				Canvas.IconAt(FVector2D(X, Top.Y + (H - Style.IconSize) * 0.5f), Style.IconSize, Badge.Icon, Badge.Color);
				X += Style.IconSize + 5.f;
			}
			Canvas.Text(FVector2D(X, Top.Y + (H - TextSize.Y) * 0.5f), Badge.Text, TextFont, TextColor);
			return FVector2D(W, H);
		}

		// Steps: the selected crew member's pill is filled with their colour (red if a step goes wrong),
		// the others' are dark with a rim in their colour.
		const bool bFilled = Badge.bFocus;
		const FLinearColor Ground = Badge.bProblem ? MessageColor : Badge.Color;
		const FLinearColor Ink = bFilled ? BadgeInk(Ground) : Ground;
		float W = 3.f;
		for (int32 Index = 0; Index < Badge.Entries.Num(); ++Index)
		{
			const FDCBadgeEntry& Entry = Badge.Entries[Index];
			W += Entry.Number > 0 ? Style.Circle + 4.f : 2.f;
			W += Style.IconSize;
			if (Badge.bFocus && !Entry.Verb.IsEmpty() && Badge.Entries.Num() <= 2)
			{
				W += 4.f + MeasureBadgeText(Entry.Verb, TextFont).X;
			}
			W += Index + 1 < Badge.Entries.Num() ? 9.f : 8.f;
		}
		if (bFilled)
		{
			Canvas.Box(Top, FVector2D(W, H), BadgePill(H * 0.5f), Ground);
		}
		else
		{
			Canvas.Box(Top, FVector2D(W, H), BadgePill(H * 0.5f), Ground);
			Canvas.Box(Top + FVector2D(1.5f, 1.5f), FVector2D(W - 3.f, H - 3.f), BadgePill(H * 0.5f - 1.5f), BackdropColor.CopyWithNewOpacity(0.9f));
		}
		float X = Top.X + 3.f;
		for (int32 Index = 0; Index < Badge.Entries.Num(); ++Index)
		{
			const FDCBadgeEntry& Entry = Badge.Entries[Index];
			if (Entry.Number > 0)
			{
				// The number in a circle: light on the filled pill, the crew colour on the dark one.
				const FVector2D CirclePos(X, Top.Y + (H - Style.Circle) * 0.5f);
				const FLinearColor Disc = Entry.bProblem ? (bFilled ? FLinearColor::White : MessageColor) : (bFilled ? FLinearColor(0.95f, 0.93f, 0.88f) : Ground);
				const FLinearColor Digit = Entry.bProblem && bFilled ? MessageColor : DarkTextColor;
				Canvas.Box(CirclePos, FVector2D(Style.Circle, Style.Circle), BadgePill(Style.Circle * 0.5f), Disc);
				const FText Number = FText::AsNumber(Entry.Number);
				const FVector2D NumberSize = MeasureBadgeText(Number, NumberFont);
				Canvas.Text(CirclePos + (FVector2D(Style.Circle, Style.Circle) - NumberSize) * 0.5f, Number, NumberFont, Digit);
				X += Style.Circle + 4.f;
			}
			else
			{
				X += 2.f;
			}
			const FName IconName = Entry.bProblem ? FName(TEXT("triangle-exclamation-point")) : Entry.Icon;
			Canvas.IconAt(FVector2D(X, Top.Y + (H - Style.IconSize) * 0.5f), Style.IconSize, IconName, Ink);
			X += Style.IconSize;
			if (Badge.bFocus && !Entry.Verb.IsEmpty() && Badge.Entries.Num() <= 2)
			{
				const FVector2D VerbSize = MeasureBadgeText(Entry.Verb, TextFont);
				Canvas.Text(FVector2D(X + 4.f, Top.Y + (H - VerbSize.Y) * 0.5f), Entry.Verb, TextFont, Ink);
				X += 4.f + VerbSize.X;
			}
			X += Index + 1 < Badge.Entries.Num() ? 9.f : 8.f;
		}
		return FVector2D(W, H);
	}

	/** Paints all signs; the owner remembers where they went for clicks. */
	class SDCWorldBadgePainter : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCWorldBadgePainter) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UDCWorldBadges* InOwner)
		{
			Owner = InOwner;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
			int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override
		{
			using namespace DCItemWidgets;
			UDCWorldBadges* Badges = Owner.Get();
			if (!Badges)
			{
				return LayerId;
			}
			Badges->Drawn.Reset();
			Badges->DrawnScale = Geometry.Scale;
			APlayerController* Player = Badges->GetOwningPlayer();
			const UDCHeistStateSubsystem* Heist = Player ? UDCHeistStateSubsystem::Get(Player) : nullptr;
			const UDCPlanSubsystem* Plans = Player ? UDCPlanSubsystem::Get(Player) : nullptr;
			if (!Heist || !Plans || Plans->GetMode() != EDCPlanMode::Planning || Geometry.Scale <= 0.f)
			{
				return LayerId;
			}
			const UDCFloorSubsystem* Floors = UDCFloorSubsystem::Get(Player);
			const int32 Shown = Floors && Floors->HasSeveralFloors() ? Floors->GetShownFloor() : TNumericLimits<int32>::Max();

			// Quiet signs first, so the selected crew member's lie on top; door signs and moments last.
			TArray<const FDCWorldBadge*> Order;
			for (const FDCWorldBadge& Badge : Heist->GetStepBadges())
			{
				if (!Badge.bFocus)
				{
					Order.Add(&Badge);
				}
			}
			for (const FDCWorldBadge& Badge : Heist->GetStepBadges())
			{
				if (Badge.bFocus)
				{
					Order.Add(&Badge);
				}
			}
			for (const FDCWorldBadge& Badge : Heist->GetDoorBadges())
			{
				Order.Add(&Badge);
			}
			for (const FDCWorldBadge& Badge : Heist->GetMomentBadges())
			{
				Order.Add(&Badge);
			}

			struct FLaid
			{
				const FDCWorldBadge* Badge;
				FVector2D Floor;
				FVector2D Pos;
				FVector2D Size;
			};
			TArray<FLaid> Laid;
			const FBadgeCanvas Measure{};
			const FVector2D Area = Geometry.GetLocalSize();
			for (const FDCWorldBadge* Badge : Order)
			{
				if (Badge->Storey > Shown)
				{
					continue;
				}
				FVector2D Pixel;
				if (!Player->ProjectWorldLocationToScreen(Badge->Anchor, Pixel, false))
				{
					continue;
				}
				const FVector2D Floor = Pixel / Geometry.Scale;
				if (Floor.X < -200.0 || Floor.Y < -200.0 || Floor.X > Area.X + 200.0 || Floor.Y > Area.Y + 200.0)
				{
					continue;
				}
				const FVector2D Size = LayBadge(*Badge, FVector2D::ZeroVector, Measure);
				const float Stem = Badge->Kind == EDCBadgeKind::Door ? 0.f : BadgeStyleOf(*Badge).Stem;
				FVector2D Pos(Floor.X - Size.X * 0.5, Floor.Y - Stem - Size.Y);
				// Signs that would cover each other move up, above the one in the way.
				for (int32 Try = 0; Try < 8; ++Try)
				{
					bool bMoved = false;
					for (const FLaid& Other : Laid)
					{
						const bool bOverlap = Pos.X < Other.Pos.X + Other.Size.X && Pos.X + Size.X > Other.Pos.X
							&& Pos.Y < Other.Pos.Y + Other.Size.Y && Pos.Y + Size.Y > Other.Pos.Y;
						if (bOverlap)
						{
							Pos.Y = Other.Pos.Y - Size.Y - 3.0;
							bMoved = true;
						}
					}
					if (!bMoved)
					{
						break;
					}
				}
				Laid.Add({ Badge, Floor, Pos, Size });
			}

			const FVector2D Cursor = Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
			const FLaid* Hovered = nullptr;
			int32 Layer = LayerId;
			for (const FLaid& Item : Laid)
			{
				const FDCWorldBadge& Badge = *Item.Badge;
				FBadgeCanvas Canvas;
				Canvas.Geometry = &Geometry;
				Canvas.Out = &Out;
				Canvas.Layer = Layer + 1;
				Canvas.Alpha = Badge.bFocus || Badge.Kind == EDCBadgeKind::Door ? 1.f : 0.85f;
				// The stem from the place on the floor up to the sign; a moment gets a dot where the crew member is.
				if (Badge.Kind != EDCBadgeKind::Door)
				{
					const FLinearColor StemColor = (Badge.bProblem ? MessageColor : Badge.Color).CopyWithNewOpacity(Canvas.Alpha);
					FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(),
						TArray<FVector2f>{ FVector2f(Item.Floor), FVector2f(static_cast<float>(Item.Floor.X), static_cast<float>(Item.Pos.Y + Item.Size.Y)) }, ESlateDrawEffect::None, StemColor, true,
						Badge.bFocus ? 2.f : 1.5f);
					if (Badge.Kind == EDCBadgeKind::Moment)
					{
						Canvas.Box(Item.Floor - FVector2D(6.0, 6.0), FVector2D(12.0, 12.0), BadgePill(6.f), FLinearColor::White);
						Canvas.Box(Item.Floor - FVector2D(4.0, 4.0), FVector2D(8.0, 8.0), BadgePill(4.f), Badge.Color);
					}
				}
				LayBadge(Badge, Item.Pos, Canvas);
				Layer += 3;
				const FSlateRect Rect(Item.Pos, Item.Pos + Item.Size);
				if (Badge.Kind == EDCBadgeKind::Step)
				{
					Badges->Drawn.Add({ Rect, Badge.Crew, Badge.Step });
				}
				if (Rect.ContainsPoint(Cursor))
				{
					Hovered = &Item;
				}
			}

			// The details of the sign under the mouse, above it.
			if (Hovered && !Hovered->Badge->Details.IsEmpty())
			{
				const TArray<FText>& Lines = Hovered->Badge->Details;
				float Width = 0.f;
				float Height = 0.f;
				TArray<FVector2D> Sizes;
				for (int32 Index = 0; Index < Lines.Num(); ++Index)
				{
					const FVector2D Size = MeasureBadgeText(Lines[Index], Font(12, Index == 0));
					Sizes.Add(Size);
					Width = FMath::Max(Width, static_cast<float>(Size.X));
					Height += Size.Y + 2.f;
				}
				const FVector2D BoxSize(Width + 20.f, Height + 14.f);
				FVector2D BoxPos(Hovered->Pos.X + Hovered->Size.X * 0.5 - BoxSize.X * 0.5, Hovered->Pos.Y - BoxSize.Y - 6.0);
				BoxPos.X = FMath::Clamp(BoxPos.X, 4.0, FMath::Max(4.0, Area.X - BoxSize.X - 4.0));
				BoxPos.Y = FMath::Max(4.0, BoxPos.Y);
				FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(FVector2f(BoxSize), FSlateLayoutTransform(FVector2f(BoxPos))), FramedBrush(),
					ESlateDrawEffect::None, BackdropColor.CopyWithNewOpacity(0.96f));
				FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(FVector2f(Hovered->Size + FVector2D(4.0, 4.0)),
					FSlateLayoutTransform(FVector2f(Hovered->Pos - FVector2D(2.0, 2.0)))), BadgePill(static_cast<float>(Hovered->Size.Y) * 0.5f + 2.f),
					ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.18f));
				float Y = BoxPos.Y + 7.f;
				for (int32 Index = 0; Index < Lines.Num(); ++Index)
				{
					FSlateDrawElement::MakeText(Out, Layer + 2, Geometry.ToPaintGeometry(FVector2f(Sizes[Index]), FSlateLayoutTransform(FVector2f(static_cast<float>(BoxPos.X) + 10.f, Y))),
						Lines[Index], Font(12, Index == 0), ESlateDrawEffect::None, Index == 0 ? TextColor : MutedColor);
					Y += Sizes[Index].Y + 2.f;
				}
				Layer += 3;
			}
			return Layer;
		}

	private:
		TWeakObjectPtr<UDCWorldBadges> Owner;
	};
}

TSharedRef<SWidget> UDCWorldBadges::RebuildWidget()
{
	Super::RebuildWidget();
	Painter = SNew(SDCWorldBadgePainter, this);
	return Painter.ToSharedRef();
}

void UDCWorldBadges::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Painter.Reset();
}

bool UDCWorldBadges::FindStepAt(const FVector2D& ViewportPosition, FName& OutCrew, int32& OutStep) const
{
	if (DrawnScale <= 0.f)
	{
		return false;
	}
	const FVector2D Local = ViewportPosition / DrawnScale;
	// The last drawn lies on top.
	for (int32 Index = Drawn.Num() - 1; Index >= 0; --Index)
	{
		if (Drawn[Index].Rect.ContainsPoint(Local) && !Drawn[Index].Crew.IsNone() && Drawn[Index].Step != INDEX_NONE)
		{
			OutCrew = Drawn[Index].Crew;
			OutStep = Drawn[Index].Step;
			return true;
		}
	}
	return false;
}
