#pragma once

#include "CoreMinimal.h"
#include "Match/RingRules.h"

/** Qué pasaría con una pulsación en un instante (S-030, R-ACT-10): si cuenta, o por qué no. */
enum class ERingPressKind : uint8
{
	/** Cuenta: cae dentro de un pulso sin resolver. */
	Counts,
	/** Antes del primer pulso: el anuncio. */
	Announcement,
	/** Entre dos pulsos: ninguno la admite (R-MIN-2). */
	BetweenPulses,
	/** Todos los pulsos están resueltos o vencidos. */
	Finished,
	/** La espera para empezar (S-042, R-MIN-19): la prueba aún no ha empezado; ninguna pulsación cuenta ni penaliza. */
	Waiting,
};

/** Cómo empezó una prueba del anillo tras la espera para empezar (S-042, R-MIN-19; nombre propuesto). */
enum class ERingStartCause : uint8
{
	/** No ha empezado, o empezó sin espera (Begin). */
	None,
	/** El jugador pulsó "Empezar", con el ratón o con su tecla. */
	Button,
	/** La cuenta llegó a cero: empezó sola, sin castigo. */
	Countdown,
};

/** Respuesta de FRingTrialRun::QueryPress: qué sería una pulsación en Now. Nombre propuesto en S-030. */
struct FRingPressQuery
{
	ERingPressKind Kind = ERingPressKind::Finished;
	/** Con Counts, el pulso (de 0 en adelante) al que iría; INDEX_NONE en otro caso. */
	int32 Pulse = INDEX_NONE;
	/** Con Counts, Now menos la coincidencia del pulso, en segundos: negativo si es antes. 0 en otro caso. */
	double OffsetSeconds = 0.0;
};

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
	/** Instante (reloj real) en que empieza el primer pulso: tras el anuncio (desde S-042, empezar más el respiro). Sin sentido mientras espera. */
	double StartSeconds = 0.0;
	/** Precisión de cada pulso ya resuelto, en orden. */
	TArray<double> Resolved;
	/** Espera para empezar (S-042, R-MIN-19): instante en que se abrió, espera máxima y respiro entre empezar y el primer pulso. */
	double OpenSeconds = 0.0;
	double MaxWaitSeconds = 0.0;
	double BreatherSeconds = 0.0;
	/** Instante en que empezó (pulsación de "Empezar" o fin de la cuenta), antes del respiro. */
	double StartedAtSeconds = 0.0;
	/** Si la prueba espera a que el jugador esté listo: ningún pulso corre. */
	bool bWaiting = false;
	/** Cómo empezó; None mientras espera o si empezó con Begin. */
	ERingStartCause StartCause = ERingStartCause::None;

	/** Empieza la prueba sin espera: el primer pulso de InSequence arranca en InStartSeconds. Borra lo resuelto. */
	void Begin(const FRingSequence& InSequence, double InStartSeconds);

	/**
	 * Abre la prueba con la espera para empezar (S-042, R-MIN-19): guarda InSequence, ya sorteada, y no arranca ningún pulso. Empezará con Start o,
	 * a InOpenSeconds + InMaxWaitSeconds, con StartIfDue; el primer pulso arranca InBreatherSeconds después de empezar. Borra lo resuelto.
	 */
	void Open(const FRingSequence& InSequence, double InOpenSeconds, double InMaxWaitSeconds, double InBreatherSeconds);

	/**
	 * El jugador pulsa "Empezar" en Now (S-042, R-MIN-19). Si la prueba espera y la cuenta no ha llegado a cero, empieza en Now (causa Button) y el
	 * primer pulso arranca en Now más el respiro. Esta pulsación no es una pulsación del anillo: no resuelve nada. Devuelve si empezó por ella;
	 * false si no esperaba o si la cuenta ya había vencido (entonces empieza sola, en su instante, como StartIfDue).
	 */
	bool Start(double Now);

	/**
	 * Si la prueba espera y en Now la cuenta ha llegado a cero, empieza sola (causa Countdown) en el instante en que venció, no en Now: el primer
	 * pulso arranca a la espera máxima más el respiro. Sin castigo: no resuelve nada. Devuelve si empezó ahora.
	 */
	bool StartIfDue(double Now);

	/** Si la prueba espera a que el jugador esté listo (S-042). */
	bool IsWaiting() const { return bWaiting; }

	/** Instante en que la cuenta llega a cero: apertura más espera máxima. */
	double StartDueSeconds() const { return OpenSeconds + MaxWaitSeconds; }

	/** Segundos que le quedan a la cuenta en Now, de 0 en adelante; 0 si no espera. */
	double CountdownSeconds(double Now) const;

	/** Segundos que esperó desde que se abrió hasta que empezó; 0 si aún espera o empezó con Begin. */
	double WaitedSeconds() const;

	/**
	 * Pulsación en Now. Primero cierra los pulsos vencidos (Advance). Cuenta si Now cae dentro de un pulso sin resolver: lo resuelve con su
	 * precisión. Devuelve si contó; una segunda pulsación en el mismo pulso, una entre pulsos, antes del primero o tras el último, no.
	 */
	bool Press(double Now);

	/**
	 * Qué sería una pulsación en Now (S-030, R-ACT-10), sin mutar nada: cuenta (con su pulso y su desfase respecto a la coincidencia, con
	 * signo) o por qué no. Es lo que decide Press, que la usa; los pulsos vencidos sin pulsar se saltan como lo hace Advance.
	 */
	FRingPressQuery QueryPress(double Now) const;

	/** Cierra con precisión 0 los pulsos sin resolver que han vencido en Now. */
	void Advance(double Now);

	/** Verdadero cuando todos los pulsos están resueltos. Una secuencia sin pulsos está terminada, salvo mientras espera para empezar (S-042). */
	bool IsFinished() const;

	/** Precisiones de los pulsos resueltos, en orden; al terminar, una por pulso. */
	const TArray<double>& Precisions() const { return Resolved; }

	/** Número de pulsos de la prueba. */
	int32 NumPulses() const { return Sequence.Num(); }

	/** Si en Now aún no ha empezado el primer pulso: la espera para empezar (S-042) o el anuncio, que desde S-042 es el respiro. */
	bool IsAnnouncing(double Now) const { return bWaiting || Now < StartSeconds; }

	/**
	 * Pulso en curso en Now, para pintar: el primero sin resolver si Now está dentro de su duración; INDEX_NONE en la espera, en el anuncio,
	 * entre pulsos o al terminar.
	 */
	int32 CurrentPulse(double Now) const;

	/**
	 * Fracción del cierre del primer pulso sin resolver en Now, para pintar: 0 al empezar el pulso, 1 en la coincidencia, acotada entre 0
	 * y 1 con cualquier tiempo. En la espera para empezar, 0 (S-042); terminada la prueba, 1.
	 */
	double CloseFraction(double Now) const;

	/**
	 * Si Now está dentro de la zona buena del primer pulso sin resolver (S-031, R-VIS-15): desde la coincidencia menos media zona hasta la
	 * coincidencia más media zona, esta no incluida. Falso en el anuncio, entre pulsos y con la prueba terminada. No muta nada.
	 */
	bool IsInSweetZone(double Now) const;
};
