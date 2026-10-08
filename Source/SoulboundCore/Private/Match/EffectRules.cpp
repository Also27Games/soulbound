#include "Match/EffectRules.h"

#include "Match/BondRaceRules.h"
#include "Match/MatchEvents.h"
#include "Match/VoidRules.h"

using Soulbound::Rules::LogEvent;

namespace Soulbound::Rules
{
	bool IsSeatInPlay(const FMatchState& State, int32 Seat)
	{
		// Decisión del autor del 08/10/2026: el activo con el turno empezado y los participantes de un duelo o un trilero en
		// curso. Entre turnos (donde se detiene el paso a paso) nadie lo está.
		// TODO(R-TUR-11): con las interrupciones del Warden (S-015), un efecto sobre quien está en juego cortará su turno en
		// vez de rechazarse.
		const bool bActiveInTurn = Seat == State.ActiveSeat && State.TurnPhase != ETurnPhase::BetweenTurns;
		const bool bInShellGame = State.ShellGame.IsActive() && (Seat == State.ShellGame.Thief || Seat == State.ShellGame.Victim);
		return bActiveInTurn || State.Duel.IsParticipant(Seat) || bInShellGame;
	}

	EEffectResult CanBanish(const FMatchState& State, int32 Seat, bool bFury)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EEffectResult::InvalidSeat;
		}
		// R-VAC-1: dentro, no es objetivo de ningún efecto salvo Liberación.
		if (State.Seats[Seat].IsInVoid())
		{
			return EEffectResult::InVoid;
		}
		// R-CAR-7: durante la carrera nadie va al Vacío salvo por la Furia.
		if (State.MatchPhase == EMatchPhase::BondRace && !bFury)
		{
			return EEffectResult::BondRace;
		}
		return IsSeatInPlay(State, Seat) ? EEffectResult::InPlay : EEffectResult::Applied;
	}

	EEffectResult Banish(FMatchState& State, int32 Seat, bool bFury, double NowSeconds)
	{
		const EEffectResult Result = CanBanish(State, Seat, bFury);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-VAC-1, R-TAB-24: la ficha sale del tablero y no ocupa casilla. R-VAC-4: la cuenta empieza de cero.
		// TODO(R-VAC-1): el Vacío de la Furia (dentro no funcionan Desvío ni Liberación, y el Warden se niega con frase) es de
		// S-015; hoy bFury solo cuenta para la regla de la carrera y para el registro.
		FSeatState& SeatState = State.Seats[Seat];
		SeatState.TileId = INDEX_NONE;
		SeatState.VoidTurns = 0;
		LogEvent(State, EMatchEventType::Banished, NowSeconds, Seat, bFury ? 1 : 0);
		return EEffectResult::Applied;
	}

	EEffectResult CanReleaseVoid(const FMatchState& State)
	{
		// R-EFE-17: sin nadie dentro, no es válida.
		const bool bSomeoneInside = State.Seats.ContainsByPredicate([](const FSeatState& Seat) { return Seat.IsInVoid(); });
		return bSomeoneInside ? EEffectResult::Applied : EEffectResult::NobodyInVoid;
	}

	EEffectResult ReleaseVoid(FMatchState& State, double NowSeconds)
	{
		const EEffectResult Result = CanReleaseVoid(State);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-VAC-5: todos salen a la vez a su hogar, sin esperar turnos. Si el activo esperaba en su turno del Vacío, la
		// máquina de turnos cierra la espera al ver que ya no está dentro (R-VAC-3).
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			LeaveVoid(State, Seat, EVoidExit::Released, NowSeconds);
		}
		return EEffectResult::Applied;
	}

	EEffectResult CanSendHome(const FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EEffectResult::InvalidSeat;
		}
		// R-DIR-2: en el Vacío o ya en su hogar, no es objetivo válido (R-EFE-4, R-NIC-2).
		if (State.Seats[Seat].IsInVoid())
		{
			return EEffectResult::InVoid;
		}
		if (State.Seats[Seat].TileId == HomeTile(State, Seat))
		{
			return EEffectResult::AlreadyHome;
		}
		return IsSeatInPlay(State, Seat) ? EEffectResult::InPlay : EEffectResult::Applied;
	}

	EEffectResult SendHome(FMatchState& State, int32 Seat, double NowSeconds)
	{
		const EEffectResult Result = CanSendHome(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-4, R-NIC-1: no es un movimiento suyo (no resuelve casilla ni cambia turnos); el hogar no encierra.
		const int32 Home = HomeTile(State, Seat);
		State.Seats[Seat].TileId = Home;
		LogEvent(State, EMatchEventType::SentHome, NowSeconds, Seat, Home);
		return EEffectResult::Applied;
	}
}
