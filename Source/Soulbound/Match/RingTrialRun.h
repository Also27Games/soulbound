#pragma once

#include "CoreMinimal.h"
#include "Match/RingRules.h"

/**
 * Una prueba del anillo en curso (S-026; nombre propuesto en el PR, aún no está en el glosario). Regla sin mundo, como FCurtain y
 * FStepClock: recibe el tiempo como dato y no sabe de UWorld ni de timers. El anfitrión la crea con una secuencia (MakeRingSequence),
 * le pasa las pulsaciones y el paso del tiempo, y al terminar convierte las precisiones con RingRules. El tiempo es el reloj real
 * (FPlatformTime::Seconds, decisión del autor del 06/11/2026), el mismo para la pulsación, el avance y el pintado.
 *
 * El pulso i dura desde su inicio hasta que vence su zona buena (FRingSequence::ExpiryAt). Dentro, cuenta solo la primera pulsación,
 * esté o no en la zona (antes de la zona da 0, decisión del autor del 06/11/2026); entre pulsos, ninguna (R-MIN-2). Los pulsos se
 * resuelven en orden: con una pulsación, en el acto; sin ella, al vencer, con precisión 0.
 */
struct FRingTrialRun
{
	/** La secuencia que se juega. */
	FRingSequence Sequence;
	/** Instante (reloj real) en que empieza el primer pulso: tras el anuncio. */
	double StartSeconds = 0.0;
	/** Precisión de cada pulso ya resuelto, en orden. */
	TArray<double> Resolved;

	/** Empieza la prueba: el primer pulso de InSequence arranca en InStartSeconds. Borra lo resuelto. */
	void Begin(const FRingSequence& InSequence, double InStartSeconds);

	/**
	 * Pulsación en Now. Primero cierra los pulsos vencidos (Advance). Cuenta si Now cae dentro de un pulso sin resolver: lo resuelve con su
	 * precisión. Devuelve si contó; una segunda pulsación en el mismo pulso, una entre pulsos, antes del primero o tras el último, no.
	 */
	bool Press(double Now);

	/** Cierra con precisión 0 los pulsos sin resolver que han vencido en Now. */
	void Advance(double Now);

	/** Verdadero cuando todos los pulsos están resueltos. Una secuencia sin pulsos está terminada. */
	bool IsFinished() const;

	/** Precisiones de los pulsos resueltos, en orden; al terminar, una por pulso. */
	const TArray<double>& Precisions() const { return Resolved; }

	/** Número de pulsos de la prueba. */
	int32 NumPulses() const { return Sequence.Num(); }

	/** Si en Now aún no ha empezado el primer pulso: el anuncio. */
	bool IsAnnouncing(double Now) const { return Now < StartSeconds; }

	/**
	 * Pulso en curso en Now, para pintar: el primero sin resolver si Now está dentro de su duración; INDEX_NONE en el anuncio, entre
	 * pulsos o al terminar.
	 */
	int32 CurrentPulse(double Now) const;

	/**
	 * Fracción del cierre del primer pulso sin resolver en Now, para pintar: 0 al empezar el pulso, 1 en la coincidencia, acotada entre 0
	 * y 1 con cualquier tiempo. Terminada la prueba, 1.
	 */
	double CloseFraction(double Now) const;
};
