#pragma once

#include "CoreMinimal.h"
#include "Match/EffectCatalog.h"
#include "Warden/WardenParams.h"
#include "WardenEvents.generated.h"

namespace Soulbound::Match
{
	/** Opciones de un evento: de una a cuatro (R-COM-1). Una, decreto; varias, dilema. Regla que acota los datos. */
	inline constexpr int32 MaxEventOptions = 4;
}

/** Forma de entrega de un evento (R-COM-1). En S-014 solo se aplicaba la directa; el Oráculo, desde S-017. Nombre propuesto en S-014. */
UENUM()
enum class EEventDelivery : uint8
{
	/** El Warden directo. */
	Direct,
	/**
	 * El Oráculo del Caos (R-ORA-1): desde S-017, un golpe con esta entrega llega como sentencia que hay que consultar; también es
	 * candidato de las sentencias de un Nodo y de una Consulta forzada. Solo decretos. SbEvent lo aplica como directa.
	 */
	Oracle,
	/** Directa u Oráculo, a elección del Warden (S-016): desde S-017, un golpe va por el Oráculo a partes iguales; una escena, directa. */
	Any,
};

/** Aplicación de un efecto (R-COM-1, S-014, nombre propuesto): qué efecto, a quién, cuánto y de qué forma. */
USTRUCT()
struct SOULBOUNDCORE_API FEffectApplication
{
	GENERATED_BODY()

	UPROPERTY()
	EEffect Effect = EEffect::Halt;

	/** Dirección (R-DIR-1); None solo en el Sello. */
	UPROPERTY()
	EEffectTarget Target = EEffectTarget::None;

	/** Cantidad: de 1 a Soulbound::Effects::MaxAmount en Turno perdido y Turno extra; 1 en el resto. */
	UPROPERTY()
	int32 Amount = 1;

	/** Variante (Perder una runa, Duelo forzado); None en el resto. */
	UPROPERTY()
	EEffectVariant Variant = EEffectVariant::None;
};

/** Opción de un evento (R-COM-1, S-014, nombre propuesto): lista ordenada de aplicaciones, que se resuelven en orden. */
USTRUCT()
struct SOULBOUNDCORE_API FWardenEventOption
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FEffectApplication> Applications;
};

/**
 * Evento del Warden (glosario `WardenEvent`, R-COM-1, S-014): identificador, texto, forma de entrega y de una a cuatro
 * opciones. Una opción, decreto; varias, dilema (R-COM-2). Desde S-016 (R-SAC-1, R-SAC-2), su saco: nombre, acto, canal,
 * peso por acto, usos máximos, ventana y corte. Un evento sin acto está fuera de los sacos: solo lo lanzan SbEvent y los tests.
 */
USTRUCT()
struct SOULBOUNDCORE_API FWardenEventDef
{
	GENERATED_BODY()

	/** Identificador en los datos (`id`), único en el archivo. */
	UPROPERTY()
	FString Id;

	/** Texto del evento, en español. */
	UPROPERTY()
	FString Text;

	UPROPERTY()
	EEventDelivery Delivery = EEventDelivery::Direct;

	UPROPERTY()
	TArray<FWardenEventOption> Options;

	/** Nombre del evento en el juego (`name`, S-016), el que dice la crónica. Vacío en los de prueba: se usa el identificador. */
	UPROPERTY()
	FString Name;

	/** Si está en un saco (glosario `EventBag`): los datos le dan un acto (`act`). S-016. */
	UPROPERTY()
	bool bInBag = false;

	/** Acto de su saco (`act`: "I", "II" o "III"); solo vale con bInBag. Los sacos son acumulativos (R-SAC-1). S-016. */
	UPROPERTY()
	EAct Act = EAct::ActOne;

	/** Canal (`channel`: "strike" o "scene"), golpe o escena (R-WAR-4). Obligatorio en un saco. S-016. */
	UPROPERTY()
	EWardenChannel Channel = EWardenChannel::Strike;

