#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Warden/WardenBrain.h"
#include "WardenHostSubsystem.generated.h"

/**
 * Anfitrión del Warden (glosario "WardenHost"): UWorldSubsystem que existe solo en mundos de juego y solo actúa
 * con autoridad (nunca en un cliente). Es dueño del reloj de evaluación, llama al cerebro sin mundo y pide al
 * anfitrión de partida que aplique lo que este decide (R-WAR-1). No decide nada por sí mismo ni tiene partida
 * propia: evalúa sobre el estado del anfitrión de partida, que es el único que lo escribe (S-005).
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

	/** Segundos entre evaluaciones (R-WAR-3: 15 s, más o menos 5 en el GDD; aquí fijo). */
	static constexpr float EvaluationPeriodSeconds = 15.f;

#if !UE_BUILD_SHIPPING
	/**
	 * Lanza el evento del Warden EventId entre turnos, como entrega directa (S-014, comando SbEvent): pide al anfitrión de
	 * partida que lo aplique con el sorteo del Warden, el de este anfitrión. Hasta que el cerebro del Warden elija eventos
	 * (S-016), es la única vía para lanzarlos en juego. Devuelve lo que contestó el anfitrión de partida.
	 */
	struct FDebugEditOutcome DebugLaunchEvent(const FString& EventId);
#endif

private:
	void Evaluate();

	/** Siembra el sorteo del Warden con la semilla de la partida más 2 (+1 es la de los decisores), la primera vez. */
	void SeedFrom(const class UMatchHostSubsystem& Match);

	FTimerHandle EvaluationTimer;
	FRandomStream Rng;
	int32 EvaluationCount = 0;
	bool bSeeded = false;
};
