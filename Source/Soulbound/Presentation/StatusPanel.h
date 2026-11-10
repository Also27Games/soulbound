#pragma once

#include "CoreMinimal.h"
#include "StatusText.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * Panel de líneas del HUD (S-027, R-HUD-1 y R-HUD-2; nombre propuesto): el panel de estado de arriba a la izquierda y, con otro título, el de
 * depuración, debajo. Slate puro, con el patrón de SLegendPanel: fondo oscuro, una línea por FPanelLine con su color, ajuste de línea al ancho
 * que le den y la letra que diga su atributo, que la vista escala con la altura de la pantalla (R-HUD-3). Solo pinta lo que le dan
 * (StatusLines, DebugLines): sin estado de partida.
 */
class SStatusPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStatusPanel) : _FontSize(13), _LargeScale(1.5f) {}
		/** Tamaño de la letra normal, en unidades de Slate; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Cuánto más grande va una línea con bLarge (la petición al jugador y el resultado). */
		SLATE_ARGUMENT(float, LargeScale)
		/** Título del panel; vacío, sin título (el panel de estado). */
		SLATE_ARGUMENT(FString, Title)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Cambia las líneas; si son las mismas que ya pinta, no hace nada. */
	void SetLines(const TArray<Soulbound::Presentation::FPanelLine>& NewLines);

private:
	TAttribute<int32> FontSize;
	float LargeScale = 1.5f;
	TSharedPtr<SVerticalBox> LinesBox;
	TArray<Soulbound::Presentation::FPanelLine> Lines;
};
