#pragma once

#include "CoreMinimal.h"

/**
 * El telón (S-024, R-PRO-1 precisión; nombre propuesto en el PR, aún no está en el glosario). Regla sin mundo, como
 * FStepClock: recibe el tiempo del mundo como dato y no sabe de UWorld. Mientras está bajado, el prólogo del Warden no
 * empieza; la presentación lo sube cuando la pantalla de carga se ha ido, o se sube solo pasado un tope. El anfitrión de
 * partida decide cuándo llamarlo y no contiene la regla.
 */
struct FCurtain
{
	/** Verdadero mientras el telón está bajado. */
	bool bDown = false;
	/** Tiempo del mundo en que se bajó. */
	double LoweredWorldSeconds = 0.0;
	/** Segundos que espera como máximo; cero o negativo, el tope vence en el acto. */
	double TimeoutSeconds = 0.0;

	/** Baja el telón en WorldNow y anota desde cuándo; TimeoutSeconds es el tope de espera. */
	void Lower(double WorldNow, double InTimeoutSeconds);

	/** Sube el telón. Devuelve true si estaba bajado; subirlo dos veces no hace nada la segunda y devuelve false. */
	bool Raise(double WorldNow);

	/** Verdadero mientras está bajado. */
	bool IsDown() const { return bDown; }

	/** Verdadero si el telón sigue bajado y el tope ha vencido en WorldNow. Subido o sin bajar, siempre falso. */
	bool TimedOut(double WorldNow) const;
};
