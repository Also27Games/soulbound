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
 * La ficha de un asiento ha llegado a una casilla de su recorrido (S-020): asiento, casilla y si es la última. La vista lo
 * convierte en la intención de avance de ese asiento, humano o dummy.
 */
DECLARE_DELEGATE_ThreeParams(FOnTokenTileEntered, int32 /*Seat*/, int32 /*TileId*/, bool /*bLast*/);

/**
 * Ficha de un asiento en pantalla (Token; nombre propuesto en S-005 para el glosario): una esfera básica del motor
 * con el color del asiento, que pasa a cono cuando el asiento es Elegido (S-006). Lleva el número del asiento escrito
 * encima. Si es el asiento activo (S-007) se eleva, gira despacio y un aro del color del asiento (el SeatRing) rodea
 * su casilla. Desde S-019, con la carta de prueba del kit si está (la pieza no se escala ni se tiñe: es una sola malla con
 * color por vértice) y un aro del color del asiento en su base: tenue para un Eco, vivo para un Elegido. La raíz de la
 * ficha es su base, sobre la losa. Desde S-020 recorre el camino de su movimiento casilla a casilla (WalkPath) y avisa al
 * entrar en cada una. Adaptador de presentación, sin reglas: la coloca, le da forma y la activa la vista.
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

	/** Pinta la ficha (sin carta), su número, el aro del activo y el aro del rol. */
	void SetColor(const FLinearColor& Color);

	/** Forma de Elegido (cono) o de Eco (esfera); con la carta, el aro del rol vivo o tenue. Conserva el color. */
	void SetElectShape(bool bElect);

	/**
	 * Parámetros de la presentación que da la vista: letra del número, elevación y giro del asiento activo, el aro del activo
	 * (diámetro y grosor), el crecimiento del activo y el diámetro de la ficha de formas básicas, en cm. Se llama una vez al
	 * crear la ficha.
	 */
	void Configure(float InLabelSize, float InLift, float InSpinDegreesPerSecond, float InRingDiameter, float InRingWidth, float InActiveScale, float InTokenDiameter);

	/**
	 * Cambia la forma básica por la carta del kit (S-019, R-VIS-6) y le pone el aro del rol en la base: su diámetro es
	 * RoleRingScale por la huella de la carta, su grosor RoleRingWidth (cm), y un Eco lo lleva con EchoBrightness del brillo
	 * de un Elegido. RoleRingMaterial necesita un parámetro de color "Color". Devuelve el componente de la carta para que la
	 * vista le ponga los materiales del kit.
	 */
	UStaticMeshComponent* UseCard(UStaticMesh* Card, UMaterialInterface* RoleRingMaterial, float RoleRingScale, float RoleRingWidth, float EchoBrightness);

	/** Enseña u oculta el número del asiento (la tecla de las capturas, S-019). */
	void SetLabelVisible(bool bVisible);

	/**
	 * Lleva la ficha a Location deslizándola en línea recta desde donde esté, en SlideSeconds. La primera vez, o con
	 * SlideSeconds a 0, la coloca sin deslizar. Location es la base de la ficha, sobre la losa. Es solo presentación: el
	 * estado ya está en el destino.
	 */
	void MoveTo(const FVector& Location, float SlideSeconds);

	/**
	 * Recorre el camino Tiles (S-020, R-TAB-26): desde donde esté, en línea recta hasta Points[0], después hasta Points[1]... (las
	 * bases sobre las losas, por donde van los senderos), a velocidad constante Speed (cm por segundo): cada tramo dura su
	 * longitud entre Speed, así el tramo largo del Altar no sale disparado. Al llegar a cada una avisa por OnTileEntered; un punto con casilla INDEX_NONE es solo de paso (el círculo del Altar, R-VIS-6): no avisa.
	 * Sustituye al recorrido o al deslizamiento que hubiera. Es solo presentación: quien decide qué pasa es el motor.
	 */
	void WalkPath(const TArray<int32>& Tiles, const TArray<FVector>& Points, float Speed);

	/** Si está recorriendo un camino. */
	bool IsWalking() const { return WalkIndex < WalkTiles.Num(); }

	/** Casillas del recorrido a las que aún no ha llegado, en orden y sin los puntos de paso; vacío si no recorre nada. */
	TArray<int32> RemainingWalk() const;

	/** Deja de recorrer el camino donde esté; la vista la coloca después donde diga el estado. */
	void StopWalk();

	/** Aviso de llegada a cada casilla del recorrido (S-020). */
	FOnTokenTileEntered OnTileEntered;

	/** Asiento activo: se eleva, gira y enseña el aro. Inactivo, vuelve a su sitio sin giro y sin aro. */
	void SetActive(bool bInActive);

private:
	/** Un aro de segmentos (las formas básicas del motor no traen un anillo) bajo Parent, a la altura Z. */
	void BuildRing(USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& OutSegments, float Diameter, float Width, float Z);
	void PaintRoleRing();

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

	/** Aro del rol en la base de la carta (S-019); vacío sin carta. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> RoleRingSegments;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> RoleRingMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> EchoMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ElectMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	/** Duración del tramo en curso del recorrido: su longitud entre WalkSpeed. */
	float WalkSegmentSeconds() const;

	/** Recorrido en curso (S-020): casillas, sus puntos, por cuál va, cuánto lleva del tramo y a qué velocidad (cm/s) va. */
	TArray<int32> WalkTiles;
	TArray<FVector> WalkPoints;
	int32 WalkIndex = 0;
	FVector WalkSegmentStart = FVector::ZeroVector;
	float WalkElapsed = 0.f;
	float WalkSpeed = 13.f;
	/** Sube con cada recorrido nuevo o cortado: si cambia durante un aviso, el aviso cambió el recorrido y Tick no sigue con el viejo. */
	int32 WalkGeneration = 0;

	FVector SlideStart = FVector::ZeroVector;
	FVector SlideTarget = FVector::ZeroVector;
	float SlideDuration = 0.f;
	float SlideElapsed = 0.f;
	bool bPlaced = false;
	FLinearColor RingColor = FLinearColor::White;
	float Lift = 2.5f;
	float ActiveScale = 1.3f;
	float CurrentScale = 1.f;
	float SpinDegreesPerSecond = 90.f;
	float RingDiameter = 8.5f;
	float RingWidth = 0.5f;
	float TokenDiameter = 2.2f;
	float CurrentLift = 0.f;
	float EchoBrightness = 0.5f;
	int32 Seat = INDEX_NONE;
	bool bActive = false;
	bool bCard = false;
	bool bElect = false;
};
