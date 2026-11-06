#pragma once

#include "CoreMinimal.h"

/**
 * Size class of an item. Decides where it can go:
 * Small fits in pockets, Medium in a backpack, Large only in both hands, Huge is too much for one person.
 */
enum class EDCItemSize : uint8
{
	Small,
	Medium,
	Large,
	Huge
};

/** Where a crew member can wear or hold something. */
enum class EDCEquipSlot : uint8
{
	None,
	Head,
	Body,
	Vest,
	Back,
	LeftHand,
	RightHand
};

/** Graybox look in the world until there are real meshes. */
enum class EDCItemShape : uint8
{
	Box,
	Cylinder,
	Sphere
};

/** What a crew member can do with an item (in the inventory or in a container in the world). */
enum class EDCItemAction : uint8
{
	Open,
	Close,
	/** Pick the lock with the lockpicks (quiet, takes a while, the lock stays intact). */
	PickLock,
	/** Break it open with the crowbar (fast, the lock is broken for good). */
	Force,
	/** From a container in the world into the inventory. */
	Take,
	/** From the inventory onto the floor. */
	Drop,
	/** Smash the glass (display case): very loud, it stays broken. */
	Smash
};

/**
 * Everything that is the same for all items of one kind ("Brecheisen", "Schreibtisch", …).
 * Furniture is an item too: it is just too big or too heavy to carry, or fixed in place.
 */
struct FDCItemDef
{
	FName Id;
	FText Name;
	/** Category shown in the info panel, e.g. "Werkzeug", "Beute", "Möbel". */
	FText Kind;
	FText Description;

	float MassKg = 0.f;
	/** Space it takes up in a container (outer size). */
	float VolumeLiters = 0.f;
	EDCItemSize Size = EDCItemSize::Small;
	/** Fence value in euros (0 = worthless). */
	int32 Value = 0;
	/** False: fixed in place (built-in, bolted down) – nobody can move it. */
	bool bMovable = true;

	/** Head, Body, Vest or Back if it can be worn there; None otherwise. */
	EDCEquipSlot WearSlot = EDCEquipSlot::None;

	/** > 0 makes it a container (pockets, drawer, safe, cushions as a hiding place, …). */
	float CapacityLiters = 0.f;
	/** Largest size class that fits into this container. */
	EDCItemSize MaxContentSize = EDCItemSize::Small;
	/** Has a door, lid or drawer front: contents can only be reached while it is open. */
	bool bOpenable = false;
	/** Rigid (box, safe): outer volume stays the same. Soft (bag, backpack): grows with the contents. */
	bool bRigid = true;
	/** Can be locked. 0 = no lock; otherwise the time it takes to pick it. */
	float LockPickSeconds = 0.f;
	/** The lock can be broken with a crowbar. */
	bool bForceable = false;
	/** Glass that can be smashed with the crowbar (opens it for good, very loud). */
	bool bBreakable = false;
	/** Built-in parts every item of this kind has (the drawers of a desk). They cannot be taken out. */
	TArray<FName> FixedParts;

	EDCItemShape Shape = EDCItemShape::Box;
	/** Size of the world representation in cm (X, Y, Z). */
	FVector ShapeSize = FVector(20.0);
	FLinearColor Color = FLinearColor::Gray;

	bool IsContainer() const { return CapacityLiters > 0.f; }
	bool HasLock() const { return LockPickSeconds > 0.f; }
	/** Furniture and other big things stand in the way. */
	bool IsObstacle() const { return Size >= EDCItemSize::Large; }
};

namespace DCItems
{
	/** All item kinds the game knows. */
	DERCLOU3_API const TArray<FDCItemDef>& GetAll();
	/** Definition by id; nullptr if there is none. */
	DERCLOU3_API const FDCItemDef* Find(FName Id);

	DERCLOU3_API FText DescribeSize(EDCItemSize Size);
	DERCLOU3_API FText DescribeSlot(EDCEquipSlot Slot);
	DERCLOU3_API FText DescribeAction(EDCItemAction Action);

	/** "2,5 kg", "12 L", "4.500 €" (German number format). */
	DERCLOU3_API FText FormatKg(float Kg);
	DERCLOU3_API FText FormatLiters(float Liters);
	DERCLOU3_API FText FormatEuro(int32 Euro);

	/** Tools the actions need. */
	inline const FName LockpicksId(TEXT("lockpicks"));
	inline const FName CrowbarId(TEXT("crowbar"));
	/** Breaking a lock open takes this long. */
	constexpr float ForceSeconds = 3.f;
	constexpr float SmashSeconds = 1.5f;

	/** How far noises carry in open space (cm). */
	namespace Noise
	{
		constexpr float Footstep = 110.f;
		/** Extra footstep range at full load / with both hands full. */
		constexpr float FootstepLoad = 220.f;
		constexpr float FootstepBothHands = 120.f;
		constexpr float PickClick = 140.f;
		constexpr float DoorOpen = 320.f;
		constexpr float DoorClose = 420.f;
		constexpr float Force = 1300.f;
		constexpr float Smash = 1700.f;
		/** Dropping: range per kg, capped. */
		constexpr float DropPerKg = 25.f;
		constexpr float DropMax = 900.f;
	}
}
