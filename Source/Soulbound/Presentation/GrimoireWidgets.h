#pragma once

#include "CoreMinimal.h"
#include "GrimoireBook.h"
#include "GrimoireFont.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"

class SBorder;
class STextBlock;
class SWidget;
class UTexture2D;

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

	/**
	 * Las dos columnas de una fila del libro (S-048, R-HUD-20; S-049, sacadas de SGrimoirePanel para compartirlas con el libro de "Cómo se juega"): la
	 * izquierda mide la página izquierda; el hueco, el lomo; la derecha, lo que queda. Así nada cruza el lomo.
	 * @param Left, Right El contenido de cada página.
	 * @param LeftWidth, SpineWidth El ancho de la página izquierda y el del lomo; se leen cada vez que se pinta.
	 * @param RightAlign Cómo se alinea en vertical la página derecha.
	 */
	TSharedRef<SWidget> GrimoireColumns(const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right, const TAttribute<float>& LeftWidth,
		const TAttribute<float>& SpineWidth, EVerticalAlignment RightAlign);

	/**
	 * La marca de agua de una hoja de "Cómo se juega" (S-048, segunda ampliación, R-HUD-20; S-049, sacada de SGrimoirePanel para compartirla con el
	 * libro del menú): en el sitio de GrimoireWatermarks, en coordenadas de la zona de contenido (su origen, Content.Min), detrás del texto y sin recibir
	 * clics. Sin pincel, nada.
	 * @param Brush El pincel de la marca; lo mantiene vivo quien lo da. Nulo, ninguna.
	 * @param Opacity Su opacidad (HowToPlayWatermarkOpacity).
	 * @param SizeFraction Su tamaño en fracción de la hoja (HowToPlayWatermarkSize).
	 * @param bRight La de la hoja derecha o la de la izquierda.
	 * @param Layout La zona de contenido de ahora (GrimoireBookLayout); se lee cada vez que se pinta.
	 * @param Shown Si se ve ahora (en el Grimorio, solo en "Cómo se juega"); se lee cada vez que se pinta.
	 */
	TSharedRef<SWidget> GrimoireWatermark(const FSlateBrush* Brush, float Opacity, float SizeFraction, bool bRight, TFunction<FGrimoireBookLayout()> Layout,
		TFunction<bool()> Shown);

	/**
	 * El pincel de una textura del libro (S-049, R-HUD-22, nombre aprobado; antes, repetido en la vista): con su tamaño de GrimoireTextureSize, nunca
	 * GetSizeX ni GetSizeY, que en el editor dan el de la textura de relleno mientras se compila. Lo usan la imagen del libro, la marca de agua y las
	 * láminas, en la vista y en el libro del menú.
	 * @param Texture La textura; la mantiene viva quien la carga. No nula.
	 * @param DrawAs Cómo se pinta: Box para la imagen del libro, Image para la marca de agua y las láminas.
	 * @param Margin Los márgenes del borde (solo para Box).
	 */
	FSlateBrush GrimoireTextureBrush(UTexture2D* Texture, ESlateBrushDrawType::Type DrawAs, const FMargin& Margin = FMargin());

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
