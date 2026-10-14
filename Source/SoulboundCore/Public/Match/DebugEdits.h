#pragma once

#include "CoreMinimal.h"
#include "Board/SealRules.h"
#include "Match/EffectRules.h"
#include "Match/WardenEventRules.h"

struct FMatchState;
struct FRandomStream;

/** Tipo de una edición de depuración (S-011, nombre propuesto): es el Value del evento DebugEdit. */
enum class EDebugEditKind : uint8
{
	GiveRune,
	TakeRune,
	Move,
	Bond,
	Seal,
	Unseal,
	EndRace,
	FixRoll,
	FixNextSeat,
	FixTrial,
	/** Fijar el resultado del próximo duelo del humano (S-012, SbDuel). */
	FixDuel,
	/** Desterrar a un asiento al Vacío (S-013, SbBanish). */
	Banish,
	/** Liberación: sacar a todos del Vacío (S-013, SbRelease). */
	ReleaseVoid,
	/** Vuelta al hogar de un asiento (S-013, SbSendHome). */
	SendHome,
	/** Lanzar un evento del Warden (S-014, SbEvent). */
	LaunchEvent,
};

/** Resultado de una edición de depuración (S-011, nombre propuesto). Solo Applied cambia el estado. */
enum class EDebugEditResult : uint8
{
	/** La edición se aplicó y dejó su evento DebugEdit. */
	Applied,
	/** El asiento no existe. */
	InvalidSeat,
	/** La casilla no existe. */
	InvalidTile,
	/** El tipo de runa o el Dominio no existen. */
	InvalidDomain,
	/** La cantidad no es positiva. */
	InvalidAmount,
	/** El tablero está Primordial: aún no hay reservas (R-TAB-8). */
	BoardPrimordial,
	/** El Dominio no tiene Fuente de Poder. */
	NoSource,
	/** La reserva de la Fuente no llega a la cantidad pedida. */
	NotEnoughReserve,
	/** El asiento no tiene tantas runas de ese tipo. */
	NotEnoughRunes,
	/** La partida no está en la carrera por el vínculo. */
	NotInBondRace,
	/** El asiento no es un Eco Perdido (ya es Elegido). */
	NotLostEcho,
	/** El Nicho del Dominio no está Libre. */
	NicheNotFree,
	/** La casilla no está sellada. */
	NotSealed,
	/** La tirada fijada no está entre 1 y el máximo de dos dados. */
	InvalidRoll,
};

/**
 * Ediciones de depuración del estado (S-011): llevan la partida a un estado de prueba. Son herramientas de desarrollo:
 * las usan los comandos de consola del juego y los tests. Cada edición reutiliza las reglas siempre que existen, deja
 * el estado coherente y devuelve si se aplicó y, si no, por qué, sin cambiar nada. Cada edición aplicada registra un
 * evento DebugEdit (Value = EDebugEditKind, con el asiento afectado o INDEX_NONE) justo antes de los eventos de las
 * reglas que reutiliza; una rechazada no deja nada. No saltan esperas de la máquina de turnos ni comprueban el
 * confinamiento de los Ecos.
 */
namespace Soulbound::Debug
{
	/** Tirada máxima que se puede fijar: la de dos dados (Soulbound::Dice::MaxCount por las caras de un dado). */
	SOULBOUNDCORE_API int32 MaxFixedRoll();

	/**
	 * Da Count runas del tipo RuneType al asiento Seat, sacadas de la reserva de la primera Fuente de ese Dominio: el
	 * Brazal sube Count y la reserva baja Count. Rechaza el asiento o el tipo inexistentes, una cantidad no positiva, el
	 * tablero Primordial, un Dominio sin Fuente y una reserva menor que Count.
	 */
	SOULBOUNDCORE_API EDebugEditResult GiveRune(FMatchState& State, int32 Seat, int32 RuneType, int32 Count, double NowSeconds);

	/**
	 * Quita Count runas del tipo RuneType al asiento Seat y las devuelve a la reserva de su Fuente, con la regla
	 * ReturnRuneToSource (deja un RuneReturned por runa). Rechaza el asiento o el tipo inexistentes, una cantidad no
	 * positiva y un Brazal con menos de Count runas de ese tipo.
	 */
	SOULBOUNDCORE_API EDebugEditResult TakeRune(FMatchState& State, int32 Seat, int32 RuneType, int32 Count, double NowSeconds);

