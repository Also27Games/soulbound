#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"
#include "Match/DebugEdits.h"
#include "Match/MatchState.h"

/**
 * Texto de un evento del registro (R-TUR-15) para la vista de depuración (S-007). La vista tiene su propia función y
 * no comparte la de la crónica de la simulación (Simulation/MatchSimulation.cpp): compartirla obligaría a que
 * SoulboundCore ofreciera textos. El motor solo añadió el evento Moved (S-007), sin texto.
 */
namespace Soulbound::Presentation
{
	/** A quién pertenece un evento, para colorear su línea de la crónica: al asiento, al Warden o a la carrera. */
	enum class EEventSource : uint8
	{
		Seat,
		Warden,
		Race,
		/** Una edición de depuración (S-011): su línea lleva un color propio para que nunca pase por una partida normal. */
		Debug,
	};

	/** Nombre corto de una casilla, el de su etiqueta: "D3·41" (Dominio e identificador) o "ALTAR". */
	FString TileShortName(const FBoardState& Board, int32 TileId);

	/** Frase del evento, en español, con el número de asiento y los datos del evento. Board da los nombres de casilla. */
	FString DescribeEvent(const FMatchEvent& Event, const FBoardState& Board);

	/** Nombre en español del tipo de una edición de depuración (EDebugEditKind en Event.Value), para la crónica. */
	FString DebugKindName(int32 Kind);

	/** Por qué se rechazó una edición de depuración (S-011), en español, para el mensaje en pantalla de los comandos. */
	FString DescribeDebugResult(EDebugEditResult Result);

	/** Por qué se rechazó un Sello de depuración (S-011): el motivo de CanSealTile, en español. */
	FString DescribeSealResult(ESealResult Result);

	/** Quién origina el evento: los Sellos son del Warden, los de vínculo y fin de carrera de la carrera, el resto del asiento. */
	EEventSource EventSource(EMatchEventType Type);
}
