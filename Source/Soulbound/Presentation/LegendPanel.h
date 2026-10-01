#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Una muestra de la leyenda: un cuadrito de color y su texto. */
struct FLegendChip
{
	FLinearColor Color = FLinearColor::White;
	FString Text;
};

/** Una fila de la leyenda: un título y sus muestras. */
struct FLegendRow
{
	FString Title;
	TArray<FLegendChip> Chips;
};

/**
 * Leyenda (glosario "Legend", nombre aceptado en S-007): texto fijo en pantalla con el significado de cada color y
 * forma de la vista del tablero. Solo pinta las filas que le dan; los colores los pone quien la crea, así la leyenda
 * y la vista no pueden contradecirse. Slate puro.
 */
class SLegendPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLegendPanel) {}
		/** Tamaño de la letra. */
		SLATE_ARGUMENT(int32, FontSize)
		/** Las filas, de arriba abajo. */
		SLATE_ARGUMENT(TArray<FLegendRow>, Rows)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
