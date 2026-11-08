#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DCPlanGhost.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Planning: the "ghost" of the selected crew member - where their next step begins (at the end
 * of their plan, or after the picked step). A dimmed, slightly smaller figure with a light ring,
 * red where the crew member gets stuck. Clicking it selects the crew member. As a variant it can
 * look like the preview pins instead.
 */
UCLASS()
class DERCLOU3_API ADCPlanGhost : public AActor
{
	GENERATED_BODY()

public:
	/** Only something to look at: never part of the walkable area (spawning it must not rebuild the nav mesh). */
	virtual bool IsComponentRelevantForNavigation(UActorComponent* Component) const override { return false; }
	ADCPlanGhost();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Puts the ghost of Crew at the floor location. bStuck: red (the crew member hangs there);
	 * bAsPin: looks like a preview pin instead of a figure.
	 */
	void Show(FName InCrew, const FLinearColor& Color, const FVector& FloorLocation, const FText& Text, bool bStuck, bool bAsPin);
	FName GetCrew() const { return Crew; }
	FText GetText() const { return Text; }
	bool IsStuck() const { return bShownStuck; }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pole;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	FName Crew;
	FText Text;
	bool bShownStuck = false;
	bool bShownAsPin = false;
	FLinearColor ShownColor = FLinearColor::Transparent;
};
