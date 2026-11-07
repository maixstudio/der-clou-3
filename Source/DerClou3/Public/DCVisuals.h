#pragma once

#include "CoreMinimal.h"

class UMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FDCItemDef;

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
	 * Text for labels in the world (TextRender): their font only has plain Latin-1, so dashes,
	 * German quotes, arrows and the ellipsis become plain characters.
	 */
	DERCLOU3_API FText PlainText(const FText& Text);
}
