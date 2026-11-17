#pragma once

#include "CoreMinimal.h"
#include "ChronicleLines.h"
#include "GrimoireBook.h"
#include "StatusText.h"
#include "Widgets/SCompoundWidget.h"

class SChroniclePanel;
class SLegendPanel;
class STextBlock;
class SVerticalBox;

namespace Soulbound::Presentation
{
	/**
	 * La tarjeta de un asiento del Grimorio (S-027, R-HUD-4), la misma que reutiliza la carta del Elegido (S-029, R-VIS-13): papel, Elegido y
	 * Dominio, afinidad, las seis runas con su poder y los poderes activos. Sus textos se parten con WrapTextAt a TextWidth, que no depende de lo
	 * pintado antes (S-029, R-VIS-21). Desde S-044 es la tarjeta de la pestaña "Tú".
	 * @param Row La fila de GrimoireRows.
	 * @param FontSize Tamaño de la letra, se lee cada vez que se pinta.
	 * @param DomainColors Color de cada Dominio, para el cuadrito de cada runa.
	 * @param TextWidth Ancho al que se parten los textos (GrimoireCardTextWidth), se lee cada vez que se pinta.
	 */
	TSharedRef<SWidget> MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth);
}

/**
 * El Grimorio en PC (glosario "Grimorio", `Grimoire`): un panel centrado, bajo demanda, que no pausa nada (S-027, R-HUD-4). Desde S-044 (R-HUD-10) es
 * el libro del juego, con cinco pestañas: "Tú" (la tarjeta del humano, su insolencia, la runa de su hogar si es un Eco y, al pie, una línea por poder),
 * "La mesa" y "El tablero" (filas de leyenda), "Cómo se juega" (el texto del menú y las teclas) y "Crónica". Cada pestaña se desplaza con la rueda; el
 * panel tiene ancho y alto fijos (los de la capa) para no cambiar de tamaño al cambiar de pestaña. Las pestañas se cambian con un clic en su nombre
 * (OnTabClicked); cuál está activa lo dice el atributo ActiveTab. Slate puro: solo pinta lo que le dan.
 */
class SGrimoirePanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTabClicked, Soulbound::Presentation::EGrimoireTab);

	SLATE_BEGIN_ARGS(SGrimoirePanel) : _FontSize(13) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Color de cada Dominio, el de la vista, para el cuadrito de cada runa. */
		SLATE_ARGUMENT(TArray<FLinearColor>, DomainColors)
		/** Lo que dice el título sobre cómo se cierra y se cambia de pestaña ("G o H para cerrar; Izquierda y Derecha, pestañas"). */
		SLATE_ARGUMENT(FString, KeysHint)
		/** Ancho del panel (GrimoirePanelWidth de la capa); se lee cada vez que se pinta y no depende de lo pintado (S-029, R-VIS-21). */
		SLATE_ATTRIBUTE(float, PanelWidth)
		/** S-044: alto del panel, fijo; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(float, PanelHeight)
		/** S-044: la pestaña activa. */
		SLATE_ATTRIBUTE(Soulbound::Presentation::EGrimoireTab, ActiveTab)
		/** S-044: el clic en el nombre de una pestaña. */
		SLATE_EVENT(FOnTabClicked, OnTabClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** "Tú": se rehace solo si cambia respecto a lo último que pintó (R-VIS-21). */
	void SetYou(const Soulbound::Presentation::FGrimoireYouView& View);
	/** "La mesa", con las filas de GrimoireTableRows; solo se rehace si cambian. */
	void SetTable(const TArray<FLegendRow>& Rows);
	/** "El tablero", con las filas de BoardLegendRows; solo se rehace si cambian. */
	void SetBoard(const TArray<FLegendRow>& Rows);
	/** "Cómo se juega", con el texto de GrimoireHowToPlayText. */
	void SetHowToPlay(const FString& Text);
	/** "Crónica", con las líneas de ChronicleLines (la más reciente arriba). */
	void SetChronicle(const TArray<FChronicleLine>& NewestFirst);

private:
	/** El ancho al que se parten los textos dentro de una pestaña: el del panel menos sus márgenes y la barra de desplazamiento. */
	float PageTextWidth() const;

	TAttribute<int32> FontSize;
	TAttribute<float> PanelWidth;
	TAttribute<float> PanelHeight;
	TAttribute<Soulbound::Presentation::EGrimoireTab> ActiveTab;
	FOnTabClicked OnTabClicked;
	TArray<FLinearColor> DomainColors;
	/** Lo último que se pintó en "Tú", para no rehacer lo que no cambia. */
	Soulbound::Presentation::FGrimoireYouView LastYou;
	bool bHasYou = false;
	TSharedPtr<SVerticalBox> YouBody;
	TSharedPtr<SLegendPanel> TablePanel;
	TSharedPtr<SLegendPanel> BoardPanel;
	TSharedPtr<STextBlock> HowToPlayText;
	TSharedPtr<SChroniclePanel> Chronicle;
};
