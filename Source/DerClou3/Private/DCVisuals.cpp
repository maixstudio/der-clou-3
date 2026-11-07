#include "DCVisuals.h"

#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace DCVisuals
{
	UStaticMesh* Cube()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	UStaticMesh* Cylinder()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}

	UStaticMesh* Sphere()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}

	FText PlainText(const FText& Text)
	{
		FString Plain = Text.ToString();
		Plain.ReplaceInline(TEXT("–"), TEXT("-"));
		Plain.ReplaceInline(TEXT("—"), TEXT("-"));
		Plain.ReplaceInline(TEXT("„"), TEXT("\""));
		Plain.ReplaceInline(TEXT("“"), TEXT("\""));
		Plain.ReplaceInline(TEXT("”"), TEXT("\""));
		Plain.ReplaceInline(TEXT("→"), TEXT("->"));
		Plain.ReplaceInline(TEXT("…"), TEXT("..."));
		return FText::FromString(Plain);
	}

	void Paint(UMeshComponent* Mesh, const FLinearColor& Color)
	{
		// The basic shapes default to DefaultMaterial, which has no colour parameter.
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Mesh || !Base)
		{
			return;
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Mesh);
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Mesh->SetMaterial(0, Instance);
	}
}
