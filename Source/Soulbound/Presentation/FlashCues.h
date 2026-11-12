#pragma once

#include "CoreMinimal.h"

struct FMatchState;

namespace Soulbound::Presentation
{
	/** Qué parpadea: la ficha de un asiento o la Fuente de un tipo de runa (glosario `PowerSource`; su Dominio es el tipo). */
	enum class EFlashTarget : uint8
	{
		Token,
		Source,
	};

	/**
	 * Un destello (S-029, R-VIS-12; nombre propuesto `Flash`): qué parpadea, si es claro (se gana) o rojo (se pierde), cuánto dura y cuánto
	 * espera tras el instante en que se leyó. Los de un mismo objetivo salen encadenados: el k-ésimo espera k duraciones.
	 */
	struct FFlashCue
	{
		EFlashTarget Target = EFlashTarget::Token;
		/** El asiento si Target es Token; el tipo de runa (el Dominio de la Fuente) si es Source. */
		int32 Id = INDEX_NONE;
		/** Claro (gana una runa) o rojo (la pierde). */
		bool bGain = true;
		float DurationSeconds = 0.f;
		float DelaySeconds = 0.f;

		bool operator==(const FFlashCue& Other) const
		{
			return Target == Other.Target && Id == Other.Id && bGain == Other.bGain && DurationSeconds == Other.DurationSeconds && DelaySeconds == Other.DelaySeconds;
		}
	};

	/**
	 * Convierte los eventos nuevos del registro en destellos (R-VIS-12): RuneObtained (ficha y Fuente, claro), RuneReturned (ficha y Fuente,
	 * rojo), RuneStolen (ladrón claro, víctima rojo), RunesSeized (ganador claro, perdedor rojo) y FinalChallengeFailed (la ficha, rojo; el
	 * RuneReturned que le sigue del mismo asiento y tipo solo aporta la Fuente, para no destellar dos veces la ficha). Robo y confiscación no
	 * tocan ninguna Fuente: la runa no pasa por ella.
	 * @param State Estado de la partida, en solo lectura.
	 * @param FromIndex Primer evento del registro por leer (los anteriores ya se leyeron).
	 * @param FlashSeconds Duración de cada destello (ABoardViewActor::FlashSeconds).
	 * @return Los destellos en el orden del registro, con el retardo de encadenado ya puesto.
	 */
	SOULBOUND_API TArray<FFlashCue> FlashCuesFromEvents(const FMatchState& State, int32 FromIndex, float FlashSeconds);
}
