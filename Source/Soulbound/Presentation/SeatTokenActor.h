#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeatTokenActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Ficha de un asiento en pantalla (Token; nombre propuesto en S-005 para el glosario): una esfera básica del motor
 * con el color del asiento, que pasa a cono cuando el asiento es Elegido (S-006). Lleva el número del asiento escrito
 * encima. Si es el asiento activo (S-007) se eleva, gira despacio y un aro del color del asiento (el SeatRing) rodea
 * su casilla. Adaptador de presentación, sin reglas: la coloca, le da forma y la activa la vista según el estado.
 */
UCLASS(NotBlueprintable)
class SOULBOUND_API ASeatTokenActor : public AActor
{
	GENERATED_BODY()

public:
	ASeatTokenActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Asiento al que representa. */
	void SetSeat(int32 InSeat) { Seat = InSeat; }
	int32 GetSeat() const { return Seat; }

	/** Pinta la ficha, su número y su aro. */
	void SetColor(const FLinearColor& Color);

	/** Forma de Elegido (cono) o de Eco (esfera). Conserva el color. */
	void SetElectShape(bool bElect);

	/**
	 * Parámetros de la presentación que da la vista: letra del número, elevación y giro del asiento activo, y el aro
	 * (diámetro y grosor, en cm). Se llama una vez al crear la ficha.
	 */
	void Configure(float InLabelSize, float InLift, float InSpinDegreesPerSecond, float InRingDiameter, float InRingWidth);

	/** Asiento activo: se eleva, gira y enseña el aro. Inactivo, vuelve a su sitio sin giro y sin aro. */
	void SetActive(bool bInActive);

private:
	void BuildRing();

	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY()
	TObjectPtr<USceneComponent> RingRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> EchoMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ElectMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	FLinearColor RingColor = FLinearColor::White;
	float Lift = 45.f;
	float SpinDegreesPerSecond = 90.f;
	float RingDiameter = 130.f;
	float RingWidth = 8.f;
	float CurrentLift = 0.f;
	int32 Seat = INDEX_NONE;
	bool bActive = false;
};
