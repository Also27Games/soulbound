#pragma once

#include "CoreMinimal.h"
#include "Warden/WardenParams.h"

struct FMatchState;

/**
 * Disparadores contextuales (glosario `ContextTrigger`, R-DIS-1 a R-DIS-3, S-017): cinco condiciones sobre el estado, con
 * umbrales en datos, y lo que cada una hace con los pesos y las direcciones de los eventos, también en datos (las tablas de
 * FWardenTriggerParams). Funciones puras del estado y el reloj, sin mundo (R-WAR-1): las usan el cerebro en cada evaluación, las
 * sentencias del Oráculo y los eventos al lanzarse. Varios activos a la vez se multiplican (R-DIS-1).
 */
namespace Soulbound::Rules
{
	/**
	 * Disparadores activos en NowSeconds (R-DIS-1), en el orden de EWardenTrigger y, dentro de cada uno, de asiento: uno por
	 * asiento que lo causa (Partida estancada, sin asiento). Sin Warden, ninguno. Nombre propuesto en S-017.
	 *  - Líder claro: quien tiene ClearLeaderGap tipos de runa distintos o más que el segundo.
	 *  - Victoria inminente: quien tiene ImminentVictoryTypes tipos o más, o está en el Altar con los seis.
	 *  - Jugador ignorado: fuera del Vacío, más de IgnoredMinutes sin ser objetivo (FSeatState::LastTargetedSeconds).
	 *  - Partida estancada: fuera de la carrera, más de StalledMinutes sin runa obtenida ni duelo (FMatchState::LastProgressSeconds).
	 *  - Acaparador: quien tiene HoarderCount runas o más de un mismo tipo.
	 */
	SOULBOUNDCORE_API TArray<FActiveTrigger> ActiveTriggers(const FMatchState& State, double NowSeconds);

	/**
	 * Peso del evento EventIndex en el acto Act con los disparadores Triggers (R-DIS-1, R-DIS-2): el del saco por el multiplicador
	 * de cada disparador activo que lo nombra en su tabla, una vez por disparador (con bWhenActiveIsCause, solo si Director es uno de
	 * sus causantes), y por StrikesAgainstActiveMultiplier si es un golpe contra el activo y Director es uno de sus causantes.
	 * Director: quien lo dirigiría. Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API double TriggeredEventWeight(const FMatchState& State, int32 EventIndex, EAct Act, int32 Director, const TArray<FActiveTrigger>& Triggers);

	/**
	 * Probabilidad de intervenir del acto Act con los disparadores Triggers (R-DIS-1): la de los datos por
	 * ImminentVictoryInterveneMultiplier si hay Victoria inminente, sin pasar de 1. Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API double TriggeredInterveneProbability(const FMatchState& State, EAct Act, const TArray<FActiveTrigger>& Triggers);

	/**
	 * Si el evento EventIndex es un golpe contra el activo (R-DIS-2): golpe con alguna aplicación dañina (no un regalo) dirigida
	 * al activo. Es lo que multiplica StrikesAgainstActiveMultiplier. Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API bool IsStrikeAgainstActive(const FMatchState& State, int32 EventIndex);

	/**
	 * Peso de Seat en una dirección "al azar" (R-DIS-3, decisión del autor del 23/10/2026, siempre, con o sin disparadores): 1 más
	 * los segundos que lleva sin ser objetivo entre IgnoredMinutes × 60; por IgnoredRandomMultiplier si Jugador ignorado lo nombra
	 * en Triggers. Sin Warden, 1. Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API double RandomTargetWeight(const FMatchState& State, int32 Seat, double NowSeconds, const TArray<FActiveTrigger>& Triggers);

	/**
	 * Pesos de rival sugerido (R-DIS-2, decisión del autor del 23/10/2026) para la elección de rival señalado del evento EventIndex
	 * entre Rivals, en su orden: cada rival que causa un disparador cuya tabla le sugiere en este evento (bSuggestCause) suma su
	 * multiplicador, y el Jugador ignorado, IgnoredSuggestMultiplier en cualquier evento; los productos de varios se multiplican.
	 * 0 para quien no tiene sugerencia; todo 0 si no hay. Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API TArray<double> SuggestedRivalWeights(const FMatchState& State, int32 EventIndex, const TArray<int32>& Rivals, const TArray<FActiveTrigger>& Triggers);

	/**
	 * Lo que los disparadores activos Triggers cambian del evento en curso al lanzarse (R-DIS-2): la dirección preferida
	 * (FCurrentWardenEvent::bHasTargetOverride) y el Sello hacia el Altar de un causante (SealTowardsAltarSeat). Lo llama ApplyEvent.
	 * Nombre propuesto en S-017.
	 */
	SOULBOUNDCORE_API void ApplyTriggersToEvent(FMatchState& State, const TArray<FActiveTrigger>& Triggers);
}
