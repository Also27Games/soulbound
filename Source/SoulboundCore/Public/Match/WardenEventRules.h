#pragma once

#include "CoreMinimal.h"
#include "Match/DuelRules.h"
#include "Match/EffectRules.h"
#include "Match/MatchState.h"

struct FRandomStream;

/** Qué pasó al lanzar un evento del Warden (S-014, nombre propuesto). Solo Started cambia el estado. */
enum class EWardenEventStart : uint8
{
	/** El evento empezó: o ya se aplicó entero (decreto sin elecciones) o espera una elección (FMatchState::CurrentEvent). */
	Started,
	/** No hay ningún evento con ese identificador en FMatchState::WardenEvents. */
	UnknownEvent,
	/** El turno no está entre turnos (BetweenTurns): hasta las interrupciones de S-016, los eventos van entre turnos. */
	NotBetweenTurns,
	/** Ya hay un evento en curso. */
	EventInProgress,
	/** La partida terminó, o no tiene orden de turno. */
	MatchOver,
	/** El evento usa un efecto que es de otra story (Alto, Consulta forzada al Oráculo). */
	NotImplemented,
};

/**
 * Eventos del Warden (GDD 05, R-DIR-1 a R-DIR-4, R-COM-1 a R-COM-3; S-014): decretos y dilemas compuestos de efectos con
 * dirección, que se escriben en datos (Content/Data/Events/eventos.json) sin tocar código. ApplyEvent arranca uno entre
 * turnos; la máquina de turnos (AdvanceTurn en BetweenTurns, SubmitChoiceIntent) lo lleva hasta el final antes de empezar el
 * siguiente turno. Cada elección que haga falta (dilema, rival señalado, runa regalada, runa a perder) es la elección
 * pendiente del asiento que corresponda, con su reloj. Desatender al Warden nunca sale gratis (R-DIR-4). Todos los sorteos
 * salen de la semilla del evento, sacada del sorteo del Warden: nada toca el sorteo de la partida.
 */
namespace Soulbound::Rules
{
	/**
	 * Líder (glosario `Leader`, R-DIR-1): los asientos con más tipos de runa distintos; con empate, los que tienen más runas;
	 * si persiste, todos los empatados. Sin runas, empatan todos (decisión del autor del 12/10/2026, duda para el GDD 05).
	 * En orden de asiento. Se calcula sobre todos; el filtro de válidos (R-DIR-2) va después. S-014.
	 */
	SOULBOUNDCORE_API TArray<int32> LeaderSeats(const FMatchState& State);

	/**
	 * Quien dirige un evento que llega ahora (R-DIR-1): el asiento activo si su turno está en curso; entre turnos, el último
	 * que jugó (FMatchState::LastTurnSeat), o el asiento al que le toca si aún no ha jugado nadie. S-014.
	 */
	SOULBOUNDCORE_API int32 EventDirector(const FMatchState& State);

	/**
	 * Si el efecto Effect vale sobre el asiento Seat en un evento que dirige Director (R-DIR-2, R-DIR-3): en el Vacío solo
	 * Liberación (y Liberación solo para quien está dentro); el resto, la comprobación de cada efecto (Match/EffectRules.h).
	 * En los efectos globales (IsGlobalEffect) Seat no cuenta: es su comprobación global. NotImplemented en Alto y Consulta
	 * forzada. Desde S-015: la Prueba del Warden (AlreadyPending sobre quien ya la tiene), el Robo decretado (Seat la víctima,
	 * Director el ladrón) y el Duelo forzado (Seat el retado, Director el retador; sin runas el retado, NoRunes). S-014.
	 */
	SOULBOUNDCORE_API EEffectResult CanApplyEffect(const FMatchState& State, EEffect Effect, int32 Seat, int32 Director);

