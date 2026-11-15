#pragma once

#include "CoreMinimal.h"
#include "DecisionText.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SUniformGridPanel;

DECLARE_DELEGATE_OneParam(FOnDecisionOption, const Soulbound::Presentation::FDecisionOption&);

/**
 * Panel de decisión (S-028, R-DEC-2; nombre propuesto): la decisión abierta para el humano, centrada abajo. Slate puro, con el estilo de
 * SStatusPanel: fondo oscuro, el título con la letra grande, una línea de contexto, la instrucción, un botón por opción con su tecla en negrita
 * delante y el reloj de la elección como barra que se vacía, sin números. Bloqueada (la mesa espera al Warden), los botones salen desactivados y
 * en el sitio de la barra va "El Warden habla". Solo pinta lo que le dan (DecisionView): al pulsar un botón avisa con OnOption y no contesta a
 * nadie; la vista se lo pasa al controlador, que llama a la misma entrada del anfitrión que la tecla.
 * Desde S-033 (R-DEC-7) pinta también, en el mismo sitio, la espera a otro asiento y el resultado de una votación: título en gris, sin botones y
 * con la barra en gris (la espera de una prueba o un duelo y el resultado no llevan barra); el resultado añade una línea por voto.
 */
class SDecisionPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDecisionPanel) : _FontSize(13), _LargeScale(1.5f), _TimeFraction(0.f), _MaxWidth(900.f) {}
		/** Tamaño de la letra normal, en unidades de Slate; se lee cada vez que se pinta (R-DEC-4: la de los paneles de S-027). */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Cuánto más grandes van el título y los botones (StatusLargeFontScale, como la petición del panel de estado). */
		SLATE_ARGUMENT(float, LargeScale)
		/** Qué parte del plazo queda, de 0 a 1; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(float, TimeFraction)
		/**
		 * El ancho más grande que puede tener el panel, en unidades de Slate (el hueco del tablero, con su tope); se lee en cada tick. Con él se
		 * decide si los botones caben en fila (DecisionButtonsInRow).
		 */
		SLATE_ATTRIBUTE(float, MaxWidth)
		/** Se pulsó el botón de una opción. */
		SLATE_EVENT(FOnDecisionOption, OnOption)
		/**
		 * El pincel de la imagen de las caras del dado (S-039, R-DEC-15), pedido al montar la decisión: la imagen lleva las seis caras en una fila y el
		 * panel pinta el trozo de cada valor (DiceFaceUV). nullptr (o sin función) dibuja la cara por código.
		 */
		SLATE_ARGUMENT(TFunction<const FSlateBrush*()>, DiceBrush)
		/** Lado de la cara del dado, en unidades de Slate (ABoardViewActor::DiceFaceSize). */
		SLATE_ATTRIBUTE(float, DiceFaceSize)
		/** Color del cuadrado del dado dibujado por código y el de sus puntos (DiceFaceColor, DiceFacePipColor). */
		SLATE_ATTRIBUTE(FLinearColor, DiceColor)
		SLATE_ATTRIBUTE(FLinearColor, DicePipColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Pone los botones en fila o en columna según quepan con el ancho del más largo (corrección del autor del 11/11/2026). */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Cambia la decisión que enseña; si es la misma que ya pinta (sin mirar el reloj), no rehace nada. Sin vista, se vacía. */
	void SetView(const TOptional<Soulbound::Presentation::FDecisionView>& NewView);

private:
	/** Si dos vistas se pintan igual: todo menos el reloj y la identidad. */
	static bool SameLook(const TOptional<Soulbound::Presentation::FDecisionView>& A, const TOptional<Soulbound::Presentation::FDecisionView>& B);

	void Rebuild();

	/** Coloca Buttons en ButtonGrid: en una fila o en una columna. La rejilla uniforme da a todas las celdas el tamaño del botón más grande. */
	void LayoutButtons(bool bInRow);

	/** Si los botones de ahora caben en fila con el ancho disponible. */
	bool ButtonsFitInRow() const;

	TAttribute<int32> FontSize;
	float LargeScale = 1.5f;
	TAttribute<float> TimeFraction;
	TAttribute<float> MaxWidth;
	/** Los botones de la decisión que se enseña y la rejilla donde van. */
	TArray<TSharedRef<SWidget>> Buttons;
	TSharedPtr<SUniformGridPanel> ButtonGrid;
	bool bButtonsInRow = true;
	FOnDecisionOption OnOption;
	TFunction<const FSlateBrush*()> DiceBrush;
	TAttribute<float> DiceFaceSize;
	TAttribute<FLinearColor> DiceColor;
	TAttribute<FLinearColor> DicePipColor;
	/** Los pinceles de las caras de la decisión que se enseña, uno por dado: cada uno la copia del pincel de la imagen con el trozo de su valor. */
	TArray<TSharedPtr<FSlateBrush>> FaceBrushes;
	TSharedPtr<SVerticalBox> Content;
	TOptional<Soulbound::Presentation::FDecisionView> View;
};
