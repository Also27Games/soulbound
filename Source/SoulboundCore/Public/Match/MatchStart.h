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
	 *  - R-TAB-2: los cuatro asientos empiezan en el Altar.
	 *  - R-TAB-5: cada Nodo de Poder recibe al azar un efecto, respetando las cantidades por efecto de los datos.
	 *  - R-TAB-6: se eligen al azar 6 menos PlayerCount Dominios y se marcan Olvidados, con su Nicho Cerrado.
	 *  - R-TUR-1: se sortea el orden de turno, una permutación de los cuatro asientos; juega primero el primero.
	 *    Se sortea después de lo anterior, así una misma semilla da los mismos Nodos y Olvidados que antes de S-003.
	 * Board: tablero válido (se comprueba con ValidateBoard). Params: parámetros de partida válidos (se comprueban con
	 * ValidateMatchParams). PlayerCount: asientos ocupados por humanos o bots, de 1 a 4; en Fase 1 los bots rellenan
	 * los huecos y siempre es 4. Random: sorteo; misma semilla, mismo resultado.
	 * Devuelve el estado inicial, o InvalidPlayerCount, InvalidBoard o InvalidMatchParams sin tocar Random.
	 */
	SOULBOUNDCORE_API FMatchStartResult StartMatch(const FBoardState& Board, const FMatchParams& Params, int32 PlayerCount, FRandomStream& Random);
}
