#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCRadialMenu.generated.h"

class SCanvas;

/** One entry of the radial menu. */
struct FDCRadialOption
{
	FText Label;
	/** Second, smaller line: duration and chance ("ca. 3 s · Erfolg 75 %") or why it is not possible. */
	FText Detail;
	/** Shown when hovering (the long form of Detail). */
	FText Tooltip;
	/** Greyed out when false (still clickable, so it can say why). */
	bool bEnabled = true;
	TFunction<void()> OnSelected;
};

/**
 * Radial menu that opens at the mouse cursor: title in the middle, options on a circle around it.
 * Built in Slate (no widget blueprint needed). Clicks next to it go through to the game.
 */
UCLASS()
class DERCLOU3_API UDCRadialMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(const FText& Title, TArray<FDCRadialOption> Options);
	void Close();
	bool IsOpen() const { return bOpen; }
	/** Self-tests: picks the open menu's first entry whose label contains the text (as a click would). False if there is none. */
	bool ChooseOption(const FString& LabelPart);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedPtr<SCanvas> Canvas;
	FGeometry LastGeometry;
	bool bOpen = false;
	TArray<FDCRadialOption> Current;
	/** Clearing waits for the next tick, never inside one of the menu's own click handlers. */
	bool bClearPending = false;
};
