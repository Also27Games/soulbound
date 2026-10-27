#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Match/WardenEvents.h"
#include "Warden/WardenParams.h"

class IDecider;

/**
 * Ritmo de la simulación (S-018, decisión 1 del autor; nombre propuesto): cuántos segundos virtuales tarda un decisor en cada cosa. Es un
 * parámetro del doble de prueba, no un dato de juego: va en código, con dos presets con nombre (decisión del autor del 27/10/2026). Cada
 * respuesta llega a su hora o a la mitad de lo que queda de su plazo, lo que ocurra antes (decisión del autor del 27/10/2026): así un
 * dummy nunca agota un plazo ni es insolente, sea cual sea el ritmo. Un tiempo de 0 es "en el acto", como hasta S-018.
 */
struct SOULBOUNDCORE_API FSimulationPace
{
	/** Confirmar destino desde que tiene la tirada, y su variación (uniforme, ± segundos). */
	double DestinationSeconds = 5.0;
	double DestinationJitterSeconds = 0.0;
	/** Contestar una elección pendiente (dados, repetir, salto, acción, trilero, Desvío, las de un evento), y su variación. */
	double ChoiceSeconds = 0.0;
	double ChoiceJitterSeconds = 0.0;
	/** Entregar el resultado de una prueba de habilidad (Fuente, Vacío, Prueba del Warden, desafío final). */
	double TrialSeconds = 0.0;
	/** Entregar los dos rendimientos de un duelo. */
	double DuelSeconds = 0.0;
	/** Consultar el Oráculo (o rechazar, Tifón). */
	double OracleSeconds = 0.0;
	/** Votar, todos a la vez, en orden de asiento. */
	double VoteSeconds = 0.0;

	/** Ritmo rápido, el de siempre: destino a los 5 s (S-003) y todo lo demás en el acto. El de los tests automáticos. */
	static FSimulationPace Fast();

	/**
	 * Ritmo humano: destino 12 s ± 4, elección 6 s ± 2, prueba y duelo 8 s, consulta 3 s, voto 4 s. Valores iniciales de la story, para
	 * ajustar cuando el test de usabilidad diga cuánto tarda una persona.
	 */
	static FSimulationPace Human();

	/** Si alguna espera tiene variación: solo entonces se saca algo del generador del ritmo. */
	bool HasJitter() const { return DestinationJitterSeconds > 0.0 || ChoiceJitterSeconds > 0.0; }
};

struct SOULBOUNDCORE_API FSimulationParams
{
	/**
	 * Semilla de la partida: inicio y dados. Los decisores reciben otro generador con semilla Seed + 1 y el cerebro del
	 * Warden otro con Seed + 2 (desde S-010, como en el juego), nunca el de la partida: la misma semilla da los mismos
	 * dados ocupe quien ocupe cada asiento.
	 */
	int32 Seed = 1;
	/**
	 * Duración de la partida en segundos virtuales (40 minutos). Con Warden, desde S-018 la partida termina antes por sí sola, en
	 * matchEndSeconds (R-VIC-4) o con la victoria de un jugador; sin Warden, solo con esa victoria o al llegar aquí.
	 */
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
	 * Ritmo de los decisores (S-018): cuánto tardan en cada cosa. Por defecto el rápido (destino a los 5 s, S-003, y lo demás en el acto),
	 * que usan los tests automáticos. Sustituye a DummyResponseSeconds. La variación del humano sale de un generador propio con semilla
	 * Seed + 4 (Seed + 3 es la del reparto de hogares); el rápido no saca nada de él.
	 */
	FSimulationPace Pace = FSimulationPace::Fast();
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
	/**
	 * Golpes y escenas que decide el cerebro en el acto, la medida del pilar 1: los lanzados (WardenEventStarted) y, desde S-017,
	 * los golpes por el Oráculo al abrir su consulta (OracleOpened con origen golpe). Las sentencias de un Nodo o de una Consulta
	 * forzada no cuentan: van en SentencesByOrigin (decisión del autor del 23/10/2026).
	 */
	int32 EffectInterventions = 0;
	/** Turnos empezados en el acto. */
	int32 TurnsStarted = 0;
	/** Turnos empezados en el acto que cortó un golpe (TurnInterrupted). */
	int32 TurnsCut = 0;
	/** Hueco más largo sin golpe ni escena que empieza en el acto, en segundos (desde el inicio, o hasta el final). */
	double MaxGapSeconds = 0.0;
	/** Sentencias del Oráculo reveladas en el acto (OracleSentence), las segundas de Tifón incluidas (S-017). */
	int32 Sentences = 0;
	/** Las mismas sentencias por el origen de su consulta, indexadas por EOracleOrigin: Nodo, forzada y golpe (S-017). */
	TArray<int32> SentencesByOrigin;
	/** Consultas del acto que acabaron en insolencia: plazo agotado sin consultar (OracleIgnored, S-017). Con dummies, 0. */
	int32 IgnoredConsults = 0;
	/** Veces que estuvo activo cada disparador en el acto (WardenTriggered), indexadas por EWardenTrigger (S-017). */
	TArray<int32> TriggerCounts;
	/** Votaciones resueltas en el acto (VoteResolved), las que acabaron en desempate y los votos al azar de quien no votó (S-017). */
	int32 Votes = 0;
	int32 VoteTies = 0;
	int32 RandomVotes = 0;
};

struct SOULBOUNDCORE_API FSimulationResult
{
	/** Registro de la partida, una línea por suceso. Dos ejecuciones con la misma semilla dan el mismo registro. */
	TArray<FString> Log;
	int32 Turns = 0;
	/** Evaluaciones del cerebro del Warden (WardenEvaluated), re-evaluaciones tras un aplazamiento incluidas. */
	int32 Evaluations = 0;
	/** Intervenciones con efecto del Warden: golpes y escenas que decide el cerebro (S-016; desde S-017, sin las sentencias de Nodo y forzadas). */
	int32 Interventions = 0;
	/** Cifras del Warden por acto (S-016), indexadas por EAct. */
	TArray<FWardenActReport> WardenActs;
	/** Furias del Warden en la partida (FuryUnleashed, R-INS-3, S-016). */
	int32 Furies = 0;
	/** Turnos que cortó el fin de la carrera (TurnInterrupted con Detail 1, R-PRO-0, S-017): 0 o 1 por partida. */
	int32 RaceEndCuts = 0;
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
 * La partida acaba al llegar a DurationSeconds o, antes, al terminar con victoria del Warden (R-CAR-8, S-006). Desde S-018, también con
 * la victoria de un jugador (R-VIC-2) y, con Warden, en matchEndSeconds (R-VIC-4): la simulación despierta en ese instante, antes que
 * cualquier otro suceso a su hora, y llama a la máquina de turnos, que corta lo que haya en marcha. El desafío final lo resuelve el decisor
 * del aspirante al instante, como el resto de minijuegos.
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
