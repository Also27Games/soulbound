#pragma once

#include "CoreMinimal.h"
#include "Match/EffectCatalog.h"
#include "WardenEvents.generated.h"

namespace Soulbound::Match
{
	/** Opciones de un evento: de una a cuatro (R-COM-1). Una, decreto; varias, dilema. Regla que acota los datos. */
	inline constexpr int32 MaxEventOptions = 4;
}

/** Forma de entrega de un evento (R-COM-1). En S-014 solo se aplica la directa. Nombre propuesto en S-014. */
UENUM()
enum class EEventDelivery : uint8
{
	/** El Warden directo. */
	Direct,
	/** El Oráculo del Caos: se carga, pero se aplica como directa hasta su story. */
	Oracle,
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
 * opciones. Una opción, decreto; varias, dilema (R-COM-2). Los usos, ventanas, sacos y canal son de S-016.
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

	/** Si es un dilema (más de una opción); si no, un decreto. */
	bool IsDilemma() const { return Options.Num() > 1; }
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
	 * Table: la tabla. Devuelve None o la primera regla incumplida con su motivo; el campo Table del resultado queda vacío.
	 */
	SOULBOUNDCORE_API FWardenEventsLoadResult ValidateWardenEvents(const FWardenEventTable& Table);

	/**
	 * Lee los eventos de un texto JSON y los valida. Se leen enteros o se rechazan enteros. Esquema en
	 * Content/Data/Events/eventos.json: `events`, una lista de {`id`, `text`, `delivery` ("direct" u "oracle"), `options`},
	 * cada opción una lista de {`effect`, `target`, `amount` opcional, `variant` opcional}. El Sello va sin `target`.
	 * Devuelve la tabla, o la regla incumplida con su motivo.
	 */
	SOULBOUNDCORE_API FWardenEventsLoadResult LoadWardenEventsFromJsonString(const FString& Json);

	/**
	 * Lee los eventos de un archivo JSON y los valida. FilePath: ruta absoluta, o relativa al directorio de trabajo.
	 * Devuelve la tabla, o FileNotFound y el resto de errores de LoadWardenEventsFromJsonString.
	 */
	SOULBOUNDCORE_API FWardenEventsLoadResult LoadWardenEventsFromFile(const FString& FilePath);
}
