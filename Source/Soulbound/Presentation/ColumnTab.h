#pragma once

#include "CoreMinimal.h"

/**
 * Las pestañas de la columna de paneles de la derecha (S-035, R-HUD-4): la leyenda y la crónica, una sola a la vista y a toda la altura.
 * Lógica pura, sin Slate: la vista la lee y la cambia con un clic en la cabecera o con ColumnTabKey.
 */
namespace Soulbound::Presentation
{
	/** Cada pestaña de la columna. */
	enum class EColumnTab : uint8
	{
		Legend,
		Chronicle,
	};

	/** La pestaña activa de la columna. Empieza en Leyenda y vuelve a ella en cada partida nueva. */
	struct FColumnTabState
	{
		EColumnTab Active = EColumnTab::Legend;

		/** Pasa a la otra pestaña: de Leyenda a Crónica y de Crónica a Leyenda. */
		void Toggle()
		{
		}

		/** Una partida nueva: vuelve a Leyenda. */
		void Reset()
		{
		}
	};
}
