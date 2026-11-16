#pragma once

#include "CoreMinimal.h"

struct FMatchState;

/**
 * La cola de la línea del Warden en pantalla (S-021, R-VOZ-1 sin audio): cada frase se enseña su tiempo o hasta la siguiente línea.
 * Desde S-041, cada frase dura según su longitud (R-VOZ-6), una burla no tapa una frase que detiene la mesa (R-VOZ-7) y un corte de turno
 * lleva su señal (R-VOZ-8). Funciones puras del tiempo que se les da, sin actor ni reloj propio, con tests como `BoardLayout`. No son
 * reglas de juego.
 */
namespace Soulbound::Presentation
{
	/** Clave de la palabra de corte en `frases.json` (S-041, R-VOZ-8). Opcional: sin ella no hay palabra de corte. */
	inline const TCHAR* const CutWordKey = TEXT("corte");

	/**
	 * Una frase de la línea del Warden lista para enseñarse (S-041, nombre propuesto): su texto, la línea pequeña de un corte, cuánto dura y
	 * cómo convive con las demás en la cola.
	 */
	struct FWardenLineItem
	{
		/** La línea, con su prefijo ("Warden, a ti:") y, en un corte del Warden, la palabra de corte. Vacía si solo hay línea pequeña. */
		FString Text;
		/** La línea pequeña de un corte (R-VOZ-8), en letra menor y color neutro; vacía si no es un corte. */
		FString CutNote;
		/** Cuánto se enseña, en segundos reales (WardenLineDuration). */
		double Seconds = 0.0;
		/** El instante de partida en que se dijo: las del mismo instante hacen cola. */
		double GameClock = 0.0;
		/** Si detiene la mesa (LineHoldsTheTable de su clave): cuenta para la espera y para el prólogo. */
		bool bHoldsTable = false;
		/** Si un comentario no la puede tapar (R-VOZ-7): las que detienen la mesa y la línea del Oráculo como golpe, que no la detiene. */
		bool bShielded = false;
		/** Si es un comentario (`comentario:*`), lo que no puede tapar a una frase protegida. */
		bool bRemark = false;
		/** Si es un corte de turno del Warden (golpe o escena que corta): oscurece el tablero (R-VOZ-8). */
		bool bWardenCut = false;

		/** Si no hay nada que enseñar: ni línea ni línea pequeña. */
		bool IsEmpty() const { return Text.IsEmpty() && CutNote.IsEmpty(); }
	};

	class FWardenLineQueue
	{
	public:
		/**
		 * Añade una frase en el instante real NowReal. Las del mismo instante de partida (la apertura: inicio, corrupción y orden) esperan su
		 * turno en la cola; una dicha más tarde sustituye a las que quedaban ("hasta la siguiente línea"). Salvo un comentario (R-VOZ-7): si la
		 * frase que se está enseñando, o alguna de las que esperan, está protegida (bShielded), se descarta y no se enseña nunca.
		 */
		void Push(const FWardenLineItem& Item, double NowReal);

		/**
		 * Qué enseñar en el instante NowReal (segundos reales, el reloj de la pantalla); cada frase dura sus Seconds. Una frase empieza a
		 * contar la primera vez que se pide. Vacía (IsEmpty) si no queda ninguna. Avanza la cola: llamarla de nuevo con el mismo instante da lo
		 * mismo.
		 */
		FWardenLineItem Current(double NowReal);

		/**
		 * Cuántas frases han empezado a enseñarse desde que se creó la cola (S-023): las que ha devuelto Current alguna vez. Las
		 * descartadas sin enseñarse no cuentan. En el prólogo todas son del instante cero y van en orden: es la frase de la apertura
		 * por la que va la línea, lo que necesita ForgottenDomainsShown.
		 */
		int32 ShownCount() const { return Shown; }

	private:
		/** Quita las frases que ya han cumplido su tiempo en NowReal, sin empezar a contar la primera si aún no se ha pedido. */
		void Advance(double NowReal);

		TArray<FWardenLineItem> Pending;
		/** El instante de partida de la última frase añadida, para saber si la nueva es del mismo instante. */
		double LastGameClock = -1.0;
		/** Desde cuándo (real) se enseña Pending[0]; negativo si aún no ha empezado. */
		double ShownSince = -1.0;
		/** Frases que han empezado a enseñarse (ShownCount). */
		int32 Shown = 0;
	};

	/**
	 * Cuánto dura una frase en pantalla (S-041, R-VOZ-6, nombre propuesto): el mayor entre MinSeconds y sus caracteres por SecondsPerChar. En
	 * un corte cuentan también los de su línea pequeña, porque hay que leer las dos (respuesta del autor al plan, 16/11/2026).
	 * @param Text La línea tal como se enseña, con su prefijo y la palabra de corte.
	 * @param CutNote La línea pequeña de un corte; vacía si no lo es.
	 * @param MinSeconds El mínimo (ABoardViewActor::WardenLineSeconds, 6 s).
	 * @param SecondsPerChar Segundos por carácter (ABoardViewActor::WardenLineSecondsPerChar, 0,065 s); cero o menos, todas duran el mínimo.
	 * @return Los segundos que se enseña.
	 */
	double WardenLineDuration(const FString& Text, const FString& CutNote, double MinSeconds, double SecondsPerChar);

