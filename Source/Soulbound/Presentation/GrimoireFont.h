#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 * La fuente del Grimorio (S-048, corrección 4 del autor del 17/11/2026, R-HUD-20): opcional, con el patrón de las imágenes. La vista carga el asset de
 * la ruta de Config (GrimoireFont) y lo mantiene vivo; si no hay, todo el texto del Grimorio sale con la fuente de hoy. Sin actor, con test.
 */
namespace Soulbound::Presentation
{
	/** La fuente propia del Grimorio y los nombres de sus tipos dentro del asset (nombre propuesto). Sin asset (Font nulo), la de hoy. */
	struct FGrimoireFont
	{
		/** El asset de fuente (un UFont); nulo si no hay. Lo mantiene vivo la vista. */
		const UObject* Font = nullptr;
		/** El tipo normal y el negrita dentro del asset (GrimoireFontRegularTypeface y GrimoireFontBoldTypeface de la vista). */
		FName RegularTypeface = TEXT("Regular");
		FName BoldTypeface = TEXT("Bold");
	};

	/**
	 * La letra de un texto del Grimorio (S-048, corrección 4).
	 * @param Font La fuente del Grimorio.
	 * @param bBold Si el texto va en negrita.
	 * @param Size El tamaño, en unidades de Slate; nunca menos de 1.
	 * @return Sin fuente, la de hoy (FCoreStyle, "Regular" o "Bold"); con ella, esa fuente con su tipo normal o negrita.
	 */
	FSlateFontInfo GrimoireFontInfo(const FGrimoireFont& Font, bool bBold, int32 Size);
}
