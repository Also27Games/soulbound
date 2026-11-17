#pragma once

#include "CoreMinimal.h"
#include "GrimoireFont.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;

/** Una línea del panel de crónica: texto con la marca de tiempo ya puesta y su color. */
struct FChronicleLine
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
};

/**
 * Panel de crónica (glosario "ChroniclePanel", nombre aceptado en S-007): versión de depuración de la Crónica del
 * GDD 07. Un SScrollBox con una línea de texto por suceso, la más reciente arriba. Solo pinta lo que le dan; no sabe
 * de reglas ni de la partida. Slate puro, sin widgets ni assets.
 */
class SChroniclePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChroniclePanel) : _FontSize(11), _Background(true) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta (S-027: escala con la pantalla). */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** S-048 (corrección 3 del autor del 17/11/2026): si lleva su caja oscura; sobre la imagen del libro, no. */
		SLATE_ARGUMENT(bool, Background)
		/** S-048 (corrección 4): la fuente del Grimorio; sin asset, la de hoy. */
		SLATE_ARGUMENT(Soulbound::Presentation::FGrimoireFont, Font)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * Sustituye el contenido por NewestFirst (la más reciente primero). Conserva la posición del scroll: quien está
	 * mirando el principio de la partida sigue ahí cuando entra un suceso nuevo, y quien sigue lo último, lo sigue.
	 */
	void SetLines(const TArray<FChronicleLine>& NewestFirst);

private:
	TSharedPtr<SScrollBox> ScrollBox;
	TAttribute<int32> FontSize = 11;
	Soulbound::Presentation::FGrimoireFont TextFont;
};
