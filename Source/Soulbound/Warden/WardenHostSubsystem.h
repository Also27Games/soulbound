#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Warden/WardenBrain.h"
#include "WardenHostSubsystem.generated.h"

/**
 * Anfitrión del Warden (glosario "WardenHost"): UWorldSubsystem que existe solo en mundos de juego y solo actúa
 * con autoridad (nunca en un cliente). Es dueño del reloj de evaluación, llama al cerebro sin mundo y aplica
 * lo que este decide (R-WAR-1). No decide nada por sí mismo. Spike, paso 4: estado de prueba y mensajes en pantalla.
 */
UCLASS()
class SOULBOUND_API UWardenHostSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Segundos entre evaluaciones (R-WAR-3: 15 s, más o menos 5 en el GDD; aquí fijo). */
	static constexpr float EvaluationPeriodSeconds = 15.f;

private:
	void Evaluate();

	FTimerHandle EvaluationTimer;
	FMatchState State;
	FRandomStream Rng;
	FWardenBrain Brain;
	int32 EvaluationCount = 0;
};
