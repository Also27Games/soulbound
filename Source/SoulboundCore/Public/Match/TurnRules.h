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
	/** Válida: cambia el estado (en la de destino, desde S-020, el turno pasa al movimiento en curso, Moving). */
	Accepted,
	/** No es el asiento al que le toca. */
	WrongSeat,
	/** El turno no está esperando esa intención (en la de destino, fase distinta de RollAndChoose). */
	WrongPhase,
	/** La casilla no está entre las alcanzables con la tirada (R-TUR-7). */
	Unreachable,
	/** Llega en el límite del reloj de turno o después (R-TUR-8). El agotamiento lo aplica AdvanceTurn. */
	ClockExpired,
	/** La opción no está entre las de la elección pendiente, o es "no" en una elección que no lo admite (S-009). */
	InvalidOption,
	/** Intención de avance (S-020) de una casilla que no es la siguiente del camino: fuera de orden o de otro camino. */
	NotNextTile,
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
	 *  - BetweenTurns con un evento del Warden en curso (S-014): lo lleva (AdvanceWardenEvent) y no empieza turno; con una
	 *    elección del evento en plazo, espera. El turno siguiente empieza cuando el evento ha terminado.
	 *  - BetweenTurns con una escena del Warden esperando al hueco (S-016, FWardenState::PendingScene): la lanza
	 *    (LaunchWardenEvent) y no empieza turno.
	 *  - BetweenTurns → Start: salta a los asientos con turnos perdidos (R-TUR-10, evento TurnSkipped), empieza el
	 *    turno del siguiente, arranca su reloj (R-TUR-8), lo anota como el último que jugó (LastTurnSeat, S-014), abre un hueco
	 *    nuevo para el Warden (FWardenState::bGapUsed, S-016) y registra TurnStarted. Cuando el orden pasa por su primer asiento, juegue o se salte, empieza una ronda (StartRound, S-014).
	 *  - Start → PreRoll: con Celeridad (R-TUR-4) abre la elección de dados (DiceCount), con el plazo del reloj de turno;
	 *    sin ella, un dado sin elección.
	 *  - Start de un asiento en el Vacío (S-013, R-TUR-3, R-VAC-2), antes de nada y en este orden: su cuenta de turnos
	 *    dentro sube uno; un Eco sale a su hogar y el turno pasa a End (R-ECO-5), salvo en el Vacío de la Furia, donde juega la escalera como todos (S-016); quien tiene Desvío recibe la elección
	 *    Deflection con el plazo del reloj de turno (agotada, ChoiceExpired y es "no", sin castigo; R-POD-6); si no, o si
	 *    dice que no, el minijuego de escape con la dificultad del peldaño de su escalera (VoidEscapeLadderFor; en el Vacío de la Furia, la de la Furia) (fase SkillTrial) o, pasado el
	 *    último, la salida sin jugar y End (R-VAC-3). Si una Liberación lo saca mientras espera (Desvío o escape), la
	 *    espera se cierra y el turno pasa a End. Nada de esto toca el sorteo de la partida.
	 *  - En PreRoll con la elección de dados abierta: espera la intención (SubmitChoiceIntent); con el reloj de turno
	 *    agotado, registra TurnClockExpired, sube la insolencia y pasa a End sin tirar (R-TUR-8).
	 *  - PreRoll → RollAndChoose: tira los dados del turno (R-TUR-5), calcula las casillas alcanzables y registra Rolled.
	 *    Con la afinidad de Apep abre la oferta de repetir (Reroll, R-TUR-6), con el plazo del reloj de turno.
	 *  - En RollAndChoose: sin casillas alcanzables, pasa a MoveAndResolve sin mover (R-TAB-18), salvo con la oferta de
	 *    repetir abierta, que se espera (agotada, ChoiceExpired, sin castigo); con el reloj agotado, registra
	 *    TurnClockExpired, sube la insolencia y pasa a End sin mover (R-TUR-8); si no, espera la intención.
	 *  - Moving (S-020): espera la intención de avance de la siguiente casilla del camino (SubmitMoveProgress); pasado
	 *    FMatchParams::MoveStepSeconds desde la anterior (o desde confirmar destino), entra en ella el motor (salvaguarda), una
	 *    casilla por llamada. En la última registra Moved y pasa a MoveAndResolve.
	 *  - MoveAndResolve: resuelve el Nicho de destino (R-CAR-4, ResolveNicheLanding) y la Fuente (R-RUN-2,
	 *    ResolvePowerSourceLanding, solo si el asiento se ha movido). Si un Elegido llega a la Fuente de un Dominio
	 *    Olvidado, pasa a SkillTrial. Después (R-INT-1) ofrece el salto planar (PlanarJump) y, como mucho, una acción
	 *    contra un jugador (PlayerAction: retar a duelo, expulsar o robar; S-010, S-012), cada uno con el reloj de
	 *    elección; sin nada que ofrecer, pasa a End y registra TurnEnded. Con una elección abierta, la espera; agotado su
	 *    plazo, registra ChoiceExpired y la resuelve como "no".
	 *  - Start → PreRoll de un asiento con la Prueba del Warden pendiente (S-015, R-EFE-10): después de la salida del Vacío
	 *    (que la anula al entrar) y antes de tirar, abre la fase SkillTrial con PendingTrial WardenTrial y la dificultad de
	 *    datos; superada, sigue a PreRoll; fallada, End sin pasar por PreRoll: no consume sorteo, como un turno en el Vacío.
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
	 * Intención de destino del asiento Seat (R-TUR-7): la única forma de empezar a mover una ficha en un turno.
	 * Se valida asiento, fase, reloj y casilla, que debe estar entre las alcanzables y tener camino (MovementPath; sin él,
	 * Unreachable, S-020); si no es válida se descarta sin castigo y sin tocar el estado. Aceptada, cierra la oferta de repetir
	 * si estaba abierta (R-TUR-6), para el reloj de turno (R-TUR-8: cubre hasta confirmar), guarda el camino en
	 * FMatchState::Movement y pasa a Moving (S-020): la ficha aún no se ha movido.
	 * TileId: casilla elegida. NowSeconds: instante en que llega la intención; arranca el plazo de la salvaguarda.
	 * Devuelve Accepted si el turno pasó a Moving, o el motivo del rechazo.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitDestinationIntent(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds);

	/**
	 * Intención de avance del asiento Seat (S-020, R-TUR-7): la ficha entra en la casilla TileId de su camino. La entrega la
	 * presentación al ver entrar la ficha, una por casilla y en orden; la simulación y los dummies, el camino entero en el acto.
	 * Se valida asiento, fase (Moving) y que TileId sea la siguiente del camino; si no, se descarta sin castigo y sin tocar el
	 * estado. Aceptada, la ficha pasa a esa casilla y se registra TileEntered; en la última se registra Moved y el turno pasa a
	 * MoveAndResolve, donde la casilla resuelve. Las casillas de paso no resuelven nada.
	 * TileId: casilla en la que entra. NowSeconds: instante en que llega; arranca el plazo de la salvaguarda de la siguiente.
	 * Devuelve Accepted, WrongSeat, WrongPhase o NotNextTile. Nombre propuesto en S-020.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitMoveProgress(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds);

	/**
	 * Resultado del minijuego de habilidad del asiento Seat (R-TAB-7), la única forma de salir de SkillTrial, como la
	 * intención de destino lo es de RollAndChoose. Si se supera, el asiento obtiene la runa de la Fuente; si no, no. En
	 * ambos casos el turno sigue en MoveAndResolve con el salto planar y la acción contra un jugador (R-INT-1, S-009), o
	 * pasa a End si no hay nada que ofrecer. Sin plazo en S-008 (TODO(R-MIN) en TurnRules.cpp).
	 * Escape del Vacío (S-013, PendingTrial VoidEscape, R-VAC-2, R-VAC-3): superado, el asiento sale a su hogar; fallado,
	 * sigue dentro. En los dos casos el turno pasa a End sin tirar (ApplyVoidEscapeResult).
	 * Prueba del Warden (S-015, PendingTrial WardenTrial, R-EFE-10): superada, el turno sigue en PreRoll con normalidad;
	 * fallada, pasa a End sin tirar y sin consumir el sorteo (ApplyWardenTrialResult).
	 * bPassed: si superó la prueba. NowSeconds: marca de tiempo de los eventos.
	 * Devuelve Accepted, o WrongSeat o WrongPhase (también sin prueba pendiente, PendingTrial None, o con un escape que una
	 * Liberación dejó sin objeto) sin tocar el estado. Nombre propuesto en S-008.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitSkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds);

	/**
	 * Intención que contesta la elección pendiente del asiento Seat (S-009; glosario "Intención"): la única forma de
	 * elegir dados, repetir, saltar, retar a duelo, expulsar, robar, apartar la runa de Koschei, elegir esfera o usar Desvío. Se valida asiento (el de
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
	 *  - Deflection (S-013, R-POD-6): usarlo devuelve la runa a su Fuente (RuneReturned), saca al asiento a su hogar
	 *    (LeftVoid) y el turno sigue en PreRoll, entero; "no" sigue con la escalera de escape (UseDeflection).
	 *  - Dilemma, ChosenRival, GiftRune y LoseRune (S-014): las del evento del Warden en curso, entre turnos; el evento sigue
	 *    (AnswerWardenEventChoice). Ninguna admite "no".
	 * NowSeconds: instante en que llega. Random: el sorteo de la partida (solo lo usa la repetición).
	 * Devuelve Accepted, WrongSeat, WrongPhase (no hay elección pendiente de ese tipo, o una Liberación dejó sin objeto la de
	 * Desvío), ClockExpired o InvalidOption.
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

	// S-015: duelos y trileros que nacen de un evento del Warden. Se juegan en las fases Duel y ShellGame de siempre, con sus
	// relojes y elecciones, pero entre turnos: no terminan ningún turno ni tocan TurnCount ni el último que jugó. Al acabar,
	// la máquina vuelve a BetweenTurns y el evento sigue por su siguiente aplicación (FCurrentWardenEvent::bWaitsForAction).

	/**
	 * Empieza el trilero de un evento (R-EFE-12, R-EFE-13): Thief roba a Victim con BeginSteal. Sin runas la víctima o con el
	 * robo anulado (Koschei con una runa), no hay trilero y el evento sigue en la misma llamada; si no, la máquina pasa a la
	 * fase ShellGame, con la elección de Koschei abierta para la víctima si toca (R-ROB-5). No valida: lo hace CanDecreeTheft.
	 * NowSeconds: marca de los eventos. Nombre propuesto en S-015.
	 */
	SOULBOUNDCORE_API void BeginEventSteal(FMatchState& State, int32 Thief, int32 Victim, double NowSeconds);

	/**
	 * Empieza el Duelo forzado de un evento (R-EFE-13): Challenger reta a Challenged (BeginForcedDuel) y la máquina pasa a la
	 * fase Duel. Al resolverse, con bAllRunes el ganador se queda todas las runas del perdedor sin trilero; si no, el botín es
	 * un trilero (R-DUE-5). No valida: lo hace CanForceDuel. Nombre propuesto en S-015.
	 */
	SOULBOUNDCORE_API void BeginEventDuel(FMatchState& State, int32 Challenger, int32 Challenged, bool bAllRunes, double NowSeconds);

	/**
	 * Empieza el Duelo contra el Warden de quien desatendió un Duelo forzado (R-DIR-4, S-015): la máquina pasa a la fase Duel,
	 * con Challenged de único participante (BeginWardenDuel). Si no tiene runas, no hay duelo y el evento sigue. No valida.
	 * Nombre propuesto en S-015.
	 */
	SOULBOUNDCORE_API void BeginEventWardenDuel(FMatchState& State, int32 Challenged, bool bAllRunes, double NowSeconds);

	/**
	 * Si el turno en curso se puede cortar ahora (R-TUR-11, S-016): fase Start, PreRoll, RollAndChoose, Moving (S-020) o MoveAndResolve. Nunca
	 * en Duel, ShellGame ni SkillTrial (ahí el Warden aplaza, R-WAR-7), ni entre turnos ni en End. Nombre propuesto en S-016.
	 */
	SOULBOUNDCORE_API bool CanInterruptTurn(const FMatchState& State);

	/**
	 * Corta el turno en curso (glosario "Corte", R-TUR-11, R-TAB-26, S-016): registra TurnInterrupted con la fase, cierra las
	 * elecciones abiertas, pierde la tirada pendiente respetando el consumo fijo (si el turno llegó a PreRoll, saca de Random
	 * los valores que falten: los cuatro en PreRoll, los dos de la repetición si siguen pendientes), deja la ficha donde está
	 * (su casilla ya resolvió al llegar; a medio camino, en Moving, la última en la que entró, sin resolverla y con el resto del
	 * camino descartado, R-TAB-26, S-020), termina el turno (TurnEnded) y lo deja entre turnos como cualquier fin de turno
	 * (turno extra, siguiente del orden, fin de carrera). El cortado queda como el último que jugó (LastTurnSeat).
	 * NowSeconds: marca de los eventos. Random: el sorteo de la partida. Devuelve false, sin cambiar nada, si CanInterruptTurn
	 * no lo permite. Nombre propuesto en S-016.
	 */
	SOULBOUNDCORE_API bool InterruptTurn(FMatchState& State, double NowSeconds, FRandomStream& Random);

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
	 * Sube un nivel la insolencia del asiento Seat, sin pasar de Soulbound::Match::MaxInsolence (R-INS-1). Desde S-016, con
	 * Warden en la partida, cada subida trae su castigo (R-INS-2, R-INS-3, Match/InsolenceRules.h), sorteado ahora con el nivel
	 * anterior: se aplica en el acto si el asiento no está en juego ni en el Vacío; si está en juego, al dejar de estarlo
	 * (FSeatState::PendingPunishments); si está en el Vacío, a su salida (VoidPunishments), salvo la Furia. Sin Warden, solo
	 * cuenta. NowSeconds: marca de los eventos (cambio de firma de S-016, hecho a mano). Devuelve false si el asiento no existe.
	 */
	SOULBOUNDCORE_API bool RaiseInsolence(FMatchState& State, int32 Seat, double NowSeconds);
}
