#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

struct FRandomStream;

namespace Soulbound::Dice
{
	/** Caras de un dado (R-TUR-5: los dados son de seis caras). Definición de dado, no un valor de ajuste. */
	inline constexpr int32 Faces = 6;

	/** Dados que tira todo jugador por defecto (R-TUR-5; dos solo con Celeridad, R-TUR-4, GDD de runas). */
	inline constexpr int32 DefaultCount = 1;

	/**
	 * Máximo de dados de una tirada (R-TUR-4: uno o dos con Celeridad). Es también lo que consume del sorteo cada tirada
	 * y cada repetición, se tiren uno o dos (decisión del autor del 04/10/2026). Definición de regla, no un valor de ajuste.
	 */
	inline constexpr int32 MaxCount = 2;
}

/** Qué pasó con una intención de destino (glosario "Intención"). Solo Accepted cambia el estado. Nombre propuesto en S-003. */
enum class EIntentResult : uint8
{
	/** Válida: la ficha se mueve y el turno pasa a MoveAndResolve. */
	Accepted,
	/** No es el asiento al que le toca. */
	WrongSeat,
	/** El turno no está esperando destino (fase distinta de RollAndChoose). */
	WrongPhase,
	/** La casilla no está entre las alcanzables con la tirada (R-TUR-7). */
	Unreachable,
	/** Llega en el límite del reloj de turno o después (R-TUR-8). El agotamiento lo aplica AdvanceTurn. */
	ClockExpired,
	/** La opción no está entre las de la elección pendiente, o es "no" en una elección que no lo admite (S-009). */
	InvalidOption,
};

/**
 * Máquina de turnos (R-TUR-1 a R-TUR-15, GDD 02). El tiempo es un dato: las funciones reciben el instante actual
 * en segundos del reloj de partida y nunca leen un reloj (ADR-001). El motor no conoce a los decisores: humano,
 * bot o dummy envían su destino por SubmitDestinationIntent y sus elecciones por SubmitChoiceIntent (S-009), y quien los consulta (simulación o anfitrión) avanza
 * el turno con AdvanceTurn (R-TUR-14).
 */
namespace Soulbound::Rules
{
	/**
	 * Tira DiceCount dados de seis caras con el sorteo de la partida y devuelve la suma (R-TUR-5).
	 * Random: el FRandomStream de la partida; misma semilla, misma tirada. DiceCount: de 1 en adelante; con menos,
	 * devuelve 0 sin tocar Random.
	 */
	SOULBOUNDCORE_API int32 RollDice(FRandomStream& Random, int32 DiceCount);

	/**
	 * Avanza el turno una fase si no hay una decisión pendiente (R-TUR-2):
	 *  - BetweenTurns → Start: salta a los asientos con turnos perdidos (R-TUR-10, evento TurnSkipped), empieza el
	 *    turno del siguiente, arranca su reloj (R-TUR-8) y registra TurnStarted.
	 *  - Start → PreRoll: con Celeridad (R-TUR-4) abre la elección de dados (DiceCount), con el plazo del reloj de turno;
	 *    sin ella, un dado sin elección.
	 *  - En PreRoll con la elección de dados abierta: espera la intención (SubmitChoiceIntent); con el reloj de turno
	 *    agotado, registra TurnClockExpired, sube la insolencia y pasa a End sin tirar (R-TUR-8).
	 *  - PreRoll → RollAndChoose: tira los dados del turno (R-TUR-5), calcula las casillas alcanzables y registra Rolled.
	 *    Con la afinidad de Apep abre la oferta de repetir (Reroll, R-TUR-6), con el plazo del reloj de turno.
	 *  - En RollAndChoose: sin casillas alcanzables, pasa a MoveAndResolve sin mover (R-TAB-18), salvo con la oferta de
	 *    repetir abierta, que se espera (agotada, ChoiceExpired, sin castigo); con el reloj agotado, registra
	 *    TurnClockExpired, sube la insolencia y pasa a End sin mover (R-TUR-8); si no, espera la intención.
	 *  - MoveAndResolve: resuelve el Nicho de destino (R-CAR-4, ResolveNicheLanding) y la Fuente (R-RUN-2,
	 *    ResolvePowerSourceLanding, solo si el asiento se ha movido). Si un Elegido llega a la Fuente de un Dominio
	 *    Olvidado, pasa a SkillTrial. Después (R-INT-1) ofrece el salto planar (PlanarJump) y, como mucho, una acción
	 *    contra un jugador (Expel), cada uno con el reloj de elección; sin nada que ofrecer, pasa a End y registra
	 *    TurnEnded. Con una elección abierta, la espera; agotado su plazo, registra ChoiceExpired y la resuelve como "no".
	 *  - SkillTrial: espera el resultado del minijuego (SubmitSkillTrialResult); no avanza ni cambia nada.
	 * Todo turno que llega a PreRoll consume del sorteo cuatro valores de dado, dos de la tirada y dos de la repetición,
	 * se elijan uno o dos dados, se repita o no y aunque el reloj se agote antes de tirar (decisión del autor del
	 * 04/10/2026): así los dados de una semilla no dependen de los decisores.
	 *  - End → BetweenTurns: si el asiento tiene turnos extra, consume uno y vuelve a jugar (R-TUR-9); si no, pasa al
	 *    siguiente del orden de turno. Con el tiempo de carrera agotado, la carrera termina aquí (R-CAR-1, R-TAB-27).
	 *  - En BetweenTurns con el tiempo de carrera agotado, la carrera termina antes de empezar otro turno.
	 * Al empezar un turno y al terminarlo, los turnos perdidos se descuentan contra los extra uno a uno (caso límite
	 * del GDD 02), así un turno extra concedido durante el turno también se compensa.
	 * NowSeconds: instante actual del reloj de partida; es la marca de tiempo de los eventos.
	 * Random: el sorteo de la partida (solo lo usa la tirada).
	 * Devuelve true si cambió el estado; false si espera una intención (destino, elección o minijuego), si la partida no
	 * tiene orden de turno o si la partida terminó (R-CAR-8): entonces no cambia nada.
	 */
	SOULBOUNDCORE_API bool AdvanceTurn(FMatchState& State, double NowSeconds, FRandomStream& Random);

