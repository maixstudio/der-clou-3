#pragma once

#include "CoreMinimal.h"
#include "NWActivatableScreen.h"

#include "NWPlannerScreen.generated.h"

class SNWPlannerPanel;
class UNWPlanEditor;

/**
 * The heist planner: one timeline track per crew member, an action palette, ghost-run result
 * and execution controls. The content is a native Slate panel (docs/03: the timeline is a
 * C++ widget for precise layout); this class hosts it as a CommonUI screen.
 */
UCLASS()
class NWUI_API UNWPlannerScreen : public UNWActivatableScreen
{
	GENERATED_BODY()

public:
	void SetPlanEditor(UNWPlanEditor* InEditor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNWPlanEditor> PlanEditor;

	TSharedPtr<SNWPlannerPanel> Panel;
};
