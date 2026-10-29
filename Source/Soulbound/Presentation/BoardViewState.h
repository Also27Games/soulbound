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
	 * Qué Dominios pinta Olvidados la vista (S-023, R-PRO-1, R-WAR-10, nombre propuesto): fuera del prólogo, los del estado; en el
	 * prólogo, solo aquellos cuya frase de corrupción ya se ha enseñado: el DomainForgotten que va justo antes de cada
	 * programado:corrupcion del instante cero dice el Dominio. Así todos se ven encendidos durante la bienvenida y cada frase apaga el
	 * suyo. Es un número de frases y no una lista de posiciones porque en el prólogo todas son del instante cero y se enseñan en orden.
	 * @param State Estado de la partida, en solo lectura.
	 * @param bInPrologue Si el prólogo está en marcha (UMatchHostSubsystem::IsInPrologue).
	 * @param OpeningLinesShown Cuántas frases de la apertura han empezado a enseñarse (FWardenLineQueue::ShownCount).
	 * @return Un valor por Dominio de State.Board.Domains, verdadero si se pinta Olvidado.
	 */
	SOULBOUND_API TArray<bool> ForgottenDomainsShown(const FMatchState& State, bool bInPrologue, int32 OpeningLinesShown);

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

	/**
	 * Las teclas de la vista para el HUD (S-022, R-CAM-1): qué vista está activa, sentada o cenital, y con qué tecla se cambia a la otra; y las
	 * teclas de ocultar. Es lo que dice ABoardViewActor::DescribeViewKeys.
	 * @param bSeated Si la vista activa es la sentada.
	 * @param ViewKey Nombre de la tecla que alterna la vista (SeatedViewKey).
	 * @param OverlayKey Nombre de la tecla de las etiquetas.
	 * @param PanelsKey Nombre de la tecla de los paneles.
	 * @param DebugTextKey Nombre de la tecla del texto de depuración.
	 * @return La línea de teclas.
	 */
	SOULBOUND_API FString ViewKeysText(bool bSeated, const FString& ViewKey, const FString& OverlayKey, const FString& PanelsKey, const FString& DebugTextKey);

	/**
	 * La pantalla del resultado (S-018, R-PRO-4; desde S-022, con la vuelta al menú): quién gana y por qué, y en la línea siguiente cómo se
	 * vuelve al menú ("Intro o el botón: volver al menú"). Vacía si la partida no ha terminado.
	 * @param State Estado de la partida, en solo lectura.
	 * @param ReturnKey Nombre de la tecla que vuelve al menú (ReturnToMenuKey).
	 * @return El texto del resultado, en dos líneas, o vacío.
	 */
	SOULBOUND_API FString MatchResultText(const FMatchState& State, const FString& ReturnKey);
}