	/**
	 * Aplica Application sobre el asiento Seat (INDEX_NONE en los globales) en un evento que dirige Director: comprueba
	 * CanApplyEffect y, si vale, registra EffectApplied y aplica el efecto. Pointer: quien señaló a Seat (rival señalado), o
	 * INDEX_NONE; si lo hay, una Vuelta al hogar o un Vacío cobran su marca (R-EFE-8). Rune: el tipo elegido en Runa regalada y
	 * Perder una runa "chosen", o INDEX_NONE (al azar al perderla). Random: el sorteo del Warden (Sello, runa al azar).
	 * Devuelve el resultado de la comprobación; si no es Applied, no cambia nada (NotImplemented en Alto y Consulta forzada).
	 * Robo decretado y Duelo forzado empiezan su trilero o su duelo, que la máquina juega entre turnos (S-015); en el Duelo
	 * forzado, Pointer es el retador (INDEX_NONE: Director), así que con "dos rivales señalados" Seat es el segundo señalado.
	 * S-014.
	 */
	SOULBOUNDCORE_API EEffectResult ApplyEffect(FMatchState& State, const FEffectApplication& Application, int32 Seat, int32 Director, int32 Pointer, int32 Rune,
		double NowSeconds, FRandomStream& Random);

	/**
	 * Reloj de un dilema de OptionCount opciones (R-COM-2): la base más un incremento por cada opción a partir de la segunda
	 * (FMatchParams::DilemmaClockSeconds, DilemmaClockPerOptionSeconds). S-014.
	 */
	SOULBOUNDCORE_API double DilemmaClockSeconds(const FMatchParams& Params, int32 OptionCount);

	/**
	 * Si se puede lanzar ahora el evento EventId: MatchOver, NotBetweenTurns, EventInProgress, UnknownEvent o NotImplemented
	 * (si alguna aplicación de cualquier opción usa un efecto de otra story; decisión del autor del 12/10/2026), en ese orden;
	 * Started si se puede. No cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EWardenEventStart CanApplyEvent(const FMatchState& State, const FString& EventId);

	/**
	 * Lanza el evento EventId como entrega directa del Warden (R-COM-1; la del Oráculo es de su story), entre turnos: saca la
	 * semilla del evento de Random (el sorteo del Warden, un valor), fija quién lo dirige (EventDirector) y registra
	 * WardenEventStarted. Un decreto se aplica ya; un dilema abre la elección Dilemma para quien lo dirige, con
	 * DilemmaClockSeconds. Las aplicaciones de la opción se resuelven en orden, cada una validada por separado (R-COM-3,
	 * R-DIR-2): sin objetivos válidos, WardenTaunt y la siguiente. Avanza hasta la primera elección o el final.
	 * NowSeconds: marca de los eventos. Devuelve CanApplyEvent; si no es Started, no cambia nada. Nombre propuesto en S-014.
	 */
	SOULBOUNDCORE_API EWardenEventStart ApplyEvent(FMatchState& State, const FString& EventId, double NowSeconds, FRandomStream& Random);

	/**
	 * Lleva el evento en curso (lo llama AdvanceTurn en BetweenTurns): con una elección abierta, espera a su plazo y, agotado,
	 * desatiende (WardenIgnored, insolencia) y resuelve como dice R-DIR-4 y R-COM-2; sin elección, sigue con la aplicación que
	 * toque. Devuelve false, sin cambiar nada, si no hay evento en curso o la elección aún está en plazo. S-014.
	 */
	SOULBOUNDCORE_API bool AdvanceWardenEvent(FMatchState& State, double NowSeconds);

	/**
	 * Aplica la respuesta Option a la elección Kind de un evento, ya validada y cerrada por SubmitChoiceIntent: la opción del
	 * dilema (DilemmaChosen), el rival señalado o la runa (regalada o a perder) del objetivo que la tenía; después sigue el
	 * evento. S-014.
	 */
	SOULBOUNDCORE_API void AnswerWardenEventChoice(FMatchState& State, EChoiceKind Kind, int32 Option, double NowSeconds);

	/**
	 * Lo que deja un Duelo contra el Warden ya resuelto (R-DIR-4, S-015), que la máquina llama con el FDuelResult de
	 * ResolveDuel: si gana el Warden (Winner INDEX_NONE), registra WardenDuelWon y lo que estaba en juego vuelve a su Fuente
	 * (R-RUN-6, RuneReturned): una runa del jugador al azar con el sorteo del evento, o todas con AllRunes; si gana el jugador,
	 * registra WardenDuelLost y no pierde nada. La insolencia ya subió al desatender. Nombre propuesto en S-015.
	 */
	SOULBOUNDCORE_API void SettleWardenDuel(FMatchState& State, const FDuelResult& Result, double NowSeconds);
}
