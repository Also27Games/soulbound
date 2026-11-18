#pragma once

#include "CoreMinimal.h"
#include "GrimoireFont.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"

class SBorder;
class STextBlock;
class SWidget;

/**
 * Las piezas de Slate que comparten el Grimorio (SGrimoirePanel) y el libro de "Cómo se juega" (S-049, R-HUD-21): el texto con la letra del Grimorio,
 * la línea pequeña del pie, el desplazamiento de una hoja, el marco de los botones y de las pestañas, la pulsación que cuenta los clics rápidos y
 * los colores. Sacadas de GrimoirePanel.cpp sin cambiarlas, para que el libro del menú y el del Grimorio se pinten con las mismas.
 */
namespace Soulbound::Presentation
{
	/**
	 * Un texto del Grimorio con la letra del atributo; Scale la agranda (el título). Se parte con WrapTextAt a un ancho que no depende de lo
	 * pintado antes: con AutoWrapText el ancho sale del tamaño de la pasada anterior de pintado, y un texto recién creado cambiaba de tamaño de un
	 * fotograma a otro (S-029, R-VIS-21; el mismo motivo que la frase del Warden, R-HUD-3).
	 */
	TSharedRef<STextBlock> GrimoireText(const FGrimoireFont& Font, const TAttribute<int32>& Size, const TAttribute<float>& Wrap, const FString& Text,
		const FLinearColor& Color, bool bBold, float Scale = 1.f);

	/** La línea de teclas del pie del Grimorio (S-048, corrección 5): como GrimoireText, con un texto que se lee al pintar, porque cambia con la pestaña. */
	TSharedRef<STextBlock> GrimoireFootText(const FGrimoireFont& Font, const TAttribute<int32>& Size, const TAttribute<float>& Wrap,
		const TAttribute<FText>& Text, const FLinearColor& Color, bool bBold);

	/** Un SScrollBox de pestaña con la barra que reserva GrimoireColumnTextWidth (S-048, corrección 1): su grosor, su margen y el hueco hasta el texto. */
	TSharedRef<SWidget> GrimoireScroll(const TSharedRef<SWidget>& Content);

	/**
	 * El marco de un botón de página o de una pestaña (S-048, correcciones tras probar la maquetación del 18/11/2026, R-HUD-18 y R-HUD-10): una caja
	 * del color del marco con el contenido dentro, a GrimoireFrameWidth de su propia escala pintada; así cada lado mide al menos un píxel y se ve.
	 */
	TSharedRef<SBorder> GrimoireFrame(const TAttribute<FSlateColor>& Color, const TSharedRef<SWidget>& Content);

	/**
	 * Lo que hace un widget del Grimorio al pulsarlo (S-048, retoques del autor del 18/11/2026, R-HUD-18 y R-HUD-10, nombre propuesto): la misma
	 * acción con la pulsación y con el doble clic, y se queda los dos. Slate entrega el segundo de dos clics rápidos solo como doble clic; atendiendo
	 * solo la pulsación, se perdía uno de cada dos. Lo usan los botones de página, las pestañas y el fondo del panel (con una acción vacía).
	 * @param Widget El widget; sustituye lo que tuviera en su pulsación y en su doble clic.
	 * @param OnPress La acción; vacía, nada (solo se queda los clics).
	 */
	void GrimoireOnPress(const TSharedRef<SWidget>& Widget, TFunction<void()> OnPress);

	/** Cuánto más pequeña va la línea de teclas del pie del Grimorio (S-048, corrección 2 del autor del 17/11/2026). */
	inline constexpr float GrimoireFootScale = 0.85f;

	inline const FLinearColor GrimoireTitleColor(1.f, 0.85f, 0.4f);
	inline const FLinearColor GrimoireTextColor(0.9f, 0.9f, 0.9f);
	/** El color de la línea de teclas del pie (S-048, corrección 2): un gris claro, más discreto que el texto. */
	inline const FLinearColor GrimoireFootColor(0.7f, 0.7f, 0.7f);

	/**
	 * Los colores de las pestañas: los de la cabecera de la columna (S-035), que repiten el gris del panel de decisión y su fondo negro. Activa: fondo
	 * claro y texto oscuro en negrita; inactiva: fondo oscuro con borde, que se ilumina al pasar el ratón (TabHover, en GrimoirePanel.cpp). Los botones de
	 * página van como una pestaña inactiva.
	 * TODO(R-HUD-4): el mismo gris está ya en DecisionPanel.cpp (la cabecera de la columna, que también lo repetía, se fue en S-045); con este, dos:
	 * si un tercer panel lo necesita, extraerlo a un sitio común de Presentation y que lo lean de allí.
	 */
	inline const FLinearColor TabLight = FLinearColor(FColor(190, 190, 190));
	inline const FLinearColor TabDark = FLinearColor(0.04f, 0.04f, 0.04f, 1.f);
}
