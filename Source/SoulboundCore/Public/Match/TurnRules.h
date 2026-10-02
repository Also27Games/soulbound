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
};

/**
 * Máquina de turnos (R-TUR-1 a R-TUR-15, GDD 02). El tiempo es un dato: las funciones reciben el instante actual
 * en segundos del reloj de partida y nunca leen un reloj (ADR-001). El motor no conoce a los decisores: humano,
 * bot o dummy envían su destino por SubmitDestinationIntent, y quien los consulta (simulación o anfitrión) avanza
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
	 *  - Start → PreRoll, sin decisión.
	 *  - PreRoll → RollAndChoose: tira (R-TUR-5), calcula las casillas alcanzables y registra Rolled.
	 *  - En RollAndChoose: sin casillas alcanzables, pasa a MoveAndResolve sin mover (R-TAB-18); con el reloj agotado,
	 *    registra TurnClockExpired, sube la insolencia y pasa a End sin mover (R-TUR-8); si no, espera la intención.
	 *  - MoveAndResolve → End: resuelve el Nicho de destino (R-CAR-4, ResolveNicheLanding) y la Fuente (R-RUN-2,
	 *    ResolvePowerSourceLanding, solo si el asiento se ha movido) y registra TurnEnded. Si un Elegido llega a la
	 *    Fuente de un Dominio Olvidado, pasa a SkillTrial en vez de a End.
	 *  - SkillTrial: espera el resultado del minijuego (SubmitSkillTrialResult); no avanza ni cambia nada.
	 *  - End → BetweenTurns: si el asiento tiene turnos extra, consume uno y vuelve a jugar (R-TUR-9); si no, pasa al
	 *    siguiente del orden de turno. Con el tiempo de carrera agotado, la carrera termina aquí (R-CAR-1, R-TAB-27).
	 *  - En BetweenTurns con el tiempo de carrera agotado, la carrera termina antes de empezar otro turno.
	 * Al empezar un turno y al terminarlo, los turnos perdidos se descuentan contra los extra uno a uno (caso límite
	 * del GDD 02), así un turno extra concedido durante el turno también se compensa.
	 * NowSeconds: instante actual del reloj de partida; es la marca de tiempo de los eventos.
	 * Random: el sorteo de la partida (solo lo usa la tirada).
	 * Devuelve true si cambió el estado; false si espera la intención de destino, si la partida no tiene orden de turno
	 * o si la partida terminó (R-CAR-8): entonces no cambia nada.
	 */
	SOULBOUNDCORE_API bool AdvanceTurn(FMatchState& State, double NowSeconds, FRandomStream& Random);

	/**
	 * Intención de destino del asiento Seat (R-TUR-7): la única forma de mover una ficha en un turno.
	 * Se valida asiento, fase, reloj y casilla; si no es válida se descarta sin castigo y sin tocar el estado.
	 * TileId: casilla elegida. NowSeconds: instante en que llega la intención.
	 * Devuelve Accepted si la ficha se movió y el turno pasó a MoveAndResolve, o el motivo del rechazo.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitDestinationIntent(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds);

	/**
	 * Resultado del minijuego de habilidad del asiento Seat (R-TAB-7), la única forma de salir de SkillTrial, como la
	 * intención de destino lo es de RollAndChoose. Si se supera, el asiento obtiene la runa de la Fuente; si no, no. En
	 * ambos casos el turno pasa a End. Sin plazo en S-008 (TODO(R-MIN) en TurnRules.cpp).
	 * bPassed: si superó la prueba. NowSeconds: marca de tiempo de los eventos.
	 * Devuelve Accepted, o WrongSeat o WrongPhase sin tocar el estado. Nombre propuesto en S-008.
	 */
	SOULBOUNDCORE_API EIntentResult SubmitSkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds);

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
