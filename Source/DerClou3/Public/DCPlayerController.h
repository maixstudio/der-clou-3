#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DCPlayerController.generated.h"

class ADCClickMarker;
class ADCCrewCharacter;
class ADCDoor;
class ADCItemPickup;
class UDCContainerPanel;
class UDCInfoPanel;
class UDCInventoryPanel;
class UDCRadialMenu;

/**
 * Mouse control: click a crew member to select it, click the floor to send the selected
 * crew member there, click a door or an item to choose what to do with it (radial menu).
 * I opens the inventory of the selected crew member.
 */
UCLASS()
class DERCLOU3_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADCPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void HandleClick();
	void Select(ADCCrewCharacter* Crew);
	void OpenDoorMenu(ADCDoor* Door);
	void OpenPickupMenu(ADCItemPickup* Pickup);
	void UpdateHoverCursor();
	ADCItemPickup* FindPickup(FName ItemId) const;

	/** Command-line self-test -DCSelfTest=Inventory: pick up the test items, check the rules, screenshot, quit. */
	void StartInventorySelfTest();
	/** -DCSelfTest=Containers: search furniture, open, pick and force locks, take and hide things. */
	void StartContainerSelfTest();
	void FinishSelfTest(bool bPassed);
	void OnContainerReached(ADCItemPickup* Pickup);
	/** Each step returns true when it is done. */
	TArray<TFunction<bool()>> SelfTestSteps;

	UPROPERTY(Transient)
	TObjectPtr<ADCCrewCharacter> Selected;

	UPROPERTY(Transient)
	TObjectPtr<ADCClickMarker> Marker;

	UPROPERTY(Transient)
	TObjectPtr<UDCRadialMenu> RadialMenu;

	UPROPERTY(Transient)
	TObjectPtr<UDCInfoPanel> InfoPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDCInventoryPanel> InventoryPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDCContainerPanel> ContainerPanel;

	FDelegateHandle ContainerReachedHandle;

	TWeakObjectPtr<ADCItemPickup> HoveredPickup;
};
