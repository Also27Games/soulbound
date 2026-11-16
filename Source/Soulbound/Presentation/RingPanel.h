#pragma once

#include "CoreMinimal.h"
#include "RingPanelModel.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Panel del anillo (S-026, S-031; nombre propuesto en el PR): la runa (una imagen teñida, o un disco si no hay textura), la franja verde de la zona
 * buena, el aro cerrándose hacia ella (verde mientras está dentro), la instrucción en dos líneas, una marca de color por pulso y la barra de progreso con
 * la marca del objetivo. Sin cifras de precisión (R-VIS-17). Slate puro, como la crónica y la leyenda; pide su modelo en cada pintado.
 *
 * Desde S-042 (R-MIN-19) tiene un hijo, la fila de "Empezar" (el botón, su tecla y la cuenta), que el propio panel coloca en su franja bajo el aro
 * (RingPanelLayout, con su tamaño de verdad), para que la fila y lo que pinta el panel usen las mismas medidas. El panel no recibe clics
 * (SelfHitTestInvisible en la vista); su hijo, sí.
 */
class SRingPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRingPanel) : _FontSize(18) {}
		/** Tamaño de la letra del título; el resto se escala a partir de él. */
		SLATE_ARGUMENT(int32, FontSize)
		/** Lo que hay que pintar ahora. */
		SLATE_ARGUMENT(TFunction<FRingPanelModel()>, Model)
		/** El pincel de la imagen de la runa (R-VIS-18), pedido en cada pintado; nullptr (o sin función) pinta un disco relleno del mismo color. */
		SLATE_ARGUMENT(TFunction<const FSlateBrush*()>, RuneBrush)
		/** La fila de "Empezar" (S-042): el panel la centra en su franja bajo el aro. Su visibilidad la decide quien la crea. */
		SLATE_DEFAULT_SLOT(FArguments, StartRow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** El tamaño del panel, proporcional a la letra; la fila no lo cambia (va dentro, en su franja). */
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/**
	 * Coloca la fila de "Empezar" en la franja de RingPanelLayout calculada con el tamaño que le toca al panel (AllottedGeometry), la misma con que
	 * pinta: centrada a lo ancho y en el alto de la franja, con su tamaño, sin pasar del ancho del panel.
	 */
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;

	/** Color de una precisión: de rojo (0) a verde (1), pasando por amarillo. */
	static FLinearColor PrecisionColor(double Precision);

private:
	int32 FontSize = 18;
	TFunction<FRingPanelModel()> Model;
	TFunction<const FSlateBrush*()> RuneBrush;
};
