#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/**
 * Texto de un evento del registro (R-TUR-15) para la vista de depuración (S-007). La vista tiene su propia función y
 * no comparte la de la crónica de la simulación (Simulation/MatchSimulation.cpp): compartirla obligaría a que
 * SoulboundCore ofreciera textos, y la story pide no tocarlo.
 */
namespace Soulbound::Presentation
{
	/** A quién pertenece un evento, para colorear su línea de la crónica: al asiento, al Warden o a la carrera. */
	enum class EEventSource : uint8
	{
		Seat,
		Warden,
		Race,
	};

	/** Frase del evento, en español, con el número de asiento y los datos del evento. */
	FString DescribeEvent(const FMatchEvent& Event);

	/** Quién origina el evento: los Sellos son del Warden, los de vínculo y fin de carrera de la carrera, el resto del asiento. */
	EEventSource EventSource(EMatchEventType Type);
}
