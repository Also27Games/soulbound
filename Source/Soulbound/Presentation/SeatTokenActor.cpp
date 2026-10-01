#include "SeatTokenActor.h"

#include "PresentationShapes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASeatTokenActor::ASeatTokenActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(Soulbound::Presentation::SpherePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(Soulbound::Presentation::ConePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(Soulbound::Presentation::BasicMaterialPath);
	EchoMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
	ElectMesh = Cone.Succeeded() ? Cone.Object : nullptr;
	Mesh->SetStaticMesh(EchoMesh);
	BaseMaterial = Material.Succeeded() ? Material.Object : nullptr;

	// La ficha no recibe clics ni choca con nada: el clic va a la casilla que hay debajo.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ASeatTokenActor::SetColor(const FLinearColor& Color)
{
	Soulbound::Presentation::SetComponentColor(*Mesh, BaseMaterial, Color);
}

void ASeatTokenActor::SetElectShape(bool bElect)
{
	UStaticMesh* Wanted = bElect ? ElectMesh : EchoMesh;
	// La instancia dinámica del material va en la ranura del componente, así que el color se conserva al cambiar de malla.
	if (Wanted && Mesh->GetStaticMesh() != Wanted)
	{
		Mesh->SetStaticMesh(Wanted);
	}
}
