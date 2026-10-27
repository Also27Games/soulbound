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
	 * Si hay que detener la partida ahora: modo activo, no detenida ya, sin paso concedido, entre turnos, la partida sin
	 * terminar y sin un evento del Warden en curso (S-014: el evento se resuelve entre turnos y sus elecciones tienen reloj;
	 * la partida se detiene al acabar, antes del siguiente turno). Un turno extra pendiente también espera: cada turno es un
	 * paso.
	 */
	bool ShouldHold(bool bStepMode, ETurnPhase TurnPhase, EMatchPhase MatchPhase, bool bWardenEventInProgress = false) const;

	/** Detiene la partida en WorldNow: el reloj queda en el segundo de partida de ese instante. */
	void Hold(double WorldNow);

	/** Suelta un turno. Si no espera, no hace nada y devuelve false. Reanuda el reloj desde donde se detuvo. */
	bool Step(double WorldNow);

	/** Anota la fase del turno tras avanzar la máquina: al salir de BetweenTurns se retira el paso concedido. */
	void NoteTurnPhase(ETurnPhase TurnPhase);

	/**
	 * Reanuda el reloj sin conceder turno (S-014): si esperaba, el reloj sigue desde donde se detuvo; el paso concedido se
	 * retira. Lo usa el anfitrión al lanzar un evento del Warden con la partida detenida, para que corran sus relojes.
	 */
	void Resume(double WorldNow);

	/** Sale del modo: si esperaba, reanuda el reloj; el paso concedido se retira (Resume). */
	void Disable(double WorldNow);

	/**
	 * Adelanta el reloj de partida DeltaSeconds (S-018, SbClock): desplaza el origen y, si espera, también el segundo congelado. Con
	 * un DeltaSeconds negativo no hace nada: el reloj de partida nunca va hacia atrás.
	 */
	void Skip(double DeltaSeconds);
};
