#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/**
 * Reloj del modo paso a paso (glosario "StepMode", nombre aceptado en S-007). Regla sin mundo: recibe el tiempo del
 * mundo como dato y decide cuándo detener la partida, cuándo soltar un turno y qué segundo de partida marca el reloj.
 * El anfitrión de partida lo usa y no contiene la regla. Congelar es desplazar el origen (OriginWorldSeconds), no
 * parar nada del motor: el estado de la partida y sus reglas no se enteran.
 */
struct FStepClock
{
	/** Tiempo del mundo en que empezó la partida; se desplaza al reanudar para que la pausa no cuente. */
	double OriginWorldSeconds = 0.0;
	/** Verdadero mientras la partida espera el paso. */
	bool bHeld = false;
	/** Segundo de partida en que se detuvo, el que marca el reloj mientras dura la espera. */
	double FrozenMatchSeconds = 0.0;
	/** Verdadero desde que se suelta un paso hasta que el turno sale de BetweenTurns: ese turno no se vuelve a detener. */
	bool bStepGranted = false;

	/** Segundo de partida en WorldNow: el congelado si espera, o el tiempo del mundo menos el origen. */
	double MatchSeconds(double WorldNow) const;

	/**
	 * Si hay que detener la partida ahora: modo activo, no detenida ya, sin paso concedido, entre turnos y la partida
	 * sin terminar. Un turno extra pendiente también espera: cada turno es un paso.
	 */
	bool ShouldHold(bool bStepMode, ETurnPhase TurnPhase, EMatchPhase MatchPhase) const;

	/** Detiene la partida en WorldNow: el reloj queda en el segundo de partida de ese instante. */
	void Hold(double WorldNow);

	/** Suelta un turno. Si no espera, no hace nada y devuelve false. Reanuda el reloj desde donde se detuvo. */
	bool Step(double WorldNow);

	/** Anota la fase del turno tras avanzar la máquina: al salir de BetweenTurns se retira el paso concedido. */
	void NoteTurnPhase(ETurnPhase TurnPhase);

	/** Sale del modo: si esperaba, reanuda el reloj; el paso concedido se retira. */
	void Disable(double WorldNow);
};
