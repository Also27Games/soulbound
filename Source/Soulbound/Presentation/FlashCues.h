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
		/** La casilla de un Sello (S-037, R-VIS-25): `Id` es el identificador de la casilla. */
		Tile,
	};

	/**
	 * Hacia dónde va el destello (S-029, R-VIS-12, punto 8 de la story, corrección del autor del 13/11/2026): hacia dentro es recibir y hacia fuera es
	 * perder, igual para la ficha que para la Fuente. El aro o el halo se cierra (Inward) o se abre (Outward) mientras dura.
	 */
	enum class EFlashDirection : uint8
	{
		Inward,
		Outward,
	};

	/** De dónde sale el color del destello: el del Dominio de su runa, o el de perder (ABoardViewActor::FlashLossColor). */
	enum class EFlashColorSource : uint8
	{
		Rune,
		Loss,
		/** El color del Sello (S-037, R-VIS-25): ABoardViewActor::SealMarkColor. */
		Seal,
	};

	/**
	 * Un destello (S-029, R-VIS-12; nombre propuesto `Flash`): qué parpadea, hacia dónde va, de qué color, cuánto dura y cuánto espera tras el instante en
	 * que se leyó. Los de un mismo objetivo salen encadenados: el k-ésimo espera k duraciones.
	 */
	struct FFlashCue
	{
		EFlashTarget Target = EFlashTarget::Token;
		/** El asiento si Target es Token; el tipo de runa (el Dominio de la Fuente) si es Source. */
		int32 Id = INDEX_NONE;
		EFlashDirection Direction = EFlashDirection::Inward;
		EFlashColorSource ColorSource = EFlashColorSource::Rune;
		/** El tipo de runa implicado (el Dominio cuyo color usa ColorSource Rune); INDEX_NONE si el evento no lo trae. */
		int32 RuneType = INDEX_NONE;
		float DurationSeconds = 0.f;
		float DelaySeconds = 0.f;

		bool operator==(const FFlashCue& Other) const
		{
			return Target == Other.Target && Id == Other.Id && Direction == Other.Direction && ColorSource == Other.ColorSource && RuneType == Other.RuneType
				&& DurationSeconds == Other.DurationSeconds && DelaySeconds == Other.DelaySeconds;
		}
	};

	/**
	 * Convierte los eventos nuevos del registro en destellos (R-VIS-12, punto 8):
	 *  - RuneObtained: la ficha hacia dentro con el color de la runa, y la Fuente hacia fuera con ese color.
	 *  - RuneReturned: la ficha hacia fuera con el color de perder, y la Fuente hacia dentro con el color de su runa.
	 *  - RuneStolen y RunesSeized: el que se queda la runa (Seat), hacia dentro con su color; el que la pierde (OtherSeat), hacia fuera con el de perder. Ninguna
	 *    Fuente: la runa no pasa por ella.
	 *  - SealPlaced: la casilla hacia dentro con el color del Sello; SealReleased: la casilla hacia fuera con el mismo (S-037, R-VIS-25).
	 *  - FinalChallengeFailed: la ficha hacia fuera con el de perder; el RuneReturned que le sigue del mismo asiento y tipo solo aporta la Fuente, para no
	 *    destellar dos veces la ficha.
	 * @param State Estado de la partida, en solo lectura.
	 * @param FromIndex Primer evento del registro por leer (los anteriores ya se leyeron).
	 * @param FlashSeconds Duración de cada destello (ABoardViewActor::FlashSeconds).
	 * @return Los destellos en el orden del registro, con el retardo de encadenado ya puesto.
	 */
	SOULBOUND_API TArray<FFlashCue> FlashCuesFromEvents(const FMatchState& State, int32 FromIndex, float FlashSeconds);

	/**
	 * El color de un destello (punto 8): el de perder si ColorSource es Loss; si no, el del Dominio de su runa, y el claro de siempre si el tipo no se conoce
	 * o no tiene color.
	 * @param Cue El destello.
	 * @param DomainColors El color de cada Dominio, el de la leyenda.
	 * @param ClearColor El claro de siempre (ABoardViewActor::FlashGainColor).
	 * @param LossColor El color de perder (ABoardViewActor::FlashLossColor).
	 * @param SealColor El color del Sello (ABoardViewActor::SealMarkColor), el de los destellos de casilla (S-037).
	 * @return El color del destello.
	 */
	SOULBOUND_API FLinearColor FlashColorOf(const FFlashCue& Cue, const TArray<FLinearColor>& DomainColors, const FLinearColor& ClearColor, const FLinearColor& LossColor,
		const FLinearColor& SealColor);
}
