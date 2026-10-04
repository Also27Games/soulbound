#pragma once

#include "CoreMinimal.h"
#include "Elects/Elects.h"

/** Qué regla incumple una tabla de Elegidos rechazada. Nombre propuesto en S-008. */
enum class EElectsError : uint8
{
	None,
	/** No se pudo leer el archivo. */
	FileNotFound,
	/** El texto no es JSON, o falta un campo, o tiene un tipo o un valor desconocido (poder, afinidad, Dominio), o no hay seis Elegidos, uno por Dominio, con seis runas cada uno. */
	MalformedData,
	/** Alguna fila o columna del mapa de runa a poder no contiene los seis poderes una vez (R-ELE-3). */
	NotLatinSquare,
	/** La diagonal del mapa no es Celeridad: la runa del propio Dominio da Celeridad (R-ELE-3, R-ECO-7). */
	DiagonalNotHaste,
};

/** Resultado de cargar o validar los Elegidos: o éxito con la tabla, o fallo con la regla incumplida y un motivo legible. */
struct SOULBOUNDCORE_API FElectsLoadResult
{
	EElectsError Error = EElectsError::None;

	/** Motivo legible del rechazo. Vacío si hay éxito. */
	FString Message;

	/** La tabla cargada. Solo tiene sentido si IsSuccess(). */
	FElectsTable Table;

	bool IsSuccess() const { return Error == EElectsError::None; }
};

namespace Soulbound::Rules
{
	/**
	 * Comprueba que una tabla de Elegidos cumple R-ELE-1 y R-ELE-3: seis Elegidos con nombre, uno por Dominio, seis
	 * entradas por mapa, cada fila y cada columna con los seis poderes una vez y la diagonal en Celeridad.
	 * Table: la tabla a comprobar. Devuelve None o la primera regla incumplida con su motivo; el campo Table del
	 * resultado queda vacío.
	 */
	SOULBOUNDCORE_API FElectsLoadResult ValidateElects(const FElectsTable& Table);

	/**
	 * Lee los Elegidos de un texto JSON y los valida con ValidateElects. Se leen enteros o se rechazan enteros.
	 * Json: el contenido del archivo (esquema en Content/Data/Elects/elegidos.json: `elects`, una fila por Dominio con
	 * `domain`, `name`, `affinity` y `powers`, que mapea cada runa "0" a "5" a un poder).
	 * Devuelve la tabla, o la regla incumplida con su motivo.
	 */
	SOULBOUNDCORE_API FElectsLoadResult LoadElectsFromJsonString(const FString& Json);

	/**
	 * Lee los Elegidos de un archivo JSON y los valida. FilePath: ruta absoluta, o relativa al directorio de trabajo.
	 * Devuelve la tabla, o FileNotFound y el resto de errores de LoadElectsFromJsonString.
	 */
	SOULBOUNDCORE_API FElectsLoadResult LoadElectsFromFile(const FString& FilePath);
}
