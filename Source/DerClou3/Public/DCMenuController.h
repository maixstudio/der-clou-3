#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DCMenuController.generated.h"

class ACameraActor;
class UDCMenuWidget;

/**
 * The start menu over the villa at night: no planning interface, the crew and the room names hidden,
 * a camera that drifts slowly along the street. Esc closes an open page.
 */
UCLASS()
class DERCLOU3_API ADCMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADCMenuPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	UDCMenuWidget* GetMenu() const { return Menu; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDCMenuWidget> Menu;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MenuCamera;

	double DriftTime = 0.0;
	void PlaceCamera();
};
