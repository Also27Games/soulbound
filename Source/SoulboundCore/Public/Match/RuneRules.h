#pragma once

#include "CoreMinimal.h"
#include "Elects/Elects.h"
#include "Match/MatchState.h"

/** Qué pasó al terminar un movimiento en una casilla. Nombre propuesto en S-008. */
enum class ERuneLanding : uint8
{
	/** Nada: la casilla no es una Fuente, el asiento no se ha movido, el tablero es Primordial o el asiento no puede obtener aquí. */
	None,
	/** El asiento obtiene una runa (R-RUN-2). */
	Obtained,
	/** La Fuente no tiene reserva (R-RUN-4): no hay runa. */
	Depleted,
	/** Un Elegido en la Fuente de un Dominio Olvidado debe superar el minijuego: el turno espera el resultado (R-RUN-2, R-TAB-7). */
	TrialPending,
};

/**
 * Runas y poderes (GDD 04, con las reglas de Fuente de los GDD 01 y 03). Estado en FSeatState::Runes (el Brazal) y en
 * FTileState::RuneReserve (la reserva de cada Fuente). Solo estas reglas los escriben tras el Despertar; el trilero
 * (Match/ShellGameRules.h) pasa runas de un Brazal a otro con TransferRune (S-010).
 */
namespace Soulbound::Rules
{
	/**
	 * Resuelve que el asiento Seat termina su movimiento en su casilla actual, si es una Fuente (R-RUN-2 a R-RUN-5):
	 *  - Solo si el asiento se ha movido este turno (R-RUN-3) y el tablero está Despierto (R-TAB-8).
	 *  - Un Elegido obtiene en cualquier Fuente; si el Dominio es Olvidado, antes debe superar el minijuego: la función
	 *    deja la prueba pendiente (FMatchState::PendingTrial) y devuelve TrialPending sin tocar el Brazal. Un Eco solo
	 *    obtiene en Fuentes de Dominios Olvidados y sin minijuego (R-ECO-3, R-CAR-7b).
	 *  - Con reserva cero no hay runa ni minijuego: se registra SourceDepleted (R-RUN-4) y devuelve Depleted. Solo se
	 *    registra si el asiento podría haber obtenido: un Eco en la Fuente de un Dominio con dueño no deja evento.
	 *  - Al obtener, el Brazal sube uno en el tipo de la Fuente (su Dominio) y la reserva baja uno (RuneObtained).
	 * NowSeconds: marca de tiempo de los eventos. Devuelve qué pasó.
	 */
	SOULBOUNDCORE_API ERuneLanding ResolvePowerSourceLanding(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Qué pasaría si el asiento Seat terminara su movimiento en la casilla TileId (S-040, R-VIC-5; nombre propuesto en S-040). Es la condición
	 * que aplica ResolvePowerSourceLanding, sin el "se ha movido este turno" (R-RUN-3), y la usa también la meta del asiento (SeatGoal), para
	 * que regla y consulta no se separen. Solo lee:
	 *  - None si el asiento no existe, el tablero no está Despierto (R-TAB-8), la casilla no es una Fuente o el asiento es un Eco y la Fuente
	 *    es de un Dominio con dueño (R-ECO-3, R-CAR-7b).
	 *  - Depleted si la Fuente no tiene reserva (R-RUN-4).
	 *  - TrialPending si el asiento es Elegido y la Fuente es de un Dominio Olvidado (R-TAB-7).
	 *  - Obtained en el resto, aunque ya tenga runas de ese tipo (R-RUN-5).
	 * @param State El estado de la partida.
	 * @param Seat El asiento.
	 * @param TileId La casilla.
	 * @return Lo que pasaría, sin cambiar nada.
	 */
	SOULBOUNDCORE_API ERuneLanding PreviewPowerSourceLanding(const FMatchState& State, int32 Seat, int32 TileId);

	/**
	 * Aplica el resultado del minijuego pendiente del asiento Seat (lo llama SubmitSkillTrialResult): si se supera,
	 * registra SkillMinigamePassed y obtiene la runa de la Fuente (RuneObtained); si no, registra SkillMinigameFailed y nada más. Limpia la prueba pendiente.
	 * Devuelve true si el asiento obtuvo la runa.
	 */
	SOULBOUNDCORE_API bool ApplySkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds);

	/**
	 * Devuelve una runa del asiento Seat a la reserva de la Fuente SourceTile (R-RUN-6, R-TAB-4b): el Brazal baja uno en
	 * RuneType, la reserva de la Fuente sube uno y se registra RuneReturned. La usan Desvío (S-013, UseDeflection) y
	 * las ediciones de depuración; la usarán el desafío final y el Warden.
	 * RuneType: tipo de runa, de 0 a 5. SourceTile: Fuente de ese Dominio. NowSeconds: marca del evento.
	 * Devuelve false, sin cambiar nada, si el asiento no tiene runas de ese tipo o la casilla no es una Fuente de ese Dominio.
	 */
	SOULBOUNDCORE_API bool ReturnRuneToSource(FMatchState& State, int32 Seat, int32 RuneType, int32 SourceTile, double NowSeconds);

