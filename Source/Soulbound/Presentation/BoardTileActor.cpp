#include "BoardTileActor.h"

#include "PresentationShapes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABoardTileActor::ABoardTileActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(Soulbound::Presentation::CylinderPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(Soulbound::Presentation::CubePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	RoundMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	SquareMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	Mesh->SetStaticMesh(RoundMesh);
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;

	// Solo el trazado del ratón (canal de visibilidad) la ve: no estorba a nada más.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCastShadow(false);
}

void ABoardTileActor::SetColor(const FLinearColor& Color)
{
	Soulbound::Presentation::SetComponentColor(*Mesh, BaseMaterial, Color);
}

void ABoardTileActor::SetSquareShape(bool bSquare)
{
	UStaticMesh* Wanted = bSquare ? SquareMesh : RoundMesh;
	if (Wanted && Mesh->GetStaticMesh() != Wanted)
	{
		Mesh->SetStaticMesh(Wanted);
	}
}
