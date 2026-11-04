#pragma once

#include "CoreMinimal.h"
#include "NWPlanTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;
class UNWPlanEditor;

/**
 * Planner UI in plain Slate: header with ghost-run result and execution controls, action palette,
 * and the timeline (one track per crew member, blocks sized by their simulated duration).
 * Rebuilds itself on the next tick after the plan editor reports a change.
 */
class SNWPlannerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWPlannerPanel) {}
		SLATE_ARGUMENT(UNWPlanEditor*, Editor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SNWPlannerPanel() override;

	void SetEditor(UNWPlanEditor* InEditor);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void Rebuild();
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakePalette();
	TSharedRef<SWidget> MakeTimeline();
	TSharedRef<SWidget> MakeTrack(int32 CrewIndex, float TimelineWidth);
	TSharedRef<SWidget> MakeRuler(float TimelineWidth, float Seconds);

	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick, TAttribute<bool> IsEnabled = true, const FText& ToolTip = FText::GetEmpty());
	TSharedRef<SWidget> MakePickButton(const FText& Label, ENWPlanActionType Type);

	UNWPlanEditor* GetEditor() const { return Editor.Get(); }
	bool IsExecuting() const;
	float GetTimelineSeconds() const;

	TWeakObjectPtr<UNWPlanEditor> Editor;
	FDelegateHandle ChangedHandle;
	bool bNeedsRebuild = true;

	TSharedPtr<SScrollBox> TimelineScroll;
	float ScrollOffset = 0.f;
	float PixelsPerSecond = 4.f;
};
