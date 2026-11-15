#pragma once

#include "CoreMinimal.h"

#include "UnreachableRules.generated.h"

struct FMatchState;

/**
 * Por qué un asiento no puede ir a una casilla que no está entre sus alcanzables (R-TAB-29, S-039). Nombre propuesto en S-039.
 * El orden de la enumeración es el de la comprobación: primero el sello, después el confinamiento, y si no, la tirada.
 */
UENUM()
enum class EUnreachableReason : uint8
{
	/** La tirada no llega (cualquier otro caso). */
	Roll,
	/** Un Eco confinado a los Dominios Olvidados, ante un Dominio con dueño (R-TAB-20). */
	Confined,
	/** La casilla está sellada y el asiento no tiene Paso Libre (R-TAB-17, R-POD-2). */
	Sealed,
};

namespace Soulbound::Rules
{
	/**
	 * El motivo por el que Seat no puede ir a TileId (R-TAB-29, S-039). Mira solo la casilla pulsada y los rasgos del asiento
	 * (MakeMoverTraits), sin calcular caminos, en este orden: Sealed (casilla sellada y sin Paso Libre), Confined (el asiento está
	 * confinado y la casilla es de un Dominio con dueño, con la misma comprobación que el movimiento: IsOutsideConfinement) y Roll.
	 * No valida nada ni cambia el estado: que la casilla no sea alcanzable lo dice quien llama (SubmitDestinationIntent devuelve
	 * Unreachable); para una casilla alcanzable, o un asiento o una casilla que no existen, devuelve Roll.
	 * @param State El estado de la partida.
	 * @param Seat El asiento que quiere ir.
	 * @param TileId La casilla pulsada.
	 * @return El motivo.
	 */
	SOULBOUNDCORE_API EUnreachableReason UnreachableReason(const FMatchState& State, int32 Seat, int32 TileId);
}
