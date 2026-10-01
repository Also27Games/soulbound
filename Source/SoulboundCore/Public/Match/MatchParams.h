#pragma once

#include "CoreMinimal.h"
#include "MatchParams.generated.h"

/**
 * Parámetros de partida: valores de ajuste que no dependen del tablero. Se cargan de Content/Data/Match/partida.json;
 * ningún valor vive en el código. Lo que el GDD fija como regla (caras del dado, tope de insolencia) no está aquí:
 * son constantes con nombre junto a la regla que las usa.
 */
USTRUCT()
struct SOULBOUNDCORE_API FMatchParams
{
	GENERATED_BODY()

	/** Reloj de turno (R-TUR-8): segundos desde que empieza el turno hasta que el jugador confirma destino. En datos. */
	UPROPERTY()
	double TurnClockSeconds = 0.0;

	/** Tope de turnos extra acumulados por jugador (R-TUR-9, GDD 05). En datos. */
	UPROPERTY()
	int32 MaxExtraTurns = 0;

	/** Tope de turnos perdidos acumulados por jugador (R-TUR-10, GDD 05). En datos. */
	UPROPERTY()
	int32 MaxLostTurns = 0;

	/**
	 * Duración de la carrera por el vínculo (R-CAR-1, R-TAB-27), en segundos del reloj de partida desde su inicio.
	 * En datos; valor inicial 300 (S-006).
	 */
	UPROPERTY()
	double RaceDurationSeconds = 0.0;
};

/** Por qué se rechazaron unos parámetros de partida. */
enum class EMatchParamsError : uint8
{
	None,
	/** No se pudo leer el archivo. */
	FileNotFound,
	/** El texto no es JSON, o le falta un campo, o tiene un tipo equivocado o un valor fuera de rango. */
	MalformedData,
};

/** Resultado de cargar o validar parámetros de partida: o éxito con los parámetros, o fallo con un motivo legible. */
struct SOULBOUNDCORE_API FMatchParamsLoadResult
{
	EMatchParamsError Error = EMatchParamsError::None;

	/** Motivo legible del rechazo: qué campo y qué se esperaba. Vacío si hay éxito. */
	FString Message;

	/** Los parámetros cargados. Solo tienen sentido si IsSuccess(). */
	FMatchParams Params;

	bool IsSuccess() const { return Error == EMatchParamsError::None; }
};

namespace Soulbound::Rules
{
	/**
	 * Comprueba que unos parámetros de partida tienen sentido: reloj de turno y duración de la carrera mayores que
	 * cero, y topes enteros mayores o iguales que cero. Params: los parámetros a comprobar. Devuelve None o MalformedData con el motivo;
	 * el campo Params del resultado queda vacío.
	 */
	SOULBOUNDCORE_API FMatchParamsLoadResult ValidateMatchParams(const FMatchParams& Params);

	/**
	 * Lee parámetros de partida de un texto JSON y los valida. Se leen enteros o se rechazan enteros.
	 * Json: el contenido del archivo (esquema en Content/Data/Match/partida.json).
	 * Devuelve los parámetros, o MalformedData con su motivo.
	 */
	SOULBOUNDCORE_API FMatchParamsLoadResult LoadMatchParamsFromJsonString(const FString& Json);

	/**
	 * Lee parámetros de partida de un archivo JSON y los valida. FilePath: ruta absoluta, o relativa al directorio
	 * de trabajo. Devuelve los parámetros, o FileNotFound y el resto de errores de LoadMatchParamsFromJsonString.
	 */
	SOULBOUNDCORE_API FMatchParamsLoadResult LoadMatchParamsFromFile(const FString& FilePath);
}
