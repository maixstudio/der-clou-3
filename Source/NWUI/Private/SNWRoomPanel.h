#pragma once

#include "CoreMinimal.h"
#include "NWRoomScreen.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class SCanvas;
class SNWRoomTimeline;
class UNWRoomSession;

/** Header with status and plan controls, timeline at the bottom, radial menu layer on top. */
class SNWRoomPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWRoomPanel) {}
		SLATE_ARGUMENT(UNWRoomSession*, Session)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetSession(UNWRoomSession* InSession);

	void OpenRadialMenu(const FText& Title, TArray<FNWRadialOption> Options);
	void CloseRadialMenu();
	bool IsRadialMenuOpen() const { return bRadialOpen; }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick, TAttribute<bool> IsEnabled = true);
	TSharedRef<SWidget> MakeSpeedButton(float Speed);

	TWeakObjectPtr<UNWRoomSession> Session;
	TSharedPtr<SNWRoomTimeline> Timeline;
	TSharedPtr<SCanvas> RadialLayer;
	FGeometry CachedGeometry;
	bool bRadialOpen = false;
	/** Clearing the menu waits for the next tick (never inside one of its own click handlers). */
	bool bRadialClearPending = false;
};
