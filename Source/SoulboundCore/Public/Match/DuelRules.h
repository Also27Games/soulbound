#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/** Qué pasa cuando un asiento reta a otro (R-DUE-1, R-DUE-4). Nombre propuesto en S-012. */
enum class EDuelStart : uint8
{
	/** El retado no tiene runas: no hay duelo, la acción se pierde y se registra DuelNoRunes (R-DUE-4). */
	NoRunes,
	/** Empieza el duelo: se registra DuelChallenged y se espera el rendimiento de los dos (R-DUE-1, R-DUE-3). */
	Started,
};

/** Cómo acaba un duelo (R-DUE-5, R-DUE-6). Nombre propuesto en S-012. */
struct FDuelResult
{
	/** Asiento que gana. */
	int32 Winner = INDEX_NONE;
	/** Asiento que pierde. */
	int32 Loser = INDEX_NONE;
	/** Si el perdedor no tenía runas y volvió a su hogar (R-DUE-6): entonces no hay botín. */
	bool bLoserSentHome = false;
};

/**
 * Duelo (GDD 04, R-POD-3, R-DUE-1 a R-DUE-6, R-ELE-2; S-012): quién puede retar a quién, el estado del duelo en curso
 * (FMatchState::Duel; solo estas reglas lo escriben) y quién gana. La máquina de turnos (Match/TurnRules.h) lleva la fase
 * Duel y la intención de cada participante, y entrega el botín al trilero (BeginSteal) con el ganador de ladrón.
 * Mientras no existe el minijuego de duelo, cada participante "rinde" un número de 0 a 1 que da su decisor.
 */
namespace Soulbound::Rules
{
	/**
	 * Si el asiento Seat reta desde un Nodo de Duelo (R-DUE-2): se ha movido este turno (R-RUN-3; pasar por el Nodo no
	 * cuenta), termina en un Nodo de Duelo y la carrera ha acabado (durante la carrera es una casilla normal, decisión del
	 * autor del 05/10/2026, aunque el tablero esté Despierto). No hace falta el poder Duelo.
	 */
	SOULBOUNDCORE_API bool ChallengesFromDuelNode(const FMatchState& State, int32 Seat);

	/**
	 * Asientos a los que puede retar a duelo el asiento Seat, sin repetir y en orden de asiento (R-POD-3, R-DUE-2, R-INT-2):
	 * desde un Nodo de Duelo (ChallengesFromDuelNode), todos los demás, estén donde estén; con el poder Duelo y movido este
	 * turno, los demás de su casilla. Elegidos o Ecos, tengan runas o no: el retador no ve el Brazal ajeno, y retar a quien
	 * no tiene es R-DUE-4. Vacío si no puede retar a nadie. Nombre propuesto en S-012.
	 */
	SOULBOUNDCORE_API TArray<int32> DuelTargets(const FMatchState& State, int32 Seat);

	/**
	 * Ganador de un duelo entre Challenger y Challenged con estos rendimientos (R-DUE-1, R-ELE-2): a quien tenga la afinidad
	 * DuelAdvantage (Shuten-dōji) se le suma FMatchParams::DuelAdvantageMargin, y gana el mayor. Con empate tras el margen
	 * gana Shuten-dōji si uno de los dos lo es y, si no, el retado (decisión del autor del 06/10/2026). Función pura del
	 * estado: no cambia nada. Devuelve el asiento ganador, siempre uno de los dos.
	 */
	SOULBOUNDCORE_API int32 DuelWinner(const FMatchState& State, int32 Challenger, int32 Challenged, double ChallengerPerformance, double ChallengedPerformance);

	/**
	 * El asiento Challenger reta al asiento Challenged (R-DUE-1, R-DUE-3, R-DUE-4): sin runas el retado, se registra
	 * DuelNoRunes y no hay duelo; si no, empieza el duelo (FMatchState::Duel) y se registra DuelChallenged, con el origen del
	 * reto (ChallengesFromDuelNode). El retado no elige nada. No valida que el reto sea legal: los objetivos los da
	 * DuelTargets. NowSeconds: marca de los eventos. Devuelve qué pasó.
	 */
	SOULBOUNDCORE_API EDuelStart BeginDuel(FMatchState& State, int32 Challenger, int32 Challenged, double NowSeconds);

	/**
	 * Guarda el rendimiento Performance del asiento Seat en el duelo en curso. No valida: lo hace SubmitDuelPerformance.
	 * Devuelve true si ya están los dos rendimientos.
	 */
	SOULBOUNDCORE_API bool RecordDuelPerformance(FMatchState& State, int32 Seat, double Performance);

	/**
	 * Resuelve el duelo en curso con los dos rendimientos (R-DUE-1): decide el ganador (DuelWinner) y registra DuelWon. Si el
	 * perdedor no tiene runas, vuelve a su hogar (HomeTile) y se registra DuelLoserSentHome (R-DUE-6); no es un movimiento
	 * suyo ni cambia turnos (R-INT-4). El duelo termina; el botín (R-DUE-5) lo empieza la máquina de turnos con BeginSteal.
	 * NowSeconds: marca de los eventos. Devuelve ganador, perdedor y si el perdedor volvió a su hogar.
	 */
	SOULBOUNDCORE_API FDuelResult ResolveDuel(FMatchState& State, double NowSeconds);
}
