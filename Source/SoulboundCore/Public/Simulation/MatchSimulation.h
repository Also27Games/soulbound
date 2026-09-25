#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Warden/WardenBrain.h"

class IDecider;

struct SOULBOUNDCORE_API FSimulationParams
{
	int32 Seed = 1;
	/** Duración de la partida en segundos virtuales (40 minutos). */
	double DurationSeconds = 40.0 * 60.0;
	/** Cada cuántos segundos virtuales evalúa el Warden (R-WAR-3: 15 s). */
	double EvaluationPeriodSeconds = 15.0;
	/** Cada cuántos segundos virtuales se juega un turno. */
	double TurnPeriodSeconds = 30.0;
};

struct SOULBOUNDCORE_API FSimulationResult
{
	/** Registro de la partida, una línea por suceso. Dos ejecuciones con la misma semilla dan el mismo registro. */
	TArray<FString> Log;
	int32 Turns = 0;
	int32 Evaluations = 0;
	int32 Interventions = 0;
	/** Tiempo real que tardó la simulación. */
	double WallSeconds = 0.0;
	FMatchState FinalState;
};

/**
 * Simulación (glosario "Simulación"): una partida entera sin mundo ni red, con reloj virtual. Los mismos datos,
 * las mismas reglas y el mismo cerebro que en juego; solo cambia quién avanza el reloj. Spike, paso 3:
 * reglas de mentira (mover fichas y sellar casillas); lo que se prueba es el bucle, el determinismo y el coste.
 */
class SOULBOUNDCORE_API FMatchSimulation
{
public:
	FMatchSimulation(const FSimulationParams& InParams, TArray<TSharedPtr<IDecider>> InDeciders);

	FSimulationResult Run();

private:
	FSimulationParams Params;
	TArray<TSharedPtr<IDecider>> Deciders;
};
