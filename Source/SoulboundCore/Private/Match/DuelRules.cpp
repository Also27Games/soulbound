#include "Match/DuelRules.h"

#include "Match/BondRaceRules.h"
#include "Match/EffectRules.h"
#include "Match/MatchEvents.h"
#include "Match/RuneRules.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** Si el asiento tiene alguna runa en el Brazal. */
	bool HasRunes(const FSeatState& Seat)
	{
		return Seat.Runes.ContainsByPredicate([](int32 Count) { return Count > 0; });
	}
}

namespace Soulbound::Rules
{
	bool ChallengesFromDuelNode(const FMatchState& State, int32 Seat)
	{
		// R-DUE-2: acabada la carrera (en carrera es una casilla normal, decisión del autor del 05/10/2026) y terminando el
		// movimiento en el Nodo (R-RUN-3: quedarse sin moverse no cuenta; pasar por él tampoco, solo se mira la casilla final).
		if (!State.Seats.IsValidIndex(Seat) || !State.bActiveSeatMoved || State.MatchPhase != EMatchPhase::RuneHunt)
		{
			return false;
		}
		const int32 TileId = State.Seats[Seat].TileId;
		return State.Board.IsValidTile(TileId) && State.Board.Tiles[TileId].PowerNodeEffect == EPowerNodeEffect::Duel;
	}

	TArray<int32> DuelTargets(const FMatchState& State, int32 Seat)
	{
		TArray<int32> Targets;
		if (!State.Seats.IsValidIndex(Seat) || !State.bActiveSeatMoved)
		{
			return Targets;
		}
		const bool bFromNode = ChallengesFromDuelNode(State, Seat);
		const bool bPortable = IsPowerActive(State, Seat, EPower::Duel);
		const int32 TileId = State.Seats[Seat].TileId;
		for (int32 Other = 0; Other < State.Seats.Num(); ++Other)
		{
			// R-DUE-2: desde el Nodo, cualquiera, esté donde esté; R-POD-3: con el poder, quien comparte casilla. Cada objetivo
			// una sola vez, aunque valgan las dos. Tenga runas o no: retar a quien no tiene es R-DUE-4. R-INT-3 (S-013): quien
			// está en el Vacío no puede ser retado, tampoco desde el Nodo.
			if (Other != Seat && !State.Seats[Other].IsInVoid() && (bFromNode || (bPortable && State.Seats[Other].TileId == TileId)))
			{
				Targets.Add(Other);
			}
		}
		return Targets;
	}

	int32 DuelWinner(const FMatchState& State, int32 Challenger, int32 Challenged, double ChallengerPerformance, double ChallengedPerformance)
	{
		// R-ELE-2: el margen de Shuten-dōji se suma a su rendimiento, sea retador o retado. Es cómo aplica la afinidad el doble
		// de prueba; el minijuego real la aplicará como margen de acierto.
		const bool bChallengerAdvantage = HasAffinity(State, Challenger, EAffinity::DuelAdvantage);
		const bool bChallengedAdvantage = HasAffinity(State, Challenged, EAffinity::DuelAdvantage);
		const double Margin = State.Params.DuelAdvantageMargin;
		const double ChallengerScore = ChallengerPerformance + (bChallengerAdvantage ? Margin : 0.0);
		const double ChallengedScore = ChallengedPerformance + (bChallengedAdvantage ? Margin : 0.0);
		if (ChallengerScore != ChallengedScore)
		{
			return ChallengerScore > ChallengedScore ? Challenger : Challenged;
		}
		// Empate tras el margen (decisión del autor del 06/10/2026): Shuten-dōji si uno lo es; si no, el retado. Así un
		// duelo devuelve siempre un ganador (R-DUE-1).
		return bChallengerAdvantage && !bChallengedAdvantage ? Challenger : Challenged;
	}

	EDuelStart BeginDuel(FMatchState& State, int32 Challenger, int32 Challenged, double NowSeconds)
	{
		// R-DUE-4: sin runas el retado, no hay duelo y la acción se pierde; el registro lo deja.
		if (!State.Seats.IsValidIndex(Challenged) || !HasRunes(State.Seats[Challenged]))
		{
			LogEvent(State, EMatchEventType::DuelNoRunes, NowSeconds, Challenger, INDEX_NONE, Challenged);
			return EDuelStart::NoRunes;
		}
		// R-DUE-3: el retado no elige nada; el duelo empieza y se esperan los dos rendimientos.
		FDuelState& Duel = State.Duel;
		Duel = FDuelState();
		Duel.Challenger = Challenger;
		Duel.Challenged = Challenged;
		Duel.bFromDuelNode = ChallengesFromDuelNode(State, Challenger);
		LogEvent(State, EMatchEventType::DuelChallenged, NowSeconds, Challenger, Duel.bFromDuelNode ? 1 : 0, Challenged);
		return EDuelStart::Started;
	}

