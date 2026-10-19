#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

struct FRandomStream;

/** Por qué no se pudo iniciar una partida. */
enum class EMatchStartError : uint8
{
	None,
	/** El número de jugadores está fuera de 1 a 4. */
	InvalidPlayerCount,
	/** El tablero no pasa ValidateBoard. */
	InvalidBoard,
	/** Los parámetros de partida no pasan ValidateMatchParams. */
	InvalidMatchParams,
	/** Los Elegidos no pasan ValidateElects (S-008). */
	InvalidElects,
	/** Los eventos del Warden no pasan ValidateWardenEvents (S-014). */
	InvalidWardenEvents,
	/** Los parámetros del Warden no pasan ValidateWardenParams (S-016). */
	InvalidWardenParams,
};

/** Resultado de iniciar una partida: o éxito con el estado inicial, o fallo con un motivo legible. */
struct SOULBOUNDCORE_API FMatchStartResult
{
	EMatchStartError Error = EMatchStartError::None;

	/** Motivo legible del fallo. Vacío si hay éxito. */
	FString Message;

	/** Estado inicial de la partida. Solo tiene sentido si IsSuccess(). */
	FMatchState State;

	bool IsSuccess() const { return Error == EMatchStartError::None; }
};

namespace Soulbound::Rules
{
	/**
	 * Prepara el estado inicial de una partida sobre un tablero cargado:
	 *  - R-TAB-2, R-CAR-2: los cuatro asientos empiezan en el Altar, como Ecos Perdidos.
	 *  - R-CAR-1, R-TAB-8: la partida empieza en la carrera por el vínculo, con el tablero Primordial.
	 *  - R-TAB-5: cada Nodo de Poder recibe al azar un efecto, respetando las cantidades por efecto de los datos.
	 *  - R-TAB-6: se eligen al azar 6 menos PlayerCount Dominios y se marcan Olvidados, con su Nicho Cerrado.
	 *  - R-TUR-1: se sortea el orden de turno, una permutación de los cuatro asientos; juega primero el primero.
	 *    Se sortea después de lo anterior, así una misma semilla da los mismos Nodos y Olvidados que antes de S-003.
	 *  - R-ELE-1: los seis Elegidos viajan en el estado (FMatchState::Elects).
	 *  - R-ROB-6 (S-010): saca del sorteo, el último, el valor base de las semillas de mezcla del trilero
	 *    (FMatchState::ShellSeedBase). Desde S-010 los dados de cada semilla no son los de antes.
	 *  - S-014: los eventos del Warden viajan en el estado (FMatchState::WardenEvents), sin tocar el sorteo.
	 *  - S-016 (R-WAR-10): los parámetros del Warden también (FMatchState::WardenParams), y su estado: la semilla de su sorteo,
	 *    la de la partida más 2 (Random.GetInitialSeed, sin sacar ningún valor), los usos de cada evento y la primera evaluación,
	 *    a un intervalo del inicio.
	 * Board: tablero válido (se comprueba con ValidateBoard). Params: parámetros de partida válidos (se comprueban con
	 * ValidateMatchParams). Elects: los seis Elegidos, válidos (ValidateElects); no hay modo sin ellos. PlayerCount: asientos ocupados por humanos o bots, de 1 a 4; en Fase 1 los bots rellenan
	 * los huecos y siempre es 4. Random: sorteo; misma semilla, mismo resultado. WardenEvents: los eventos del Warden, válidos
	 * (ValidateWardenEvents); opcional (S-014): sin ellos la partida no tiene eventos que lanzar. WardenParams: los parámetros
	 * del Warden, válidos (ValidateWardenParams); opcional (S-016): sin actos, la partida no tiene Warden.
	 * Devuelve el estado inicial, o InvalidPlayerCount, InvalidBoard, InvalidMatchParams, InvalidElects, InvalidWardenEvents o
	 * InvalidWardenParams sin tocar Random.
	 */
	SOULBOUNDCORE_API FMatchStartResult StartMatch(const FBoardState& Board, const FMatchParams& Params, const FElectsTable& Elects, int32 PlayerCount, FRandomStream& Random,
		const FWardenEventTable& WardenEvents = FWardenEventTable(), const FWardenParams& WardenParams = FWardenParams());
}
