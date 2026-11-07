#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"

#include "DCGameViewportClient.generated.h"

/**
 * Draws the 3D scene into a part of the window only (like one player of a split screen), so the
 * fixed frame around it – crew cards, profile, status, plan bar – never covers the room.
 * Set as GameViewportClientClassName in DefaultEngine.ini; UDCFrame tells it the scene's part.
 */
UCLASS()
class DERCLOU3_API UDCGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	/** The scene's part of the window (0..1); lays the players out again if it changed. */
	void SetSceneRect(const FVector2D& InOrigin, const FVector2D& InSize);

	virtual void LayoutPlayers() override;

private:
	FVector2D SceneOrigin = FVector2D::ZeroVector;
	FVector2D SceneSize = FVector2D(1.0, 1.0);
};
