#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Warden/WardenBrain.h"
#include "WardenHostSubsystem.generated.h"

/**
 * Modo del anfitrión del Warden (S-016, nombre propuesto `WardenHostMode`): si el cerebro evalúa solo o está apagado para
 * probar otras cosas. Se cambia con SbWardenMode off|auto.
 */
UENUM(BlueprintType)
enum class EWardenHostMode : uint8
{
	/** Apagado: el cerebro no evalúa. SbEvent sigue lanzando eventos a mano. */
	Off,
	/** El cerebro evalúa en el instante que devolvió la evaluación anterior (R-WAR-1, R-WAR-3). */
	Auto,
};

/**
 * Anfitrión del Warden (glosario "WardenHost"): UWorldSubsystem que existe solo en mundos de juego y solo actúa
 * con autoridad (nunca en un cliente). Es dueño del ritmo de evaluación: desde S-016 no tiene un temporizador fijo, sino que
 * mira el reloj de partida y, cuando llega el instante que devolvió el cerebro (o termina lo que aplazó una evaluación), pide
 * al anfitrión de partida que evalúe y aplique (R-WAR-1). Con la partida detenida en el paso a paso no evalúa. No decide nada
 * por sí mismo ni tiene partida propia ni sorteo propio: el del Warden vive en el estado (FWardenState).
 */
UCLASS()
class SOULBOUND_API UWardenHostSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Cada cuántos segundos reales se mira si toca evaluar. No es el intervalo del Warden, que está en datos (warden.json). */
	static constexpr float PollSeconds = 0.1f;

	/** Modo del anfitrión (S-016): Auto al empezar. */
	EWardenHostMode GetMode() const { return Mode; }

	/** Cambia el modo (S-016, SbWardenMode). Apagarlo no toca el estado; al encenderlo, evalúa en cuanto toque. */
	void SetMode(EWardenHostMode InMode);

#if !UE_BUILD_SHIPPING
	/**
	 * Lanza el evento del Warden EventId entre turnos, como entrega directa (S-014, comando SbEvent): lo pide al anfitrión de
	 * partida, que lo aplica con el sorteo del Warden del estado (S-016). Devuelve lo que contestó el anfitrión de partida.
	 */
	struct FDebugEditOutcome DebugLaunchEvent(const FString& EventId);
#endif

private:
	/** Mira el reloj de partida y, si toca (IsWardenEvaluationDue), evalúa. */
	void Poll();

	FTimerHandle PollTimer;
	EWardenHostMode Mode = EWardenHostMode::Auto;
	int32 EvaluationCount = 0;
};
