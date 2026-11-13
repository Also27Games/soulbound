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
 * color por vértice) y un disco de color del rol bajo ella (S-029). La raíz de la
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

	/**
	 * Velocidad con la que la carta se orienta hacia donde va mientras recorre su camino (R-VIS-6), en grados por segundo. Corta,
	 * para que no dé un latigazo al cambiar de tramo. Valor inicial 540. Las formas de respaldo (esfera y cono) no tienen frente.
	 */
	UPROPERTY(EditAnywhere, Category = "Ficha")
	float TurnSpeedDegreesPerSecond = 540.f;

	/** Asiento al que representa. */
	void SetSeat(int32 InSeat) { Seat = InSeat; }
	int32 GetSeat() const { return Seat; }

	/** Pone el color del asiento: el del aro del turno y el del número. Sin carta, también el de la ficha hasta que llegue SetRoleColor. */
	void SetColor(const FLinearColor& Color);

	/**
	 * Color del rol (S-029, R-VIS-10): el gris de un Eco o el color del Dominio de un Elegido. Lo toma el disco de rol bajo la ficha (la carta del kit
	 * no se tiñe: es una malla con color por vértice) y, sin carta, la propia ficha. Sin cambio de color no repinta nada.
	 */
	void SetRoleColor(const FLinearColor& Color);

	/** Forma de Elegido (cono) o de Eco (esfera) de las formas de respaldo; con la carta no hace nada. */
	void SetElectShape(bool bElect);

	/**
	 * Pulso lento del aro del turno (S-029, R-VIS-11): de MinIntensity a 1 del color del asiento con el periodo dado; sin periodo, fijo.
	 * Se pone una vez al crear la ficha.
	 */
	void SetRingPulse(float PeriodSeconds, float MinIntensity);

	/**
	 * Destello (S-029, R-VIS-12): el disco de rol (la ficha, sin carta) y un aro grande de destello cambian a Color y vuelven en Seconds, con
	 * una subida y una bajada suaves. Empieza tras DelaySeconds y nunca antes de que acabe el que hubiera: los destellos se encadenan.
	 */
	void Flash(const FLinearColor& Color, float Seconds, float DelaySeconds);

	/**
	 * Parámetros de la presentación que da la vista: letra del número, elevación y giro del asiento activo, el aro del activo
	 * (diámetro y grosor), el crecimiento del activo y el diámetro de la ficha de formas básicas, en cm. Se llama una vez al
	 * crear la ficha.
	 */
	void Configure(float InLabelSize, float InLift, float InSpinDegreesPerSecond, float InRingDiameter, float InRingWidth, float InActiveScale, float InTokenDiameter);

	/**
	 * Cambia la forma básica por la carta del kit (S-019, R-VIS-6). Devuelve el componente de la carta para que la vista le ponga los materiales
	 * del kit. El disco de rol no usa ninguno de ellos: se pinta con el material básico (BaseMaterial, parámetro "Color"), como el aro del turno.
	 */
	UStaticMeshComponent* UseCard(UStaticMesh* Card);

	/** Huella de la ficha en cm: la de la carta del kit, o el diámetro de la ficha de formas básicas (S-029: de ella sale el disco de rol). */
	float GetFootprint() const { return Footprint; }

	/**
	 * El disco de rol (S-029, R-VIS-10, corrección del autor del 13/11/2026): un disco plano relleno bajo la ficha, de color gris de Eco o del Dominio
	 * del Elegido, de diámetro Diameter y con su cara inferior a DiscLift sobre la base de la ficha (la cara superior de la losa o del Altar); cuelga de la
	 * raíz, así que no sube ni gira con la ficha. Brightness multiplica el color. Se llama una vez al crear la ficha, con el diámetro de RoleDiscDiameter.
	 */
	void SetRoleDisc(float Diameter, float DiscLift, float Brightness);

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

	/** Asiento activo (o participante de un duelo o trilero entre turnos, S-029): se eleva, gira y enseña el aro, que pulsa. Inactivo, vuelve a su sitio sin giro y sin aro. */
	void SetActive(bool bInActive);

	/**
	 * Si la ficha recibe el clic (S-017: durante una votación, para votar). Fuera de ella no lo recibe, para no tapar la casilla que
	 * tiene debajo (decisión del autor del 26/10/2026). Lo recibe una esfera invisible del tamaño de la ficha: las piezas del kit no
	 * traen colisión.
	 */
	void SetClickable(bool bClickable);

private:
	/** Un aro de segmentos (las formas básicas del motor no traen un anillo) bajo Parent, a la altura Z. */
	void BuildRing(USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& OutSegments, float Diameter, float Width, float Z);
	/** Pinta el disco de rol (y la ficha sin carta) con el color del rol, o con el del destello en curso. */
	void PaintRoleDisc();
	/** Pinta el aro del turno con el color del asiento por su intensidad. */
	void PaintTurnRing(float Intensity);
	/** El destello en curso en Now: su color y su intensidad de 0 a 1; falso si no hay. */
	bool CurrentFlash(double Now, FLinearColor& OutColor, float& OutIntensity);

	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Esfera invisible que recibe el clic durante una votación (S-017, SetClickable). */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ClickTarget;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY()
	TObjectPtr<USceneComponent> RingRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;

	/** Disco de rol bajo la ficha (S-029, R-VIS-10): un cilindro básico aplastado, del color del rol. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RoleDisc;

	/** Aro grande del destello (S-029): oculto salvo mientras dura uno. */
	UPROPERTY()
	TObjectPtr<USceneComponent> FlashRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FlashSegments;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

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
	/** Color del rol (S-029) y si ya se ha puesto alguno. */
	FLinearColor RoleColor = FLinearColor::White;
	bool bRoleColorSet = false;
	float RingPulsePeriod = 0.f;
	float RingPulseMin = 0.35f;

	/** Un destello esperando o en curso, en segundos del mundo, y hasta cuándo está ocupada la ficha (S-029, encadenado). */
	struct FFlashSlot
	{
		double Start = 0.0;
		double End = 0.0;
		FLinearColor Color = FLinearColor::White;
	};
	TArray<FFlashSlot> FlashSlots;
	double FlashBusyUntil = 0.0;
	bool bFlashShown = false;
	float Lift = 2.5f;
	float ActiveScale = 1.3f;
	float CurrentScale = 1.f;
	float SpinDegreesPerSecond = 90.f;
	float RingDiameter = 8.5f;
	float RingWidth = 0.5f;
	float TokenDiameter = 2.2f;
	float CurrentLift = 0.f;
	float RoleDiscBrightness = 1.f;
	float Footprint = 2.2f;
	int32 Seat = INDEX_NONE;
	bool bActive = false;
	bool bCard = false;
	bool bElect = false;
};
