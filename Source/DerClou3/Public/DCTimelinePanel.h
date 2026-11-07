#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCTimelinePanel.generated.h"

class SVerticalBox;

/**
 * The timeline at the bottom: one row per crew member with their planned steps as blocks over
 * time (pale = walking, grey = waiting for someone else, strong = working, red = will not
 * happen), a ruler, the playback cursor, and the buttons to play, pause, go back to planning,
 * wait, undo and switch to direct control. Built in Slate.
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
	/** Changes whenever what the rows show changes (plan, schedule, playback progress). */
	FString MakeSignature() const;
	float GetScale() const;

	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Rows;
	FString Signature;
};
