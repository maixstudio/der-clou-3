#include "DCNavBlockerComponent.h"

#include "NavAreas/NavArea_Null.h"

UDCNavBlockerComponent::UDCNavBlockerComponent()
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	bDynamicObstacle = true;
	bUseSystemDefaultObstacleAreaClass = false;
	AreaClassOverride = UNavArea_Null::StaticClass();
	SetHiddenInGame(true);
}
