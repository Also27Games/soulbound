#pragma once

#include "CoreMinimal.h"
#include "GrimoireFont.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * Una muestra pequeña debajo de otra muestra (S-048, retoques de "La mesa" del 18/11/2026, R-HUD-10, nombre propuesto): un cuadrito de color y su
 * texto, como las runas de un Brazal revelado en "Jugadores" ("D2 ×1: Celeridad").
 */
struct FLegendSubChip
{
	FLinearColor Color = FLinearColor::White;
	FString Text;

	bool operator==(const FLegendSubChip& Other) const
	{
		return Color == Other.Color && Text == Other.Text;
	}
};

/** Una muestra de la leyenda: un cuadrito de color y su texto. */
struct FLegendChip
{
	FLinearColor Color = FLinearColor::White;
	FString Text;
	/** Si existe, da el texto cada vez que se pinta (la leyenda de un dato que cambia, como el rol de un asiento). */
	TFunction<FString()> TextGetter;
	/** S-048 (retoques de "La mesa" del 18/11/2026, R-HUD-10, nombre propuesto): el texto, en negrita (la línea de un Brazal revelado). */
	bool bBold = false;
	/**
	 * S-048 (retoques de "La mesa", nombre propuesto): muestras pequeñas debajo de esta, con sangría, empezando bajo su texto y repartidas por la
	 * línea mientras quepan (las runas de un Brazal revelado). Vacía, ninguna.
	 */
	TArray<FLegendSubChip> SubChips;

	/**
	 * Igualdad de lo que se pinta (S-044, R-VIS-21): color, texto, negrita y muestras de debajo. Dos muestras con TextGetter nunca son iguales (no se
	 * pueden comparar), así que un panel con muestras vivas se rehace siempre que se le den filas; las de "La mesa" del Grimorio llevan el texto puesto.
	 */
	bool operator==(const FLegendChip& Other) const
	{
		return Color == Other.Color && Text == Other.Text && bBold == Other.bBold && SubChips == Other.SubChips && !TextGetter && !Other.TextGetter;
	}
};

namespace Soulbound::Presentation
{
	/**
	 * El aire de la leyenda (S-048, retoques de "La mesa" del 18/11/2026, R-HUD-10, nombres propuestos; valores propuestos por el agente y aceptados
	 * por el autor, que se ajustan en PIE): el hueco entre dos filas ("Jugadores" y "Los seis Dominios y su Elegido"; antes, 2), el que hay entre una
	 * muestra y la siguiente (antes, 2) y, entre las muestras pequeñas de debajo (SubChips), el horizontal y el vertical. Si a 1024x768 la hoja derecha
	 * no cabe, lo primero que se baja es LegendRowGap (decisión del autor).
	 */
	inline constexpr float LegendRowGap = 12.f;
	inline constexpr float LegendChipGap = 4.f;
	inline constexpr float LegendSubChipGapX = 12.f;
	inline constexpr float LegendSubChipGapY = 2.f;

	/**
	 * Cuánto se baja el cuadrito de una muestra para quedar centrado en la primera línea de su texto (S-048, retoques de "La mesa" del 18/11/2026,
	 * R-HUD-10, nombre propuesto): así no queda en medio cuando el texto se parte. Función pura; la altura de la línea se mide con la fuente al pintar.
	 * @param LineHeight La altura de una línea del texto, en unidades de Slate.
	 * @param ChipSize El lado del cuadrito, en unidades de Slate.
	 * @return La mitad de lo que la línea es más alta que el cuadrito; cero si no lo es o si la altura no es válida.
	 */
	float LegendChipTopPadding(float LineHeight, float ChipSize);
}

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

/**
 * Leyenda (glosario "Legend", nombre aceptado en S-007): texto fijo en pantalla con el significado de cada color y
 * forma de la vista del tablero. Solo pinta las filas que le dan; los colores los pone quien la crea, así la leyenda
 * y la vista no pueden contradecirse. Slate puro. Desde S-044 también pinta "La mesa" del Grimorio, con otro encabezado y filas que cambian
 * (SetRows); desde S-045 (R-HUD-16), solo en el Grimorio, y desde la ronda de maquetación de S-048 (18/11/2026), sin las filas del tablero, que se
 * fueron con la pestaña "El tablero".
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
