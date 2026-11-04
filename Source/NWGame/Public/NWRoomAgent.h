#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "NWRoomAgent.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Visible crew member of the room prototype (graybox figure). Driven by ANWRoomDirector. */
UCLASS()
class NWGAME_API ANWRoomAgent : public AActor
{
	GENERATED_BODY()

public:
	ANWRoomAgent();

	void Setup(int32 InAgentIndex, const FText& InName, const FLinearColor& InColor);
	int32 GetAgentIndex() const { return AgentIndex; }
	void SetSelected(bool bSelected);

	/** Moves to the location and turns towards the walking direction. */
	void UpdatePose(const FVector& Location, float DeltaSeconds);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UStaticMeshComponent> Nose;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UStaticMeshComponent> SelectionRing;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UTextRenderComponent> NameLabel;

private:
	int32 AgentIndex = INDEX_NONE;
	float TargetYaw = 0.f;
};
