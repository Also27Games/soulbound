#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/**
 * Reglas de la victoria (S-018; GDD 04, la victoria; GDD 06, R-PRO-4 y R-WAR-4b): el aspirante, el desafío final y los tres finales de
 * la partida (victoria de un jugador, victoria del Warden por tiempo y, desde S-006, por nadie vinculado). Junto con Match/BondRaceRules.h,
 * las únicas que escriben la fase y el resultado de la partida. La máquina de turnos las llama: el fin por tiempo, al principio de
 * AdvanceTurn; el anuncio del aspirante, al principio y al final; el desafío, al empezar el turno del aspirante y con su resultado.
 */
namespace Soulbound::Rules
{
	/** Tipos de runa distintos en el Brazal del asiento SeatState: de 0 a Soulbound::Board::DomainCount (R-VIC-1, R-DIS-1). */
	SOULBOUNDCORE_API int32 RuneTypeCount(const FSeatState& SeatState);

	/**
	 * Si el asiento Seat es aspirante (glosario `Aspirant`, R-VIC-1, R-ECO-6): está en el Altar con los seis tipos de runa distintos, sea
	 * Elegido o Eco. Función pura del estado, la misma condición que Victoria inminente "en el Altar con los seis" (R-DIS-1). Devuelve false
	 * si el asiento no existe o está en el Vacío.
	 */
	SOULBOUNDCORE_API bool IsAspirant(const FMatchState& State, int32 Seat);

	/**
	 * Anuncia a los nuevos aspirantes (R-PRO-4): por cada asiento que es aspirante sin anuncio, registra AspirantAnnounced en NowSeconds; a
	 * quien dejó de serlo le quita el anuncio, así se anuncia otra vez si vuelve a serlo. Con la partida terminada no hace nada.
	 * Devuelve true si registró algún anuncio.
	 */
	SOULBOUNDCORE_API bool SyncAspirants(FMatchState& State, double NowSeconds);

	/**
	 * Si la partida debe terminar por tiempo ahora (R-VIC-4, R-WAR-4b): con Warden en la partida (FWardenParams::HasWarden), sin terminar
	 * y con el reloj en FWardenParams::MatchEndSeconds o después. Sin Warden no hay fin por tiempo (decisión del autor del 27/10/2026:
	 * se mira si hay parámetros del Warden, no si el anfitrión evalúa; con SbWardenMode off la partida termina igual a las 40:00).
	 */
	SOULBOUNDCORE_API bool IsMatchEndDue(const FMatchState& State, double NowSeconds);

	/**
	 * La partida termina con victoria del Warden por Reason (R-CAR-8, R-VIC-4): fase MatchOver, resultado WardenVictory, motivo en
	 * FMatchState::EndReason y el evento WardenVictory con el motivo en Value. No descarta nada de lo que hubiera en marcha: eso es de
	 * quien llama (la máquina de turnos, en el fin por tiempo). Devuelve false, sin cambiar nada, si la partida ya había terminado.
	 */
	SOULBOUNDCORE_API bool DeclareWardenVictory(FMatchState& State, EMatchEndReason Reason, double NowSeconds);

	/**
	 * Aplica el resultado del desafío final de Seat (R-VIC-2, R-VIC-3, R-RUN-6). Superado: la partida termina en ese instante con su
	 * victoria (MatchOver, PlayerVictory, FMatchState::WinnerSeat, MatchWon) y la máquina queda entre turnos sin nada pendiente. Fallado:
	 * FinalChallengeFailed con el tipo de la runa que le arranca el Warden, una de las que tiene contando repetidas (RandomRuneType con el
	 * sorteo del Warden, WardenDraw), que vuelve a la reserva de su Fuente (RuneReturned), y la Vuelta al hogar (SendHome); el turno lo
	 * termina la máquina, sin tirada y sin consumir el sorteo de los dados. En los dos casos la prueba pendiente se cierra.
	 * Devuelve bPassed. No valida: lo hace SubmitSkillTrialResult.
	 */
	SOULBOUNDCORE_API bool ApplyFinalChallengeResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds);
}
