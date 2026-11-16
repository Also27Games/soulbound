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
	/** El uso de la prueba, sin el nivel: "PRUEBA DEL WARDEN" (R-VIS-22). */
	FString Title;
	/** Las líneas de la instrucción bajo el título, cada una centrada y pintada por separado; durante toda la prueba, antes y después del anuncio (R-VIS-17). Vacío al acabar. */
	TArray<FString> Instruction;
	/** La nota del resultado, bajo el título, al acabar; vacía durante la prueba. */
	FString Note;
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
	/** El texto del botón "Empezar" mientras la prueba espera (S-042, R-MIN-19); vacío con la prueba empezada o sin prueba: no hay botón. */
	FString StartLabel;
	/** Su tecla, bajo el botón: "o pulsa Intro" (S-042); vacía sin botón. */
	FString StartKeyHint;
	/** La cuenta atrás, bajo la tecla: "Empieza sola en N s" (S-042); vacía sin botón. */
	FString StartCountdown;
};

struct FRingTrialRun;

/**
 * Dónde va cada cosa del panel del anillo, de arriba abajo, en unidades del panel (S-042, corrección del autor del 16/11/2026; nombre propuesto). Lo
 * calcula RingPanelLayout; lo usan SRingPanel para pintar y la vista para poner la fila de "Empezar" en su franja.
 */
struct FRingPanelLayout
{
	/** Altura del centro del aro y de la runa. */
	float CenterY = 0.f;
	/** Radio mayor del aro (el de la espera y el de cada pulso al empezar). */
	float MaxRadius = 0.f;
	/** Franja del botón "Empezar", su tecla y la cuenta: debajo del aro y encima de la barra. Arriba y abajo. */
	float StartBandTop = 0.f;
	float StartBandBottom = 0.f;
	/** Borde de arriba de la barra de progreso (R-VIS-16). */
	float BarTop = 0.f;
	/** Altura del centro de las marcas de los pulsos. */
	float DotsY = 0.f;
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

	/**
	 * El nombre del uso de la prueba (R-VIS-22), sin el nivel: lo usa la línea "Prueba pendiente" del panel de estado. Desde S-042 el panel del anillo
	 * lleva RingStakeTitle. Trial: la prueba del anillo. bDuel: si es un duelo (gana a Trial). Devuelve "DUELO", "FUENTE DE UN DOMINIO OLVIDADO",
	 * "PRUEBA DEL WARDEN", "ESCAPE DEL VACÍO", "DESAFÍO FINAL" o, sin uso conocido, "PRUEBA".
	 */
	FString RingTitleText(ESkillTrial Trial, bool bDuel);

	/**
	 * Título del panel del anillo con lo que te juegas (S-042, R-VIS-30; nombre propuesto): el nombre de la prueba y lo que pasa si fallas o pierdes,
	 * con las frases aprobadas por el autor el 16/11/2026, sacadas de lo que aplica el motor. Se enseña durante la prueba y en su resultado.
	 * @param Trial La prueba del anillo.
	 * @param bDuel Si es un duelo (gana a Trial).
	 * @return Por ejemplo "Prueba del Warden: si fallas, pierdes el turno"; sin uso conocido, "Prueba".
	 */
	FString RingStakeTitle(ESkillTrial Trial, bool bDuel);

	/**
	 * La cuenta atrás de la espera para empezar (S-042, R-MIN-19; nombre propuesto): "Empieza sola en N s", con N los segundos que quedan
	 * redondeados hacia arriba (5,7 s dicen 6).
	 * @param RemainingSeconds Segundos que le quedan a la cuenta; negativos cuentan como 0.
	 * @return El texto de la cuenta.
	 */
	FString RingStartCountdownText(double RemainingSeconds);

	/**
	 * Pone en Model el botón "Empezar", su tecla y la cuenta atrás si Run espera a que el jugador esté listo (S-042, R-MIN-19; nombre propuesto); con
	 * la prueba empezada, los deja vacíos.
	 * @param Model El modelo del panel que se está rellenando.
	 * @param Run La prueba del anillo en curso.
	 * @param Now El reloj real con que se pinta.
	 * @param KeyDisplayName El nombre que da Unreal a la tecla de empezar (RingStartKey): "Enter" dice "o pulsa Intro".
	 */
	void AddRingStartPrompt(FRingPanelModel& Model, const FRingTrialRun& Run, double Now, const FString& KeyDisplayName);

	/** Alto del panel del anillo respecto a su ancho (S-042): deja sitio al título partido y a la franja de "Empezar" bajo el aro. */
	inline constexpr float RingPanelHeightRatio = 1.35f;

	/**
	 * Ancho del panel del anillo (S-026; S-042 lo saca de SRingPanel para que la vista y los tests lo compartan; nombre propuesto).
	 * @param FontSize La letra del título (RingFontSize); por debajo de 8, 8.
	 * @return 420 a 18 puntos, proporcional a la letra.
	 */
	float RingPanelWidth(int32 FontSize);

	/**
	 * Dónde va cada cosa del panel del anillo (S-042, R-MIN-19; corrección del autor del 16/11/2026 tras PIE: el botón "Empezar" no tapa la runa ni
	 * la franja verde; nombre propuesto). Desde abajo: las marcas de los pulsos, la barra y, encima, la franja de "Empezar", que no depende del
	 * título (el botón va siempre en el mismo sitio). El aro, entre el final de los textos y esa franja, centrado; tan grande como siempre (0,36 del
	 * lado menor) si cabe, y más pequeño si no. La franja se reserva también con la prueba empezada, para que el aro no se mueva al desaparecer el botón.
	 * @param Width Ancho del panel.
	 * @param Height Alto del panel.
	 * @param TextBottom Dónde acaban los textos de arriba (título e instrucción, o la nota del resultado).
	 * @param Scale La letra del panel entre 18: escala los márgenes.
	 * @return Las posiciones; con un panel sin sitio para el aro, MaxRadius 0.
	 */
	FRingPanelLayout RingPanelLayout(float Width, float Height, float TextBottom, float Scale);

	/**
	 * Las líneas de la instrucción del anillo (R-VIS-17): dos, que el panel pinta cada una centrada y por separado bajo el título mientras la prueba
	 * está abierta. Son las mismas en el anuncio y con pulsos resueltos; ninguna habla de "pulso".
	 */
	TArray<FString> RingInstructionLines();
}
