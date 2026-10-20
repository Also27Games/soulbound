#pragma once

#include "CoreMinimal.h"
#include "Match/EffectCatalog.h"
#include "Match/MatchState.h"

struct FRandomStream;

/**
 * Insolencia con castigo y Furia (glosario "Insolencia", "Furia del Warden", "Vacío de la Furia"; R-INS-1 a R-INS-3, S-016).
 * Cada subida de insolencia (RaiseInsolence, Match/TurnRules.h) trae un castigo inmediato al insolente, sorteado por gravedad
 * con la tabla de los datos (FWardenParams::InsolenceTable) según el nivel anterior a la subida; desde el nivel 4, la Furia, sin
 * sorteo. Dentro de la gravedad, un efecto válido sobre él, con ApplyEffect y una aplicación sintética (efecto, dirección
 * activo), como el de un evento. Todos los sorteos son del Warden (WardenDraw): nada toca los dados. Sin Warden en la
 * partida (sin tabla), no hay castigos (decisión del autor del 19/10/2026).
 */
namespace Soulbound::Rules
{
	/**
	 * Efectos con que puede castigar una gravedad (R-INS-2, decisión del autor del 19/10/2026): los del catálogo de esa
	 * gravedad, implementados y que admiten la dirección activo, en el orden del catálogo. Así nunca un regalo (otra gravedad)
	 * ni un efecto que necesite a otro jugador (no admite activo). El Alto sale en los leves, pero un castigo nunca corta un
	 * turno: no es válido (R-TAB-28). Severity: Light, Medium o Severe; con Gift, vacío.
	 */
	SOULBOUNDCORE_API TArray<EEffect> InsolencePunishmentEffects(EEffectSeverity Severity);

	/**
	 * Gravedad del castigo de una subida desde el nivel LevelBefore (R-INS-2), con la fila de la tabla de Params que lo cubre:
	 * la Furia sin sorteo, o leve, media o grave por sus porcentajes, con Random. Leve si ninguna fila lo cubre.
	 */
	SOULBOUNDCORE_API EInsolencePunishment DrawInsolencePunishment(const FWardenParams& Params, int32 LevelBefore, FRandomStream& Random);

	/**
	 * Aplica sobre Seat el castigo Punishment (R-INS-2, R-INS-3): registra InsolencePunished y, con la Furia, UnleashFury; si
	 * no, sortea un efecto de su gravedad (InsolencePunishmentEffects; en el Acto I, lo grave es Vuelta al hogar, R-CAR-7) y,
	 * si no vale sobre él, otro de la misma, hasta que uno valga (ApplyEffect, dirección activo) o, sin ninguno, la burla
	 * (WardenTaunt). No mira si está en juego ni en el Vacío: eso lo decide quien lo llama (RaiseInsolence, la descarga).
	 */
	SOULBOUNDCORE_API void PunishInsolence(FMatchState& State, int32 Seat, const FPendingPunishment& Punishment, double NowSeconds);

	/**
	 * La Furia (R-INS-3): registra FuryUnleashed, la insolencia de Seat vuelve a 0 y va al Vacío de la Furia (Banish con la
	 * Furia, también en la carrera; FSeatState::bFuryVoid). Si ya estaba en el Vacío, su Vacío pasa a ser de la Furia y su
	 * cuenta de turnos dentro vuelve a 0.
	 */
	SOULBOUNDCORE_API void UnleashFury(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Castiga a Seat ahora o lo deja esperando (R-INS-2): en el Vacío, a su salida (salvo la Furia); en juego (IsSeatInPlay), a
	 * que deje de estarlo; si no, en el acto (PunishInsolence).
	 */
	SOULBOUNDCORE_API void PunishOrDefer(FMatchState& State, int32 Seat, const FPendingPunishment& Punishment, double NowSeconds);

	/**
	 * Descarga los castigos que esperaban a que su asiento dejara de estar en juego (FSeatState::PendingPunishments), en orden de
	 * asiento: los que ya no están en juego, se castigan (PunishOrDefer: en el Vacío, a su salida). Lo llama la máquina de turnos
	 * al acabar un turno y al acabar el duelo o el trilero de un evento.
	 */
	SOULBOUNDCORE_API void DischargePendingPunishments(FMatchState& State, double NowSeconds);

	/** Castiga a Seat lo que se le aplazó en el Vacío (FSeatState::VoidPunishments), ya fuera (PunishOrDefer). Lo llama LeaveVoid. */
	SOULBOUNDCORE_API void DischargeVoidPunishments(FMatchState& State, int32 Seat, double NowSeconds);
}
