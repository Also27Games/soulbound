#pragma once

#include "CoreMinimal.h"
#include "Match/TurnRules.h"

/**
 * Lo que decide si el bucle de avance del anfitrión de partida da otra vuelta (S-032; nombres propuestos en el PR, aún no están en el
 * glosario). Reglas sin mundo, como FStepClock y FCurtain: reciben el tiempo del mundo como dato y no saben de UWorld. El anfitrión
 * (UMatchHostSubsystem::Advance) las usa y no contiene la regla.
 */
namespace Soulbound::Match
{
	/** Qué hace el bucle de avance tras entregar al motor la respuesta de un decisor. */
	enum class EAfterAnswer : uint8
	{
		/** El motor la aceptó: el estado cambió y se vuelve al principio de la vuelta. */
		Repeat,
		/** El motor no la aceptó: se descarta y la vuelta sigue hasta AdvanceTurn, que caduca lo que esté vencido. */
		Continue,
	};

	/**
	 * R-ANF-1: decide qué hace el bucle tras entregar una respuesta al motor. Repite solo con EIntentResult::Accepted; con cualquier
	 * otro resultado (también uno que se añada al enum) sigue. Repetir tras un rechazo es lo que dejaba el juego en bucle: el motor no
	 * había cambiado nada y el decisor volvía a contestar lo mismo.
	 * @param Result Lo que devolvió el motor al recibir la respuesta.
	 * @return Repeat si fue aceptada; Continue en cualquier otro caso.
	 */
	EAfterAnswer AfterAnswer(EIntentResult Result);
}

/**
 * R-ANF-2: cuenta las vueltas del bucle de avance de una sola llamada y niega las que pasan del tope. El tope no arregla ningún
 * bucle: garantiza que uno se vea en el acta en vez de congelar el juego.
 */
struct FAdvanceGuard
{
	/** Vueltas que se permiten en una llamada. Constante del anfitrión; el valor inicial es 1000 (una llamada normal da menos de diez). */
	int32 Limit = 1000;
	/** Vueltas pedidas en la llamada en curso, las negadas incluidas. */
	int32 Laps = 0;

	/**
	 * Cuenta una vuelta.
	 * @return Verdadero si cabe en el tope (de la primera a la Limit); falso en la siguiente y en todas las que pasan.
	 */
	bool Allow();

	/** Empieza una llamada nueva: el contador vuelve a cero. */
	void Reset();
};

/**
 * Decide cuándo avisa el anfitrión de que el bucle superó su tope, para que el aviso no se repita en cada tick (ajuste del autor, S-032).
 * El primero avisa; después, como mucho uno cada MinIntervalSeconds, y dice cuántas veces se ha superado el tope desde el aviso anterior.
 */
struct FAdvanceTripNotice
{
	/** Segundos mínimos entre dos avisos. Valor inicial 5. */
	double MinIntervalSeconds = 5.0;
	/** Tiempo del mundo del último aviso; solo vale si bNoticed. */
	double LastNoticeWorldSeconds = 0.0;
	/** Verdadero desde el primer aviso. */
	bool bNoticed = false;
	/** Veces que se ha superado el tope desde el último aviso, sin contar las que se informan ahora. */
	int32 PendingTrips = 0;

	/**
	 * Anota que el tope se ha superado en WorldNow.
	 * @param WorldNow Tiempo del mundo, en segundos.
	 * @param OutTrips Si devuelve verdadero, cuántas veces se ha superado desde el aviso anterior (en el primero, 1), esta incluida.
	 * @return Verdadero si toca avisar ahora; falso si es demasiado pronto (la cuenta sigue en PendingTrips).
	 */
	bool NoteTrip(double WorldNow, int32& OutTrips);
};
