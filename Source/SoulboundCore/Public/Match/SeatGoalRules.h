#pragma once

#include "CoreMinimal.h"

#include "SeatGoalRules.generated.h"

struct FMatchState;

/**
 * Qué persigue un asiento en este momento (R-VIC-5, S-040; glosario "Meta del asiento", nombre propuesto en S-040). La consulta (SeatGoal)
 * lo decide en el orden de los valores: None, Niche, Altar y, si no, Runes. El orden de los valores no es el de la carrera.
 */
UENUM()
enum class ESeatGoal : uint8
{
	/** Nada: la partida ha terminado o el asiento no existe. Sin casillas. */
	None,
	/** Un Nicho Libre: en la carrera por el vínculo, mientras el asiento no se ha vinculado. */
	Niche,
	/** Las runas que le faltan, en las Fuentes donde las obtendría ahora; la lista puede estar vacía. */
	Runes,
	/** El Altar: el asiento tiene los seis tipos de runa, esté o no ya en él. */
	Altar,
};

/** La meta de un asiento (R-VIC-5, S-040, nombre propuesto): qué persigue y en qué casillas, en orden ascendente. */
struct FSeatGoal
{
	ESeatGoal Kind = ESeatGoal::None;
	TArray<int32> Tiles;
};

namespace Soulbound::Rules
{
	/**
	 * La meta del asiento Seat ahora (R-VIC-5, S-040). Solo lee: no cambia el estado ni decide nada, y usa las mismas comprobaciones que las
	 * reglas, sin copiarlas. En este orden:
	 *  - None, sin casillas: la partida ha terminado o el asiento no existe.
	 *  - Niche: la partida está en la carrera por el vínculo y el asiento no es Elegido. Las casillas son los Nichos Libres (ENicheState::Free);
	 *    los reclamados y los cerrados de los Dominios Olvidados no cuentan.
	 *  - Altar: el asiento tiene los seis tipos de runa (RuneTypeCount), esté o no en el Altar. La casilla es el Altar.
	 *  - Runes: en cualquier otro caso. Las casillas son las Fuentes en las que, si terminara ahí su movimiento ahora, obtendría o jugaría la
	 *    prueba para obtener (PreviewPowerSourceLanding da Obtained o TrialPending) una runa de un tipo que no tiene. Puede no haber ninguna.
	 * No mira si el asiento puede llegar con su tirada ni su confinamiento: dice dónde está la meta, no por dónde se va.
	 * @param State El estado de la partida.
	 * @param Seat El asiento.
	 * @return La meta y sus casillas, en orden ascendente.
	 */
	SOULBOUNDCORE_API FSeatGoal SeatGoal(const FMatchState& State, int32 Seat);
}
