#pragma once

#include "CoreMinimal.h"
#include "../Profile/HintsMode.h"
#include "Warden/WardenLines.h"

struct FMatchState;

/**
 * Las instrucciones del Warden (S-021, A3; decisión del autor del 22/10/2026: el Warden enseña a jugar): qué momento acaba de darse y para qué
 * asiento, y si se enseña según el ajuste de ayudas y el perfil. Todo son funciones puras sobre el estado y el registro de eventos, sin actor,
 * sin mundo y sin disco, con tests como `BoardLayout`. No es regla de juego: no toca el estado del motor ni el registro.
 */
namespace Soulbound::Presentation
{
	/** Un momento de instrucción que acaba de darse (S-021) y el asiento humano al que le toca. */
	struct FInstructionCue
	{
		EInstructionMoment Moment = EInstructionMoment::Roll;
		int32 Seat = INDEX_NONE;
	};

	/**
	 * El seguimiento de qué momentos se han dado en la partida (S-021, R-INS, nombre propuesto). Cada momento se da una vez por partida, para el
	 * primer asiento humano al que le toca, y queda dado para toda la mesa. Detectar un momento lo consume, se enseñe o no (el ajuste lo decide
	 * después, ShouldTeach): cambiar el ajuste a mitad de partida no hace que salgan tarde las instrucciones.
	 */
	class FInstructionTracker
	{
	public:
		/**
		 * Mira lo que ha pasado desde la última vez (los eventos nuevos del registro y el estado de ahora) y devuelve los momentos que se dan,
		 * en el orden en que ocurren. HumanSeats: los asientos humanos de esta máquina, en orden de asiento; los dummies no reciben nada. Un
		 * registro más corto que lo recorrido es una partida nueva: el seguimiento empieza de cero.
		 */
		TArray<FInstructionCue> Observe(const FMatchState& State, const TArray<int32>& HumanSeats);

	private:
		/** Cuántos eventos del registro se han mirado. */
		int32 Scanned = 0;
		/** Los momentos ya dados, un bit por EInstructionMoment. */
		uint32 Given = 0;
	};

	/**
	 * Si el ajuste Mode enseña la instrucción Moment, dado el perfil (Seen: las claves `instruccion:<momento>` que ya ha visto), y la anota en
	 * Seen cuando la enseña (S-021, decisión del autor del 28/10/2026: en siempre también se anota, porque se ha visto). Automático: solo si
	 * Seen no la tiene. Siempre: sí. Nunca: no, y no anota nada.
	 */
	bool TeachAndRecord(EHintsMode Mode, TArray<FString>& Seen, EInstructionMoment Moment);

	/**
	 * Si el texto de petición ("← tu turno: clic en una casilla blanca" y equivalentes) se enseña: siempre en Siempre; en Automático, hasta que
	 * Seen tiene las diez instrucciones; nunca en Nunca. Las opciones numeradas y el tiempo se muestran siempre: son información de la partida.
	 */
	bool ShowsRequestHints(EHintsMode Mode, const TArray<FString>& Seen);

	/** El ajuste a partir de lo que se escribe en la consola ("auto", "siempre", "nunca"; sin distinguir mayúsculas). Falso si no es ninguno. */
	bool ParseHintsMode(const FString& Text, EHintsMode& OutMode);

	/** El nombre del ajuste en español, para la consola: "automático", "siempre" o "nunca". */
	FString HintsModeText(EHintsMode Mode);
}
