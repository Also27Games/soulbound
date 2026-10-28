#include "Match/VictoryRules.h"

#include "Match/BondRaceRules.h"
#include "Match/EffectRules.h"
#include "Match/MatchEvents.h"
#include "Match/RuneRules.h"
#include "Warden/WardenRules.h"
#include "Warden/WardenVoice.h"

namespace Soulbound::Rules
{
	int32 RuneTypeCount(const FSeatState& SeatState)
	{
		return SeatState.Runes.FilterByPredicate([](int32 Count) { return Count > 0; }).Num();
	}

	bool IsAspirant(const FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return false;
		}
		// R-VIC-1, R-ECO-6: el Altar con los seis tipos, Elegido o Eco. Quien está en el Vacío no está en ninguna casilla.
		const FSeatState& SeatState = State.Seats[Seat];
		return !SeatState.IsInVoid() && State.Board.IsAltar(SeatState.TileId) && RuneTypeCount(SeatState) == Soulbound::Board::DomainCount;
	}

	bool SyncAspirants(FMatchState& State, double NowSeconds)
	{
		if (State.IsMatchOver())
		{
			return false;
		}
		bool bAnnounced = false;
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			const bool bAspirant = IsAspirant(State, Seat);
			FSeatState& SeatState = State.Seats[Seat];
			// R-PRO-4: una vez por cada vez que pasa a aspirante; al dejar de serlo, el anuncio se olvida y vuelve con él.
			if (bAspirant && !SeatState.bAspirantAnnounced)
			{
				LogEvent(State, EMatchEventType::AspirantAnnounced, NowSeconds, Seat);
				SpeakWardenLine(State, Soulbound::Warden::LineAspirant, NowSeconds, Seat);
				bAnnounced = true;
			}
			SeatState.bAspirantAnnounced = bAspirant;
		}
		return bAnnounced;
	}

	bool IsMatchEndDue(const FMatchState& State, double NowSeconds)
	{
		// R-VIC-4: el fin es un evento programado del Warden (R-PRO-0); sin Warden en la partida no hay fin por tiempo.
		return State.WardenParams.HasWarden() && !State.IsMatchOver() && NowSeconds >= State.WardenParams.MatchEndSeconds;
	}

	bool DeclareWardenVictory(FMatchState& State, EMatchEndReason Reason, double NowSeconds)
	{
		if (State.IsMatchOver())
		{
			return false;
		}
		State.MatchPhase = EMatchPhase::MatchOver;
		State.Outcome = EMatchOutcome::WardenVictory;
		State.EndReason = Reason;
		LogEvent(State, EMatchEventType::WardenVictory, NowSeconds, INDEX_NONE, static_cast<int32>(Reason));
		// S-021 (R-PRO-5): el fin por tiempo tiene su frase; la victoria por no haber vínculos, la de la victoria.
		SpeakWardenLine(State, Reason == EMatchEndReason::TimeOut ? Soulbound::Warden::LineTimeOut : Soulbound::Warden::LineVictory, NowSeconds);
		return true;
	}

	bool ApplyFinalChallengeResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		State.PendingTrial = ESkillTrial::None;
		if (bPassed)
		{
			// R-VIC-2: la partida termina en ese instante; nada queda pendiente y nada más avanza.
			State.MatchPhase = EMatchPhase::MatchOver;
			State.Outcome = EMatchOutcome::PlayerVictory;
			State.WinnerSeat = Seat;
			State.TurnPhase = ETurnPhase::BetweenTurns;
			LogEvent(State, EMatchEventType::MatchWon, NowSeconds, Seat);
			SpeakWardenLine(State, Soulbound::Warden::LineVictory, NowSeconds, Seat);
			return true;
		}
		// R-VIC-3, R-RUN-6: el Warden le arranca una runa al azar entre todas las que tiene (los duplicados cuentan), que vuelve a su
		// Fuente, y lo manda a su hogar. El aspirante tiene las seis: siempre hay runa y nunca está en su hogar, que no es el Altar.
		FRandomStream Draw = WardenDraw(State);
		const int32 RuneType = RandomRuneType(State, Seat, Draw);
		LogEvent(State, EMatchEventType::FinalChallengeFailed, NowSeconds, Seat, RuneType);
		if (RuneType != INDEX_NONE)
		{
			ReturnRuneToSource(State, Seat, RuneType, State.Board.FindPowerSource(RuneType), NowSeconds);
		}
		// La Vuelta al hogar como la del efecto (SentHome con la casilla), pero sin CanSendHome: esa rechaza a quien está en juego, y el
		// aspirante lo está hasta que la máquina termine su turno. Es la consecuencia de su propio turno, no un efecto sobre él.
		const int32 Home = HomeTile(State, Seat);
		State.Seats[Seat].TileId = Home;
		LogEvent(State, EMatchEventType::SentHome, NowSeconds, Seat, Home);
		return false;
	}
}
