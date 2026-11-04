#include "NWPlannerScreen.h"

#include "SNWPlannerPanel.h"

void UNWPlannerScreen::SetPlanEditor(UNWPlanEditor* InEditor)
{
	PlanEditor = InEditor;
	if (Panel.IsValid())
	{
		Panel->SetEditor(InEditor);
	}
}

TSharedRef<SWidget> UNWPlannerScreen::RebuildWidget()
{
	// Keep the CommonUI bookkeeping of the base classes, then replace the (empty) widget tree.
	Super::RebuildWidget();
	Panel = SNew(SNWPlannerPanel).Editor(PlanEditor);
	return Panel.ToSharedRef();
}

void UNWPlannerScreen::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Panel.Reset();
}
