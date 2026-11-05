#pragma once

#include "CoreMinimal.h"

class UMeshComponent;
class UStaticMesh;

/** Small helpers for graybox visuals built from the engine's basic shapes. */
namespace DCVisuals
{
	/** Engine cube / cylinder / sphere (100 cm). */
	DERCLOU3_API UStaticMesh* Cube();
	DERCLOU3_API UStaticMesh* Cylinder();
	DERCLOU3_API UStaticMesh* Sphere();

	/** Gives the mesh a plain colour (BasicShapeMaterial with its "Color" parameter). */
	DERCLOU3_API void Paint(UMeshComponent* Mesh, const FLinearColor& Color);
}
