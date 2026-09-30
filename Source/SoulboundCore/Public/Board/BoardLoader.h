#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"

/** Qué regla incumple un tablero rechazado. Cada valor corresponde a una comprobación de ValidateBoard o del lector. */
enum class EBoardError : uint8
{
	None,
	/** No se pudo leer el archivo. */
	FileNotFound,
	/** El texto no es JSON o le falta un campo obligatorio, con un tipo equivocado o un valor fuera de rango. */
	MalformedData,
	/** Identificadores de casilla no contiguos o repetidos, o una conexión con una casilla inexistente, consigo misma o en un solo sentido (R-TAB-1). */
	InvalidGraph,
	/** No hay exactamente una casilla Altar (R-TAB-2). */
	AltarCount,
	/** No hay exactamente seis Dominios con identificadores de 0 a 5 (R-TAB-3). */
	DomainCount,
	/** Una casilla que no es el Altar no pertenece a un Dominio válido, o el Altar pertenece a uno. */
	TileWithoutDomain,
	/** Un Dominio no tiene ninguna conexión con el Altar (R-TAB-3). */
	DomainNotConnectedToAltar,
	/** Un Dominio no tiene conexión con uno de sus dos vecinos del anillo (R-TAB-3). */
	BrokenRing,
	/** Un Dominio no tiene exactamente un Nicho (R-TAB-4). */
	NicheCount,
	/** Un Nicho no tiene exactamente una conexión (R-TAB-4c). */
	NicheNotDeadEnd,
	/** Un Dominio no tiene el número de Fuentes de Poder que dicen los datos (R-TAB-4). */
	PowerSourceCount,
	/** El número de Nodos de Poder no coincide con la suma de las cantidades por efecto (R-TAB-5). */
	PowerNodeCount,
};

/** Resultado de cargar o validar un tablero: o éxito con el tablero, o fallo con la regla incumplida y un motivo legible. */
struct SOULBOUNDCORE_API FBoardLoadResult
{
	EBoardError Error = EBoardError::None;

	/** Motivo legible del rechazo: qué regla se incumple y dónde. Vacío si hay éxito. */
	FString Message;

	/** El tablero cargado. Solo tiene sentido si IsSuccess(). */
	FBoardState Board;

	bool IsSuccess() const { return Error == EBoardError::None; }
};

namespace Soulbound::Rules
{
	/**
	 * Comprueba que un tablero cumple la estructura del GDD 01: grafo coherente (R-TAB-1), un Altar (R-TAB-2),
	 * seis Dominios en anillo conectados al Altar y a sus vecinos (R-TAB-3), un Nicho sin salida y las Fuentes
	 * de los datos por Dominio (R-TAB-4, 4c) y tantos Nodos como piden las cantidades por efecto (R-TAB-5).
	 * Board: el tablero a comprobar. Devuelve la primera regla incumplida con su motivo, o None si es válido;
	 * el campo Board del resultado queda vacío.
	 */
	SOULBOUNDCORE_API FBoardLoadResult ValidateBoard(const FBoardState& Board);

	/**
	 * Lee un tablero de un texto JSON y lo valida con ValidateBoard. Se lee entero o se rechaza entero.
	 * Json: el contenido del archivo (esquema en Content/Data/Boards/tablero_pruebas.json).
	 * El bloque opcional `editor.positions` ("id": [x, y]) rellena FTileState::LayoutPosition; sin él, las casillas
	 * quedan sin posición y el tablero carga igual. Un bloque mal formado es MalformedData.
	 * Devuelve el tablero, o la regla incumplida con su motivo.
	 */
	SOULBOUNDCORE_API FBoardLoadResult LoadBoardFromJsonString(const FString& Json);

	/**
	 * Lee un tablero de un archivo JSON y lo valida. FilePath: ruta absoluta, o relativa al directorio de trabajo.
	 * Devuelve el tablero, o FileNotFound y el resto de errores de LoadBoardFromJsonString.
	 */
	SOULBOUNDCORE_API FBoardLoadResult LoadBoardFromFile(const FString& FilePath);
}
