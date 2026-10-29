#pragma once

#include "CoreMinimal.h"

struct FMatchState;

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

		/**
		 * Cuántas frases han empezado a enseñarse desde que se creó la cola (S-023): las que ha devuelto Current alguna vez. Las
		 * descartadas sin enseñarse no cuentan. En el prólogo todas son del instante cero y van en orden: es la frase de la apertura
		 * por la que va la línea, lo que necesita ForgottenDomainsShown.
		 */
		int32 ShownCount() const { return Shown; }

	private:
		TArray<FString> Pending;
		/** El instante de partida de la última frase añadida, para saber si la nueva es del mismo instante. */
		double LastGameClock = -1.0;
		/** Desde cuándo (real) se enseña Pending[0]; negativo si aún no ha empezado. */
		double ShownSince = -1.0;
		/** Frases que han empezado a enseñarse (ShownCount). */
		int32 Shown = 0;
	};

	/**
	 * Cuánto dura el prólogo (S-022, R-PRO-1, decisión del autor del 29/10/2026): los segundos reales que tarda la apertura del Warden en
	 * enseñarse entera. Es el número de WardenSpoke del instante cero del registro de State por LineSeconds, los segundos de cada línea
	 * (ABoardViewActor::WardenLineSeconds). Sin frases en la partida no hay ningún WardenSpoke y da cero: ni prólogo ni espera.
	 * @param State Estado de la partida recién empezada, en solo lectura.
	 * @param LineSeconds Segundos que se enseña cada frase.
	 * @return Los segundos del prólogo; cero si no hay apertura.
	 */
	double OpeningSeconds(const FMatchState& State, double LineSeconds);

	/**
	 * La espera del Warden (S-023, decisión del autor del 29/10/2026, nombre propuesto): cuántos segundos reales detiene la mesa lo que
	 * ha dicho el Warden desde la posición FromIndex del registro. Cada WardenSpoke cuya clave detiene la mesa suma LineSeconds. Detiene
	 * toda frase salvo los comentarios (`comentario:*`), los avisos del final, la victoria, el fin por tiempo, nadie vinculado (la partida
	 * ya ha terminado) y las instrucciones (`instruccion:*`), y salvo las del instante cero, que son del prólogo. Qué detiene sale de la
	 * clave, no del tipo de evento: una clave nueva detiene por defecto.
	 * @param State Estado de la partida, en solo lectura.
	 * @param FromIndex Primera posición del registro que se mira: las anteriores ya se contaron.
	 * @param LineSeconds Segundos que se enseña cada frase (ABoardViewActor::WardenLineSeconds).
	 * @return Los segundos de espera; cero si nada de lo nuevo detiene la mesa.
	 */
	double WardenWaitSeconds(const FMatchState& State, int32 FromIndex, double LineSeconds);
}
