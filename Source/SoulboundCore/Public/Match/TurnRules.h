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
	 *    contra un jugador (PlayerAction: retar a duelo, expulsar o robar; S-010, S-012), cada uno con el reloj de
	 *    elección; sin nada que ofrecer, pasa a End y registra TurnEnded. Con una elección abierta, la espera; agotado su
	 *    plazo, registra ChoiceExpired y la resuelve como "no".
	 *  - SkillTrial: espera el resultado del minijuego (SubmitSkillTrialResult); no avanza ni cambia nada.
	 *  - Duel (S-012): espera el rendimiento de los dos participantes (SubmitDuelPerformance); no avanza ni cambia nada.
	 *  - ShellGame (S-010, R-ROB-2): el trilero. Espera la runa de Koschei (agotada, ChoiceExpired y se juega con todas,
	 *    R-ROB-5), el fin de la muestra y el de la mezcla, y la elección de esfera del ladrón (agotada, ShellGameExpired y
	 *    no roba, R-ROB-3); después pasa a End. El trilero no toca el sorteo de la partida (R-ROB-6).
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
	 * elegir dados, repetir, saltar, expulsar, robar, apartar la runa de Koschei o elegir esfera. Se valida asiento (el de
	 * FPendingChoice::Seat, que desde S-010 puede no tener el turno), tipo de elección (Kind debe ser el de la pendiente:
	 * una respuesta tardía no contesta otra elección), plazo y opción; si no es válida se descarta sin castigo y sin
	 * tocar el estado. Option: una respuesta válida (FPendingChoice::IsValidAnswer: un valor de Options, o en PlayerAction
	 * el índice de una de ActionOptions), o INDEX_NONE para "no" si la elección lo admite.
	 * Efectos al aceptarla:
	 *  - DiceCount: fija los dados del turno y registra DiceChosen; la tirada la hace AdvanceTurn.
	 *  - Reroll: tira de nuevo con los mismos dados, recalcula las alcanzables y registra Rerolled (R-TUR-6); "no" cierra
	 *    la oferta. En los dos casos se sacan del sorteo los dos valores de la repetición.
	 *  - PlanarJump: la ficha salta a la casilla y se registra PlanarJumped (R-POD-5); después se ofrece la acción.
	 *  - PlayerAction (S-010): expulsar manda al objetivo a su hogar y registra Expelled (R-POD-4), y termina el turno;
	 *    robar empieza el robo (R-ECO-4, BeginSteal): sin runas o anulado termina el turno, y si no la máquina pasa a la
	 *    fase ShellGame, con la elección de Koschei abierta para la víctima si toca (R-ROB-5). Retar (S-012) empieza el
	 *    duelo (BeginDuel): sin runas el retado, termina el turno (R-DUE-4); si no, la máquina pasa a la fase Duel.
	 *  - SafeRune (S-010): Koschei aparta la runa y empieza la muestra del trilero.
	 *  - ShellPick (S-010): la runa de la esfera elegida pasa al ladrón (RuneStolen, R-ROB-3) y termina el turno.
	 * NowSeconds: instante en que llega. Random: el sorteo de la partida (solo lo usa la repetición).
	 * Devuelve Accepted, WrongSeat, WrongPhase (no hay elección pendiente de ese tipo), ClockExpired o InvalidOption.
	 * Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitChoiceIntent(FMatchState& State, int32 Seat, EChoiceKind Kind, int32 Option, double NowSeconds, FRandomStream& Random);

	/**
	 * Rendimiento del asiento Seat en el duelo en curso (S-012, R-DUE-1): la única forma de salir de la fase Duel, como el
	 * resultado del minijuego lo es de SkillTrial. Cada participante entrega el suyo una vez, en cualquier orden; con los dos,
	 * el duelo se resuelve (ResolveDuel): el perdedor sin runas vuelve a su hogar y el turno termina (R-DUE-6); si no, el
	 * ganador le roba con el trilero (R-DUE-5, fase ShellGame), sea retador o retado, y al acabar termina el turno del
	 * retador. Sin plazo en S-012 (TODO(R-MIN) en TurnRules.cpp).
	 * Performance: de 0 a 1. NowSeconds: marca de tiempo de los eventos.
	 * Devuelve Accepted; WrongPhase si no hay duelo en curso o el asiento ya entregó el suyo; WrongSeat si no participa;
	 * InvalidOption si Performance no está entre 0 y 1. Si no es Accepted, no toca el estado. Nombre propuesto en S-012.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitDuelPerformance(FMatchState& State, int32 Seat, double Performance, double NowSeconds);

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
