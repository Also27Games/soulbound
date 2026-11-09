#pragma once

#include "CoreMinimal.h"
#include "StatusText.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

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
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rehace el contenido con estas filas y estas líneas de poderes. */
	void SetContent(const TArray<Soulbound::Presentation::FGrimoireRow>& Rows, const TArray<FString>& PowerLines);

private:
	/** La tarjeta de un asiento. */
	TSharedRef<SWidget> MakeCard(const Soulbound::Presentation::FGrimoireRow& Row) const;

	TAttribute<int32> FontSize;
	TArray<FLinearColor> DomainColors;
	TSharedPtr<SVerticalBox> Body;
};
