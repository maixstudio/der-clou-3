#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DCPlan.h"

#include "DCWaitDialog.generated.h"

class ADCCrewCharacter;
class SVerticalBox;

/**
 * "Worauf soll Vic warten?" – one guided dialog for every kind of waiting: a fixed time, until
 * another crew member has done one of their steps (they then give a signal), or until they are done
 * with everything. Pick an option, read the sentence that says what will happen, confirm.
 * Built in Slate, centred over the game.
 */
UCLASS()
class DERCLOU3_API UDCWaitDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(ADCCrewCharacter* InCrew);
	void Close();
	bool IsOpen() const { return bOpen; }

	/** Picks an option (also used by the self-test). */
	void Choose(const FDCWaitChoice& InChoice);
	bool HasChoice() const { return Choice.IsSet(); }
	/** Adds the chosen wait to the plan and closes; false if nothing was chosen. */
	bool Confirm();

	/** The sentence under the options: what will happen. */
	FText DescribeChoice() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildOptions();
	bool IsChosen(const FDCWaitChoice& Option) const;

	TWeakObjectPtr<ADCCrewCharacter> Crew;
	TOptional<FDCWaitChoice> Choice;
	bool bOpen = false;
	bool bDirty = false;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Options;
};
