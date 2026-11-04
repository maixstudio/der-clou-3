#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/** One arrow on the planner timeline: from the moment a signal is sent to the end of a wait. */
struct FNWTimelineArrow
{
	FVector2D From = FVector2D::ZeroVector;
	FVector2D To = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
};

/** Paints dependency arrows over the timeline tracks. Purely visual, never hit-tested. */
class SNWDependencyArrows : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNWDependencyArrows) {}
		SLATE_ARGUMENT(TArray<FNWTimelineArrow>, Arrows)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<FNWTimelineArrow> Arrows;
};
