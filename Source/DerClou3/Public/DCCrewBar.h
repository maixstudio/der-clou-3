#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCCrewBar.generated.h"

class ADCCrewCharacter;
class SVerticalBox;

/**
 * The crew at a glance, on the left edge: one card per crew member with colour and initial,
 * name, best skills and what they plan or do right now. Clicking a card selects the crew member
 * and moves the camera to them; "Profil" opens their profile and inventory. Built in Slate.
 */
UCLASS()
class DERCLOU3_API UDCCrewBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Width of the bar including its margin (other windows on the left start after it). */
	static constexpr float Width = 276.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildCards();
	TSharedRef<SWidget> MakeCard(ADCCrewCharacter* Member, int32 Number);

	TSharedPtr<SWidget> Root;
	TSharedPtr<SVerticalBox> Cards;
	int32 CardCount = -1;
};
