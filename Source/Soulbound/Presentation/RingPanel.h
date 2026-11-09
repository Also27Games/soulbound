#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/**
 * Lo que pinta el panel del anillo en un instante (S-026). Lo calcula la vista con el reloj real; el panel no sabe de la partida ni de
 * reglas.
 */
struct FRingPanelModel
{
	/** Si hay algo que enseñar: una prueba del humano en curso, o el resultado de la última durante un momento. */
	bool bVisible = false;
	/** El uso y su dificultad: "PRUEBA DEL WARDEN · normal". */
	FString Title;
	/** "pulso 2 de 3", o el aviso del anuncio. */
	FString Counter;
	/** Si se pinta el anillo sobre la runa (durante la prueba; no en el resultado). */
	bool bShowRing = false;
	/** Fracción del cierre: 0 con el anillo en su radio mayor, 1 tocando la runa. */
	double CloseFraction = 0.0;
	/** Media zona buena como fracción del cierre: dónde se pinta la guía de la zona buena. */
	double ZoneFraction = 0.0;
	/** Un elemento por pulso: su precisión si ya se resolvió. */
	TArray<TOptional<double>> Pulses;
	/** El texto grande: la precisión del último pulso, o "SUPERADA", "FALLADA" o el rendimiento al acabar. */
	FString Big;
	FLinearColor BigColor = FLinearColor::White;
};

/**
 * Panel del anillo (S-026; nombre propuesto en el PR): la runa como círculo, el anillo cerrándose hacia ella, la guía de la zona buena, el
 * contador de pulsos y la precisión de cada uno como número y color. Slate puro, sin widgets ni assets, como la crónica y la leyenda.
 * Pide su modelo en cada pintado.
 */
class SRingPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRingPanel) : _FontSize(18) {}
		/** Tamaño de la letra del título; el resto se escala a partir de él. */
		SLATE_ARGUMENT(int32, FontSize)
		/** Lo que hay que pintar ahora. */
		SLATE_ARGUMENT(TFunction<FRingPanelModel()>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/** Color de una precisión: de rojo (0) a verde (1), pasando por amarillo. */
	static FLinearColor PrecisionColor(double Precision);

private:
	int32 FontSize = 18;
	TFunction<FRingPanelModel()> Model;
};
