#pragma once

#include "CoreMinimal.h"
#include "WardenLines.generated.h"

struct FElectsTable;
struct FWardenEventTable;

/**
 * Tipo de comentario del Warden (R-COM-1, glosario `Remark`, S-021, nombre propuesto): once, no los doce del GDD 06. La presentación
 * de un Elegido (el duodécimo del GDD) no es un comentario: va por su propia frase, `presentacion:<dominio>` (IntroductionKey), que
 * dice `StartMatch`/`ClaimNiche` al vincularse (decisión del autor del 28/10/2026). La clave de la frase de cada tipo en frases.json
 * es `comentario:<tipo>` (WardenRemarkKey). Los elige la evaluación (MockLast, FiveRunes, Hurry, Hoarder, CutLaugh) o los dispara el
 * suceso (Beggar, Fool, Slow, Ungrateful, NotThisTime); LostBet es de la Fase 2 y no se dispara (corte A2).
 */
UENUM()
enum class EWardenRemarkKind : uint8
{
	/** Burla al último: el asiento con menos runas, o el último del orden si empatan. */
	MockLast,
	/** Aviso: alguien tiene cinco tipos de runa. */
	FiveRunes,
	/** Prisa: el activo lleva más de la mitad de su reloj sin entregar intención. */
	Hurry,
	/** El disparador Acaparador está activo. */
	Hoarder,
	/** Mendigo: retar a quien no tiene runas. */
	Beggar,
	/** Pardillo: retador sin runas que pierde. */
	Fool,
	/** Lento: trilero sin elegir. */
	Slow,
	/** Desagradecido: regalo ignorado (`WardenTaunt`, R-DIR-4). */
	Ungrateful,
	/** Apuesta fallida: Fase 2, la clave existe y no se dispara. */
	LostBet,
	/** Risa al cortar un turno. */
	CutLaugh,
	/** "Esta vez no" de la Furia (`FuryRefusal`). */
	NotThisTime,
};

/**
 * Momento de instrucción (decisión del autor del 22/10/2026, S-021, nombre propuesto): los diez momentos en que el Warden enseña a
 * jugar, y desde S-027 (R-INS-1) cuatro Ayudas más: Eco, robar, Vacío y anillo. Su frase es `instruccion:<momento>` (InstructionKey). El
 * enum vive en el motor porque el cargador exige sus claves; quien decide cuándo se da cada uno es la presentación (`FInstructionTracker`,
 * corte A3). Los nuevos van al final: el perfil guarda las claves, no los valores, pero así nada cambia de posición.
 */
UENUM()
enum class EInstructionMoment : uint8
{
	/** Primer turno propio. */
	Roll,
	/** Primera elección de destino. */
	ChooseTile,
	/** Primera vez que un Nicho libre está al alcance. */
	Niche,
	/** Primera vez que cae en una Fuente. */
	Source,
	/** Primera elección pendiente de evento. */
	Dilemma,
	/** Primer duelo. */
	Duel,
	/** Primera consulta del Oráculo. */
	Orb,
	/** Primera votación. */
	Vote,
	/** La primera vez que alguien de la mesa junta seis tipos distintos. */
	SixRunes,
	/** Primera subida de insolencia. */
	Insolence,
	/** S-027 (R-INS-1): el humano queda como Eco al acabar la carrera (BondRaceEnded con su asiento sin vínculo). */
	Echo,
	/** S-027 (R-INS-1): la primera elección PlayerAction abierta al humano que incluye robar. */
	Steal,
	/** S-027 (R-INS-1): el humano entra en el Vacío por primera vez (Banished sobre su asiento). */
	Void,
	/** S-027 (R-INS-1): el anfitrión abre el anillo para el humano por primera vez (lo detecta la vista: el anillo no es un evento del motor). */
	Ring,
};

/** Una entrada de frases.json (R-VOZ-5, S-021, nombre propuesto): una situación con una o varias variantes. */
USTRUCT()
struct SOULBOUNDCORE_API FWardenLineEntry
{
	GENERATED_BODY()

