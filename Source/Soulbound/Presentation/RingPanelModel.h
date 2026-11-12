#pragma once

#include "CoreMinimal.h"
#include "Match/RingRules.h"

struct FMatchState;

/**
 * Lo que pinta el panel del anillo en un instante (S-026, S-031). Lo calcula la vista con el reloj real; el panel no sabe de la partida ni de
 * reglas. Sin Slate: se prueba sin editor (RingPanelModelTest.cpp).
 */
struct FRingPanelModel
{
	/** Si hay algo que enseñar: una prueba del humano en curso, o el resultado de la última durante un momento. */
	bool bVisible = false;
	/** El uso y su dificultad: "PRUEBA DEL WARDEN · normal". */
	FString Title;
	/** "pulso 2 de 3", o la instrucción del anuncio. */
	FString Counter;
	/** Si se pinta el anillo sobre la runa (durante la prueba; no en el resultado). */
	bool bShowRing = false;
	/** Fracción del cierre: 0 con el anillo en su radio mayor, 1 tocando la runa. */
	double CloseFraction = 0.0;
	/** Media zona buena como fracción del cierre: hasta dónde llega la franja verde (R-VIS-14). */
	double ZoneFraction = 0.0;
	/** Color y opacidad de la franja verde (RingZoneColor, configuración de la vista). */
	FLinearColor ZoneColor = FLinearColor(0.2f, 0.9f, 0.3f, 0.55f);
	/** Si el instante actual está dentro de la zona buena del pulso en curso: el aro se pinta verde y más grueso (R-VIS-14, R-VIS-15). */
	bool bInSweetZone = false;
	/** Un elemento por pulso: su precisión si ya se resolvió. */
	TArray<TOptional<double>> Pulses;
	/** Relleno de la barra de progreso, de 0 a 1 (R-VIS-16). */
	double ProgressFill = 0.0;
	/** Posición de la marca del objetivo en la barra; vacía en el duelo y el desafío final (R-VIS-16). */
	TOptional<double> ProgressMark;
	/** Color del relleno de la barra: el de la última precisión (R-VIS-16). */
	FLinearColor ProgressColor = FLinearColor::White;
	/** Color con que se tiñe la runa: el del Dominio en una Fuente, el azul de siempre en el resto (R-VIS-18). */
	FLinearColor RuneColor = FLinearColor(0.55f, 0.75f, 1.f);
	/** El resultado al acabar ("SUPERADA", "FALLADA"); vacío durante la prueba (R-VIS-17). */
	FString Big;
	FLinearColor BigColor = FLinearColor::White;
};

/** Relleno de la barra y, si la hay, posición de su marca (R-VIS-16). */
struct FRingProgress
{
	/** Suma de las precisiones resueltas dividida por el número de pulsos, de 0 a 1. */
	double Fill = 0.0;
	/** Posición de la marca: el umbral con el criterio de media; vacía con los demás. */
	TOptional<double> Mark;
};

namespace Soulbound::Presentation
{
	/**
	 * Radio del borde interior del aro en un instante (R-VIS-19). Con CloseFraction 1 (la coincidencia) es RuneRadius, de modo que el aro toca la runa
	 * exactamente entonces; con 0, MaxRadius; entre los dos, lineal. CloseFraction se acota a [0, 1]: pasada la coincidencia el aro se queda en el borde
	 * de la runa.
	 */
	double RingInnerRadius(double CloseFraction, double RuneRadius, double MaxRadius);

	/**
	 * Relleno y marca de la barra de progreso (R-VIS-16). Precisions: las resueltas. NumPulses: pulsos de la prueba. Criterion y PassThreshold: los del
	 * motor (FRingTrialParams), que la barra enseña sin reinterpretarlos. Devuelve el relleno (suma / NumPulses, acotado a [0, 1]; 0 sin pulsos) y la
	 * marca (PassThreshold) solo con MeanAtLeastThreshold.
	 */
	FRingProgress RingProgress(TConstArrayView<double> Precisions, int32 NumPulses, ERingCriterion Criterion, double PassThreshold);

	/**
	 * Dominio con que se tiñe la runa (R-VIS-18). State: la partida, en solo lectura. Seat: el asiento humano. Trial: la prueba del anillo. En una
	 * Fuente de un Dominio Olvidado, el Dominio de la casilla del asiento; en cualquier otra prueba, en un duelo (None), en el Vacío o con un
	 * asiento que no existe, INDEX_NONE.
	 */
	int32 RingRuneDomain(const FMatchState& State, int32 Seat, ESkillTrial Trial);
}
