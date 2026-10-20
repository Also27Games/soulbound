#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/**
 * El Vacío (glosario "El Vacío", GDD 05, R-VAC-1 a R-VAC-6; S-013): la escalera de escape, la salida al hogar y Desvío.
 * Estado en FSeatState::TileId (INDEX_NONE dentro) y FSeatState::VoidTurns. Estas reglas, y las de efectos que mandan al
 * Vacío o sacan de él (Match/EffectRules.h), son las únicas que lo escriben. La máquina de turnos (Match/TurnRules.h) las
 * llama al empezar el turno de un asiento desterrado, en este orden (R-VAC-2): Eco, Desvío, escalera. Ninguna toca el
 * sorteo de la partida.
 */
namespace Soulbound::Rules
{
	/**
	 * Escalera de escape del asiento Seat (R-VAC-2, R-ELE-2): en el Vacío de la Furia, la suya para todos
	 * (FMatchParams::FuryEscapeLadder, R-INS-3, S-016); si no, la de la afinidad VoidEscape (Mictlantecuhtli,
	 * FMatchParams::VoidEscapeAffinityLadder) si la tiene, y si no la principal (FMatchParams::VoidEscapeLadder). El peldaño
	 * del turno n dentro es el elemento n - 1; el turno siguiente al último sale sin jugar. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API const TArray<ESkillDifficulty>& VoidEscapeLadderFor(const FMatchState& State, int32 Seat);

	/**
	 * El asiento Seat sale del Vacío por el motivo Reason (R-VAC-3): reaparece en su hogar (HomeTile), su cuenta de turnos
	 * dentro vuelve a 0 y se registra LeftVoid con el motivo. No toca el turno: si termina o se juega lo decide quien llama.
	 * NowSeconds: marca del evento. Devuelve false, sin cambiar nada, si el asiento no existe o no está en el Vacío.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API bool LeaveVoid(FMatchState& State, int32 Seat, EVoidExit Reason, double NowSeconds);

	/**
	 * Tipo de runa que da Desvío al asiento Seat (R-POD-6), si lo tiene activo (IsPowerActive): el único de su mapa que da
	 * Desvío. INDEX_NONE si no tiene Desvío activo (un Eco nunca lo tiene). Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API int32 DeflectionRuneType(const FMatchState& State, int32 Seat);

	/**
	 * El asiento Seat usa Desvío para salir del Vacío (R-POD-6): una runa del tipo que se lo da vuelve a su Fuente
	 * (ReturnRuneToSource, RuneReturned) y sale al hogar (LeaveVoid con Deflected). Si era la última de ese tipo, pierde
	 * Desvío en el acto (R-ELE-5). Después el asiento juega su turno: eso lo hace la máquina de turnos.
	 * NowSeconds: marca de los eventos. Devuelve false, sin cambiar nada, si no está en el Vacío o no tiene Desvío activo.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API bool UseDeflection(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Aplica el resultado del minijuego de escape del asiento Seat (R-VAC-2, lo llama SubmitSkillTrialResult): registra
	 * VoidEscapePassed o VoidEscapeFailed con la dificultad pendiente; superado, sale al hogar (LeaveVoid con Escaped).
	 * Limpia la prueba pendiente. El turno termina en los dos casos (R-VAC-3): eso lo hace la máquina de turnos.
	 * NowSeconds: marca de los eventos. Devuelve true si el asiento salió. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API bool ApplyVoidEscapeResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds);
}
