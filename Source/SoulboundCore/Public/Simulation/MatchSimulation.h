#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Match/WardenEvents.h"
#include "Warden/WardenParams.h"

class IDecider;

struct SOULBOUNDCORE_API FSimulationParams
{
	/**
	 * Semilla de la partida: inicio y dados. Los decisores reciben otro generador con semilla Seed + 1 y el cerebro del
	 * Warden otro con Seed + 2 (desde S-010, como en el juego), nunca el de la partida: la misma semilla da los mismos
	 * dados ocupe quien ocupe cada asiento.
	 */
	int32 Seed = 1;
	/** Duración de la partida en segundos virtuales (40 minutos). */
	double DurationSeconds = 40.0 * 60.0;
	/**
	 * Si el Warden juega (S-016): el cerebro evalúa en el instante que él mismo devuelve, con los eventos y los parámetros de
	 * abajo. Apagado, la partida no tiene Warden (como SbWardenMode off): así el test de determinismo de S-010 conserva sus
	 * tiradas (decisión del autor del 19/10/2026). Sin eventos o sin parámetros del Warden, tampoco juega.
	 */
	bool bWardenEnabled = true;
	/** Eventos del Warden ya cargados (LoadWardenEventsFromFile), los de Content/Data/Events/eventos.json. S-016. */
	FWardenEventTable WardenEvents;
	/** Parámetros del Warden ya cargados (LoadWardenParamsFromFile), los de Content/Data/Warden/warden.json. S-016. */
	FWardenParams WardenParams;
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

/** Cifras del Warden en un acto de una partida simulada (informe de S-016). Nombre propuesto en S-016. */
struct SOULBOUNDCORE_API FWardenActReport
{
	/** Golpes y escenas aplicados en el acto. */
	int32 EffectInterventions = 0;
	/** Turnos empezados en el acto. */
	int32 TurnsStarted = 0;
	/** Turnos empezados en el acto que cortó un golpe (TurnInterrupted). */
	int32 TurnsCut = 0;
	/** Hueco más largo sin golpe ni escena que empieza en el acto, en segundos (desde el inicio, o hasta el final). */
	double MaxGapSeconds = 0.0;
};

struct SOULBOUNDCORE_API FSimulationResult
{
	/** Registro de la partida, una línea por suceso. Dos ejecuciones con la misma semilla dan el mismo registro. */
	TArray<FString> Log;
	int32 Turns = 0;
	/** Evaluaciones del cerebro del Warden (WardenEvaluated), re-evaluaciones tras un aplazamiento incluidas. */
	int32 Evaluations = 0;
	/** Intervenciones con efecto del Warden: golpes y escenas aplicados (S-016). */
	int32 Interventions = 0;
	/** Cifras del Warden por acto (S-016), indexadas por EAct. */
	TArray<FWardenActReport> WardenActs;
	/** Furias del Warden en la partida (FuryUnleashed, R-INS-3, S-016). */
	int32 Furies = 0;
	/** Tiempo real que tardó la simulación. */
	double WallSeconds = 0.0;
	FMatchState FinalState;
};

/**
 * Simulación (glosario "Simulación"): una partida entera sin mundo ni red, con reloj virtual. Los mismos datos,
 * las mismas reglas y el mismo cerebro que en juego; solo cambia quién avanza el reloj. Inicia la partida con
 * StartMatch y juega los turnos con la máquina de turnos del motor (Match/TurnRules.h): la simulación consulta a
 * los decisores y avanza el reloj, pero no contiene reglas de turno. Desde S-016, el Warden real: el cerebro evalúa en el
 * instante que él mismo devuelve (EvaluateWarden, el mismo contrato que el anfitrión del Warden) y sus decisiones se aplican
 * con las reglas del motor; con bWardenEnabled apagado, la partida no tiene Warden.
 * La partida acaba al llegar a DurationSeconds o, antes, al terminar con victoria del Warden (R-CAR-8, S-006).
 * Los duelos (S-012) se resuelven al instante: los dos participantes rinden con su decisor y el botín es un trilero.
 * Tres sorteos separados, en simulación y en juego (decisión del autor del 05/10/2026, S-010): partida (inicio, dados y
 * sorteos de reglas) con Seed, decisores con Seed + 1 y Warden con Seed + 2, igual que UWardenHostSubsystem. Antes el
 * Warden de la simulación compartía el sorteo de la partida: un trilero, que dura tiempo de partida, cambiaba en qué
 * turno caía cada evaluación y con ello los dados.
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
