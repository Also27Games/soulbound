#pragma once

#include "CoreMinimal.h"

struct FMatchState;

namespace Soulbound::Presentation
{
	/**
	 * Cuándo se enseña la carta del Elegido (S-029, R-VIS-13; nombre propuesto `ElectCard`): desde el Bonded del asiento humano hasta
	 * ElectCardSeconds después, o hasta que el jugador haga clic, o hasta que abra el Grimorio. Los vínculos de los dummies no la enseñan.
	 * Lee el registro de forma incremental, como la crónica; el reloj lo da quien la usa (la vista), no el motor.
	 */
	class SOULBOUND_API FElectCardTracker
	{
	public:
		/**
		 * Lee los eventos nuevos del registro; un Bonded del asiento humano abre la carta en Now. Con el registro más corto que lo leído
		 * (partida nueva), empieza de cero. La primera llamada lee desde el principio: quien arranca a mitad de partida llama antes a SkipExisting.
		 * @param State Estado de la partida.
		 * @param HumanSeat Asiento humano; INDEX_NONE si no hay.
		 * @param Now Reloj de la vista, en segundos.
		 */
		void Observe(const FMatchState& State, int32 HumanSeat, double Now);

		/** Da por leído todo el registro actual sin abrir nada (la vista arranca con una partida en marcha). */
		void SkipExisting(const FMatchState& State);

		/** Cierra la carta (clic del jugador). */
		void Dismiss();

		/** El jugador abre el Grimorio: cierra la carta, y mientras siga abierto no se enseña ninguna. */
		void SetGrimoireOpen(bool bOpen);

		/** Si la carta se ve en Now con ElectCardSeconds de duración. */
		bool IsVisible(double Now, double ElectCardSeconds) const;

		/** Dominio del Elegido de la carta (el Value del Bonded); INDEX_NONE si no hay carta abierta. */
		int32 GetDomainId() const { return DomainId; }

	private:
		int32 Scanned = 0;
		bool bOpen = false;
		bool bGrimoireOpen = false;
		double OpenedAt = 0.0;
		int32 DomainId = INDEX_NONE;
	};
}
