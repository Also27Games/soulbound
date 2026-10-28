#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/**
 * La voz del Warden (S-021, GDD 06: R-WAR-9, R-WAR-10, R-VOZ-5): elegir una variante de la tabla de frases con un sorteo propio y dejar
 * el `WardenSpoke` en el registro. El motor no guarda texto en el registro: la presentación lo lee de la tabla con la posición de la
 * entrada (Value) y la variante (Detail).
 */

/** Variante elegida de una entrada de la tabla (S-021, nombre propuesto): posición de la entrada y de la variante. */
struct SOULBOUNDCORE_API FWardenLineChoice
{
	int32 EntryIndex = INDEX_NONE;
	int32 Variant = INDEX_NONE;

	bool IsValid() const { return EntryIndex != INDEX_NONE && Variant != INDEX_NONE; }
};

namespace Soulbound::Rules
{
	/**
	 * Un sorteo de la voz que no es de una frase (S-021, R-COM-1): el que elige el tipo de un comentario. Como WardenDraw, la semilla de la
	 * voz combinada con su cuenta (FWardenState::VoiceSeed y VoiceDraws): no gasta el sorteo del Warden ni el de los dados.
	 */
	SOULBOUNDCORE_API FRandomStream VoiceDraw(FMatchState& State);

	/** `insolencia:<gravedad>` (leve, media, grave) para una gravedad de la tabla; `furia` para la Furia (R-INS-2, R-INS-3). */
	SOULBOUNDCORE_API FString InsolenceLineKey(EInsolencePunishment Punishment);

	/**
	 * Elige la variante de la frase Key (R-WAR-9). Con una variante, esa; con varias, una al azar distinta de la última usada para
	 * esa clave, que se recuerda en FWardenState::LastLineVariant. El sorteo es propio de la voz (FWardenState::VoiceSeed y VoiceDraws,
	 * la semilla de la partida más 5): no gasta valores del sorteo del Warden ni del de los dados. Con una variante no hay sorteo.
	 * State: la partida (se escribe el sorteo y la última variante). Key: la clave de la tabla de la partida.
	 * Devuelve la elección, o una inválida si la tabla no tiene la clave (la tabla vacía no habla).
	 */
	SOULBOUNDCORE_API FWardenLineChoice DrawWardenLine(FMatchState& State, const FString& Key);

	/**
	 * Dice la frase Key (R-WAR-10): la elige con DrawWardenLine y deja `WardenSpoke` (Seat el destinatario, INDEX_NONE a la mesa; Value
	 * la posición de la entrada; Detail la variante) en el registro con la marca NowSeconds. Sin la clave en la tabla no hace nada.
	 * Devuelve true si habló.
	 */
	SOULBOUNDCORE_API bool SpeakWardenLine(FMatchState& State, const FString& Key, double NowSeconds, int32 Seat = INDEX_NONE);
}
