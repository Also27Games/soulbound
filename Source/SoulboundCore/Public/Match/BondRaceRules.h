#pragma once

#include "CoreMinimal.h"
#include "Board/MovementRules.h"
#include "Match/MatchState.h"

struct FRandomStream;

/**
 * Carrera por el vínculo y Elegidos (GDD 03), con las transiciones del tablero que dispara (GDD 01): el vínculo al
 * reclamar un Nicho, el Despertar, el fin de carrera, el hogar y el confinamiento de los Ecos. Son las únicas reglas
 * que escriben el rol y el Dominio de un asiento, el dueño y el Nicho de un Dominio tras el inicio, el estado del
 * tablero y la fase y el resultado de la partida. La máquina de turnos (Match/TurnRules.h) las llama; el tiempo es
 * un dato, como allí.
 */
namespace Soulbound::Rules
{
	/**
	 * Resuelve que el asiento Seat termina su movimiento en su casilla actual, si es un Nicho (R-CAR-4 a 6):
	 *  - Un Eco en un Nicho Libre durante la carrera lo reclama: el Nicho pasa a Reclamado, el Dominio tiene dueño
	 *    (OwnerSeat) y el asiento pasa a Elegido de ese Dominio (evento Bonded). Sigue tirando un dado.
	 *  - Si era el primer vínculo, el tablero pasa de Primordial a Despierto (R-TAB-9, evento BoardAwakening).
	 *  - Si era el último Nicho Libre, la carrera termina en ese instante (R-CAR-1, EndBondRace).
	 *  - Terminar en un Nicho Reclamado o Cerrado, o hacerlo un Elegido, no cambia nada. Fuera de la carrera, tampoco.
	 * NowSeconds: marca de tiempo de los eventos. Random: el sorteo de la partida (lo usa el fin de carrera).
	 * Devuelve true si el asiento se vinculó.
	 */
	SOULBOUNDCORE_API bool ResolveNicheLanding(FMatchState& State, int32 Seat, double NowSeconds, FRandomStream& Random);

	/**
	 * El asiento Seat reclama el Nicho del Dominio DomainId y se vincula a su Elegido (R-CAR-4, R-CAR-5): lo que sigue a
	 * llegar a un Nicho Libre, sin mirar dónde está la ficha. Lo usan ResolveNicheLanding y la edición de depuración
	 * BondSeat (S-011), así que las dos dejan el mismo estado por construcción: Nicho Reclamado, dueño, rol, Dominio del
	 * asiento y evento Bonded; el Despertar si es el primer vínculo (R-TAB-9) y el fin de carrera si era el último Nicho
	 * Libre (R-CAR-1). No valida: quien llama comprueba que la partida está en la carrera, que el asiento es un Eco y que
	 * el Nicho está Libre. Nombre propuesto en S-011.
	 */
	SOULBOUNDCORE_API void ClaimNiche(FMatchState& State, int32 Seat, int32 DomainId, double NowSeconds, FRandomStream& Random);

	/**
	 * Termina la carrera (R-CAR-8, R-TAB-10 a 12), en este orden:
	 *  - Sin ningún Elegido: la partida termina con victoria del Warden (evento WardenVictory) y nada más cambia.
	 *  - Si no, todo Dominio sin dueño pasa a Olvidado con su Nicho Cerrado; cada Eco recibe al azar un Dominio
	 *    Olvidado distinto como hogar (HomeAssigned); el Eco que esté en un Dominio con dueño vuelve a su hogar
	 *    (ReturnedHome, decisión del autor del 01/10/2026); la partida pasa a la caza de runas.
	 *  - Se registra el fin (BondRaceEnded, con el número de Elegidos).
	 * 	  NowSeconds: marca de tiempo de los eventos. El reparto de hogares baraja con su propia semilla
	 *	  (FMatchState::HomeAssignmentSeed): misma semilla, misma asignación, y no toca los dados (S-016).
	 *	  Devuelve false, sin cambiar nada, si la partida no está en la carrera.
	 */
	SOULBOUNDCORE_API bool EndBondRace(FMatchState& State, double NowSeconds);

	/**
	 * Casilla hogar del asiento Seat (R-TAB-13): el Nicho de su Dominio si lo tiene (el reclamado si es Elegido, desde
	 * el vínculo; el Cerrado del Dominio asignado si es Eco tras la carrera), y el Altar si no (todo Eco durante la
	 * carrera). Devuelve INDEX_NONE si el asiento no existe.
	 */
	SOULBOUNDCORE_API int32 HomeTile(const FMatchState& State, int32 Seat);

	/**
	 * Rasgos de movimiento del asiento Seat según el estado (R-TAB-20, R-ECO-2): un Eco tras la carrera queda
	 * confinado al Altar y los Dominios Olvidados, salvo durante la Noche de los Ecos (R-EFE-18, S-014); durante la carrera,
	 * y un Elegido siempre, sin restricción. Paso Libre
	 * (R-POD-2, R-ELE-2): un Elegido con la runa que se lo da según su mapa, o Fenrir siempre por su afinidad.
	 * Es la función que usa la máquina de turnos para calcular las casillas alcanzables, así que la interfaz y el motor
	 * comparten criterio (R-TAB-21). Asiento inexistente: rasgos por defecto.
	 */
	SOULBOUNDCORE_API FMoverTraits MakeMoverTraits(const FMatchState& State, int32 Seat);
}
