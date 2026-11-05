#pragma once

#include "CoreMinimal.h"
#include "DCDoor.h"
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

	/** Walks to the door and uses it. Replaces whatever the crew member was doing. */
	void InteractWithDoor(ADCDoor* Door, EDCDoorAction Action);

	/** What the crew member is doing right now (shown under the name). */
	FText GetActivityText() const;

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

	UPROPERTY(VisibleAnywhere, Category = "Crew")
	TObjectPtr<UTextRenderComponent> ActivityLabel;

private:
	enum class ETaskPhase : uint8
	{
		Idle,
		Walking,         // plain move order
		ToDoor,          // walking to the stand point in front of the door
		AtDoor,          // hand on the door
		WaitingForDoor,  // door swings open, then walk through
		ThroughDoor      // walking to the other side
	};

	/** Starts an AI move; false if the target cannot be reached. */
	bool StartMove(const FVector& Location);
	bool HasArrived() const;
	void SetPhase(ETaskPhase InPhase);
	void UpdateTask(float DeltaSeconds);
	void UpdateLabels(float DeltaSeconds);
	void ShowNotice(const FText& Text);

	bool bSelected = false;
	ETaskPhase Phase = ETaskPhase::Idle;
	TWeakObjectPtr<ADCDoor> TaskDoor;
	EDCDoorAction TaskAction = EDCDoorAction::Open;
	FVector PassTarget = FVector::ZeroVector;
	float PhaseTime = 0.f;
	/** Short feedback such as "Tür ist verschlossen", shown for a moment instead of the activity. */
	FText Notice;
	float NoticeTime = 0.f;
};
