#pragma once

#include "CoreMinimal.h"
#include "NWActivatableScreen.h"

#include "NWRoomScreen.generated.h"

class SNWRoomPanel;
class UNWRoomSession;

/** One entry of the radial menu. */
struct FNWRadialOption
{
	FText Label;
	TFunction<void()> OnSelected;
};

/**
 * Screen of the room prototype: status header with plan controls, a clear timeline at the bottom
 * and a radial menu that opens at the mouse cursor (doors, windows, the selected figure).
 */
UCLASS()
class NWUI_API UNWRoomScreen : public UNWActivatableScreen
{
	GENERATED_BODY()

public:
	void SetSession(UNWRoomSession* InSession);

	void OpenRadialMenu(const FText& Title, TArray<FNWRadialOption> Options);
	void CloseRadialMenu();
	bool IsRadialMenuOpen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNWRoomSession> Session;

	TSharedPtr<SNWRoomPanel> Panel;
};
