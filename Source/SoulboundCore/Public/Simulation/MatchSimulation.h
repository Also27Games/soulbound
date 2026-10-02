#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Warden/WardenBrain.h"

class IDecider;

struct SOULBOUNDCORE_API FSimulationParams
{
	/**
	 * Semilla de la partida: inicio, dados y cerebro del Warden. Los decisores reciben otro generador con semilla
	 * Seed + 1 y nunca el de la partida: la misma semilla da los mismos dados ocupe quien ocupe cada asiento.
	 */
	int32 Seed = 1;
	/** Duración de la partida en segundos virtuales (40 minutos). */
	double DurationSeconds = 40.0 * 60.0;
	/** Cada cuántos segundos virtuales evalúa el Warden (R-WAR-3: 15 s). */
	double EvaluationPeriodSeconds = 15.0;
	/**
	 * Segundos virtuales que tarda un decisor en confirmar destino desde que tiene la tirada. Un dummy responde al
	 * instante; este retardo da a la partida turnos de duración creíble. Valor inicial 5 s (S-003).
	 */
	double DummyResponseSeconds = 5.0;
	/** Tablero ya cargado y validado (LoadBoardFromFile). La simulación no carga archivos. */
	FBoardState Board;
	/** Parámetros de partida ya cargados (LoadMatchParamsFromFile). */
	FMatchParams MatchParams;
	/** Los seis Elegidos ya cargados (LoadElectsFromFile). Obligatorios, como en StartMatch (S-008). */
	FElectsTable Elects;
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
 * las mismas reglas y el mismo cerebro que en juego; solo cambia quién avanza el reloj. Inicia la partida con
 * StartMatch y juega los turnos con la máquina de turnos del motor (Match/TurnRules.h): la simulación consulta a
 * los decisores y avanza el reloj, pero no contiene reglas de turno. El cerebro del Warden sigue siendo el del spike.
 * La partida acaba al llegar a DurationSeconds o, antes, al terminar con victoria del Warden (R-CAR-8, S-006).
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
