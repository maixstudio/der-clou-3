#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SLeafWidget.h"

class UNWRoomSession;

/**
 * Timeline of the room prototype, painted directly from the live session every frame:
 * one lane per crew member, one labelled block per command (width = real duration),
 * running command outlined, failed commands red, queued commands grey, white playhead.
 * The time scale adapts so the whole plan always fits the available width.
 */
class SNWRoomTimeline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNWRoomTimeline) {}
		SLATE_ARGUMENT(UNWRoomSession*, Session)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetSession(UNWRoomSession* InSession) { Session = InSession; }

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	static constexpr float NameWidth = 110.f;
	static constexpr float RulerHeight = 20.f;
	static constexpr float LaneHeight = 44.f;
	static constexpr float LaneGap = 6.f;

private:
	struct FBlock
	{
		FVector2D Position;
		FVector2D Size;
		int32 Record = INDEX_NONE;
	};

	/** Seconds shown and pixels per second for the given width. */
	void ComputeScale(float Width, float& OutSeconds, float& OutPixelsPerSecond) const;
	void LayoutBlocks(float Width, TArray<FBlock>& OutBlocks) const;
	FText GetHoverText() const;

	TWeakObjectPtr<UNWRoomSession> Session;
	int32 HoveredRecord = INDEX_NONE;
};
