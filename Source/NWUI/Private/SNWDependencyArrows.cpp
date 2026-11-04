#include "SNWDependencyArrows.h"

#include "Rendering/DrawElements.h"

void SNWDependencyArrows::Construct(const FArguments& InArgs)
{
	Arrows = InArgs._Arrows;
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SNWDependencyArrows::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	constexpr float Thickness = 2.f;
	constexpr double HeadLength = 7.0;
	constexpr double HeadWidth = 4.0;

	for (const FNWTimelineArrow& Arrow : Arrows)
	{
		// Leave the source track horizontally, bend towards the target track, arrive horizontally.
		const double Bend = FMath::Clamp(FMath::Abs(Arrow.To.X - Arrow.From.X) * 0.5, 12.0, 60.0);
		const FVector2D P1(Arrow.From.X + Bend, Arrow.From.Y);
		const FVector2D P2(Arrow.To.X - Bend, Arrow.To.Y);
		FSlateDrawElement::MakeCubicBezierSpline(OutDrawElements, LayerId, Geometry, Arrow.From, P1, P2, Arrow.To,
			Thickness, ESlateDrawEffect::None, Arrow.Color);

		// Dot where the signal is sent, arrow head where the wait ends.
		const TArray<FVector2D> Dot = { Arrow.From + FVector2D(-2.0, 0.0), Arrow.From + FVector2D(2.0, 0.0) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry, Dot, ESlateDrawEffect::None, Arrow.Color, true, 5.f);

		const FVector2D Tangent = Arrow.To - P2;
		const FVector2D Direction = Tangent.IsNearlyZero() ? FVector2D(1.0, 0.0) : Tangent.GetSafeNormal();
		const FVector2D Normal(-Direction.Y, Direction.X);
		const FVector2D Base = Arrow.To - Direction * HeadLength;
		const TArray<FVector2D> Head = { Base + Normal * HeadWidth, Arrow.To, Base - Normal * HeadWidth };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry, Head, ESlateDrawEffect::None, Arrow.Color, true, Thickness);
	}
	return LayerId + 1;
}
