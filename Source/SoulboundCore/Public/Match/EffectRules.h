#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/**
 * Resultado de un efecto (glosario "Efecto", GDD 05, S-013, nombre propuesto): si se aplicó o por qué su objetivo no es
 * válido (R-DIR-2). Solo Applied cambia el estado.
 */
enum class EEffectResult : uint8
{
	/** El efecto se aplicó y dejó su evento. */
	Applied,
	/** El asiento no existe. */
	InvalidSeat,
	/** El objetivo está en el Vacío: no es objetivo válido de ningún efecto salvo Liberación (R-VAC-1, R-INT-3). */
	InVoid,
	/**
	 * El objetivo está en juego: es el asiento activo con el turno empezado (cualquier fase salvo BetweenTurns) o participa
	 * en un duelo o un trilero en curso (decisión del autor del 08/10/2026; las interrupciones son de S-015).
	 */
	InPlay,
	/** Vuelta al hogar a quien ya está en su hogar (R-EFE-4, R-NIC-2). */
	AlreadyHome,
	/** Liberación sin nadie en el Vacío (R-EFE-17). */
	NobodyInVoid,
	/** Destierro durante la carrera sin la Furia (R-CAR-7). */
	BondRace,
};

/**
 * Efectos del Warden (glosario "Efecto", GDD 05; S-013): cada uno con su comprobación de objetivo válido (R-DIR-2), que
 * dice si se puede y por qué no, y su aplicación, que comprueba lo mismo y no cambia nada si no vale. Hoy solo los llaman
 * las ediciones de depuración (SbBanish, SbRelease, SbSendHome); quién los impone y cuándo es del Warden (S-014, S-015),
 * que añadirá aquí el resto del catálogo. Ninguno toca el sorteo de la partida ni el turno en curso.
 */
namespace Soulbound::Rules
{
	/**
	 * Si el asiento Seat está en juego (decisión del autor del 08/10/2026): es el asiento activo y su turno ha empezado
	 * (TurnPhase distinta de BetweenTurns), o participa en el duelo en curso, o es el ladrón o la víctima del trilero en
	 * curso. Entre turnos nadie está en juego. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API bool IsSeatInPlay(const FMatchState& State, int32 Seat);

	/**
	 * Si el asiento Seat puede ser desterrado (R-EFE-14, R-CAR-7), en este orden: InvalidSeat; InVoid si ya está dentro;
	 * BondRace durante la carrera salvo con bFury (la Furia del Warden, R-CAR-7); InPlay si está en juego. Applied si vale.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult CanBanish(const FMatchState& State, int32 Seat, bool bFury);

	/**
	 * Destierra al asiento Seat (R-VAC-1, R-TAB-24, R-EFE-14): su ficha sale del tablero (TileId INDEX_NONE), su cuenta de
	 * turnos dentro empieza de cero (R-VAC-4) y se registra Banished (Value 1 con la Furia). bFury: si lo destierra la
	 * Furia, lo único que permite desterrar en la carrera. El Vacío de la Furia, donde no funcionan Desvío ni Liberación,
	 * es de S-015. NowSeconds: marca del evento. Devuelve CanBanish; si no es Applied, no cambia nada.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult Banish(FMatchState& State, int32 Seat, bool bFury, double NowSeconds);

	/** Si la Liberación es válida (R-EFE-17): NobodyInVoid si nadie está en el Vacío; Applied si alguien. S-013. */
	SOULBOUNDCORE_API EEffectResult CanReleaseVoid(const FMatchState& State);

	/**
	 * Liberación (glosario `ReleaseVoid`, R-VAC-5, R-EFE-17): todos los del Vacío salen a la vez a su hogar, sin esperar
	 * turnos, en orden de asiento (LeaveVoid con Released, todos con la misma marca). Si uno de ellos esperaba en su turno
	 * (elección de Desvío o escape), la máquina de turnos cierra la espera y termina el turno (R-VAC-3; decisión aceptada
	 * por el autor el 08/10/2026). NowSeconds: marca de los eventos. Devuelve CanReleaseVoid; si no es Applied, no cambia
	 * nada. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult ReleaseVoid(FMatchState& State, double NowSeconds);

	/**
	 * Si el asiento Seat puede recibir la Vuelta al hogar (R-EFE-4, R-NIC-1, R-NIC-2), en este orden: InvalidSeat; InVoid;
	 * AlreadyHome si ya está en su hogar (HomeTile); InPlay si está en juego. Applied si vale. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult CanSendHome(const FMatchState& State, int32 Seat);

	/**
	 * Vuelta al hogar (glosario `SendHome`, R-EFE-4): la ficha del asiento Seat va a su hogar (HomeTile) y se registra
	 * SentHome con la casilla. No es un movimiento suyo: no resuelve casilla ni cambia sus turnos; el siguiente se juega con
	 * normalidad (R-NIC-1). NowSeconds: marca del evento. Devuelve CanSendHome; si no es Applied, no cambia nada.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult SendHome(FMatchState& State, int32 Seat, double NowSeconds);
}
