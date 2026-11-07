#include "DCVisuals.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DCItemTypes.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
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

	UStaticMesh* ItemModel(FName ItemId)
	{
		// Looked up once per id; ids without a model stay null without warnings.
		static TMap<FName, TWeakObjectPtr<UStaticMesh>> Cache;
		static TSet<FName> Missing;
		if (ItemId.IsNone() || Missing.Contains(ItemId))
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(ItemId))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		const FString Name = TEXT("SM_") + ItemId.ToString();
		const FString Package = TEXT("/Game/DerClou/Items/Meshes/") + Name;
		UStaticMesh* Model = FPackageName::DoesPackageExist(Package) ? LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + Name)) : nullptr;
		if (!Model)
		{
			Missing.Add(ItemId);
			return nullptr;
		}
		Cache.Add(ItemId, Model);
		return Model;
	}

	bool SetItemLook(UStaticMeshComponent* Mesh, const FDCItemDef& Def)
	{
		if (!Mesh)
		{
			return false;
		}
		if (UStaticMesh* Model = ItemModel(Def.Id))
		{
			Mesh->SetStaticMesh(Model);
			Mesh->SetRelativeScale3D(FVector::OneVector);
			Mesh->EmptyOverrideMaterials();
			return true;
		}
		switch (Def.Shape)
		{
		case EDCItemShape::Box: Mesh->SetStaticMesh(Cube()); break;
		case EDCItemShape::Cylinder: Mesh->SetStaticMesh(Cylinder()); break;
		case EDCItemShape::Sphere: Mesh->SetStaticMesh(Sphere()); break;
		}
		// Basic shapes are 100 cm with the pivot in the centre.
		Mesh->SetRelativeScale3D(Def.ShapeSize / 100.0);
		Paint(Mesh, Def.Color);
		return false;
	}
}
