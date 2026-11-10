#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "../Match/RingTrialRun.h"

/**
 * El acta de partida (S-030; glosario propuesto "Acta de partida", `MatchRecord`): un archivo de texto por partida, escrito mientras se
 * juega, con el que se reconstruye la partida entera leyéndolo. Aquí solo está el texto: funciones puras, sin actor, sin disco y sin mundo
 * (como EventText.h), probadas sin editor. Quien lo escribe en el archivo es el anfitrión de partida (UMatchHostSubsystem).
 * El acta no inventa datos: escribe el registro de eventos (FMatchState::EventLog) con los textos de EventText.h.
 */
namespace Soulbound::Presentation
{
	/**
	 * La marca de tiempo de una línea del acta, "[mm:ss.d]", con Seconds del reloj de partida (décimas truncadas; los minutos pasan de 59
	 * sin cambiar de formato). Un tiempo negativo se queda en cero.
	 */
	FString MatchRecordClock(double Seconds);

	/**
	 * La línea del acta del evento Index del registro de State (R-ACT-1): "[01:23.4] #57 Rolled asiento=2 otro=- valor=7 detalle=- | El
	 * asiento 2 saca un 7.". Marca, índice, nombre del tipo (EMatchEventType tal cual, sin el prefijo del enum), los cuatro campos (con - en
	 * los INDEX_NONE) y tras | la frase de DescribeEvent. Van todos los eventos, TileEntered y WardenTriggered incluidos, y nunca se funden
	 * dos en una línea. En un WardenSpoke la frase va completa (R-ACT-2): el prefijo de WardenLinePrefix y el texto sin acotaciones de audio;
	 * si la entrada no existe en la tabla, "frase desconocida". HumanSeat decide el prefijo ("a ti"); RingSeat, los textos de las pruebas
	 * (como DescribeEvent). Vacía si Index no está en el registro.
	 */
	FString MatchRecordLine(const FMatchState& State, int32 Index, int32 HumanSeat, int32 RingSeat);

	/**
	 * Las líneas del principio del acta (R-ACT-3): fecha y hora local de inicio, texto de la build (lo que pase el anfitrión), semilla, ruta
	 * y número de casillas del tablero, asiento del humano y número de asientos, duración prevista (matchEndSeconds de los parámetros del
	 * Warden) y una línea por Dominio con el nombre de su Elegido, en orden de Dominio. Todas empiezan por "# ".
	 */
	TArray<FString> MatchRecordHeader(const FMatchState& State, int32 Seed, int32 HumanSeat, const FString& BuildText, const FString& BoardPath, const FDateTime& StartedAt);

	/**
	 * Las líneas del final del acta (R-ACT-4): el resultado ("un jugador gana: asiento N", "el Warden gana: nadie se vinculó" o "el Warden
	 * gana: tiempo"), el reloj y la ronda en que acaba, y una línea por asiento con su rol, su Elegido o su Dominio Olvidado, las runas por
	 * tipo y en total, su insolencia y si está en el Vacío. Si la partida no ha terminado, la primera línea dice "partida abandonada" con el
	 * reloj, la ronda, la fase del turno y el asiento activo, y el resto es igual.
	 */
	TArray<FString> MatchRecordSummary(const FMatchState& State);

	/** El nombre del archivo del acta (R-ACT-5): "partida-AAAAMMDD-HHMMSS-semilla-N.log". */
	FString MatchRecordFileName(const FDateTime& StartedAt, int32 Seed);

	/** Una línea del acta que no es del registro de eventos (R-ACT-7): "[01:23.4] host | texto", con Prefix "host", "warden" o "humano". */
	FString MatchRecordHostLine(double Seconds, const TCHAR* Prefix, const FString& Text);

	/** Lo que devolvió el motor a una intención (R-ACT-8), en texto: el nombre de EIntentResult tal cual. */
	FString IntentResultText(EIntentResult Result);

	/**
	 * El texto de una pulsación del anillo del humano (R-ACT-10), con lo que la consulta dijo de ella: si cuenta, el pulso (de 1 en
	 * adelante) y el desfase en segundos respecto a su coincidencia, con signo (negativo, antes); si no, por qué (anuncio, entre pulsos,
	 * prueba terminada).
	 */
	FString RingPressLine(const FRingPressQuery& Query);

	/**
	 * El texto de "anillo cerrado" del acta (R-ACT-10): el motivo y los fotogramas del motor por segundo durante la prueba (Frames contados
	 * entre abrir y cerrar, entre Seconds reales). Con Seconds de cero o menos, 0.0.
	 */
	FString RingClosedRecordText(const FString& Why, int64 Frames, double Seconds);
}
