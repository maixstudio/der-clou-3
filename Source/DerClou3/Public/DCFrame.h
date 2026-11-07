#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCFrame.generated.h"

/**
 * The fixed frame around the scene, like in a role-playing game: a status strip on top, the crew
 * cards on the left, the profile on the right, the plan bar at the bottom. The 3D scene is drawn
 * only inside (UDCGameViewportClient), with a thin line in the colour of the mode around it.
 * The other windows place themselves into these zones using the sizes below (Slate units).
 */
UCLASS()
class DERCLOU3_API UDCFrame : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr float TopHeight = 44.f;
	static constexpr float LeftWidth = 292.f;
	static constexpr float RightWidth = 452.f;
	static constexpr float BottomHeight = 330.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedPtr<SWidget> Root;
};
