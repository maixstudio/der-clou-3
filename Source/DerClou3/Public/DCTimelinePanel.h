#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCTimelinePanel.generated.h"

class SBox;
class SVerticalBox;
struct FDCSchedule;
struct FGeometry;
struct FPointerEvent;

/**
 * The planning panel at the bottom, built in Slate.
 *
 * Left: the plan of the selected crew member as a list of sentences – number (as on the floor),
 * time, what happens, how long and how likely, what it changes in what they carry, and who has to
 * wait for whom ("wartet 10 s – bis Vic mit „Hintertür knacken“ fertig ist"). Steps can be moved
 * and deleted there; below the title it says what the hands hold once the plan has run.
 *
 * Right: the timeline of all crew members (pale = walking, grey = waiting, strong = working,
 * red = will not happen), with lines from the step someone waits for to the one who waits.
 * Clicking or dragging in it sets a preview time: pins in the room show where everyone is then
 * and what they do.
 */
UCLASS()
class DERCLOU3_API UDCTimelinePanel : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildRows();
	void RebuildList(const FDCSchedule& Schedule, FName Focus);
	void RebuildTracks(const FDCSchedule& Schedule, FName Focus);
	/** Click or drag in the timeline: preview time from x. */
	void OnTrackMouse(const FGeometry& Geometry, const FPointerEvent& Event, float Scale);
	/** Changes whenever what the rows show changes (plan, schedule, selection, playback progress). */
	FString MakeSignature() const;
	float GetScale() const;

	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> ListHeader;
	TSharedPtr<SVerticalBox> ListRows;
	TSharedPtr<SBox> Tracks;
	FString Signature;
};
