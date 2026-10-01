#pragma once

#include "CoreMinimal.h"
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
	SLATE_BEGIN_ARGS(SChroniclePanel) {}
		/** Tamaño de la letra. */
		SLATE_ARGUMENT(int32, FontSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * Sustituye el contenido por NewestFirst (la más reciente primero). Conserva la posición del scroll: quien está
	 * mirando el principio de la partida sigue ahí cuando entra un suceso nuevo, y quien sigue lo último, lo sigue.
	 */
	void SetLines(const TArray<FChronicleLine>& NewestFirst);

private:
	TSharedPtr<SScrollBox> ScrollBox;
	int32 FontSize = 11;
};
