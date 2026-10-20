#pragma once

#include "CoreMinimal.h"
#include "MatchParams.generated.h"

namespace Soulbound::Match
{
	/**
	 * Peldaños máximos de la escalera de escape del Vacío (R-VAC-6, decisión del autor del 08/10/2026): con tres, el cuarto
	 * turno dentro sale sin jugar, así que nadie pasa del cuarto. Es la regla que acota los datos, no un valor de ajuste.
	 */
	inline constexpr int32 MaxVoidEscapeSteps = 3;
}

/**
 * Dificultad de una prueba (glosario "Minijuego de habilidad"). Nombre propuesto en S-008. Desde S-013 vive aquí porque la
 * escalera de escape del Vacío, que está en los parámetros de partida, es una lista de dificultades.
 */
UENUM()
enum class ESkillDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
};

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

	/**
	 * Escalera de escape del Vacío (R-VAC-2, S-013): dificultad del minijuego en el primer turno propio dentro, en el
	 * segundo y así; el turno siguiente al último peldaño sale sin jugar. De 1 a Soulbound::Match::MaxVoidEscapeSteps
	 * peldaños (R-VAC-6). En datos (voidEscapeLadder); valor inicial difícil, normal, fácil.
	 */
	UPROPERTY()
	TArray<ESkillDifficulty> VoidEscapeLadder;

	/**
	 * Escalera de escape de Mictlantecuhtli (afinidad VoidEscape, R-ELE-2, S-013): la misma regla con menos peldaños; de 1 a
	 * los de VoidEscapeLadder. En datos (voidEscapeAffinityLadder); valor inicial fácil.
	 */
	UPROPERTY()
	TArray<ESkillDifficulty> VoidEscapeAffinityLadder;

	/**
	 * Escalera del Vacío de la Furia (R-INS-3, S-016, decisión del autor del 20/10/2026): la Furia castiga al jugador, no al
	 * personaje, y dentro de su Vacío todos juegan esta, sin la salida del Eco ni la afinidad de Mictlantecuhtli. La misma regla
	 * que la principal: de 1 a Soulbound::Match::MaxVoidEscapeSteps peldaños. En datos (furyEscapeLadder); valor inicial difícil,
	 * difícil, normal.
	 */
	UPROPERTY()
	TArray<ESkillDifficulty> FuryEscapeLadder;

	/** Revelar Brazal (R-EFE-2, S-014): rondas que dura revelado. Entero de 1 en adelante. En datos; valor inicial 1. */
	UPROPERTY()
	int32 RevealBracerRounds = 0;

	/** Marcado (R-EFE-8, S-014): rondas que dura la marca sin cobrarse. Entero de 1 en adelante. En datos; valor inicial 3. */
	UPROPERTY()
	int32 MarkRounds = 0;

	/**
	 * Noche de los Ecos (R-EFE-18, S-014): rondas tras las que termina sola si no llega la Ley de Sombras. Entero de 1 en
	 * adelante. En datos; valor inicial 8.
	 */
	UPROPERTY()
	int32 EchoNightMaxRounds = 0;

	/**
	 * Reloj del dilema (R-COM-2, S-014): segundos base para elegir opción; se suma DilemmaClockPerOptionSeconds por cada
	 * opción a partir de la segunda. Mayor que cero. En datos; valor inicial 8.
	 */
	UPROPERTY()
	double DilemmaClockSeconds = 0.0;

	/** Reloj del dilema (R-COM-2, S-014): incremento por opción, en segundos. Cero o más. En datos; valor inicial 3. */
	UPROPERTY()
	double DilemmaClockPerOptionSeconds = 0.0;

	/**
	 * Reloj corto (R-DIR-1, R-EFE-11, R-EFE-16, S-014): segundos para señalar a un rival, elegir la runa regalada o la runa a
	 * perder. Mayor que cero. En datos; valor inicial 6 (decisión del autor del 12/10/2026).
	 */
	UPROPERTY()
	double ShortChoiceClockSeconds = 0.0;

	/**
	 * Prueba del Warden (R-EFE-10, S-015): dificultad del minijuego de habilidad que juega quien la tiene pendiente al empezar
	 * su turno. Una de las tres. En datos (wardenTrialDifficulty); valor inicial normal.
	 */
	UPROPERTY()
	ESkillDifficulty WardenTrialDifficulty = ESkillDifficulty::Normal;

	/**
	 * Duelo contra el Warden (S-015): rendimiento fijo del Warden, de 0 a 1, con el que se bate quien desatiende un Duelo forzado.
	 * Es lo que decide si el Warden gana la mayoría de las veces. En datos (wardenDuelPerformance); valor inicial 0,75.
	 */
	UPROPERTY()
	double WardenDuelPerformance = 0.0;
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
	 * cero, al menos un intercambio en la mezcla, el margen del duelo entre 0 y 1, el 1 excluido (S-012), y las escaleras de
	 * escape del Vacío (S-013): la principal de 1 a Soulbound::Match::MaxVoidEscapeSteps peldaños, la de la afinidad de 1 a
	 * los de la principal y, desde S-016, la del Vacío de la Furia como la principal, todas con dificultades conocidas; y los de los efectos y eventos del Warden (S-014): rondas de
	 * Brazal revelado, marca y tope de la Noche de 1 en adelante, reloj base del dilema y reloj corto mayores que cero, e
	 * incremento por opción del dilema de cero en adelante; y los de S-015: la dificultad de la Prueba del Warden, una de las
	 * tres, y el rendimiento del Warden en su duelo, de 0 a 1. Params: los parámetros a comprobar. Devuelve None o
	 * MalformedData con el motivo; el campo Params del resultado queda vacío.
	 */
	SOULBOUNDCORE_API FMatchParamsLoadResult ValidateMatchParams(const FMatchParams& Params);

	/**
	 * Lee parámetros de partida de un texto JSON y los valida. Se leen enteros o se rechazan enteros.
	 * Json: el contenido del archivo (esquema en Content/Data/Match/partida.json). Las escaleras del Vacío son listas de
	 * textos "easy", "normal" y "hard"; otro texto es MalformedData.
	 * Devuelve los parámetros, o MalformedData con su motivo.
	 */
	SOULBOUNDCORE_API FMatchParamsLoadResult LoadMatchParamsFromJsonString(const FString& Json);

	/**
	 * Lee parámetros de partida de un archivo JSON y los valida. FilePath: ruta absoluta, o relativa al directorio
	 * de trabajo. Devuelve los parámetros, o FileNotFound y el resto de errores de LoadMatchParamsFromJsonString.
	 */
	SOULBOUNDCORE_API FMatchParamsLoadResult LoadMatchParamsFromFile(const FString& FilePath);
}
