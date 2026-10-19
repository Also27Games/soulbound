#pragma once

#include "CoreMinimal.h"
#include "Match/EffectCatalog.h"
#include "WardenParams.generated.h"

namespace Soulbound::Warden
{
	/** Actos de una partida (R-WAR-2: tres). Es la estructura de la partida, no un valor de ajuste. */
	inline constexpr int32 ActCount = 3;

	/** Canales del Warden (R-WAR-4: golpe, escena y comentario). */
	inline constexpr int32 ChannelCount = 3;

	/** Gravedades de un castigo de insolencia (R-INS-2: leve, media y grave). */
	inline constexpr int32 InsolenceSeverityCount = 3;
}

/**
 * Acto de la partida (glosario `ActOne`, `ActTwo`, `ActThree`; R-WAR-2): los tramos que cambian el comportamiento del Warden.
 * Es una función del estado (CurrentAct), no un contador. Nombre del glosario; el tipo, propuesto en S-016.
 */
UENUM()
enum class EAct : uint8
{
	ActOne,
	ActTwo,
	ActThree,
};

/**
 * Canal del Warden (R-WAR-4, glosario `Strike`, `Scene`, `Remark`): cómo llega una intervención. Un evento de los datos va
 * por golpe o escena; el comentario no aplica nada. Nombre propuesto en S-016.
 */
UENUM()
enum class EWardenChannel : uint8
{
	/** Golpe: llega en cualquier instante de un turno y lo corta (R-TUR-11). */
	Strike,
	/** Escena: entra entre turnos y no corta, salvo que el evento lo diga (`cutsTurn`). */
	Scene,
	/** Comentario: sin efecto y sin cortar. En S-016 es solo una decisión registrada (WardenRemark). */
	Remark,
};

/** Lo que el Warden hace en un acto (R-WAR-3): probabilidad de intervenir, pesos de canal y objetivo de cortes. S-016. */
USTRUCT()
struct SOULBOUNDCORE_API FWardenActParams
{
	GENERATED_BODY()

	/** Inicio del acto en segundos del reloj de partida (R-WAR-2). En datos; valores iniciales 0, 300 y 1620. */
	UPROPERTY()
	double StartSeconds = 0.0;

	/** Probabilidad de intervenir en cada evaluación (R-WAR-3), de 0 a 1. En datos; valores iniciales 0,20, 0,35 y 0,50. */
	UPROPERTY()
	double InterveneProbability = 0.0;

	/**
	 * Pesos de canal (R-WAR-3), indexados por EWardenChannel: golpe, escena y comentario. Cero o más. En datos; valores iniciales
	 * 40/20/40, 45/25/30 y 50/25/25.
	 */
	UPROPERTY()
	TArray<double> ChannelWeights;

	/**
	 * Objetivo de turnos cortados en el acto (decisión del autor del 19/10/2026: se mide e imprime, no se exige), de 0 a 1. En
	 * datos; valores iniciales 0,15, 0,25 y 0,35.
	 */
	UPROPERTY()
	double CutTarget = 0.0;
};

/**
 * Fila de la tabla de insolencia (R-INS-2): para los niveles anteriores a la subida que cubre, el porcentaje de cada gravedad
 * del castigo (leve, media, grave), o la Furia sin sorteo. S-016.
 */
USTRUCT()
struct SOULBOUNDCORE_API FInsolenceRow
{
	GENERATED_BODY()

	/** Niveles de insolencia anteriores a la subida a los que se aplica la fila, de 0 a MaxInsolence - 1. */
	UPROPERTY()
	TArray<int32> Levels;

	/** Porcentajes de leve, media y grave, que suman 100. Vacío en la fila de la Furia. */
	UPROPERTY()
	TArray<int32> SeverityPercents;

	/** Si la subida desde estos niveles es la Furia (R-INS-3): sin sorteo. */
	UPROPERTY()
	bool bFury = false;
};

/**
 * Parámetros del Warden (S-016): los valores de ajuste del cerebro, en Content/Data/Warden/warden.json; ninguno vive en el
 * código. Lo que es regla (tres actos, tres canales, tope de insolencia) es una constante. Viajan en FMatchState::WardenParams.
 * Sin actos (Acts vacío), la partida no tiene Warden: el cerebro no decide nada.
 */
USTRUCT()
struct SOULBOUNDCORE_API FWardenParams
{
	GENERATED_BODY()

	/** Los tres actos, en orden (R-WAR-2, R-WAR-3). */
	UPROPERTY()
	TArray<FWardenActParams> Acts;

