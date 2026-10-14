#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"
#include "Match/DebugEdits.h"
#include "Match/MatchState.h"
#include "Match/WardenEventRules.h"

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

	/**
	 * Frase del evento, en español, con el número de asiento y los datos del evento. Board da los nombres de casilla;
	 * WardenEvents, el texto de los eventos del Warden (S-014), si se tiene.
	 */
	FString DescribeEvent(const FMatchEvent& Event, const FBoardState& Board, const FWardenEventTable* WardenEvents = nullptr);

	/** Nombre en español de un efecto (EEffect en un Value), para la crónica y la vista (S-014). */
	FString EffectName(int32 Effect);

	/** Dirección en español ("al líder"...; vacía sin dirección), de un EEffectTarget en un Detail (S-014). */
	FString EffectTargetName(int32 Target);

	/** Una aplicación de efecto en español: "Turno extra ×2 al activo" (S-014). */
	FString DescribeApplication(const FEffectApplication& Application);

	/** Una opción de un evento: sus aplicaciones en orden, separadas por " y luego " (S-014). */
	FString DescribeOption(const FWardenEventOption& Option);

	/** Por qué no se lanzó un evento (S-014: SbEvent), en español. */
	FString DescribeWardenEventStart(EWardenEventStart Result);

	/** Qué decide una elección (EChoiceKind en un Value), en español, para la crónica (S-014: la desatención). */
	FString ChoiceKindText(int32 Kind);

	/** Nombre en español del tipo de una edición de depuración (EDebugEditKind en Event.Value), para la crónica. */
	FString DebugKindName(int32 Kind);

	/** Por qué se rechazó una edición de depuración (S-011), en español, para el mensaje en pantalla de los comandos. */
	FString DescribeDebugResult(EDebugEditResult Result);

	/** Por qué se rechazó un Sello de depuración (S-011): el motivo de CanSealTile, en español. */
	FString DescribeSealResult(ESealResult Result);

	/** Por qué se rechazó un efecto de depuración (S-013: SbBanish, SbRelease, SbSendHome): el motivo de la regla, en español. */
	FString DescribeEffectResult(EEffectResult Result);

	/** Nombre en español de una dificultad del minijuego (ESkillDifficulty en un Value), para la crónica (S-013). */
	FString DifficultyText(int32 Difficulty);

	/** Motivo de una salida del Vacío (EVoidExit en el Value de LeftVoid), en español, para la crónica (S-013). */
	FString VoidExitText(int32 Exit);

	/** Quién origina el evento: los Sellos son del Warden, los de vínculo y fin de carrera de la carrera, el resto del asiento. */
	EEventSource EventSource(EMatchEventType Type);
}
