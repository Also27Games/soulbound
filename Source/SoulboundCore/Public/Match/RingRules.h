#pragma once

#include "CoreMinimal.h"
#include "Match/MatchParams.h"
#include "Match/MatchState.h"

// Reglas del anillo (glosario "Anillo", RingPulse; GDD 09; S-026, nombre propuesto RingRules): el minijuego de habilidad sin mundo.
// Funciones puras, sin estado de partida: precisión de un pulso, secuencia de una prueba, parámetros de cada uso y resultado. Los
// valores salen de FMatchParams::Ring (partida.json); el criterio de cada uso es regla y vive aquí (decisión del autor del 06/11/2026).

/** Cómo se juzga una prueba del anillo (R-MIN-4, R-MIN-12). Regla del GDD 09, no dato. Nombre propuesto en S-026. */
enum class ERingCriterion : uint8
{
	/** Superada si la media de precisiones es mayor o igual que el umbral de la dificultad (Fuente, Prueba del Warden, escape). */
	MeanAtLeastThreshold,
	/** Superada si ninguna precisión es 0 (desafío final, R-MIN-12). */
	NoMiss,
	/** No se supera ni se falla: el rendimiento es la media, de 0 a 1 (duelo). */
	Performance,
};

/** Parámetros de un uso del anillo ya resueltos (R-MIN-6, R-MIN-8): lo que se juega en una prueba concreta. Nombre propuesto en S-026. */
struct FRingTrialParams
{
	/** Dificultad con que se juega. */
	ESkillDifficulty Difficulty = ESkillDifficulty::Normal;
	/** Segundos del cierre del anillo, desde que empieza el pulso hasta la coincidencia. */
	double CloseSeconds = 0.0;
	/** Zona buena como fracción del cierre. */
	double SweetZone = 0.0;
	/** Número de pulsos. */
	int32 Pulses = 0;
	/** Umbral de la media (solo con MeanAtLeastThreshold). */
	double PassThreshold = 0.0;
	/** Cómo se juzga. */
	ERingCriterion Criterion = ERingCriterion::MeanAtLeastThreshold;

	/** Anchura de la zona buena en segundos, centrada en la coincidencia. */
	double SweetZoneSeconds() const { return CloseSeconds * SweetZone; }
};

/**
 * Secuencia de una prueba (R-MIN-3; nombre propuesto en S-026): cuándo empieza cada pulso, en segundos desde el inicio de la prueba, y
 * cuánto dura su cierre. El pulso i empieza en PulseStarts[i], coincide en PulseStarts[i] + CloseSeconds y vence al acabar su zona buena,
 * media zona después de la coincidencia. Todos los pulsos se cierran (sin sentido inverso en S-026).
 */
struct FRingSequence
{
	/** Inicio de cada pulso; el primero en 0. */
	TArray<double> PulseStarts;
	/** Segundos del cierre del anillo. */
	double CloseSeconds = 0.0;
	/** Anchura de la zona buena en segundos. */
	double SweetZoneSeconds = 0.0;

	/** Número de pulsos. */
	int32 Num() const { return PulseStarts.Num(); }
	/** Instante de la coincidencia del pulso Pulse. */
	double CoincidenceAt(int32 Pulse) const { return PulseStarts[Pulse] + CloseSeconds; }
	/** Instante en que vence el pulso Pulse: el final de su zona buena. */
	double ExpiryAt(int32 Pulse) const { return CoincidenceAt(Pulse) + SweetZoneSeconds * 0.5; }
};

namespace Soulbound::Ring
{
	/**
	 * Tolerancia de la comparación de la media con el umbral (decisión del autor del 06/11/2026): sin ella, 0,6 / 0,5 / 0,55 da una media
	 * de 0,5499… en coma flotante y no supera un umbral de 0,55 (CA-MIN-12). Es aritmética, no un valor de ajuste.
	 */
	inline constexpr double MeanTolerance = 1e-9;
}

namespace Soulbound::Rules
{
	/**
	 * Precisión de un pulso (R-MIN-2): 1 en la coincidencia exacta, decrece linealmente hasta 0 en el borde de la zona buena; fuera, 0.
	 * CoincidenceSeconds: instante de la coincidencia. PressSeconds: instante de la pulsación, sin valor si no la hubo. SweetZoneSeconds:
	 * anchura de la zona buena en segundos. Devuelve max(0, 1 − |d| / (Z / 2)); sin pulsación o con zona no positiva, 0.
	 */
	SOULBOUNDCORE_API double PulsePrecision(double CoincidenceSeconds, TOptional<double> PressSeconds, double SweetZoneSeconds);

	/**
	 * Parámetros del anillo para la prueba Trial con la dificultad Difficulty (R-MIN-6, R-MIN-8), la que el motor dejó en
	 * FMatchState::PendingDifficulty. Ring: el bloque de datos. Cierre, zona y umbral de la dificultad; pulsos del uso; en el desafío final,
	 * su zona propia y el criterio sin fallos (R-MIN-12). Devuelve vacío si Trial es None.
	 */
	SOULBOUNDCORE_API TOptional<FRingTrialParams> RingParamsForTrial(const FRingParams& Ring, ESkillTrial Trial, ESkillDifficulty Difficulty);

	/** Parámetros del anillo en un duelo (R-MIN-8): la dificultad del duelo de datos, sus pulsos y el criterio de rendimiento. Ring: el bloque de datos. */
	SOULBOUNDCORE_API FRingTrialParams RingParamsForDuel(const FRingParams& Ring);

	/**
	 * Secuencia de una prueba (R-MIN-3). Ring: el bloque de datos (huecos mínimo y máximo). Trial: el uso (cierre, zona, pulsos). Seed:
	 * la semilla; la misma semilla da la misma secuencia. El primer pulso empieza en 0 y cada siguiente tras un hueco al azar entre
	 * PulseGapMinSeconds y PulseGapMaxSeconds contado desde que vence el anterior (decisión del autor del 06/11/2026). Sin pulsos, vacía.
	 */
	SOULBOUNDCORE_API FRingSequence MakeRingSequence(const FRingParams& Ring, const FRingTrialParams& Trial, int32 Seed);

	/**
	 * Resultado de una prueba (R-MIN-4, R-MIN-12). Trial: el uso. Precisions: una por pulso. MeanAtLeastThreshold: media ≥ umbral (con
	 * Soulbound::Ring::MeanTolerance); NoMiss: ninguna precisión es 0. Sin precisiones, o con el criterio Performance, falso.
	 */
	SOULBOUNDCORE_API bool RingTrialPassed(const FRingTrialParams& Trial, TConstArrayView<double> Precisions);

	/** Rendimiento de un duelo (R-MIN-4): la media de Precisions, acotada de 0 a 1. Sin precisiones, 0. */
	SOULBOUNDCORE_API double RingDuelPerformance(TConstArrayView<double> Precisions);
}
