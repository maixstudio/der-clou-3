#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCNoiseHud.generated.h"

/**
 * Top centre of the screen: the last noticeable noise ("Lärm: Schublade aufgebrochen – sehr laut")
 * for a few seconds, and a red banner once someone has called the police. Built in Slate.
 */
UCLASS()
class DERCLOU3_API UDCNoiseHud : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SWidget> Root;
};
