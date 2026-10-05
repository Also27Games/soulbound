#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

// Cabecera privada del motor: la comparten las reglas que escriben en el registro de eventos (Match/TurnRules.cpp,
// Match/BondRaceRules.cpp, Match/RuneRules.cpp y Match/ShellGameRules.cpp). No es API del módulo.
namespace Soulbound::Rules
{
	/**
	 * Añade un evento al registro de partida (R-TUR-15). NowSeconds: marca de tiempo del reloj de partida. Seat: asiento
	 * afectado (o quien actúa, si hay dos), o INDEX_NONE. Value: el dato del evento (ver FMatchEvent::Value), o INDEX_NONE.
	 * OtherSeat: el segundo asiento, el afectado cuando Seat actúa (S-010), o INDEX_NONE.
	 */
	inline void LogEvent(FMatchState& State, EMatchEventType Type, double NowSeconds, int32 Seat, int32 Value = INDEX_NONE, int32 OtherSeat = INDEX_NONE)
	{
		FMatchEvent& Event = State.EventLog.AddDefaulted_GetRef();
		Event.Type = Type;
		Event.ClockSeconds = NowSeconds;
		Event.Seat = Seat;
		Event.OtherSeat = OtherSeat;
		Event.Value = Value;
	}
}
