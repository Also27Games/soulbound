#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

struct FRandomStream;

/**
 * Sentencias del Oráculo del Caos (glosario `Oracle`, R-ORA-4, S-017): qué decretos puede mostrar el orbe y con qué peso. La
 * sentencia no es azar puro ni es el Warden: mezcla las dos cosas con el sesgo de los datos (FWardenParams::OracleBias). Funciones
 * puras del estado; la consulta en sí (abrirla, el plazo, la intención) es de la máquina de turnos (Match/TurnRules.h).
 */
namespace Soulbound::Rules
{
	/**
	 * Decretos que el orbe puede mostrar ahora a Consultant (R-ORA-1, R-ORA-4), en el orden de la tabla: de los sacos del acto en
	 * NowSeconds y los anteriores, con peso en el acto, en su ventana y con usos; con entrega por Oráculo o "any"; de una sola
	 * opción (un dilema nunca llega por el Oráculo); con todos sus efectos implementados y sin Consulta forzada (una sentencia no
	 * abre otra consulta); y con algún objetivo válido para el consultante como quien lo dirige. Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API TArray<int32> OracleSentenceCandidates(const FMatchState& State, int32 Consultant, double NowSeconds);

	/**
	 * Probabilidad de cada candidato (R-ORA-4): (1 − sesgo) / N + sesgo × peso / suma de pesos, con el peso del cerebro (el del
	 * saco en el acto por los multiplicadores de los disparadores Triggers, TriggeredEventWeight); si todos pesan cero, uniforme.
	 * Devuelve pares (evento, probabilidad) en el orden de OracleSentenceCandidates, que suman 1; vacío sin candidatos.
	 * Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API TArray<TPair<int32, double>> OracleSentenceProbabilities(const FMatchState& State, int32 Consultant, double NowSeconds,
		const TArray<FActiveTrigger>& Triggers);

	/**
	 * Sortea una sentencia para Consultant con las probabilidades de OracleSentenceProbabilities y un valor de Random (el sorteo
	 * del Warden). Devuelve el evento, o INDEX_NONE si no hay candidatos (el orbe muestra "nada", OracleEmpty). Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API int32 DrawOracleSentence(const FMatchState& State, int32 Consultant, double NowSeconds, const TArray<FActiveTrigger>& Triggers,
		FRandomStream& Random);
}
