#pragma once

#include "CoreMinimal.h"
#include "BoardLayout.h"

struct FMatchState;

/** Lo que se enseña en una casilla según la partida (S-019, R-VIS-5). */
struct SOULBOUND_API FTileViewState
{
	/** Estructura que se ve junto a la casilla; None si ninguna. */
	EBoardStructure Structure = EBoardStructure::None;

	/** La runa flotante sobre el pilón de la Fuente, mientras le queda reserva. */
	bool bFloatingRune = false;

	/** Las cadenas del Sello sobre la losa, mientras la casilla está sellada. */
	bool bSealChains = false;
};

/**
 * Estado de vista del tablero (S-019; nombre propuesto `BoardViewState`): qué elementos del kit se enseñan, calculado del
 * estado de partida sin actor ni mundo. Todo se lee del estado; ninguna regla de juego vive aquí.
 */
struct SOULBOUND_API FBoardViewState
{
	/** Una por casilla, indexada por su identificador. */
	TArray<FTileViewState> Tiles;

	/** Una por asiento: si su ficha está en el tablero (fuera, en el Vacío; R-VIS-6). */
	TArray<bool> TokensOnBoard;
};

namespace Soulbound::Presentation
{
	/**
	 * Calcula qué enseña la vista del tablero (R-VIS-5, 6): las estructuras de Nodo solo desde el Despertar (R-TAB-8), el
	 * pilón siempre y su runa flotante solo con reserva, las cadenas solo en una casilla sellada, el trono solo si está
	 * configurado, y las fichas fuera del tablero en el Vacío.
	 * @param State Estado de la partida, en solo lectura.
	 * @param bNicheThroneConfigured Si la vista tiene pieza de trono (puede no existir: variante "el Nicho es el trono").
	 * @return El estado de vista.
	 */
	SOULBOUND_API FBoardViewState ComputeBoardViewState(const FMatchState& State, bool bNicheThroneConfigured);

		/**
		 * Tinte del suelo de una cuña (S-019, R-VIS-2, R-VIS-5): blanco mezclado hacia el color del Dominio por Strength, y
		 * apagado por ForgottenDim si el Dominio está Olvidado. Es el valor del parámetro `Color` de M_ColorVertice.
		 * @param DomainColor Color del Dominio.
		 * @param bForgotten Si el Dominio está Olvidado.
		 * @param Strength Cuánto se acerca el blanco al color del Dominio, de 0 (blanco) a 1 (el color entero).
		 * @param ForgottenDim Factor que multiplica el color si el Dominio está Olvidado.
		 * @return El color con que multiplicar el color por vértice del suelo.
		 */
		SOULBOUND_API FLinearColor WedgeFloorTint(const FLinearColor& DomainColor, bool bForgotten, float Strength, float ForgottenDim);
}
