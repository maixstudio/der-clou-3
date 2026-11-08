#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** What the loading screen shows; the game flow changes it while it is up. */
struct FDCLoadingState
{
	/** "Villa Lindenhof", or the game title on the way to the menu. */
	FText Title;
	/** "Einbruch", "Übung", "Hauptmenü". */
	FText Kind;
	/** "Lade …", "Berechne Wege …". */
	FText Status;
	FText Tip;
	/** 0..1; shown smoothed. */
	float Progress = 0.f;
};

/**
 * Full-screen loading screen: game title small at the top, the place being loaded large at the bottom
 * left with what is being done and a thin bar, one tip below. Takes all clicks.
 */
class SDCLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDCLoadingScreen) {}
		SLATE_ARGUMENT(TSharedPtr<FDCLoadingState>, State)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** A tip the loading screens take turns with. */
	static FText NextTip();

private:
	TSharedPtr<FDCLoadingState> State;
	float ShownProgress = 0.f;
};
