#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

/**
 * Formas y material básicos del motor para la vista de depuración (S-005). Ningún asset nuevo: solo /Engine/BasicShapes.
 * Los actores los piden con ConstructorHelpers::FObjectFinder en su constructor, para que el empaquetado los incluya.
 */
namespace Soulbound::Presentation
{
	inline constexpr const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	inline constexpr const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	inline constexpr const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	inline constexpr const TCHAR* ConePath = TEXT("/Engine/BasicShapes/Cone.Cone");
	inline constexpr const TCHAR* BasicMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	/** Nombre del parámetro de color del material básico del motor. */
	inline const FName ColorParameter(TEXT("Color"));

	/** Da al componente una instancia dinámica del material y le pone el color. Crea la instancia la primera vez. */
	inline void SetComponentColor(UStaticMeshComponent& Component, UMaterialInterface* BaseMaterial, const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component.GetMaterial(0));
		if (!Material && BaseMaterial)
		{
			Material = UMaterialInstanceDynamic::Create(BaseMaterial, &Component);
			Component.SetMaterial(0, Material);
		}
		if (Material)
		{
			Material->SetVectorParameterValue(ColorParameter, Color);
		}
	}
}
