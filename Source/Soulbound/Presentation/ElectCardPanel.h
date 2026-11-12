#pragma once

#include "CoreMinimal.h"
#include "StatusText.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * La carta del Elegido (glosario `ElectCard`; S-029, R-VIS-13): un panel centrado que sale al vincularse el jugador humano, con el nombre del
 * Elegido, su Dominio, su afinidad en una frase y su mapa de runa a poder. Reutiliza la tarjeta del Grimorio (MakeGrimoireCard) para su asiento.
 * Un clic en cualquier parte del panel lo cierra (OnDismissed). Slate puro: solo pinta la fila que le dan.
 */
class SElectCardPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SElectCardPanel) : _FontSize(13) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Color de cada Dominio, el de la vista, para el cuadrito de cada runa. */
		SLATE_ARGUMENT(TArray<FLinearColor>, DomainColors)
		/** Ancho del panel; se lee cada vez que se pinta y no depende de lo pintado. */
		SLATE_ATTRIBUTE(float, PanelWidth)
		/** Se llama al hacer clic en el panel. */
		SLATE_EVENT(FSimpleDelegate, OnDismissed)
		/** Cómo se abre el Grimorio, para la línea de pie ("G"). */
		SLATE_ARGUMENT(FString, GrimoireKey)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rehace la carta con la fila del asiento humano, solo si cambia respecto a la última pintada. */
	void SetRow(const Soulbound::Presentation::FGrimoireRow& Row);

private:
	TAttribute<int32> FontSize;
	TAttribute<float> PanelWidth;
	TArray<FLinearColor> DomainColors;
	TSharedPtr<SVerticalBox> Body;
	Soulbound::Presentation::FGrimoireRow LastRow;
	bool bHasRow = false;
};