	/**
	 * Peso en cada acto (`weights`, glosario `EventBag`), indexado por EAct, cero o más. Por defecto, 1 en su acto y en los
	 * siguientes y 0 en los anteriores (la story: donde el GDD no dice nada). S-016.
	 */
	UPROPERTY()
	TArray<double> Weights;

	/** Usos máximos por partida (glosario `MaxUses`, `maxUses`), de 1 en adelante; 0, sin límite. S-016. */
	UPROPERTY()
	int32 MaxUses = 0;

	/**
	 * Ventana (glosario `EventWindow`, `window`: `from` y `to`, los dos opcionales): segundos del reloj de partida entre los
	 * que es candidato. Sin `to`, hasta el final (WindowToSeconds negativo). S-016.
	 */
	UPROPERTY()
	double WindowFromSeconds = 0.0;

	UPROPERTY()
	double WindowToSeconds = -1.0;

	/** Si corta el turno en curso (glosario `CutsTurn`, `cutsTurn`): por defecto, sí en un golpe y no en una escena. S-016. */
	UPROPERTY()
	bool bCutsTurn = false;

	/**
	 * Si es una votación (glosario `Vote`, `vote`, R-VOT-6, S-017): una escena de una sola opción con un solo efecto dirigido al más
	 * votado (MostVoted), con entrega directa. Nombre propuesto en S-017.
	 */
	UPROPERTY()
	bool bVote = false;

	/** Si es un dilema (más de una opción); si no, un decreto. */
	bool IsDilemma() const { return Options.Num() > 1; }

	/** Nombre para la crónica: Name, o el identificador si no lo tiene. */
	const FString& DisplayName() const { return Name.IsEmpty() ? Id : Name; }

	/** Si el instante NowSeconds está dentro de su ventana. */
	bool IsInWindow(double NowSeconds) const { return NowSeconds >= WindowFromSeconds && (WindowToSeconds < 0.0 || NowSeconds <= WindowToSeconds); }

	/** Peso en el acto Act; 0 si no lo tiene. */
	double WeightIn(EAct InAct) const { return Weights.IsValidIndex(static_cast<int32>(InAct)) ? Weights[static_cast<int32>(InAct)] : 0.0; }
};

/** Tabla de eventos del Warden (S-014, nombre propuesto): los de Content/Data/Events/eventos.json, en el orden del archivo. */
USTRUCT()
struct SOULBOUNDCORE_API FWardenEventTable
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FWardenEventDef> Events;

	/** Posición del evento con identificador Id, o INDEX_NONE si no está. */
	int32 IndexOf(const FString& Id) const { return Events.IndexOfByPredicate([&Id](const FWardenEventDef& Event) { return Event.Id == Id; }); }
};

/** Qué incumple un archivo de eventos rechazado (CA-EFE-1). Nombre propuesto en S-014. */
enum class EWardenEventsError : uint8
{
	None,
	/** No se pudo leer el archivo. */
	FileNotFound,
	/** El texto no es JSON, o falta un campo, o tiene un tipo equivocado (id, texto, entrega, opciones). */
	MalformedData,
	/** Un efecto que no está en el catálogo (R-EFE-0). */
	UnknownEffect,
	/** Una dirección que no existe o que el efecto no permite, o que falta (R-EFE-0, R-DIR-1). */
	TargetNotAllowed,
	/** Una cantidad fuera de 1 a 2 en Turno perdido o Turno extra, o en un efecto que no la admite. */
	InvalidAmount,
	/** Una variante que no existe o que el efecto no admite. */
	VariantNotAllowed,
	/** Un evento sin opciones o con más de cuatro, o una opción sin aplicaciones (R-COM-1). */
	InvalidOptionCount,
	/** Dos eventos con el mismo identificador. */
	DuplicateId,
	/**
	 * Un saco mal formado (S-016, R-SAC-2): acto o canal que no existen, canal que falta en un evento con acto, peso negativo,
	 * pesos que no son tres, usos máximos menores que 1 o una ventana vacía o negativa.
	 */
	InvalidBag,
	/**
	 * Un dilema que puede llegar por el Oráculo, con entrega "oracle" o "any" (R-ORA-1, S-017): el Oráculo solo entrega decretos.
	 * Nombre propuesto en S-017.
	 */
	OracleDilemma,
	/**
	 * Una votación mal formada (R-VOT-6, S-017): con más de una opción o de un efecto, con entrega que no es directa, con una dirección
	 * que no es MostVoted, con un efecto que no la admite (AdmitsMostVoted) o en un saco que no es de escena; o MostVoted en un evento
	 * que no es votación. Nombre propuesto en S-017.
	 */
	InvalidVote,
};