	EDuelStart BeginForcedDuel(FMatchState& State, int32 Challenger, int32 Challenged, bool bAllRunes, double NowSeconds)
	{
		// R-DUE-4: sin runas el retado no hay duelo (el motor ya filtra a quien no las tiene: CanForceDuel).
		if (!State.Seats.IsValidIndex(Challenged) || !HasRunes(State.Seats[Challenged]))
		{
			LogEvent(State, EMatchEventType::DuelNoRunes, NowSeconds, Challenger, INDEX_NONE, Challenged);
			return EDuelStart::NoRunes;
		}
		// R-EFE-13: nace de un evento, no de un Nodo (bActiveSeatMoved puede ser de un turno anterior): Value 2 en el registro.
		FDuelState& Duel = State.Duel;
		Duel = FDuelState();
		Duel.Challenger = Challenger;
		Duel.Challenged = Challenged;
		Duel.bAllRunes = bAllRunes;
		LogEvent(State, EMatchEventType::DuelChallenged, NowSeconds, Challenger, 2, Challenged);
		return EDuelStart::Started;
	}

	EDuelStart BeginWardenDuel(FMatchState& State, int32 Challenged, bool bAllRunes, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Challenged) || !HasRunes(State.Seats[Challenged]))
		{
			return EDuelStart::NoRunes;
		}
		// El Warden reta: el empate es del retado (DuelWinner, decisión de S-012). Su rendimiento es un dato fijo.
		FDuelState& Duel = State.Duel;
		Duel = FDuelState();
		Duel.bAgainstWarden = true;
		Duel.Challenged = Challenged;
		Duel.bAllRunes = bAllRunes;
		Duel.ChallengerPerformance = State.Params.WardenDuelPerformance;
		LogEvent(State, EMatchEventType::WardenDuelStarted, NowSeconds, Challenged, FMath::RoundToInt(State.Params.WardenDuelPerformance * 100.0), INDEX_NONE,
			bAllRunes ? 1 : 0);
		return EDuelStart::Started;
	}

	bool RecordDuelPerformance(FMatchState& State, int32 Seat, double Performance)
	{
		FDuelState& Duel = State.Duel;
		(Seat == Duel.Challenger ? Duel.ChallengerPerformance : Duel.ChallengedPerformance) = Performance;
		// Contra el Warden, su rendimiento ya está y solo falta el del jugador.
		return Duel.bAgainstWarden ? Duel.HasPerformed(Duel.Challenged) : Duel.HasPerformed(Duel.Challenger) && Duel.HasPerformed(Duel.Challenged);
	}

	FDuelResult ResolveDuel(FMatchState& State, double NowSeconds)
	{
		const FDuelState Duel = State.Duel;
		State.Duel = FDuelState();
		FDuelResult Result;
		Result.bAgainstWarden = Duel.bAgainstWarden;
		Result.bAllRunes = Duel.bAllRunes;
		Result.Winner = DuelWinner(State, Duel.Challenger, Duel.Challenged, Duel.ChallengerPerformance, Duel.ChallengedPerformance);
		Result.Loser = Result.Winner == Duel.Challenger ? Duel.Challenged : Duel.Challenger;
		if (Duel.bAgainstWarden)
		{
			// Challenger es INDEX_NONE: ganar el reto es ganar el Warden. Lo que pierde el jugador, y su registro, es de
			// SettleWardenDuel (necesita el sorteo del evento).
			return Result;
		}
		LogEvent(State, EMatchEventType::DuelWon, NowSeconds, Result.Winner, Result.Winner == Duel.Challenged ? 1 : 0, Result.Loser);
		// R-EFE-8 (S-014): ganarle un duelo a un Marcado cobra su marca, sea el ganador el retador o el retado.
		CollectMark(State, Result.Winner, Result.Loser, NowSeconds);
		// R-DUE-6: el perdedor sin runas (solo puede ser un retador sin runas desde un Nodo) vuelve a su hogar. No es un
		// movimiento suyo: no resuelve casilla ni cambia turnos (R-INT-4), como la Expulsión.
		if (!HasRunes(State.Seats[Result.Loser]))
		{
			const int32 Home = HomeTile(State, Result.Loser);
			State.Seats[Result.Loser].TileId = Home;
			Result.bLoserSentHome = true;
			LogEvent(State, EMatchEventType::DuelLoserSentHome, NowSeconds, Result.Winner, Home, Result.Loser);
		}
		return Result;
	}

	int32 SeizeAllRunes(FMatchState& State, int32 Winner, int32 Loser, double NowSeconds)
	{
		int32 Seized = 0;
		if (!State.Seats.IsValidIndex(Winner) || !State.Seats.IsValidIndex(Loser) || Winner == Loser)
		{
			return Seized;
		}
		// R-EFE-13: una a una, en orden de tipo, con un registro por runa. No es un trilero: Koschei no aparta nada.
		for (int32 RuneType = 0; RuneType < State.Seats[Loser].Runes.Num(); ++RuneType)
		{
			while (TransferRune(State, Loser, Winner, RuneType))
			{
				LogEvent(State, EMatchEventType::RunesSeized, NowSeconds, Winner, RuneType, Loser);
				++Seized;
			}
		}
		return Seized;
	}
}
