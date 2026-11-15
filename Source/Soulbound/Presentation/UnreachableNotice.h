#pragma once

#include "CoreMinimal.h"

/**
 * La vida del aviso de casilla inalcanzable (S-039, R-DEC-16; nombre propuesto): cuándo se ve y cuándo se quita. Función pura del tiempo que se le
 * da, sin actor ni reloj propio, con tests, como FWardenLineQueue. No es una regla de juego ni una frase del Warden.
 */
namespace Soulbound::Presentation
{
	class FUnreachableNotice
	{
	public:
		/**
		 * Enseña Text desde el instante NowReal (segundos reales). Un aviso nuevo sustituye al que hubiera, aunque sea el mismo texto, y vuelve a
		 * contar desde cero.
		 */
		void Show(const FString& Text, double NowReal);

		/** Lo quita ya: un destino aceptado o el final del turno de tirada del humano. */
		void Dismiss();

		/**
		 * El texto que se ve en el instante NowReal si cada aviso dura Seconds, o vacío si no hay ninguno o ya pasó su tiempo. No cambia nada: llamarla
		 * con el mismo instante da lo mismo.
		 */
		FString Current(double NowReal, double Seconds) const;

	private:
		FString Text;
		double ShownAt = 0.0;
	};
}
