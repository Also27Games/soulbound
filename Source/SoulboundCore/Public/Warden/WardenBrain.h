#pragma once

#include "CoreMinimal.h"
#include "Warden/WardenParams.h"

struct FMatchState;
struct FRandomStream;

/** Qué decide el cerebro en una evaluación (R-WAR-1, S-016, nombre propuesto). Es el Detail del evento WardenDecided. */
enum class EWardenDecisionKind : uint8
{
	/** Nada: no interviene, o cae en el margen de un acto o de la partida (R-WAR-4d). */
	None,
	/** Golpe con evento (R-WAR-4): corta el turno en curso, si lo hay. */
	Strike,
	/** Escena con evento: entre turnos; con un turno en curso, espera al hueco, salvo que el evento corte. */
	Scene,
	/** Comentario, sin texto ni efecto (S-016). */
	Remark,
	/** Aplazada (R-WAR-7): un duelo, un trilero, un minijuego o un evento en curso; se vuelve a evaluar al terminar. */
	Deferred,
};

/** Lo que devuelve una evaluación (R-WAR-1, S-016): la decisión y el instante de la siguiente. Nombre propuesto en S-016. */
struct SOULBOUNDCORE_API FWardenDecision
{
	EWardenDecisionKind Kind = EWardenDecisionKind::None;

	/** Posición del evento en FMatchState::WardenEvents, en un golpe o una escena; INDEX_NONE en el resto. */
	int32 EventIndex = INDEX_NONE;

	/** Acto en que se evaluó (CurrentAct). */
	EAct Act = EAct::ActOne;

	/** Si la garantía obligó a intervenir con efecto (R-WAR-5). */
	bool bForcedByGuarantee = false;

	/**
	 * Instante de la siguiente evaluación (R-WAR-3): el intervalo de los datos más o menos su variación, sorteada. En una
	 * aplazada no cuenta: se vuelve a evaluar en cuanto termina lo que la aplazó.
	 */
	double NextEvaluationSeconds = 0.0;

	/**
	 * Si el golpe se entrega por el Oráculo (R-ORA-1, S-017): con entrega "oracle", siempre; con "any", a partes iguales con el
	 * sorteo del cerebro. Su decreto es la sentencia del cortado (decisión del autor del 23/10/2026).
	 */
	bool bViaOracle = false;

	/** Disparadores activos al evaluar (R-DIS-1, R-WAR-10): quedan en el registro tras WardenDecided (WardenTriggered). S-017. */
	TArray<FActiveTrigger> Triggers;

	/** Si es una intervención con efecto: golpe o escena. */
	bool IsIntervention() const { return Kind == EWardenDecisionKind::Strike || Kind == EWardenDecisionKind::Scene; }
};

/**
 * IA directora del Warden (glosario `WardenBrain`, R-WAR-1, S-016): clase sin mundo. Recibe el estado, el reloj de partida y
 * un sorteo; devuelve una decisión y el instante de la siguiente evaluación. No aplica nada: lo hace quien tiene la autoridad
 * (EvaluateWarden, en Warden/WardenRules.h), igual en el anfitrión y en la simulación. Los parámetros son los del estado
 * (FMatchState::WardenParams). Sustituye al cerebro del spike (Sellos sueltos).
 */
class SOULBOUNDCORE_API FWardenBrain
{
public:
	/**
	 * Evalúa en el instante NowSeconds (R-WAR-1, R-WAR-3 a R-WAR-8): aplaza durante un duelo, un trilero, un minijuego o un
	 * evento en curso; no inicia nada en el margen de un acto o de la partida; interviene con la probabilidad del acto o
	 * forzado por la garantía; elige canal por los pesos del acto y evento por el peso del acto entre los candidatos
	 * (EventCandidates); sin candidato en el canal, prueba el otro con efecto, y sin ninguno, comentario. Random: el sorteo
	 * del Warden; consume siempre los mismos valores. Determinista: mismo estado, reloj y sorteo, misma decisión. Con la
	 * partida terminada o sin Warden, nada.
	 */
	FWardenDecision Evaluate(const FMatchState& State, double NowSeconds, FRandomStream& Random) const;
};

namespace Soulbound::Rules
{
	/**
	 * Acto en el instante NowSeconds (R-WAR-2): el I mientras dura la carrera y antes del inicio del II; el fin de la carrera
	 * adelanta el II; el III desde su inicio. Es una función del estado, no un contador. Sin Warden, el I.
	 */
	SOULBOUNDCORE_API EAct CurrentAct(const FMatchState& State, double NowSeconds);

	/**
	 * Si NowSeconds cae en el margen (R-WAR-4d): los últimos FWardenParams::MarginSeconds antes del fin de su acto por tiempo
	 * (el inicio del siguiente) o del fin de la partida. El fin de la carrera no tiene margen: no se sabe cuándo llega.
	 */
	SOULBOUNDCORE_API bool IsInWardenMargin(const FMatchState& State, double NowSeconds);

	/**
	 * Si el Warden debe aplazar ahora (R-WAR-7): fase Duel, ShellGame o SkillTrial, un evento en curso, o MoveAndResolve con la
	 * casilla de destino aún por resolver (un instante: la ficha ya llegó y su casilla resuelve al avanzar).
	 */
	SOULBOUNDCORE_API bool IsWardenDeferred(const FMatchState& State);

	/**
	 * Eventos candidatos del canal Channel en NowSeconds (R-WAR-8, R-SAC-1), en el orden de la tabla: de los sacos del acto
	 * actual y los anteriores, de ese canal, con entrega directa o "any" (el Oráculo es de S-017), sin efectos sin implementar,
	 * dentro de su ventana, con usos y con peso en el acto, y con objetivos válidos para quien lo dirigiría (R-DIR-2, R-DIR-3):
	 * un decreto, si alguna aplicación tiene objetivo; un dilema, si al menos dos opciones lo tienen. Con un turno en curso que
	 * el evento cortaría, la validez se mira como quedaría tras el corte; el Alto solo vale cortando.
	 */
	SOULBOUNDCORE_API TArray<int32> EventCandidates(const FMatchState& State, EWardenChannel Channel, double NowSeconds);
}
