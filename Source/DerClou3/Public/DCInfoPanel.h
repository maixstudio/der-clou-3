#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCInfoPanel.generated.h"

/**
 * Info panel (top right) for any IDCInteractable: name, kind and its facts, refreshed every
 * frame while open. Built in Slate.
 */
UCLASS()
class DERCLOU3_API UDCInfoPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the facts of the object (must implement IDCInteractable). */
	void ShowFor(UObject* Interactable);
	void Hide();
	bool IsShown() const { return Target.IsValid(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	FText GetTitle() const;
	FText GetKind() const;
	FText GetLabels() const;
	FText GetValues() const;

	TWeakObjectPtr<UObject> Target;
	TSharedPtr<SWidget> Root;
};