	/**
	 * Coloca la ficha del asiento Seat en la casilla TileId, sin resolverla y sin contar como movimiento del turno
	 * (no toca bActiveSeatMoved ni el turno). No comprueba el confinamiento. Rechaza el asiento o la casilla inexistentes.
	 */
	SOULBOUNDCORE_API EDebugEditResult PlaceToken(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds);

	/**
	 * Vincula al asiento Seat con el Elegido del Dominio DomainId, con la misma regla que reclamar su Nicho (ClaimNiche),
	 * sin mover la ficha. Rechaza el asiento o el Dominio inexistentes, la partida fuera de la carrera, un asiento que
	 * ya es Elegido y un Nicho que no está Libre. Random: el sorteo de la partida (lo usa el fin de carrera).
	 */
	SOULBOUNDCORE_API EDebugEditResult BondSeat(FMatchState& State, int32 Seat, int32 DomainId, double NowSeconds, FRandomStream& Random);

	/** Sella la casilla TileId con las reglas de Sello: acepta y rechaza lo que CanSealTile, con su mismo motivo. */
	SOULBOUNDCORE_API ESealResult SealTileEdit(FMatchState& State, int32 TileId, double NowSeconds);

	/** Libera el Sello de TileId con ReleaseSeal. Rechaza una casilla inexistente (InvalidTile) o no sellada (NotSealed). */
	SOULBOUNDCORE_API EDebugEditResult UnsealTile(FMatchState& State, int32 TileId, double NowSeconds);

	/** Termina la carrera ahora, con EndBondRace. Rechaza la partida que no está en la carrera (NotInBondRace). */
	SOULBOUNDCORE_API EDebugEditResult EndRace(FMatchState& State, double NowSeconds, FRandomStream& Random);

	/**
	 * Fija el resultado de la próxima tirada, sea de quien sea (FMatchState::ForcedRoll). Sustituye a una fijada antes.
	 * Rechaza un valor menor que 1 o mayor que MaxFixedRoll. El evento lleva el asiento activo.
	 */
	SOULBOUNDCORE_API EDebugEditResult FixNextRoll(FMatchState& State, int32 Value, double NowSeconds);

	/**
	 * El siguiente turno que empiece será del asiento Seat y el orden sigue desde él (FMatchState::ForcedNextSeat).
	 * Rechaza un asiento inexistente.
	 */
	SOULBOUNDCORE_API EDebugEditResult FixNextSeat(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Deja en el registro que se fijó el resultado del próximo minijuego del asiento Seat (el humano), y nada más: el
	 * resultado vive en el anfitrión de partida, que es quien responde por el humano, no en el estado. Rechaza un
	 * asiento inexistente.
	 */
	SOULBOUNDCORE_API EDebugEditResult FixTrial(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Deja en el registro que se fijó el resultado del próximo duelo en que participe el asiento Seat (el humano), y nada
	 * más: como FixTrial, el resultado vive en el anfitrión de partida (S-012). Rechaza un asiento inexistente.
	 */
	SOULBOUNDCORE_API EDebugEditResult FixDuel(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Destierra al asiento Seat con la regla del efecto (Banish, sin la Furia, S-013): acepta y rechaza lo que CanBanish,
	 * con su mismo motivo (durante la carrera, BondRace).
	 */
	SOULBOUNDCORE_API EEffectResult BanishSeat(FMatchState& State, int32 Seat, double NowSeconds);

	/** Liberación con la regla del efecto (ReleaseVoid, S-013): acepta y rechaza lo que CanReleaseVoid, con su motivo. */
	SOULBOUNDCORE_API EEffectResult ReleaseVoidEdit(FMatchState& State, double NowSeconds);

	/** Vuelta al hogar del asiento Seat con la regla del efecto (SendHome, S-013): acepta y rechaza lo que CanSendHome. */
	SOULBOUNDCORE_API EEffectResult SendHomeEdit(FMatchState& State, int32 Seat, double NowSeconds);

	/**
	 * Lanza el evento del Warden EventId como entrega directa, entre turnos, con la regla de los eventos (ApplyEvent, S-014):
	 * acepta y rechaza lo que CanApplyEvent, con su mismo motivo. Aplicada, el evento de depuración (sin asiento) va antes que
	 * los del evento. Random: el sorteo del Warden, nunca el de la partida.
	 */
	SOULBOUNDCORE_API EWardenEventStart LaunchEvent(FMatchState& State, const FString& EventId, double NowSeconds, FRandomStream& Random);
}
