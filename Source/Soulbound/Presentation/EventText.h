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
	FString DescribeEvent(const FMatchEvent& Event, const FBoardState& Board, const FWardenEventTable* WardenEvents = nullptr, const FWardenLineTable* WardenLines = nullptr);

	/**
	 * La frase de un WardenSpoke tal como se enseña (S-021, R-VOZ-5): la variante de la tabla sin las acotaciones de audio entre corchetes.
	 * Vacía si el evento no apunta a una entrada o a una variante que existan.
	 */
	FString SpokenText(const FWardenLineTable& Lines, const FMatchEvent& Spoke);

	/**
	 * El prefijo de la línea del Warden (S-021, R-VOZ-3b): a quién habla, porque el Warden no pronuncia nombres. "Warden:" a la mesa (Seat
	 * INDEX_NONE), "Warden, a ti:" al asiento humano de esta máquina y "Warden, al asiento 2:" a cualquier otro. HumanSeat: el asiento humano.
	 */
	FString WardenLinePrefix(int32 Seat, int32 HumanSeat);

	/**
	 * La línea de la crónica del juego para el evento Index del registro de State, con su contexto (S-021): la sentencia del Oráculo es una
	 * sola línea, "sentencia del Oráculo: <frase>", en vez de tres; con la tabla de frases vacía conserva las líneas de siempre. El resto
	 * es DescribeEvent. Vacía si el evento no tiene línea.
	 */
	FString DescribeChronicleEvent(const FMatchState& State, int32 Index);

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

	/** En qué fase se corta un turno (ETurnPhase en un Value), en español, para la crónica (S-016: TurnInterrupted). */
	FString TurnPhaseText(int32 Phase);

	/**
	 * Lo que pierde el asiento al que un golpe le corta el turno en la fase Phase (ETurnPhase en un Value), en español: la
	 * tirada antes de mover, el salto o la acción tras mover (S-016, TurnInterrupted).
	 */
	FString TurnCutLossText(int32 Phase);

	/** Gravedad de un castigo de insolencia (EInsolencePunishment en un Detail), en español (S-016, InsolencePunished). */
	FString InsolencePunishmentText(int32 Kind);

	/**
	 * La decisión del Warden para la crónica del juego (S-017, decisión del autor del 23/10/2026), solo si interviene con efecto:
	 * "el Warden decide: golpe "Impuesto" (Acaparador: asiento 2)". Decided: el WardenDecided; Triggers: los WardenTriggered que
	 * lo siguen; bViaOracle: si el golpe llega por el Oráculo. Vacía para un comentario, nada o un aplazamiento.
	 */
	FString DescribeWardenDecision(const FMatchEvent& Decided, const TArray<FMatchEvent>& Triggers, bool bViaOracle, const FWardenEventTable* WardenEvents = nullptr);

	/** De dónde viene una consulta del Oráculo (EOracleOrigin en el Value de OracleOpened), en español (S-017). */
	FString OracleOriginText(int32 Origin);

	/** Nombre en español de un disparador contextual (EWardenTrigger en el Value de WardenTriggered): "Líder claro" (S-017). */
	FString TriggerName(int32 Trigger);

	/** Motivo de una salida del Vacío (EVoidExit en el Value de LeftVoid), en español, para la crónica (S-013). */
	FString VoidExitText(int32 Exit);

	/** Quién origina el evento: los Sellos son del Warden, los de vínculo y fin de carrera de la carrera, el resto del asiento. */
	EEventSource EventSource(EMatchEventType Type);
}
