#pragma once

#include "CoreMinimal.h"
#include "RingPanelModel.h"
#include "Widgets/SLeafWidget.h"

/**
 * Panel del anillo (S-026, S-031; nombre propuesto en el PR): la runa (una imagen teñida, o un disco si no hay textura), la franja verde de la zona
 * buena, el aro cerrándose hacia ella (verde mientras está dentro), la instrucción en dos líneas, una marca de color por pulso y la barra de progreso con
 * la marca del objetivo. Sin cifras de precisión (R-VIS-17). Slate puro, como la crónica y la leyenda; pide su modelo en cada pintado.
 */
class SRingPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRingPanel) : _FontSize(18) {}
		/** Tamaño de la letra del título; el resto se escala a partir de él. */
		SLATE_ARGUMENT(int32, FontSize)
		/** Lo que hay que pintar ahora. */
		SLATE_ARGUMENT(TFunction<FRingPanelModel()>, Model)
		/** El pincel de la imagen de la runa (R-VIS-18), pedido en cada pintado; nullptr (o sin función) pinta un disco relleno del mismo color. */
		SLATE_ARGUMENT(TFunction<const FSlateBrush*()>, RuneBrush)
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
	TFunction<const FSlateBrush*()> RuneBrush;
};
