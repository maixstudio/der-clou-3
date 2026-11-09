#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "DCWorldBadges.generated.h"

/** What a sign in the room stands for. */
enum class EDCBadgeKind : uint8
{
	/** Steps of a plan at one place: number and activity ("2 knacken · 3 durchsuchen"). */
	Step,
	/** A door the plan changes: "aufgesperrt 0:03 · Vic". */
	Door,
	/** Where a crew member is at the preview time: "Rosa: wartet auf Vic". */
	Moment
};

/** One step on a sign: its number (0 = none, e.g. waiting on the way), what it does, and whether it goes wrong. */
struct FDCBadgeEntry
{
	int32 Number = 0;
	FName Icon;
	/** Short verb, shown for the selected crew member only ("knacken"). */
	FText Verb;
	bool bProblem = false;
};

/** A sign over a place in the room, drawn by UDCWorldBadges. Made by UDCHeistStateSubsystem while planning. */
struct FDCWorldBadge
{
	EDCBadgeKind Kind = EDCBadgeKind::Step;
	/** On the floor where it happens (Door: above the door). */
	FVector Anchor = FVector::ZeroVector;
	/** Storey of the anchor (signs above the shown storey are hidden). */
	int32 Storey = 0;
	FLinearColor Color = FLinearColor::White;
	/** Whose step (NAME_None: a door sign). */
	FName Crew;
	/** The selected crew member's: large, with verbs. Others are small and quiet. */
	bool bFocus = true;
	bool bProblem = false;
	/** Step: the steps at this place, in plan order. */
	TArray<FDCBadgeEntry> Entries;
	/** Door, Moment: icon and text. */
	FName Icon;
	FText Text;
	/** Shown when the mouse is over the sign; the first line in bold. */
	TArray<FText> Details;
	/** Clicking the sign picks this step of Crew (INDEX_NONE: nothing). */
	int32 Step = INDEX_NONE;
};

/**
 * The signs over the room while planning, in one layer over the scene (under the frame and the panels):
 * per place a pill in the crew member's colour with the step numbers and the icon of what happens there,
 * plus the signs on doors the plan changes and, with the time preview, where everyone is then. They stay
 * the same size and sharp at any zoom and use the icons and fonts of the panels.
 *
 * The layer takes no clicks itself: the player controller asks FindStepAt before it looks into the world,
 * so a click on a sign picks that step. Hovering shows the details next to the sign.
 */
UCLASS()
class DERCLOU3_API UDCWorldBadges : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The step sign at this point of the viewport (pixels, as the mouse position): whose and which step. */
	bool FindStepAt(const FVector2D& ViewportPosition, FName& OutCrew, int32& OutStep) const;

	/** Where the signs were drawn last (local units), with whose step they stand for. Filled while painting. */
	struct FDrawn
	{
		FSlateRect Rect;
		FName Crew;
		int32 Step = INDEX_NONE;
	};
	TArray<FDrawn> Drawn;
	/** Local units per viewport pixel at the last paint. */
	float DrawnScale = 1.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SWidget> Painter;
};