	/**
	 * El asiento Seat recibe una runa de la Fuente SourceTile sin llegar a ella (Runa regalada, R-EFE-16, S-014): el Brazal
	 * sube uno en el tipo de la Fuente (su Dominio) y la reserva baja uno (RuneObtained). Un Eco la recibe de cualquier
	 * Dominio. NowSeconds: marca del evento. Devuelve false, sin cambiar nada, si el asiento no existe, la casilla no es una
	 * Fuente o no le queda reserva. Nombre propuesto en S-014.
	 */
	SOULBOUNDCORE_API bool GrantRuneFromSource(FMatchState& State, int32 Seat, int32 SourceTile, double NowSeconds);

	/**
	 * Si el poder Power está activo para el asiento Seat (R-ELE-5, R-ECO-1, R-ECO-7): un Elegido lo tiene mientras
	 * posea al menos una runa del tipo que se lo da según su mapa; los duplicados no cambian nada (R-RUN-5). Un Eco tras
	 * la carrera solo tiene Celeridad, mientras tenga una runa de su Dominio asignado (Runa de Liberación); un Eco en
	 * carrera, ninguno. La afinidad no cuenta como poder activo (se consulta aparte).
	 */
	SOULBOUNDCORE_API bool IsPowerActive(const FMatchState& State, int32 Seat, EPower Power);

	/** Poderes activos del asiento Seat (IsPowerActive), sin repetir y en el orden de EPower. */
	SOULBOUNDCORE_API TArray<EPower> GetActivePowers(const FMatchState& State, int32 Seat);

	/**
	 * Si el asiento Seat tiene la afinidad Affinity (glosario "Afinidad"): solo un Elegido, la de su Elegido, siempre
	 * activa. Un Eco no tiene afinidad. Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API bool HasAffinity(const FMatchState& State, int32 Seat, EAffinity Affinity);

	/**
	 * Nodos Planares a los que puede saltar el asiento Seat desde su casilla (R-POD-5, R-TAB-15): solo con el tablero
	 * Despierto, si el asiento se ha movido este turno (bActiveSeatMoved), termina en un Nodo Planar y tiene Vuelo
	 * Planar. Las opciones son los demás Nodos Planares transitables para él (IsPassable, R-TAB-21: uno sellado solo con
	 * Paso Libre, decisión del autor del 04/10/2026), en orden ascendente. Vacío si no se ofrece el salto.
	 * Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API TArray<int32> PlanarJumpOptions(const FMatchState& State, int32 Seat);

	/**
	 * Asientos que puede expulsar el asiento Seat (R-POD-4): solo si tiene Expulsión y se ha movido este turno; los
	 * demás asientos de su casilla que no estén ya en su hogar (HomeTile), en orden de asiento; nunca quien está en el Vacío
	 * (R-INT-3, S-013). Vacío si no hay a quién. Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API TArray<int32> ExpelTargets(const FMatchState& State, int32 Seat);

	/**
	 * Asientos a los que puede robar el asiento Seat (R-ECO-4, R-INT-2): solo un Eco tras la carrera que se ha movido este
	 * turno (R-RUN-3, decisión del autor del 05/10/2026: sin casillas alcanzables no hay robo); los demás asientos de su
	 * casilla, Elegidos o Ecos, tengan runas o no, en orden de asiento; nunca quien está en el Vacío (R-INT-3, S-013). Vacío
	 * durante la carrera, para un Elegido o si no hay a quién. Nombre propuesto en S-010.
	 */
	SOULBOUNDCORE_API TArray<int32> StealTargets(const FMatchState& State, int32 Seat);

	/**
	 * Opciones de la acción contra un jugador del asiento Seat (R-INT-2, S-010): los duelos (DuelTargets, S-012), las
	 * expulsiones (ExpelTargets) y los robos (StealTargets), cada una con su objetivo, en ese orden. Vacío si no tiene
	 * ninguna. Nombre propuesto en S-010.
	 */
	SOULBOUNDCORE_API TArray<FPlayerActionOption> PlayerActionOptions(const FMatchState& State, int32 Seat);

	/**
	 * Pasa una runa del tipo RuneType del Brazal de FromSeat al de ToSeat (R-ROB-3): uno menos en uno, uno más en el otro,
	 * aunque ToSeat ya tenga de ese tipo. Las reservas no cambian. Sin evento: lo deja quien la usa (RuneStolen).
	 * Devuelve false, sin cambiar nada, si algún asiento no existe, son el mismo o FromSeat no tiene runas de ese tipo.
	 * Nombre propuesto en S-010.
	 */
	SOULBOUNDCORE_API bool TransferRune(FMatchState& State, int32 FromSeat, int32 ToSeat, int32 RuneType);

	/**
	 * El asiento Seat salta a la casilla TileId (R-POD-5) y se registra PlanarJumped. No valida: las opciones las da
	 * PlanarJumpOptions y las valida SubmitChoiceIntent. Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API void ApplyPlanarJump(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds);

	/**
	 * El asiento Seat expulsa al asiento Target (R-POD-4): Target pasa a su hogar (HomeTile, R-TAB-13) y se registra
	 * Expelled (Seat quien expulsa, OtherSeat el expulsado, desde S-010). No es un movimiento de Target: no resuelve casilla
	 * ni cambia sus turnos (R-INT-4). No valida: los objetivos los da ExpelTargets. Si Target estaba Marcado, Seat cobra la
	 * marca (R-EFE-8, S-014). Nombre propuesto en S-009.
	 */
	SOULBOUNDCORE_API void ApplyExpel(FMatchState& State, int32 Seat, int32 Target, double NowSeconds);
}
