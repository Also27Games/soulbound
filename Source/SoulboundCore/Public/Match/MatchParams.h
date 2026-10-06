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

	/**
	 * Reloj de elección (S-009, decisión del autor del 04/10/2026): segundos para contestar una elección tras mover
	 * (salto planar, acción contra jugador). Agotarlo equivale a "no", sin castigo. En datos; valor inicial 10.
	 */
	UPROPERTY()
	double ChoiceClockSeconds = 0.0;

	/** Trilero (R-ROB-2, S-010): segundos de la muestra, con las runas de la víctima a la vista. En datos; valor inicial 2. */
	UPROPERTY()
	double ShellRevealSeconds = 0.0;

	/** Trilero (R-ROB-2, S-010): segundos de la mezcla de las esferas. En datos; valor inicial 4. */
	UPROPERTY()
	double ShellShuffleSeconds = 0.0;

	/** Trilero (R-ROB-2, R-ROB-6, S-010): intercambios de esferas de la mezcla. En datos; valor inicial 8. */
	UPROPERTY()
	int32 ShellSwapCount = 0;

	/**
	 * Trilero (R-ROB-2, R-ROB-3, S-010): segundos que tiene el ladrón para elegir esfera; agotados, no roba nada. En datos;
	 * valor inicial 5.
	 */
	UPROPERTY()
	double ShellPickSeconds = 0.0;

	/**
	 * Duelo (R-ELE-2, S-012): ventaja de Shuten-dōji (afinidad DuelAdvantage), que se suma a su rendimiento en el duelo. Entre
	 * 0 y 1, el 1 excluido: un margen de 1 sería ganar siempre, otra regla (decisión del autor del 06/10/2026). Es cómo aplica
	 * la afinidad el doble de prueba; el minijuego real la aplicará como margen de acierto. En datos; valor inicial 0,15.
	 */
	UPROPERTY()
	double DuelAdvantageMargin = 0.0;
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
	 * Comprueba que unos parámetros de partida tienen sentido: reloj de turno, duración de la carrera, reloj de elección
	 * y tiempos del trilero (muestra, mezcla y elección de esfera) mayores que cero, topes enteros mayores o iguales que
	 * cero y al menos un intercambio en la mezcla. Params: los parámetros a comprobar. Devuelve None o MalformedData con el
	 * motivo; el campo Params del resultado queda vacío.
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