	/** Clave de la situación: `evento:<id>`, `comentario:<tipo>`, `programado:inicio`... (ver RequiredWardenLineKeys). */
	UPROPERTY()
	FString Id;

	/** Variantes de la frase, al menos una y ninguna vacía. Las acotaciones de audio van entre corchetes (`[risa]`). */
	UPROPERTY()
	TArray<FString> Lines;
};

/**
 * Tabla de frases del Warden (R-VOZ-5, glosario `WardenLineTable`, S-021, nombre propuesto): las entradas de
 * Content/Data/Warden/frases.json, en el orden del archivo. La posición de una entrada es lo que guarda `WardenSpoke` en su Value.
 * Vacía, el Warden no habla: así los tests de reglas que no miran la voz no ven ningún `WardenSpoke`.
 */
USTRUCT()
struct SOULBOUNDCORE_API FWardenLineTable
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FWardenLineEntry> Entries;

	/** Posición de la entrada Key, o INDEX_NONE si no está. */
	int32 IndexOf(const FString& Key) const
	{
		return Entries.IndexOfByPredicate([&Key](const FWardenLineEntry& Entry) { return Entry.Id == Key; });
	}

	bool IsEmpty() const { return Entries.IsEmpty(); }
};

/** Qué incumple un archivo de frases rechazado (CA-VOZ-2, S-021, nombre propuesto). */
enum class EWardenLinesError : uint8
{
	None,
	/** No se pudo leer el archivo. */
	FileNotFound,
	/** El texto no es JSON o no es una lista de {`id`, `lines`} con `id` texto y `lines` lista de textos. */
	MalformedData,
	/** Falta una clave exigida (el mensaje la nombra). */
	MissingKey,
	/** Dos entradas con la misma clave. */
	DuplicateKey,
	/** Una entrada sin variantes, o con una variante vacía (el mensaje nombra la clave). */
	EmptyLine,
};

/** Resultado de cargar o validar las frases: o éxito con la tabla, o la regla incumplida con su motivo. Nombre propuesto en S-021. */
struct SOULBOUNDCORE_API FWardenLinesLoadResult
{
	EWardenLinesError Error = EWardenLinesError::None;

	/** Motivo legible del rechazo, con la clave afectada. Vacío si hay éxito. */
	FString Message;

	/** La tabla cargada. Solo tiene sentido si IsSuccess(). */
	FWardenLineTable Table;

	bool IsSuccess() const { return Error == EWardenLinesError::None; }
};

namespace Soulbound::Warden
{
	/** Claves de las frases programadas (R-PRO-1 a R-PRO-5, R-INS-3). Las de `programado:orden-n` salen de ScheduledOrderKey. */
	inline const FString LineStart = TEXT("programado:inicio");
	inline const FString LineCorruption = TEXT("programado:corrupcion");
	/** La última frase de la apertura, después del orden: con ella empieza el reloj de partida (R-PRO-1, S-023, nombre propuesto). */
	inline const FString LineBegin = TEXT("programado:comienzo");
	inline const FString LineRaceEnd = TEXT("programado:fin-carrera");
	inline const FString LineRaceEndAll = TEXT("programado:fin-carrera-todos");
	inline const FString LineAspirant = TEXT("programado:aspirante");
	inline const FString LineLastMinute = TEXT("programado:ultimo-minuto");
	inline const FString LineTenSeconds = TEXT("programado:diez-segundos");
	inline const FString LineTimeOut = TEXT("programado:fin-tiempo");
	/** La victoria de un jugador (`MatchWon`). La del Warden por no haber vínculos es `LineNobodyBonded` (decisión del autor del 28/10/2026). */
	inline const FString LineVictory = TEXT("programado:victoria");
	inline const FString LineNobodyBonded = TEXT("programado:nadie-vinculado");
	/**
	 * El fin de la Noche de los Ecos por el tope de rondas (R-VOZ-9, S-043, nombre propuesto): justo después de EchoNightEnded, a la mesa. Por la Ley
	 * de Sombras no: ese evento ya trae su frase.
	 */
	inline const FString LineEchoNightEnd = TEXT("programado:fin-noche-ecos");
	inline const FString LineFury = TEXT("furia");

