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
	/** Margen tras subirlo (ampliación del 3/11): durante estos segundos IsDown sigue siendo verdadero. */
	double GraceSeconds = 0.0;
	/** Verdadero desde que se sube un telón bajado; con RaisedWorldSeconds fija cuándo acaba el margen. */
	bool bRaised = false;
	/** Tiempo del mundo en que se subió. */
	double RaisedWorldSeconds = 0.0;

	/** Baja el telón en WorldNow y anota desde cuándo; InTimeoutSeconds es el tope de espera e InGraceSeconds el margen tras subirlo. */
	void Lower(double WorldNow, double InTimeoutSeconds, double InGraceSeconds = 0.0);

	/** Sube el telón y anota cuándo. Devuelve true si estaba bajado; subirlo dos veces no hace nada la segunda y devuelve false. */
	bool Raise(double WorldNow);

	/** Verdadero mientras no se ha subido, o mientras WorldNow no ha pasado de la subida más el margen. Sin bajar nunca, falso. */
	bool IsDown(double WorldNow) const;

	/** Verdadero si el telón sigue sin subir y el tope ha vencido en WorldNow. Subido o sin bajar, siempre falso (el margen no cuenta). */
	bool TimedOut(double WorldNow) const;
};