	/**
	 * Intención de destino del asiento Seat (R-TUR-7): la única forma de mover una ficha en un turno.
	 * Se valida asiento, fase, reloj y casilla; si no es válida se descarta sin castigo y sin tocar el estado.
	 * Aceptada, cierra la oferta de repetir si estaba abierta (R-TUR-6).
	 * TileId: casilla elegida. NowSeconds: instante en que llega la intención.
	 * Devuelve Accepted si la ficha se movió y el turno pasó a MoveAndResolve, o el motivo del rechazo.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitDestinationIntent(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds);

	/**
	 * Resultado del minijuego de habilidad del asiento Seat (R-TAB-7), la única forma de salir de SkillTrial, como la
	 * intención de destino lo es de RollAndChoose. Si se supera, el asiento obtiene la runa de la Fuente; si no, no. En
	 * ambos casos el turno sigue en MoveAndResolve con el salto planar y la acción contra un jugador (R-INT-1, S-009), o
	 * pasa a End si no hay nada que ofrecer. Sin plazo en S-008 (TODO(R-MIN) en TurnRules.cpp).
	 * bPassed: si superó la prueba. NowSeconds: marca de tiempo de los eventos.
	 * Devuelve Accepted, o WrongSeat o WrongPhase (también sin prueba pendiente, PendingTrial None) sin tocar el estado. Nombre propuesto en S-008.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitSkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds);

	/**
	 * Intención que contesta la elección pendiente del asiento Seat (S-009; glosario "Intención"): la única forma de
	 * elegir dados, repetir, saltar o expulsar. Se valida asiento, tipo de elección (Kind debe ser el de la pendiente:
	 * una respuesta tardía no contesta otra elección), plazo y opción; si no es válida se descarta sin castigo y sin
	 * tocar el estado. Option: una de FPendingChoice::Options, o INDEX_NONE para "no" si la elección lo admite.
	 * Efectos al aceptarla:
	 *  - DiceCount: fija los dados del turno y registra DiceChosen; la tirada la hace AdvanceTurn.
	 *  - Reroll: tira de nuevo con los mismos dados, recalcula las alcanzables y registra Rerolled (R-TUR-6); "no" cierra
	 *    la oferta. En los dos casos se sacan del sorteo los dos valores de la repetición.
	 *  - PlanarJump: la ficha salta a la casilla y se registra PlanarJumped (R-POD-5); después se ofrece la acción.
	 *  - Expel: el asiento elegido vuelve a su hogar y se registra Expelled (R-POD-4); después termina el turno.
	 * NowSeconds: instante en que llega. Random: el sorteo de la partida (solo lo usa la repetición).
	 * Devuelve Accepted, WrongSeat, WrongPhase (no hay elección pendiente de ese tipo), ClockExpired o InvalidOption.
	 * Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitChoiceIntent(FMatchState& State, int32 Seat, EChoiceKind Kind, int32 Option, double NowSeconds, FRandomStream& Random);

	/**
	 * Concede Count turnos extra al asiento Seat (R-TUR-9), sin pasar del tope de los parámetros de partida.
	 * Devuelve false, sin cambiar nada, si el asiento no existe o Count no es positivo.
	 */
	SOULBOUNDCORE_API bool GrantExtraTurns(FMatchState& State, int32 Seat, int32 Count);

	/**
	 * Impone Count turnos perdidos al asiento Seat (R-TUR-10), sin pasar del tope de los parámetros de partida.
	 * Devuelve false, sin cambiar nada, si el asiento no existe o Count no es positivo.
	 */
	SOULBOUNDCORE_API bool GrantLostTurns(FMatchState& State, int32 Seat, int32 Count);

	/**
	 * Sube un nivel la insolencia del asiento Seat, sin pasar de Soulbound::Match::MaxInsolence.
	 * El castigo de cada nivel y la Furia son de la story del Warden. Devuelve false si el asiento no existe.
	 */
	SOULBOUNDCORE_API bool RaiseInsolence(FMatchState& State, int32 Seat);
}
