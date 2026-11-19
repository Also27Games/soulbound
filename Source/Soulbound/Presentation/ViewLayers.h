#pragma once

#include "CoreMinimal.h"

/**
 * Las capas de la vista en el viewport (S-049, precisión de R-HUD-11 del autor del 19/11/2026; nombres aprobados): en qué orden se pinta cada capa
 * de Slate que ABoardViewActor añade al viewport. La interfaz de la vista va por debajo de los menús de la plantilla: todas las capas cuelgan de una
 * base con nombre, por debajo del orden con que llega al viewport un widget de UMG añadido sin orden, y conservan entre ellas el orden que tenían
 * (antes, del 9 al 14). La plantilla no se lee: si la base no basta, se ajusta tras la prueba en PIE.
 */
namespace Soulbound::Presentation
{
	/** Una capa de la vista en el viewport; cada una es una llamada a AddViewportWidgetContent de ABoardViewActor. */
	enum class EViewLayer : uint8
	{
		/** El oscurecido del corte y la penumbra de la Noche (CutDimOverlay), por debajo de todos los paneles. */
		Dim,
		/** El HUD de la partida (StatusOverlay): cajas, panel de decisión, botón del Grimorio, paneles de estado y de depuración. */
		Status,
		/** La línea del Warden (WardenLineOverlay). */
		WardenLine,
		/** El resultado y "Volver al menú" (ReturnToMenuOverlay). */
		ReturnToMenu,
		/** La carta del Elegido (ElectCardOverlay). Comparte orden con el aviso y el anillo, como antes; entre ellas manda el orden en que se añaden. */
		ElectCard,
		/** El aviso de casilla inalcanzable (NoticeOverlay). */
		Notice,
		/** El panel del anillo (RingOverlay). */
		Ring,
		/** El Grimorio (GrimoireOverlay), la más alta de la vista. */
		Grimoire,
	};

	/**
	 * La base de las capas de la vista: el orden del oscurecido, la más baja (valor inicial -10, aprobado por el autor el 19/11/2026). Con ella, la más
	 * alta queda en -5, por debajo de cualquier widget de UMG con orden -14 o mayor.
	 */
	constexpr int32 ViewLayerBase = -10;

	/**
	 * El orden con que llega al viewport un widget de UMG añadido sin orden (AddToViewport suma 10 al orden que recibe, que por defecto es 0). Es
	 * el valor del motor, no un ajuste: la interfaz de la vista queda por debajo.
	 */
	constexpr int32 DefaultViewportWidgetZOrder = 10;

	/**
	 * El orden de una capa de la vista en el viewport.
	 * @param Layer La capa.
	 * @return ViewLayerBase más la distancia de la capa al oscurecido (de 0 a 5; la carta, el aviso y el anillo comparten la suya).
	 */
	int32 ViewLayerZOrder(EViewLayer Layer);
}
