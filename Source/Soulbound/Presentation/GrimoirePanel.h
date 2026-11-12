#pragma once

#include "CoreMinimal.h"
#include "StatusText.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

namespace Soulbound::Presentation
{
	/**
	 * La tarjeta de un asiento del Grimorio (S-027, R-HUD-4), la misma que reutiliza la carta del Elegido (S-029, R-VIS-13): papel, Elegido y
	 * Dominio, afinidad, las seis runas con su poder y los poderes activos. Sus textos se parten con WrapTextAt a TextWidth, que no depende de lo
	 * pintado antes (S-029, R-VIS-21).
	 * @param Row La fila de GrimoireRows.
	 * @param FontSize Tamaño de la letra, se lee cada vez que se pinta.
	 * @param DomainColors Color de cada Dominio, para el cuadrito de cada runa.
	 * @param TextWidth Ancho al que se parten los textos (GrimoireCardTextWidth), se lee cada vez que se pinta.
	 */
	TSharedRef<SWidget> MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth);
}

/**
 * El Grimorio en PC (glosario "Grimorio", `Grimoire`; S-027, R-HUD-4): un panel centrado, bajo demanda, con una tarjeta por asiento (papel,
 * Elegido y Dominio, afinidad con su frase, las seis runas con su cantidad y el poder que da cada una, marcando las activas, y los poderes
 * activos) y, al pie, una línea por poder con su frase. No pausa nada. Slate puro: solo pinta lo que le dan (GrimoireRows,
 * GrimoirePowerLines), con la letra de su atributo.
 */
class SGrimoirePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGrimoirePanel) : _FontSize(13) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Color de cada Dominio, el de la vista, para el cuadrito de cada runa. */
		SLATE_ARGUMENT(TArray<FLinearColor>, DomainColors)
		/** Cómo se cierra, para el título ("G o H"). */
		SLATE_ARGUMENT(FString, CloseKeys)
		/** Ancho del panel (GrimoirePanelWidth de la capa); se lee cada vez que se pinta y no depende de lo pintado (S-029, R-VIS-21). */
		SLATE_ATTRIBUTE(float, PanelWidth)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * Rehace el contenido con estas filas y estas líneas de poderes, solo si cambian respecto a lo último que pintó (S-029, R-VIS-21: antes se
	 * rehacía en cada refresco y el panel temblaba).
	 */
	void SetContent(const TArray<Soulbound::Presentation::FGrimoireRow>& Rows, const TArray<FString>& PowerLines);

private:
	TAttribute<int32> FontSize;
	TAttribute<float> PanelWidth;
	/** Lo último que se pintó, para no rehacer lo que no cambia. */
	TArray<Soulbound::Presentation::FGrimoireRow> LastRows;
	TArray<FString> LastPowerLines;
	bool bHasContent = false;
	TArray<FLinearColor> DomainColors;
	TSharedPtr<SVerticalBox> Body;
};
