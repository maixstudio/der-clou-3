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

/** Everything that is the same for all items of one kind ("Brecheisen", "Rucksack", …). */
struct FDCItemDef
{
	FName Id;
	FText Name;
	/** Category shown in the info panel, e.g. "Werkzeug", "Beute". */
	FText Kind;
	FText Description;

	float MassKg = 0.f;
	/** Space it takes up in a container. */
	float VolumeLiters = 0.f;
	EDCItemSize Size = EDCItemSize::Small;
	/** Fence value in euros (0 = worthless). */
	int32 Value = 0;

	/** Head, Body, Vest or Back if it can be worn there; None otherwise. */
	EDCEquipSlot WearSlot = EDCEquipSlot::None;
	/** > 0 makes it a container (pockets of clothes, vest, backpack). */
	float CapacityLiters = 0.f;
	/** Largest size class that fits into this container. */
	EDCItemSize MaxContentSize = EDCItemSize::Small;

	EDCItemShape Shape = EDCItemShape::Box;
	/** Size of the world representation in cm (X, Y, Z). */
	FVector ShapeSize = FVector(20.0);
	FLinearColor Color = FLinearColor::Gray;

	bool IsContainer() const { return CapacityLiters > 0.f; }
};

namespace DCItems
{
	/** All item kinds the game knows. */
	DERCLOU3_API const TArray<FDCItemDef>& GetAll();
	/** Definition by id; nullptr if there is none. */
	DERCLOU3_API const FDCItemDef* Find(FName Id);

	DERCLOU3_API FText DescribeSize(EDCItemSize Size);
	DERCLOU3_API FText DescribeSlot(EDCEquipSlot Slot);

	/** "2,5 kg", "12 L", "4.500 €" (German number format). */
	DERCLOU3_API FText FormatKg(float Kg);
	DERCLOU3_API FText FormatLiters(float Liters);
	DERCLOU3_API FText FormatEuro(int32 Euro);
}
