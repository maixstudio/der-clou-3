#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCRadialMenu.generated.h"

class SCanvas;

/** One entry of the radial menu. */
/** How the detail line of an entry reads: plain, cyan (waits for someone), red (a problem). */
enum class EDCOptionTone : uint8
{
	Normal,
	Waits,
	Problem
};

struct FDCRadialOption
{
	FText Label;
	/** Line icon before the label (file name in Content/Slate/Icons, see DCItemWidgets::Icon); None = no icon. */
	FName Icon;
	/** Second, smaller line: duration and chance ("ca. 3 s · Erfolg 75 %") or why it is not possible. */
	FText Detail;
	/** Shown when hovering (the long form of Detail). */
	FText Tooltip;
	/** Greyed out when false (still clickable, so it can say why). */
	bool bEnabled = true;
	EDCOptionTone Tone = EDCOptionTone::Normal;
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
	/**
	 * Another ring at the same spot, e.g. "Schublade öffnen mit …" from an entry of this one. Built on the next tick
	 * (an entry's click must never clear the menu it belongs to); counts as open right away.
	 */
	void OpenSub(const FText& Title, TArray<FDCRadialOption> Options);
	/** Self-tests: the open menu has an entry whose label contains the text; bOutEnabled: whether it is possible. */
	bool HasOption(const FString& LabelPart, bool* bOutEnabled = nullptr) const;
	void Close();
	bool IsOpen() const { return bOpen; }
	/** The title of the menu opened last ("Hintertür · verschlossen\nplant: Vic"). */
	const FText& GetTitle() const { return ShownTitle; }
	/** Self-tests: picks the open menu's first entry whose label contains the text (as a click would). False if there is none. */
	bool ChooseOption(const FString& LabelPart);
	/** Self-tests: open in the middle of the screen, not at the cursor (which may be outside the window). */
	bool bOpenAtCentreForTests = false;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedPtr<SCanvas> Canvas;
	FGeometry LastGeometry;
	bool bOpen = false;
	FText ShownTitle;
	TArray<FDCRadialOption> Current;
	/** Clearing waits for the next tick, never inside one of the menu's own click handlers. */
	bool bClearPending = false;
	/** Puts the title and the options round Centre (clears what was there). */
	void Build(const FText& Title, TArray<FDCRadialOption> Options, const FVector2D& Centre);
	/** Where the last ring stood (local units), for OpenSub. */
	FVector2D LastCentre = FVector2D::ZeroVector;
	/** A ring OpenSub asked for, built on the next tick. */
	bool bSubPending = false;
	FText PendingTitle;
	TArray<FDCRadialOption> PendingOptions;
};
