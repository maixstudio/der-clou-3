#include "DCGameViewportClient.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

void UDCGameViewportClient::SetSceneRect(const FVector2D& InOrigin, const FVector2D& InSize)
{
	if (SceneOrigin.Equals(InOrigin, 0.0005) && SceneSize.Equals(InSize, 0.0005))
	{
		return;
	}
	SceneOrigin = InOrigin;
	SceneSize = InSize;
	LayoutPlayers();
}

void UDCGameViewportClient::LayoutPlayers()
{
	Super::LayoutPlayers();
	for (ULocalPlayer* Player : GetOuterUEngine()->GetGamePlayers(this))
	{
		if (Player && Player->GetViewportClient() == this)
		{
			Player->Origin = SceneOrigin;
			Player->Size = SceneSize;
		}
	}
}