	/** Fin de la partida en segundos del reloj (R-WAR-4d: el margen se mide también contra él). En datos; valor inicial 2400. */
	UPROPERTY()
	double MatchEndSeconds = 0.0;

	/** Intervalo entre evaluaciones (R-WAR-3). Mayor que cero. En datos; valor inicial 15. */
	UPROPERTY()
	double EvaluationIntervalSeconds = 0.0;

	/** Variación del intervalo, más o menos, sorteada (R-WAR-3). De cero a menos que el intervalo. En datos; valor inicial 5. */
	UPROPERTY()
	double EvaluationVariationSeconds = 0.0;

	/** Garantía (glosario `InterventionGuarantee`, R-WAR-5): segundos sin golpe ni escena que no se dejan pasar. Mayor que cero. En datos; valor inicial 120. */
	UPROPERTY()
	double GuaranteeSeconds = 0.0;

	/** Margen (R-WAR-4d): últimos segundos de un acto y de la partida sin intervenciones ni comentarios. Mayor que cero. En datos; valor inicial 12. */
	UPROPERTY()
	double MarginSeconds = 0.0;

	/** Tabla de insolencia (R-INS-2): cada nivel de 0 a MaxInsolence - 1 en una sola fila. La usa la insolencia con castigo. */
	UPROPERTY()
	TArray<FInsolenceRow> InsolenceTable;

	/** Si hay Warden: los datos tienen actos. */
	bool HasWarden() const { return !Acts.IsEmpty(); }
};

/** Qué incumple un archivo del Warden rechazado (CA-SAC-1). Nombre propuesto en S-016. */
enum class EWardenParamsError : uint8
{
	None,
	/** No se pudo leer el archivo. */
	FileNotFound,
	/** El texto no es JSON, o falta un campo, o tiene un tipo equivocado, o no hay tres actos. */
	MalformedData,
	/** Los inicios de los actos no crecen (el primero en 0, cada uno mayor que el anterior y menor que el fin de la partida). */
	ActsNotIncreasing,
	/** Una probabilidad de intervenir o un objetivo de cortes fuera de [0, 1]. */
	ProbabilityOutOfRange,
	/** Un peso de canal negativo, o todos a cero en un acto. */
	NegativeWeight,
	/** Una fila de la tabla de insolencia que no suma 100, o niveles que faltan, se repiten o se salen de 0 a 4. */
	InsolenceTableInvalid,
	/** Intervalo, garantía o margen no positivos, o variación negativa o no menor que el intervalo. */
	NonPositiveDuration,
};

/** Resultado de cargar o validar los parámetros del Warden. Nombre propuesto en S-016. */
struct SOULBOUNDCORE_API FWardenParamsLoadResult
{
	EWardenParamsError Error = EWardenParamsError::None;

	/** Motivo legible del rechazo. Vacío si hay éxito. */
	FString Message;

	/** Los parámetros cargados. Solo tienen sentido si IsSuccess(). */
	FWardenParams Params;

	bool IsSuccess() const { return Error == EWardenParamsError::None; }
};

namespace Soulbound::Rules
{
	/**
	 * Comprueba unos parámetros del Warden (CA-SAC-1): tres actos con inicios crecientes desde 0 y antes del fin de la partida,
	 * probabilidades y objetivos de cortes en [0, 1], tres pesos de canal no negativos y no todos nulos, intervalo, garantía y
	 * margen positivos, variación de cero a menos que el intervalo, y la tabla de insolencia con cada nivel de 0 a
	 * MaxInsolence - 1 en una sola fila y cada fila sin Furia sumando 100. Devuelve None o la primera regla incumplida.
	 */
	SOULBOUNDCORE_API FWardenParamsLoadResult ValidateWardenParams(const FWardenParams& Params);

	/**
	 * Lee los parámetros del Warden de un texto JSON y los valida; se leen enteros o se rechazan enteros. Esquema en
	 * Content/Data/Warden/warden.json. Devuelve los parámetros, o la regla incumplida con su motivo.
	 */
	SOULBOUNDCORE_API FWardenParamsLoadResult LoadWardenParamsFromJsonString(const FString& Json);

	/** Lee los parámetros del Warden de un archivo. Devuelve los parámetros, o FileNotFound y los errores de LoadWardenParamsFromJsonString. */
	SOULBOUNDCORE_API FWardenParamsLoadResult LoadWardenParamsFromFile(const FString& FilePath);
}
