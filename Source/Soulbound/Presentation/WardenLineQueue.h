#pragma once

#include "CoreMinimal.h"

/**
 * La cola de la línea del Warden en pantalla (S-021, R-VOZ-1 sin audio): cada frase se enseña un tiempo o hasta la siguiente línea.
 * Función pura del tiempo que se le da, sin actor ni reloj propio, con tests como `BoardLayout`. No es regla de juego.
 */
namespace Soulbound::Presentation
{
	class FWardenLineQueue
	{
	public:
		/**
		 * Añade una frase dicha en el instante GameClock de la partida. Las que se dicen en el mismo instante (la apertura: inicio,
		 * corrupción y orden) esperan su turno en la cola; una dicha más tarde sustituye a las que quedaban, que se descartan ("hasta la
		 * siguiente línea").
		 */
		void Push(const FString& Text, double GameClock);

		/**
		 * Qué enseñar en el instante NowReal (segundos reales, el reloj de la pantalla) si cada frase dura LineSeconds. Una frase empieza a
		 * contar la primera vez que se pide. Vacío si no queda ninguna. Avanza la cola: llamarla de nuevo con el mismo instante da lo mismo.
		 */
		FString Current(double NowReal, double LineSeconds);

	private:
		TArray<FString> Pending;
		/** El instante de partida de la última frase añadida, para saber si la nueva es del mismo instante. */
		double LastGameClock = -1.0;
		/** Desde cuándo (real) se enseña Pending[0]; negativo si aún no ha empezado. */
		double ShownSince = -1.0;
	};
}
