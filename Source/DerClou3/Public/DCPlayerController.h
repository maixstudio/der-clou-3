#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DCPlayerController.generated.h"

class ADCClickMarker;
class ADCCrewCharacter;

/**
 * Mouse control: click a crew member to select it, click the floor to send the selected
 * crew member there.
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

	UPROPERTY(Transient)
	TObjectPtr<ADCCrewCharacter> Selected;

	UPROPERTY(Transient)
	TObjectPtr<ADCClickMarker> Marker;
};
