#include "Match/VoidRules.h"

#include "Match/BondRaceRules.h"
#include "Match/InsolenceRules.h"
#include "Match/MatchEvents.h"
#include "Match/RuneRules.h"

using Soulbound::Rules::LogEvent;

namespace Soulbound::Rules
{
	const TArray<ESkillDifficulty>& VoidEscapeLadderFor(const FMatchState& State, int32 Seat)
	{
		// R-INS-3 (decisión del autor del 20/10/2026): la Furia castiga al jugador, no al personaje: en su Vacío todos juegan su
		// escalera, también Mictlantecuhtli.
		if (State.Seats.IsValidIndex(Seat) && State.Seats[Seat].bFuryVoid)
		{
			return State.Params.FuryEscapeLadder;
		}
		// R-ELE-2: Mictlantecuhtli empieza en fácil y sale como máximo al segundo turno; los dos valores, en datos.
		return HasAffinity(State, Seat, EAffinity::VoidEscape) ? State.Params.VoidEscapeAffinityLadder : State.Params.VoidEscapeLadder;
	}

	bool LeaveVoid(FMatchState& State, int32 Seat, EVoidExit Reason, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat) || !State.Seats[Seat].IsInVoid())
		{
			return false;
		}
		// R-VAC-3: por cualquier motivo, reaparece en su hogar. Un destierro futuro empieza la cuenta de cero (R-VAC-4).
		FSeatState& SeatState = State.Seats[Seat];
		SeatState.TileId = HomeTile(State, Seat);
		SeatState.VoidTurns = 0;
		SeatState.bFuryVoid = false;
		LogEvent(State, EMatchEventType::LeftVoid, NowSeconds, Seat, static_cast<int32>(Reason));
		// R-INS-2 (S-016): lo que se le aplazó dentro, ya fuera (si sale en su turno, al terminarlo).
		DischargeVoidPunishments(State, Seat, NowSeconds);
		return true;
	}

	int32 DeflectionRuneType(const FMatchState& State, int32 Seat)
	{
		// R-POD-6: Desvío es un poder del mapa del Elegido (R-ELE-3, un tipo por poder); un Eco no lo tiene nunca.
		if (!IsPowerActive(State, Seat, EPower::Deflection))
		{
			return INDEX_NONE;
		}
		const TArray<EPower>& Map = State.Elects.Elects[State.Seats[Seat].DomainId].RuneToPower;
		for (int32 RuneType = 0; RuneType < Map.Num(); ++RuneType)
		{
			if (Map[RuneType] == EPower::Deflection && State.Seats[Seat].RuneCountOf(RuneType) > 0)
			{
				return RuneType;
			}
		}
		return INDEX_NONE;
	}

	bool UseDeflection(FMatchState& State, int32 Seat, double NowSeconds)
	{
		const int32 RuneType = DeflectionRuneType(State, Seat);
		if (!State.Seats.IsValidIndex(Seat) || !State.Seats[Seat].IsInVoid() || RuneType == INDEX_NONE)
		{
			return false;
		}
		// R-INS-3 (S-016): en el Vacío de la Furia, Desvío se ofrece pero no funciona: la runa no se consume y el Warden se niega.
		if (State.Seats[Seat].bFuryVoid)
		{
			LogEvent(State, EMatchEventType::FuryRefusal, NowSeconds, Seat, 0);
			return false;
		}
		// R-POD-6, R-RUN-6: la runa sacrificada vuelve a la reserva de su Fuente; si era la última, Desvío se pierde en el
		// acto (R-ELE-5), porque IsPowerActive se calcula del Brazal.
		if (!ReturnRuneToSource(State, Seat, RuneType, State.Board.FindPowerSource(RuneType), NowSeconds))
		{
			return false;
		}
		return LeaveVoid(State, Seat, EVoidExit::Deflected, NowSeconds);
	}

	bool ApplyVoidEscapeResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		const ESkillDifficulty Difficulty = State.PendingDifficulty;
		State.PendingTrial = ESkillTrial::None;
		if (!State.Seats.IsValidIndex(Seat) || !State.Seats[Seat].IsInVoid())
		{
			return false;
		}
		LogEvent(State, bPassed ? EMatchEventType::VoidEscapePassed : EMatchEventType::VoidEscapeFailed, NowSeconds, Seat, static_cast<int32>(Difficulty));
		// R-VAC-2: si falla, sigue dentro; si lo supera, a su hogar. El turno termina en los dos casos (R-VAC-3).
		return bPassed && LeaveVoid(State, Seat, EVoidExit::Escaped, NowSeconds);
	}
}
