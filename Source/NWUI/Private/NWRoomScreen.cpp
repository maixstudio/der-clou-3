#include "NWRoomScreen.h"

#include "SNWRoomPanel.h"

void UNWRoomScreen::SetSession(UNWRoomSession* InSession)
{
	Session = InSession;
	if (Panel.IsValid())
	{
		Panel->SetSession(InSession);
	}
}

void UNWRoomScreen::OpenRadialMenu(const FText& Title, TArray<FNWRadialOption> Options)
{
	if (Panel.IsValid())
	{
		Panel->OpenRadialMenu(Title, MoveTemp(Options));
	}
}

void UNWRoomScreen::CloseRadialMenu()
{
	if (Panel.IsValid())
	{
		Panel->CloseRadialMenu();
	}
}

bool UNWRoomScreen::IsRadialMenuOpen() const
{
	return Panel.IsValid() && Panel->IsRadialMenuOpen();
}

TSharedRef<SWidget> UNWRoomScreen::RebuildWidget()
{
	// Keep the CommonUI bookkeeping of the base classes, then replace the (empty) widget tree.
	Super::RebuildWidget();
	Panel = SNew(SNWRoomPanel).Session(Session);
	return Panel.ToSharedRef();
}

void UNWRoomScreen::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Panel.Reset();
}
