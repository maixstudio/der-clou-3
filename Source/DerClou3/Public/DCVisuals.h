#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class UMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
struct FDCItemDef;

/** A modelled crew member (Art/Characters in Blender): mesh and the animations the game plays. */
struct FDCCrewModel
{
	USkeletalMesh* Mesh = nullptr;
	UAnimSequence* Idle = nullptr;
	UAnimSequence* Walk = nullptr;
	UAnimSequence* Jog = nullptr;
	UAnimSequence* Work = nullptr;
	UAnimSequence* PickUp = nullptr;
};

/** Small helpers for graybox visuals built from the engine's basic shapes. */
namespace DCVisuals
{
	/** Engine cube / cylinder / sphere (100 cm). */
	DERCLOU3_API UStaticMesh* Cube();
	DERCLOU3_API UStaticMesh* Cylinder();
	DERCLOU3_API UStaticMesh* Sphere();

	/** Gives the mesh a plain colour (BasicShapeMaterial with its "Color" parameter). */
	DERCLOU3_API void Paint(UMeshComponent* Mesh, const FLinearColor& Color);

	/**
	 * Modelled item mesh /Game/DerClou/Items/Meshes/SM_<Id> (built by Art/Items in Blender), or null
	 * when there is none yet. Its origin is the bottom centre, sized in real centimetres.
	 */
	DERCLOU3_API UStaticMesh* ItemModel(FName ItemId);

	/**
	 * Shows the item on the component: the model with its own materials, otherwise the coloured
	 * graybox shape. Sets mesh, scale and materials, not the location. Returns true for a model
	 * (origin at the bottom), false for a shape (origin in the middle).
	 */
	DERCLOU3_API bool SetItemLook(UStaticMeshComponent* Mesh, const FDCItemDef& Def);

	/**
	 * Modelled crew member /Game/DerClou/Characters/<Name>/SK_<Name> with A_<Name>_Idle, _Walk, _Jog, _Work and
	 * _Pickup (imported by Art/Characters/ue_import_character.py). False when there is no model for the name; missing
	 * animations stay null. Origin between the feet, front along +Y.
	 */
	DERCLOU3_API bool CrewModel(const FString& CrewName, FDCCrewModel& Out);

	/**
	 * Text for labels in the world (TextRender): their font only has plain Latin-1, so dashes,
	 * German quotes, arrows and the ellipsis become plain characters.
	 */
	DERCLOU3_API FText PlainText(const FText& Text);
}
