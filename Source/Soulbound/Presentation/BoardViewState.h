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
	 * Las casillas que llevan la marca del Sello (S-037, R-VIS-24; nombre propuesto `SealMarkedTiles`): exactamente las de `FBoardState::SealedTiles`.
	 * @param State Estado de la partida, en solo lectura.
	 * @return Los identificadores de casilla, de la más antigua a la más nueva; vacío sin Sellos.
	 */
	SOULBOUND_API TArray<int32> SealMarkedTiles(const FMatchState& State);

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
	 * Con qué factor se apaga el suelo liso de una cuña (S-040, R-VIS-27, nombre propuesto): 1 si el Dominio no se ve Olvidado; ForgottenFloorDim si se
	 * ve Olvidado y se pinta con su material propio (la textura ya lo distingue); ForgottenDim si se ve Olvidado sin él, como antes de S-040. Es el
	 * factor que se pasa a WedgeFloorTint, que mezcla el tinte del Dominio igual en los tres casos.
	 * @param bForgotten Si el Dominio se ve Olvidado (ForgottenDomainsShown).
	 * @param bForgottenMaterial Si el material del suelo de los Dominios Olvidados (ForgottenFloorMaterial) está cargado.
	 * @param ForgottenFloorDim Factor con el material propio (valor inicial 0,6).
	 * @param ForgottenDim Factor sin él, el de las casillas (valor inicial 0,3).
	 * @return El factor.
	 */
	SOULBOUND_API float WedgeFloorDim(bool bForgotten, bool bForgottenMaterial, float ForgottenFloorDim, float ForgottenDim);

	/**
	 * Las teclas de la vista para el HUD (S-022, R-CAM-1): qué vista está activa, sentada o cenital, y con qué tecla se cambia a la otra; y las
	 * teclas de ocultar. Es lo que dice ABoardViewActor::DescribeViewKeys.
	 * @param bSeated Si la vista activa es la sentada.
	 * @param ViewKey Nombre de la tecla que alterna la vista (SeatedViewKey).
	 * @param OverlayKey Nombre de la tecla de las etiquetas.
	 * @param PanelsKey Nombre de la tecla de los paneles.
	 * @param DebugTextKey Nombre de la tecla que oculta el panel de estado (desde S-027; antes, el texto de depuración).
	 * @param GrimoireKey Nombre de la tecla del Grimorio (S-027).
	 * @param DebugPanelKey Nombre de la tecla del panel de depuración (S-027).
	 * @return La línea de teclas.
	 */
	SOULBOUND_API FString ViewKeysText(bool bSeated, const FString& ViewKey, const FString& OverlayKey, const FString& PanelsKey, const FString& DebugTextKey,
		const FString& GrimoireKey, const FString& DebugPanelKey);

	/**
	 * El mapa al que vuelve "Volver al menú" (S-027, R-MEN-1): el de ReturnToMenuMap si tiene valor y existe; si no, el de arranque del juego
	 * (GameDefaultMap), como en S-022, para que el juego nunca se quede sin salida.
	 * @param ReturnToMenuMap La ruta de objeto de ReturnToMenuMap ("/Game/.../UGT_MainMenuMap.UGT_MainMenuMap"); vacía si no tiene valor.
	 * @param bReturnMapExists Si el paquete de ReturnToMenuMap existe.
	 * @param GameDefaultMap La ruta de objeto de GameDefaultMap.
	 * @return El nombre del paquete del mapa que se abre ("/Game/.../UGT_MainMenuMap").
	 */
	SOULBOUND_API FString MenuMapPackage(const FString& ReturnToMenuMap, bool bReturnMapExists, const FString& GameDefaultMap);

	/**
	 * El resultado de la partida (S-018, R-PRO-4): quién gana y por qué, en una línea ("GANA EL ASIENTO 2", "GANA EL WARDEN: SE ACABÓ EL TIEMPO",
	 * "GANA EL WARDEN: NADIE SE VINCULÓ"). Desde S-041 (R-HUD-8, añadido del autor del 16/11/2026) va en el centro, encima del botón "Volver al
	 * menú", y la tecla va aparte, debajo del botón (ReturnToMenuHint). Vacío si la partida no ha terminado.
	 * @param State Estado de la partida, en solo lectura.
	 * @return El texto del resultado, o vacío.
	 */
	SOULBOUND_API FString MatchResultText(const FMatchState& State);

	/**
	 * La línea pequeña bajo el botón "Volver al menú" (S-041, R-HUD-8, nombre propuesto): "o pulsa" y la tecla. Unreal llama "Enter" a Intro: con
	 * esa tecla dice "o pulsa Intro"; con otra, su nombre. Vacía sin tecla.
	 * @param KeyDisplayName El nombre que da Unreal a la tecla de volver al menú (ReturnToMenuKey).
	 * @return La línea, o vacía.
	 */
	SOULBOUND_API FString ReturnToMenuHint(const FString& KeyDisplayName);

	/**
	 * La línea pequeña de una tecla bajo un botón (S-042, nombre propuesto; sacada de ReturnToMenuHint, que la usa): "o pulsa" y la tecla, con Intro
	 * en español. La usan "Volver al menú" (S-041) y "Empezar" del anillo (R-MIN-19).
	 * @param KeyDisplayName El nombre que da Unreal a la tecla.
	 * @return "o pulsa Intro", "o pulsa F5"…, o vacía sin tecla.
	 */
	SOULBOUND_API FString KeyHintText(const FString& KeyDisplayName);

	/**
	 * Si el tablero está en penumbra (S-043, R-VIS-31, nombre propuesto): mientras la Noche de los Ecos está activa y la partida no ha terminado.
	 * @param State Estado de la partida, en solo lectura.
	 * @return true con la Noche activa y la partida en juego.
	 */
	SOULBOUND_API bool EchoNightDimShown(const FMatchState& State);

	/**
	 * La opacidad de la penumbra en el tiempo (S-043, R-VIS-31, nombre propuesto): desde la opacidad que tenía al cambiar, en línea recta hasta la
	 * configurada si hay Noche, o hasta cero si no, al cumplirse FadeSeconds. Así, si la Noche termina mientras aún sube, baja desde donde iba.
	 * @param bEchoNight Si ahora hay penumbra (EchoNightDimShown).
	 * @param FromOpacity La opacidad en el instante del último cambio.
	 * @param SecondsSinceChange Segundos reales desde el último cambio.
	 * @param MaxOpacity La opacidad con la Noche (ABoardViewActor::EchoNightDimOpacity, 0,3).
	 * @param FadeSeconds Lo que dura la transición (ABoardViewActor::EchoNightDimFadeSeconds, 0,8 s); cero o menos, de golpe.
	 * @return La opacidad, entre cero y MaxOpacity.
	 */
	SOULBOUND_API float EchoNightDimOpacity(bool bEchoNight, float FromOpacity, double SecondsSinceChange, float MaxOpacity, double FadeSeconds);

	/**
	 * El color de la capa oscura del tablero (S-043, R-VIS-31, nombre propuesto): la penumbra y el pulso del corte (S-041) comparten una capa y se
	 * ve el más oscuro de los dos, no la suma: la opacidad es la mayor, y el color, el del que la da (negro el pulso, NightColor la penumbra).
	 * @param NightColor El color de la penumbra (ABoardViewActor::EchoNightDimColor), sin su alfa.
	 * @param NightOpacity La opacidad de la penumbra ahora (EchoNightDimOpacity).
	 * @param CutOpacity La opacidad del pulso del corte ahora (CutDimOpacity).
	 * @return El color con la opacidad en su alfa.
	 */
	SOULBOUND_API FLinearColor BoardDimColor(const FLinearColor& NightColor, float NightOpacity, float CutOpacity);
}
