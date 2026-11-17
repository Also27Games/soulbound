#pragma once

#include "CoreMinimal.h"
#include "StatusText.h"

/**
 * El aviso de casilla inalcanzable (S-039, R-DEC-16; nombre propuesto): cuándo se ve, dónde y cuándo se quita. Funciones puras del tiempo y de
 * las posiciones que se les dan, sin actor ni reloj propio, con tests, como FWardenLineQueue. No es una regla de juego ni una frase del Warden.
 * Corrección del autor del 15/11/2026 tras las pruebas en PIE: el aviso sale junto al punto del clic, no en el panel de decisión.
 */
namespace Soulbound::Presentation
{
	class FUnreachableNotice
	{
	public:
		/**
		 * Enseña Text desde el instante NowReal (segundos reales) en el punto Position: el del clic rechazado, tal como lo da quien lo enseña, sin
		 * convertir; la vista le da el absoluto de Slate y lo pasa a su capa al colocarlo (corrección tras PIE del 16/11/2026, en S-040, de un defecto de
		 * S-039: convertirlo al enseñarlo usaba la geometría de una capa que antes del primer aviso no se había pintado nunca). Un aviso nuevo sustituye
		 * al que hubiera, aunque sea el mismo texto, lo lleva al punto nuevo y vuelve a contar desde cero.
		 */
		void Show(const FString& NewText, const FVector2f& NewPosition, double NowReal);

		/** Lo quita ya: un destino aceptado o el final del turno de tirada del humano. */
		void Dismiss();

		/**
		 * El texto que se ve en el instante NowReal si cada aviso dura Seconds, o vacío si no hay ninguno o ya pasó su tiempo. No cambia nada: llamarla
		 * con el mismo instante da lo mismo.
		 */
		FString Current(double NowReal, double Seconds) const;

		/** El punto del último aviso enseñado (el del clic, tal como se le dio), sea cual sea su estado; cero si no se ha enseñado ninguno. */
		FVector2f Position() const;

	private:
		FString Text;
		FVector2f ClickPosition = FVector2f::ZeroVector;
		double ShownAt = 0.0;
	};

	/**
	 * Dónde pone el aviso su esquina superior izquierda para verse entero (S-039, R-DEC-16): dentro de Bounds y sin tapar ninguna de las zonas de
	 * Avoid (desde S-045, las de NoticeAvoidZones). Prueba, en este orden, centrado encima del punto del clic, centrado debajo, a la izquierda
	 * y a la derecha, cada uno recortado a Bounds, y se queda con el primero que no toque ninguna zona; si ninguno vale, la posición libre más cercana
	 * al clic de una rejilla sobre Bounds. Con Size mayor que Bounds, la esquina de Bounds.
	 * @param Click El punto del clic rechazado.
	 * @param Size El tamaño del aviso.
	 * @param Bounds El área visible (el viewport).
	 * @param Avoid Las zonas que el aviso no debe tapar.
	 * @param Gap La separación entre el punto del clic y el aviso.
	 * @return La esquina superior izquierda del aviso.
	 */
	FVector2f PlaceNotice(const FVector2f& Click, const FVector2f& Size, const FBox2f& Bounds, const TArray<FBox2f>& Avoid, float Gap);

	/** Las piezas del HUD que el aviso puede tener que esquivar (S-045, nombre propuesto). La columna de la derecha ya no existe (R-HUD-16). */
	enum class EHudZone : uint8
	{
		MatchPanel,
		GoalPanel,
		DecisionPanel,
		GrimoireButton,
		StatusPanel,
		DebugPanel,
	};

	/**
	 * Las zonas que esquiva el aviso (S-045, nombre propuesto): la caja de la partida, la del objetivo, el panel de decisión y el botón del Grimorio,
	 * cada una mientras se ve; el panel de estado y el de depuración, solo si su tecla los enseña. En ese orden.
	 * @param Shown Lo que se ve del HUD (HudShown).
	 * @param bDecisionPanelShown Si el panel de decisión está a la vista.
	 * @return Las zonas.
	 */
	TArray<EHudZone> NoticeAvoidZones(const FHudShown& Shown, bool bDecisionPanelShown);

	/**
	 * El área en que se coloca el aviso (S-045, R-HUD-16, nombre propuesto): la capa entera, porque el tablero usa todo el ancho; ninguna columna la
	 * recorta.
	 * @param LayerSize El tamaño de la capa del aviso, que llena el viewport.
	 * @return El rectángulo de (0, 0) a LayerSize.
	 */
	FBox2f NoticeBounds(const FVector2f& LayerSize);
}
