#pragma once

#include "CoreMinimal.h"
#include "DecisionText.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SWrapBox;

DECLARE_DELEGATE_OneParam(FOnDecisionOption, const Soulbound::Presentation::FDecisionOption&);

/**
 * Panel de decisión (S-028, R-DEC-2; nombre propuesto): la decisión abierta para el humano, centrada abajo. Slate puro, con el estilo de
 * SStatusPanel: fondo oscuro, el título con la letra grande, una línea de contexto, la instrucción, un botón por opción con su tecla en negrita
 * delante y el reloj de la elección como barra que se vacía, sin números. Bloqueada (la mesa espera al Warden), los botones salen desactivados y
 * en el sitio de la barra va "El Warden habla". Solo pinta lo que le dan (DecisionView): al pulsar un botón avisa con OnOption y no contesta a
 * nadie; la vista se lo pasa al controlador, que llama a la misma entrada del anfitrión que la tecla.
 */
class SDecisionPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDecisionPanel) : _FontSize(13), _LargeScale(1.5f), _TimeFraction(0.f) {}
		/** Tamaño de la letra normal, en unidades de Slate; se lee cada vez que se pinta (R-DEC-4: la de los paneles de S-027). */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Cuánto más grandes van el título y los botones (StatusLargeFontScale, como la petición del panel de estado). */
		SLATE_ARGUMENT(float, LargeScale)
		/** Qué parte del plazo queda, de 0 a 1; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(float, TimeFraction)
		/** Se pulsó el botón de una opción. */
		SLATE_EVENT(FOnDecisionOption, OnOption)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Cambia la decisión que enseña; si es la misma que ya pinta (sin mirar el reloj), no rehace nada. Sin vista, se vacía. */
	void SetView(const TOptional<Soulbound::Presentation::FDecisionView>& NewView);

private:
	/** Si dos vistas se pintan igual: todo menos el reloj y la identidad. */
	static bool SameLook(const TOptional<Soulbound::Presentation::FDecisionView>& A, const TOptional<Soulbound::Presentation::FDecisionView>& B);

	void Rebuild();

	TAttribute<int32> FontSize;
	float LargeScale = 1.5f;
	TAttribute<float> TimeFraction;
	FOnDecisionOption OnOption;
	TSharedPtr<SVerticalBox> Content;
	TOptional<Soulbound::Presentation::FDecisionView> View;
};
