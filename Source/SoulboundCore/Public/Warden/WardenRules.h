#pragma once

#include "CoreMinimal.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Warden/WardenBrain.h"

/**
 * Reglas del Warden (S-016): lo que hace quien tiene la autoridad con el cerebro (Warden/WardenBrain.h), igual en el anfitrión
 * del Warden y en la simulación (R-WAR-1). Evaluar, registrar y aplicar la decisión: el golpe corta el turno (InterruptTurn) y
 * lanza su evento entre turnos; la escena entra entre turnos o espera al hueco; el comentario solo se registra. Los sorteos del
 * Warden salen de su semilla en el estado (FWardenState); el de los dados solo se toca para el consumo fijo de un turno cortado.
 */
namespace Soulbound::Rules
{
	/** El siguiente sorteo del Warden: un generador con su semilla del estado combinada con el número de sorteo, que sube uno. */
	SOULBOUNDCORE_API FRandomStream WardenDraw(FMatchState& State);

	/**
	 * Si toca evaluar en NowSeconds: con Warden y la partida en curso, o la siguiente evaluación ha llegado
	 * (FWardenState::NextEvaluationSeconds) sin aplazamiento pendiente, o hay una aplazada y ya no hay nada que la aplace.
	 */
	SOULBOUNDCORE_API bool IsWardenEvaluationDue(const FMatchState& State, double NowSeconds);

	/**
	 * Evalúa y aplica (R-WAR-1, R-WAR-10): saca un sorteo del Warden (WardenDraw), pide la decisión al cerebro, registra
	 * WardenEvaluated y WardenDecided, guarda el instante siguiente o el aplazamiento y aplica (ApplyWardenDecision).
	 * MatchRandom: el sorteo de la partida, solo para el consumo fijo de un turno cortado. Devuelve la decisión. Desde S-018, con la
	 * partida terminada no evalúa: no saca sorteo, no registra nada y devuelve una decisión vacía.
	 */
	SOULBOUNDCORE_API FWardenDecision EvaluateWarden(FMatchState& State, double NowSeconds, FRandomStream& MatchRandom);

	/**
	 * Aplica una decisión del cerebro (R-WAR-4): un golpe con un turno en curso lo corta (InterruptTurn) y, en la misma llamada,
	 * lanza su evento entre turnos con el cortado como activo; sin turno en curso, lo lanza sin cortar. Una escena entre turnos
	 * se lanza; con un turno en curso, corta si su evento corta y si no espera al hueco (FWardenState::PendingScene). Un
	 * comentario deja WardenRemark. Lanzar gasta un uso y cuenta para la garantía. Desde S-017, un golpe por el Oráculo
	 * (FWardenDecision::bViaOracle) corta igual y abre la consulta del cortado con su decreto de sentencia (OpenOracle); cuenta para
	 * la garantía y ocupa el hueco ya, y el uso se gasta al revelarse. El protagonista del golpe deja de estar ignorado (R-DIS-3).
	 * No registra la decisión: eso es de EvaluateWarden, que desde S-017 deja tras WardenDecided un WardenTriggered por disparador
	 * activo. NowSeconds: marca de los eventos. MatchRandom: el sorteo de la partida (consumo fijo).
	 */
	SOULBOUNDCORE_API void ApplyWardenDecision(FMatchState& State, const FWardenDecision& Decision, double NowSeconds, FRandomStream& MatchRandom);

	/**
	 * Lanza el evento EventIndex del cerebro (S-016): ApplyEvent con un sorteo del Warden, marcando si llega cortando un turno
	 * (FCurrentWardenEvent::bInterruptedTurn), gasta un uso, ocupa el hueco y apunta la última intervención con efecto.
	 * Devuelve el resultado de ApplyEvent.
	 */
	SOULBOUNDCORE_API EWardenEventStart LaunchWardenEvent(FMatchState& State, int32 EventIndex, bool bInterruptedTurn, double NowSeconds);
}