	/** Segundos que faltan para matchEndSeconds en cada aviso del final (R-PRO-5, decisión del autor del 27/10/2026). */
	inline constexpr int32 LastMinuteWarningSeconds = 60;
	inline constexpr int32 TenSecondsWarningSeconds = 10;

	/** Cuántas posiciones tiene el orden de turno (`programado:orden-1` a `programado:orden-4`). */
	inline constexpr int32 OrderLineCount = 4;
}

namespace Soulbound::Rules
{
	/** `evento:<id>`. */
	SOULBOUNDCORE_API FString WardenEventKey(const FString& EventId);

	/** `comentario:<tipo>`, con el tipo en español y en minúsculas con guiones (burla-al-ultimo, cinco-runas...). */
	SOULBOUNDCORE_API FString WardenRemarkKey(EWardenRemarkKind Kind);

	/** `instruccion:<momento>` (tirar, elegir-casilla, nicho, fuente, dilema, duelo, orbe, voto, seis-runas, insolencia; desde S-027, eco, robar, vacio, anillo). */
	SOULBOUNDCORE_API FString InstructionKey(EInstructionMoment Moment);

	/** `programado:orden-n` para la posición Position (de 1 a OrderLineCount) del orden de turno. */
	SOULBOUNDCORE_API FString ScheduledOrderKey(int32 Position);

	/** `presentacion:<dominio>`: la presentación del Elegido de ese Dominio (0 a 5). */
	SOULBOUNDCORE_API FString IntroductionKey(int32 DomainId);

	/**
	 * Las claves que frases.json debe traer (R-VOZ-5), construidas de los enums y de las tablas, sin una lista escrita a mano:
	 * una por evento de Events que esté en algún saco (FWardenEventDef::bInBag; los de fuera pueden tener frase y no se exige), por tipo de comentario, por momento de instrucción, por gravedad de insolencia y la Furia, por frase
	 * programada y una presentación por Elegido de Elects. Events: la tabla de eventos del Warden; Elects: los Elegidos.
	 */
	SOULBOUNDCORE_API TArray<FString> RequiredWardenLineKeys(const FWardenEventTable& Events, const FElectsTable& Elects);

	/**
	 * Comprueba una tabla contra las claves exigidas Required: sin claves repetidas, ninguna ausente y ninguna variante vacía.
	 * Devuelve None o la primera regla incumplida nombrando la clave; el campo Table del resultado queda vacío.
	 */
	SOULBOUNDCORE_API FWardenLinesLoadResult ValidateWardenLines(const FWardenLineTable& Table, const TArray<FString>& Required);

	/**
	 * Lee las frases de un texto JSON y las valida contra Required. Esquema en Content/Data/Warden/frases.json: una lista de
	 * {`id`, `lines`}. Se leen enteras o se rechazan enteras. Devuelve la tabla o la regla incumplida; nunca lanza.
	 */
	SOULBOUNDCORE_API FWardenLinesLoadResult LoadWardenLinesFromJsonString(const FString& Json, const TArray<FString>& Required);

	/** Lee las frases de un archivo JSON (ruta absoluta o relativa al directorio de trabajo). Devuelve FileNotFound o lo de LoadWardenLinesFromJsonString. */
	SOULBOUNDCORE_API FWardenLinesLoadResult LoadWardenLinesFromFile(const FString& FilePath, const TArray<FString>& Required);

	/** La frase sin acotaciones de audio (`[risa]`) ni los espacios que dejan: lo que enseña la presentación (R-VOZ-5). */
	SOULBOUNDCORE_API FString StripLineCues(const FString& Line);
}
