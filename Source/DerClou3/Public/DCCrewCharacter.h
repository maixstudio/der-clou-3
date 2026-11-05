#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "DCCrewCharacter.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A crew member: a real Unreal character, controlled by an AI controller and walking on the
 * nav mesh. Graybox look until there is a proper mesh: body, head, facing marker, name and a
 * selection ring.
 */
UCLASS()
class DERCLOU3_API ADCCrewCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADCCrewCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	void SetSelected(bool bInSelected);
	bool IsSelected() const { return bSelected; }

	/** Walks to the location (projected onto the nav mesh). Returns false if it is not reachable. */
	bool MoveToLocation(const FVector& Location);

	const FText& GetCrewName() const { return CrewName; }

protected:
	UPROPERTY(EditAnywhere, Category = "Crew")
	FText CrewName;

	UPROPERTY(EditAnywhere, Category = "Crew")
	FLinearColor BodyColor = FLinearColor(1.f, 0.42f, 0.05f);

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> Nose;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UStaticMeshComponent> SelectionRing;

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UTextRenderComponent> NameLabel;

private:
	bool bSelected = false;
};
