#pragma once

#include "CoreMinimal.h"
#include "HintsMode.generated.h"

/**
 * El ajuste de ayudas del jugador (S-021, A3; glosario "ayudas", nombres propuestos): cuánto le explica el Warden y la línea de petición. Vive en el
 * perfil del jugador (UPlayerProfile), no en la partida. Desde S-022, BlueprintType con el nombre en español, para el ajuste de Opciones de UGT.
 */
UENUM(BlueprintType)
enum class EHintsMode : uint8
{
	/** Valor inicial: una instrucción se enseña solo si el perfil no la tiene, y al enseñarla se guarda. */
	Automatic UMETA(DisplayName = "Automático"),
	/** Se enseña cada instrucción una vez por partida, sin mirar el perfil. */
	Always UMETA(DisplayName = "Siempre"),
	/** No se enseña ninguna. */
	Never UMETA(DisplayName = "Nunca"),
};