	/**
	 * Si la frase de clave Key detiene la mesa (S-023, decisión del autor del 29/10/2026). La lista es de lo que no detiene: los comentarios,
	 * las instrucciones, los avisos del final, la victoria, el fin por tiempo y nadie vinculado. Una clave nueva detiene por defecto. Pública
	 * desde S-041, que la usa para la cola (R-VOZ-7).
	 */
	bool LineHoldsTheTable(const FString& Key);

	/**
	 * Las frases que dice el registro de State desde FromIndex, listas para la cola (S-041, nombre propuesto): una por WardenSpoke, con su
	 * prefijo, y cada corte de turno emparejado con su frase (R-VOZ-8). Un corte del Warden (TurnInterrupted con Detail INDEX_NONE, golpe o
	 * escena) antepone a la frase de su evento la palabra de corte y le pone debajo qué ha pasado; si no trae frase (el Oráculo como golpe) es
	 * una línea con la palabra de corte sola, que no detiene la mesa pero está protegida de las burlas. El corte del fin de la carrera
	 * (Detail 1) pone solo la línea pequeña bajo su frase; el del fin de la partida (Detail 2), nada. Las duraciones, con WardenLineDuration.
	 * @param State Estado de la partida, en solo lectura: el registro, las frases y los eventos del Warden.
	 * @param FromIndex Primera posición del registro que se mira: las anteriores ya se miraron.
	 * @param HumanSeat El asiento humano de esta máquina: "Warden, a ti:" y "Tu turno se corta".
	 * @param MinSeconds El mínimo de cada frase.
	 * @param SecondsPerChar Segundos por carácter.
	 * @return Las frases, en el orden del registro.
	 */
	TArray<FWardenLineItem> WardenLineItems(const FMatchState& State, int32 FromIndex, int32 HumanSeat, double MinSeconds, double SecondsPerChar);

	/**
	 * Cuánto dura el prólogo (S-022, R-PRO-1, decisión del autor del 29/10/2026): los segundos reales que tarda la apertura del Warden en
	 * enseñarse entera. Desde S-041 (R-VOZ-6), la suma de las duraciones de las frases del instante cero. Sin frases en la partida da cero:
	 * ni prólogo ni espera.
	 * @param State Estado de la partida recién empezada, en solo lectura.
	 * @param HumanSeat El asiento humano de esta máquina (el prefijo cuenta en la duración).
	 * @param MinSeconds El mínimo de cada frase (ABoardViewActor::WardenLineSeconds).
	 * @param SecondsPerChar Segundos por carácter (ABoardViewActor::WardenLineSecondsPerChar).
	 * @return Los segundos del prólogo; cero si no hay apertura.
	 */
	double OpeningSeconds(const FMatchState& State, int32 HumanSeat, double MinSeconds, double SecondsPerChar);

	/**
	 * La espera del Warden (S-023, decisión del autor del 29/10/2026, nombre propuesto): cuántos segundos reales detiene la mesa lo que
	 * ha dicho el Warden desde la posición FromIndex del registro. Desde S-041 (R-VOZ-6), la suma de las duraciones de las frases que detienen
	 * la mesa (LineHoldsTheTable), salvo las del instante cero, que son del prólogo. La línea del Oráculo como golpe no suma: el plazo de la
	 * consulta no se mueve.
	 * @param State Estado de la partida, en solo lectura.
	 * @param FromIndex Primera posición del registro que se mira: las anteriores ya se contaron.
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @param MinSeconds El mínimo de cada frase.
	 * @param SecondsPerChar Segundos por carácter.
	 * @return Los segundos de espera; cero si nada de lo nuevo detiene la mesa.
	 */
	double WardenWaitSeconds(const FMatchState& State, int32 FromIndex, int32 HumanSeat, double MinSeconds, double SecondsPerChar);

	/**
	 * La opacidad de la capa oscura del corte en el instante NowReal (S-041, R-VOZ-8, nombre propuesto; corrección del autor del 16/11/2026 tras
	 * PIE: un solo pulso era demasiado sutil). La capa da PulseCount pulsos seguidos de PulseSeconds cada uno: cada pulso vale MaxOpacity al
	 * empezar y baja en línea recta hasta cero al cumplirse, y el siguiente empieza ahí. Cero antes del corte, después del último pulso y sin
	 * corte. Un corte nuevo la reinicia: quien llama pasa el instante del último.
	 * @param NowReal El instante real.
	 * @param CutStartReal El instante real del último corte; negativo si no ha habido ninguno.
	 * @param MaxOpacity La opacidad al empezar cada pulso (ABoardViewActor::CutDimMaxOpacity, 0,85).
	 * @param PulseCount Cuántos pulsos (ABoardViewActor::CutDimPulseCount, 2); cero o menos, nunca se ve.
	 * @param PulseSeconds Lo que dura cada pulso (ABoardViewActor::CutDimPulseSeconds, 0,08 s); cero o menos, nunca se ve.
	 * @return La opacidad, de 0 a MaxOpacity.
	 */
	float CutDimOpacity(double NowReal, double CutStartReal, float MaxOpacity, int32 PulseCount, double PulseSeconds);
}
