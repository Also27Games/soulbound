#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeatTokenActor.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Ficha de un asiento en pantalla (Token; nombre propuesto en S-005 para el glosario): una esfera básica del motor
 * con el color del asiento, que pasa a cono cuando el asiento es Elegido (S-006). Adaptador de presentación, sin
 * reglas: la coloca y le da forma la vista según el estado.
 */
UCLASS(NotBlueprintable)
class SOULBOUND_API ASeatTokenActor : public AActor
{
	GENERATED_BODY()

public:
	ASeatTokenActor();

	/** Asiento al que representa. */
	void SetSeat(int32 InSeat) { Seat = InSeat; }
	int32 GetSeat() const { return Seat; }

	/** Pinta la ficha. */
	void SetColor(const FLinearColor& Color);

	/** Forma de Elegido (cono) o de Eco (esfera). Conserva el color. */
	void SetElectShape(bool bElect);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> EchoMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ElectMesh;

	int32 Seat = INDEX_NONE;
};
