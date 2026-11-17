#pragma once

#include "CoreMinimal.h"
#include "GrimoireFont.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/** Una muestra de la leyenda: un cuadrito de color y su texto. */
struct FLegendChip
{
	FLinearColor Color = FLinearColor::White;
	FString Text;
	/** Si existe, da el texto cada vez que se pinta (la leyenda de un dato que cambia, como el rol de un asiento). */
	TFunction<FString()> TextGetter;

	/**
	 * Igualdad de lo que se pinta (S-044, R-VIS-21): color y texto. Dos muestras con TextGetter nunca son iguales (no se pueden comparar), así que
	 * un panel con muestras vivas se rehace siempre que se le den filas; las de "La mesa" del Grimorio llevan el texto puesto.
	 */
	bool operator==(const FLegendChip& Other) const
	{
		return Color == Other.Color && Text == Other.Text && !TextGetter && !Other.TextGetter;
	}
};

/** Una fila de la leyenda: un título y sus muestras. */
struct FLegendRow
{
	FString Title;
	TArray<FLegendChip> Chips;
	/**
	 * S-048 (ronda de maquetación del 18/11/2026, R-HUD-10, nombre propuesto): una muestra debajo de otra, con el cuadrito y el texto empezando a la
	 * misma altura ("Jugadores" de "La mesa"); si no, las muestras se reparten por la línea mientras quepan.
	 */
	bool bOnePerLine = false;

	bool operator==(const FLegendRow& Other) const
	{
		return Title == Other.Title && Chips == Other.Chips && bOnePerLine == Other.bOnePerLine;
	}
};

namespace Soulbound::Presentation
{
	/**
	 * Lo que necesitan las filas del tablero de la leyenda (S-044, nombre propuesto): si la mesa lleva las losas del kit (S-019) o las formas
	 * básicas, si está cargado el suelo agrietado de los Olvidados (S-040) y los colores de la vista, para que leyenda y tablero no se contradigan.
	 */
	struct FBoardLegendLook
	{
		bool bKit = false;
		bool bForgottenFloor = false;
		FLinearColor Altar = FLinearColor::White;
		FLinearColor NicheFree = FLinearColor::White;
		FLinearColor Source = FLinearColor::White;
		FLinearColor Node = FLinearColor::White;
		/** El color de un Nicho reclamado en la muestra: el del asiento 0. */
		FLinearColor NicheClaimed = FLinearColor::White;
		FLinearColor NicheClosed = FLinearColor::White;
		FLinearColor Reachable = FLinearColor::White;
		FLinearColor SealMark = FLinearColor::White;
		FLinearColor ForgottenRing = FLinearColor::White;
	};

	/** El texto de la fila de los destellos (S-029): lo que se ve al ganar o perder una runa. */
	inline const TCHAR* const FlashLegendText = TEXT("Destello: hacia dentro y del color de la runa, la gana. Hacia fuera y en morado, la pierde");

	/**
	 * Las filas del tablero de la leyenda (S-044, sacadas de ABoardViewActor::BuildOverlay sin cambiar sus textos, revisados por el autor el
	 * 13/11/2026): "Casillas", "Nichos", "Tablero" y la de los destellos, en ese orden. Las usa la pestaña "El tablero" del Grimorio (R-HUD-10;
	 * hasta S-045, también la columna de la leyenda). Ninguna es de jugadores ni de runas.
	 * @param Look Kit, suelo de los Olvidados y colores de la vista.
	 * @return Cuatro filas.
	 */
	TArray<FLegendRow> BoardLegendRows(const FBoardLegendLook& Look);
}

/**
 * Leyenda (glosario "Legend", nombre aceptado en S-007): texto fijo en pantalla con el significado de cada color y
 * forma de la vista del tablero. Solo pinta las filas que le dan; los colores los pone quien la crea, así la leyenda
 * y la vista no pueden contradecirse. Slate puro. Desde S-044 también pinta "La mesa" y "El tablero" del Grimorio, con otro encabezado y
 * filas que cambian (SetRows).
 */
class SLegendPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLegendPanel) : _FontSize(13), _Heading(TEXT("LEYENDA")), _Background(true) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta (S-027: escala con la pantalla). */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Ancho de la columna en unidades de Slate (S-035): los textos se parten a él con WrapTextAt, no con AutoWrapText. 0 o menos, sin partir. */
		SLATE_ATTRIBUTE(float, WrapWidth)
		/** Las filas, de arriba abajo. */
		SLATE_ARGUMENT(TArray<FLegendRow>, Rows)
		/** S-044: el encabezado; vacío, sin encabezado (dentro del Grimorio, la pestaña ya lo dice). */
		SLATE_ARGUMENT(FString, Heading)
		/** S-044: si lleva su fondo oscuro (la columna sí; dentro del Grimorio, que ya tiene el suyo, no). */
		SLATE_ARGUMENT(bool, Background)
		/** S-048 (corrección 4 del autor del 17/11/2026): la fuente del Grimorio; sin asset, la de hoy. */
		SLATE_ARGUMENT(Soulbound::Presentation::FGrimoireFont, Font)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** S-044: rehace las filas, solo si cambian respecto a lo último que pintó (R-VIS-21). */
	void SetRows(const TArray<FLegendRow>& Rows);

private:
	TAttribute<int32> FontSize;
	TAttribute<float> WrapWidth;
	Soulbound::Presentation::FGrimoireFont TextFont;
	FString Heading;
	TArray<FLegendRow> LastRows;
	bool bHasRows = false;
	TSharedPtr<SVerticalBox> Body;
};