/** Resultado de cargar o validar los eventos: o éxito con la tabla, o la regla incumplida con su motivo. Nombre propuesto en S-014. */
struct SOULBOUNDCORE_API FWardenEventsLoadResult
{
	EWardenEventsError Error = EWardenEventsError::None;

	/** Motivo legible del rechazo: qué evento, qué opción y qué se esperaba. Vacío si hay éxito. */
	FString Message;

	/** La tabla cargada. Solo tiene sentido si IsSuccess(). */
	FWardenEventTable Table;

	bool IsSuccess() const { return Error == EWardenEventsError::None; }
};

namespace Soulbound::Rules
{
	/**
	 * Comprueba una tabla de eventos contra el catálogo (CA-EFE-1, R-EFE-0, R-COM-1): identificadores no vacíos y únicos,
	 * de una a cuatro opciones con al menos una aplicación cada una, y en cada aplicación una dirección permitida por su
	 * efecto (None solo en el Sello), una cantidad de 1 a 2 solo donde se admite (1 en el resto) y una variante admitida.
	 * Desde S-016 (R-SAC-2, InvalidBag): en un evento de un saco, canal golpe o escena, tres pesos no negativos y usos no
	 * negativos; en todos, una ventana que empieza en 0 o después y no acaba antes de empezar.
	 * Table: la tabla. Devuelve None o la primera regla incumplida con su motivo; el campo Table del resultado queda vacío.
	 */
	SOULBOUNDCORE_API FWardenEventsLoadResult ValidateWardenEvents(const FWardenEventTable& Table);

	/**
	 * Lee los eventos de un texto JSON y los valida. Se leen enteros o se rechazan enteros. Esquema en
	 * Content/Data/Events/eventos.json: `events`, una lista de {`id`, `text`, `delivery` ("direct", "oracle" o, desde S-016,
	 * "any"), `options`}, cada opción una lista de {`effect`, `target`, `amount` opcional, `variant` opcional}. El Sello va sin
	 * `target`. Desde S-016, el saco, opcional: `name`, `act` ("I", "II", "III"), `channel` ("strike", "scene"; obligatorio con
	 * `act`), `weights` (tres números; por defecto 1 en su acto y los siguientes), `maxUses` (de 1 en adelante; sin campo, sin
	 * límite), `window` ({`from`, `to`}, segundos) y `cutsTurn` (por defecto, el golpe corta y la escena no).
	 * Devuelve la tabla, o la regla incumplida con su motivo.
	 */
	SOULBOUNDCORE_API FWardenEventsLoadResult LoadWardenEventsFromJsonString(const FString& Json);

	/**
	 * Lee los eventos de un archivo JSON y los valida. FilePath: ruta absoluta, o relativa al directorio de trabajo.
	 * Devuelve la tabla, o FileNotFound y el resto de errores de LoadWardenEventsFromJsonString.
	 */
	SOULBOUNDCORE_API FWardenEventsLoadResult LoadWardenEventsFromFile(const FString& FilePath);
}
