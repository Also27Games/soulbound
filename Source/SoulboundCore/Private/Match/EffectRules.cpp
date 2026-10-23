#include "Match/EffectRules.h"

#include "Board/MovementRules.h"
#include "Board/SealRules.h"
#include "Match/BondRaceRules.h"
#include "Match/MatchEvents.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Match/VoidRules.h"
#include "Math/RandomStream.h"

using Soulbound::Rules::LogEvent;

namespace Soulbound::Rules
{
	bool IsSeatInPlay(const FMatchState& State, int32 Seat)
	{
		// Decisión del autor del 08/10/2026: el activo con el turno empezado y los participantes de un duelo o un trilero en
		// curso. Entre turnos (donde se detiene el paso a paso) nadie lo está. S-015: un duelo o un trilero de un evento se
		// juega entre turnos con la máquina en Duel o ShellGame; el asiento activo (al que le toca el siguiente turno) no
		// está en juego por eso.
		// TODO(R-TUR-11): con las interrupciones del Warden (S-016), un efecto sobre quien está en juego cortará su turno en
		// vez de rechazarse.
		const bool bEventAction = State.CurrentEvent.IsActive() && State.CurrentEvent.bWaitsForAction;
		const bool bActiveInTurn = !bEventAction && Seat == State.ActiveSeat && State.TurnPhase != ETurnPhase::BetweenTurns;
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
		// El Vacío de la Furia (FSeatState::bFuryVoid) lo marca quien destierra con la Furia (UnleashFury, Match/InsolenceRules.h).
		FSeatState& SeatState = State.Seats[Seat];
		SeatState.TileId = INDEX_NONE;
		SeatState.VoidTurns = 0;
		LogEvent(State, EMatchEventType::Banished, NowSeconds, Seat, bFury ? 1 : 0);
		// R-EFE-10 (S-015): la Prueba del Warden pendiente se anula al entrar en el Vacío; salir de él no la devuelve.
		if (SeatState.bWardenTrialPending)
		{
			SeatState.bWardenTrialPending = false;
			LogEvent(State, EMatchEventType::WardenTrialCancelled, NowSeconds, Seat);
		}
		return EEffectResult::Applied;
	}

	EEffectResult CanReleaseVoid(const FMatchState& State)
	{
		// R-EFE-17: sin nadie dentro, no es válida. R-INS-3 (S-016): los del Vacío de la Furia no cuentan, porque no saldrían
		// (decisión del autor del 19/10/2026: con solo ellos dentro, burla).
		const bool bSomeoneInside = State.Seats.ContainsByPredicate([](const FSeatState& Seat) { return Seat.IsInVoid() && !Seat.bFuryVoid; });
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
			// R-INS-3 (S-016): saca a todos menos al del Vacío de la Furia, y el Warden se le niega.
			if (State.Seats[Seat].IsInVoid() && State.Seats[Seat].bFuryVoid)
			{
				LogEvent(State, EMatchEventType::FuryRefusal, NowSeconds, Seat, 1);
				continue;
			}
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


// S-014: el resto del catálogo.
namespace
{
	/** Si el asiento es un Eco Perdido fuera del Vacío: a quien llegan la Ley de Sombras y el tope de la Noche. */
	bool IsEchoOnBoard(const FSeatState& Seat)
	{
		return Seat.Role == ESeatRole::LostEcho && !Seat.IsInVoid();
	}

	/** Runas del Brazal del asiento, todos los tipos. */
	int32 TotalRunes(const FSeatState& Seat)
	{
		int32 Total = 0;
		for (const int32 Count : Seat.Runes)
		{
			Total += Count;
		}
		return Total;
	}

	/** Comprobación común de los efectos sobre un asiento (R-DIR-2): que exista y que no esté en el Vacío. */
	EEffectResult CheckSeatOnBoard(const FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EEffectResult::InvalidSeat;
		}
		return State.Seats[Seat].IsInVoid() ? EEffectResult::InVoid : EEffectResult::Applied;
	}

	/**
	 * Todos los Ecos fuera del Vacío y fuera de su hogar vuelven a él (SentHome), en orden de asiento: la Ley de Sombras y el
	 * tope de la Noche (R-EFE-9, R-EFE-18). Termina la Noche si estaba activa, con el motivo Reason.
	 */
	void SendEchoesHomeAndEndNight(FMatchState& State, EEchoNightEnd Reason, double NowSeconds)
	{
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			if (IsEchoOnBoard(State.Seats[Seat]) && Soulbound::Rules::CanSendHome(State, Seat) == EEffectResult::Applied)
			{
				Soulbound::Rules::SendHome(State, Seat, NowSeconds);
			}
		}
		if (State.EchoNightUntilRound != 0)
		{
			State.EchoNightUntilRound = 0;
			LogEvent(State, EMatchEventType::EchoNightEnded, NowSeconds, INDEX_NONE, INDEX_NONE, INDEX_NONE, static_cast<int32>(Reason));
		}
	}

	/**
	 * Pasos desde From a cada casilla por casillas transitables para Mover (R-TAB-21), y la casilla anterior de cada una en un
	 * camino más corto; las vecinas se visitan en orden de identificador, así el camino no depende del orden de los datos.
	 * El Altar se atraviesa: es un camino de varios turnos, no un movimiento. MAX_int32 donde no se llega.
	 */
	void ShortestPaths(const FBoardState& Board, int32 From, const FMoverTraits& Mover, TArray<int32>& OutSteps, TArray<int32>& OutPrevious)
	{
		OutSteps.Init(MAX_int32, Board.Tiles.Num());
		OutPrevious.Init(INDEX_NONE, Board.Tiles.Num());
		if (!Board.IsValidTile(From))
		{
			return;
		}
		TArray<int32> Queue = {From};
		OutSteps[From] = 0;
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			TArray<int32> Neighbours = Board.Tiles[Queue[Index]].Neighbours;
			Neighbours.Sort();
			for (const int32 Next : Neighbours)
			{
				if (Board.IsValidTile(Next) && OutSteps[Next] == MAX_int32 && Soulbound::Rules::IsPassable(Board, Next, Mover))
				{
					OutSteps[Next] = OutSteps[Queue[Index]] + 1;
					OutPrevious[Next] = Queue[Index];
					Queue.Add(Next);
				}
			}
		}
	}

	/**
	 * Objetivo más probable del asiento Seat (R-EFE-7): el Altar si tiene los seis tipos; si no, la Fuente con reserva de un
	 * tipo que le falta, y en la que puede obtener (un Eco, solo en Dominios Olvidados; R-ECO-3), más cercana según Steps;
	 * empates, la de identificador menor. INDEX_NONE si no hay ninguna a su alcance.
	 */
	int32 LikelyGoal(const FMatchState& State, int32 Seat, const TArray<int32>& Steps)
	{
		const FSeatState& SeatState = State.Seats[Seat];
		bool bAllTypes = true;
		for (int32 RuneType = 0; RuneType < Soulbound::Board::DomainCount; ++RuneType)
		{
			bAllTypes &= SeatState.RuneCountOf(RuneType) > 0;
		}
		if (bAllTypes)
		{
			return State.Board.FindAltar();
		}
		const bool bElect = SeatState.Role == ESeatRole::Elect;
		int32 Goal = INDEX_NONE;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			const bool bSource = Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0 && State.Board.Domains.IsValidIndex(Tile.DomainId);
			if (!bSource || SeatState.RuneCountOf(Tile.DomainId) > 0 || Steps[Tile.TileId] == MAX_int32)
			{
				continue;
			}
			const bool bForgotten = State.Board.Domains[Tile.DomainId].State == EDomainState::Forgotten;
			if ((bElect || bForgotten) && (Goal == INDEX_NONE || Steps[Tile.TileId] < Steps[Goal]))
			{
				Goal = Tile.TileId;
			}
		}
		return Goal;
	}
}

namespace Soulbound::Rules
{
	bool IsMarked(const FMatchState& State, int32 Seat)
	{
		return State.Seats.IsValidIndex(Seat) && State.Seats[Seat].MarkedUntilRound > State.Round;
	}

	bool IsBracerRevealed(const FMatchState& State, int32 Seat)
	{
		return State.Seats.IsValidIndex(Seat) && State.Seats[Seat].RevealedUntilRound > State.Round;
	}

	EEffectResult CanRevealBracer(const FMatchState& State, int32 Seat)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Seat);
		// R-DIR-3: sin runas no hay Brazal que revelar.
		return Check == EEffectResult::Applied && TotalRunes(State.Seats[Seat]) == 0 ? EEffectResult::NoRunes : Check;
	}

	EEffectResult RevealBracer(FMatchState& State, int32 Seat, double NowSeconds)
	{
		const EEffectResult Result = CanRevealBracer(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-2: si ya estaba revelado, se prolonga desde donde caducaba.
		int32& Until = State.Seats[Seat].RevealedUntilRound;
		Until = (IsBracerRevealed(State, Seat) ? Until : State.Round) + State.Params.RevealBracerRounds;
		LogEvent(State, EMatchEventType::BracerRevealed, NowSeconds, Seat, Until);
		return EEffectResult::Applied;
	}

	EEffectResult CanChangeTurns(const FMatchState& State, int32 Seat)
	{
		// Decisión del autor del 08/10/2026 (R-DIR-2): a quien está en el Vacío no se le dan ni se le quitan turnos.
		return CheckSeatOnBoard(State, Seat);
	}

	EEffectResult LoseTurn(FMatchState& State, int32 Seat, int32 Count, double NowSeconds)
	{
		const EEffectResult Result = CanChangeTurns(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-5: hasta el tope de los datos; lo que excede es una Vuelta al hogar, una sola por aplicación.
		const int32 Room = FMath::Max(State.Params.MaxLostTurns - State.Seats[Seat].LostTurns, 0);
		const int32 Granted = FMath::Min(Count, Room);
		GrantLostTurns(State, Seat, Granted);
		if (Count > Granted)
		{
			// Decisión del autor del 12/10/2026: si la Vuelta al hogar no vale (ya en casa), queda la burla de la Vuelta al hogar.
			if (CanSendHome(State, Seat) == EEffectResult::Applied)
			{
				SendHome(State, Seat, NowSeconds);
			}
			else
			{
				LogEvent(State, EMatchEventType::WardenTaunt, NowSeconds, Seat, static_cast<int32>(EEffect::SendHome));
			}
		}
		return EEffectResult::Applied;
	}

	EEffectResult ExtraTurn(FMatchState& State, int32 Seat, int32 Count, double NowSeconds)
	{
		const EEffectResult Result = CanChangeTurns(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-15: con el tope que ya existe; se cancelan con los perdidos uno a uno en la máquina de turnos (R-TUR-10).
		GrantExtraTurns(State, Seat, Count);
		return EEffectResult::Applied;
	}

	TArray<int32> SealableTiles(const FMatchState& State)
	{
		TArray<int32> Tiles;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			if (CanSealTile(State, Tile.TileId) == ESealResult::Accepted)
			{
				Tiles.Add(Tile.TileId);
			}
		}
		return Tiles;
	}

	EEffectResult PlaceSeal(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		// R-EFE-6: una válida al azar con el sorteo del Warden.
		// TODO(R-EFE-6): el cerebro del Warden (S-016) elegirá la casilla con criterio; hasta entonces, al azar.
		const TArray<int32> Tiles = SealableTiles(State);
		if (Tiles.IsEmpty())
		{
			return EEffectResult::NoValidTile;
		}
		SealTile(State, Tiles[Random.RandRange(0, Tiles.Num() - 1)], NowSeconds);
		return EEffectResult::Applied;
	}

	int32 SealAgainstRivalTile(const FMatchState& State, int32 Rival)
	{
		if (!State.Seats.IsValidIndex(Rival) || !State.Board.IsValidTile(State.Seats[Rival].TileId))
		{
			return INDEX_NONE;
		}
		// R-TAB-21: el camino del rival, con su transitabilidad (Sellos, Paso Libre, confinamiento y Noche).
		TArray<int32> Steps;
		TArray<int32> Previous;
		ShortestPaths(State.Board, State.Seats[Rival].TileId, MakeMoverTraits(State, Rival), Steps, Previous);

		// R-EFE-7: la primera casilla sellable del camino más corto hasta su objetivo más probable.
		const int32 Goal = LikelyGoal(State, Rival, Steps);
		TArray<int32> Path;
		for (int32 Tile = Goal; Tile != INDEX_NONE; Tile = Previous[Tile])
		{
			Path.Insert(Tile, 0);
		}
		for (const int32 Tile : Path)
		{
			if (CanSealTile(State, Tile) == ESealResult::Accepted)
			{
				return Tile;
			}
		}
		// Sin casilla válida en el camino (o sin objetivo), la sellable más cercana a él; empates, la de identificador menor.
		int32 Closest = INDEX_NONE;
		for (const int32 Tile : SealableTiles(State))
		{
			if (Steps[Tile] != MAX_int32 && (Closest == INDEX_NONE || Steps[Tile] < Steps[Closest]))
			{
				Closest = Tile;
			}
		}
		return Closest;
	}

	int32 SealTowardsAltarTile(const FMatchState& State, int32 Seat)
	{
		return INDEX_NONE;
	}

	EEffectResult CanForcedOracle(const FMatchState& State, int32 Seat)
	{
		return EEffectResult::NotImplemented;
	}

	EEffectResult ForcedOracle(FMatchState& State, int32 Seat, double NowSeconds)
	{
		return EEffectResult::NotImplemented;
	}

	EEffectResult CanSealAgainstRival(const FMatchState& State, int32 Rival)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Rival);
		return Check == EEffectResult::Applied && SealAgainstRivalTile(State, Rival) == INDEX_NONE ? EEffectResult::NoValidTile : Check;
	}

	EEffectResult SealAgainstRival(FMatchState& State, int32 Rival, double NowSeconds)
	{
		const EEffectResult Result = CanSealAgainstRival(State, Rival);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		SealTile(State, SealAgainstRivalTile(State, Rival), NowSeconds);
		return EEffectResult::Applied;
	}

	EEffectResult CanMark(const FMatchState& State, int32 Seat, int32 Director)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Seat);
		// R-EFE-8: nadie se marca a sí mismo.
		return Check == EEffectResult::Applied && Seat == Director ? EEffectResult::SelfTarget : Check;
	}

	EEffectResult Mark(FMatchState& State, int32 Seat, int32 Director, int32 Pointer, double NowSeconds)
	{
		const EEffectResult Result = CanMark(State, Seat, Director);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-8: marcar a un Marcado reinicia la duración; cada marca lleva la suya.
		State.Seats[Seat].MarkedUntilRound = State.Round + State.Params.MarkRounds;
		LogEvent(State, EMatchEventType::Marked, NowSeconds, Seat, State.Seats[Seat].MarkedUntilRound, Pointer);
		return EEffectResult::Applied;
	}

	bool CollectMark(FMatchState& State, int32 Collector, int32 Marked, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Collector) || Collector == Marked || !IsMarked(State, Marked))
		{
			return false;
		}
		// R-EFE-8: la marca termina y quien la cobra gana un turno extra (con el tope de siempre). Concedido entre turnos, lo
		// juega tras su próximo turno (decisión del autor del 12/10/2026).
		State.Seats[Marked].MarkedUntilRound = 0;
		GrantExtraTurns(State, Collector, 1);
		LogEvent(State, EMatchEventType::MarkCollected, NowSeconds, Collector, INDEX_NONE, Marked);
		return true;
	}

	EEffectResult CanShadowLaw(const FMatchState& State)
	{
		if (State.IsEchoNight())
		{
			return EEffectResult::Applied;
		}
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			if (IsEchoOnBoard(State.Seats[Seat]) && State.Seats[Seat].TileId != HomeTile(State, Seat))
			{
				return EEffectResult::Applied;
			}
		}
		return EEffectResult::NoEchoes;
	}

	EEffectResult ShadowLaw(FMatchState& State, double NowSeconds)
	{
		const EEffectResult Result = CanShadowLaw(State);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-9: todos los Ecos a su hogar; termina la Noche. Al acabar la Noche, MakeMoverTraits los confina otra vez.
		SendEchoesHomeAndEndNight(State, EEchoNightEnd::ShadowLaw, NowSeconds);
		return EEffectResult::Applied;
	}

	EEffectResult CanEchoNight(const FMatchState& State)
	{
		// R-EFE-18: el permiso es sobre el confinamiento de R-TAB-20, que solo existe acabada la carrera.
		if (State.MatchPhase == EMatchPhase::BondRace)
		{
			return EEffectResult::BondRace;
		}
		// Decisión del autor del 14/10/2026: una Noche sobre otra no tiene sentido; el motor la rechaza y el cerebro del Warden
		// (S-016) no la decreta. Principio del autor: el Warden nunca parece equivocarse; la burla es la red de seguridad del motor.
		if (State.IsEchoNight())
		{
			return EEffectResult::AlreadyActive;
		}
		const bool bEchoes = State.Seats.ContainsByPredicate([](const FSeatState& Seat) { return Seat.Role == ESeatRole::LostEcho; });
		return bEchoes ? EEffectResult::Applied : EEffectResult::NoEchoes;
	}

	EEffectResult EchoNight(FMatchState& State, double NowSeconds)
	{
		const EEffectResult Result = CanEchoNight(State);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-18: hasta la Ley de Sombras o el tope de rondas.
		State.EchoNightUntilRound = State.Round + State.Params.EchoNightMaxRounds;
		LogEvent(State, EMatchEventType::EchoNightBegan, NowSeconds, INDEX_NONE, State.EchoNightUntilRound);
		return EEffectResult::Applied;
	}

	int32 RandomRuneType(const FMatchState& State, int32 Seat, FRandomStream& Random)
	{
		if (!State.Seats.IsValidIndex(Seat) || TotalRunes(State.Seats[Seat]) == 0)
		{
			return INDEX_NONE;
		}
		int32 Pick = Random.RandRange(0, TotalRunes(State.Seats[Seat]) - 1);
		for (int32 Type = 0; Type < State.Seats[Seat].Runes.Num(); ++Type)
		{
			if (Pick < State.Seats[Seat].Runes[Type])
			{
				return Type;
			}
			Pick -= State.Seats[Seat].Runes[Type];
		}
		return INDEX_NONE;
	}

	TArray<int32> LoseRuneOptions(const FMatchState& State, int32 Seat)
	{
		TArray<int32> Types;
		for (int32 RuneType = 0; State.Seats.IsValidIndex(Seat) && RuneType < State.Seats[Seat].Runes.Num(); ++RuneType)
		{
			if (State.Seats[Seat].Runes[RuneType] > 0)
			{
				Types.Add(RuneType);
			}
		}
		return Types;
	}

	EEffectResult CanLoseRune(const FMatchState& State, int32 Seat)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Seat);
		// R-DIR-3: sin runas, no hay efecto.
		return Check == EEffectResult::Applied && TotalRunes(State.Seats[Seat]) == 0 ? EEffectResult::NoRunes : Check;
	}

	EEffectResult LoseRune(FMatchState& State, int32 Seat, int32 RuneType, double NowSeconds, FRandomStream& Random)
	{
		const EEffectResult Result = CanLoseRune(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		if (RuneType == INDEX_NONE)
		{
			// R-EFE-11, al azar: cada runa del Brazal con la misma probabilidad, así los duplicados cuentan.
			RuneType = RandomRuneType(State, Seat, Random);
		}
		if (!LoseRuneOptions(State, Seat).Contains(RuneType))
		{
			return EEffectResult::InvalidRune;
		}
		// R-RUN-6: vuelve a su Fuente. No es un robo: la afinidad de Koschei no aparta nada.
		ReturnRuneToSource(State, Seat, RuneType, State.Board.FindPowerSource(RuneType), NowSeconds);
		return EEffectResult::Applied;
	}

	TArray<int32> GiftRuneOptions(const FMatchState& State)
	{
		TArray<int32> Types;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			if (Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0 && State.Board.Domains.IsValidIndex(Tile.DomainId))
			{
				Types.AddUnique(Tile.DomainId);
			}
		}
		Types.Sort();
		return Types;
	}

	EEffectResult CanGiftRune(const FMatchState& State, int32 Seat)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Seat);
		// R-EFE-16: sin Fuentes con reserva, el efecto no es válido.
		return Check == EEffectResult::Applied && GiftRuneOptions(State).IsEmpty() ? EEffectResult::NoReserve : Check;
	}

	EEffectResult GiftRune(FMatchState& State, int32 Seat, int32 RuneType, double NowSeconds)
	{
		const EEffectResult Result = CanGiftRune(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		if (!GiftRuneOptions(State).Contains(RuneType))
		{
			return EEffectResult::InvalidRune;
		}
		// R-EFE-16: de la primera Fuente de ese Dominio con reserva; un Eco la recibe de cualquier Dominio.
		const int32 Source = State.Board.Tiles.IndexOfByPredicate([RuneType](const FTileState& Tile)
		{
			return Tile.Type == ETileType::PowerSource && Tile.DomainId == RuneType && Tile.RuneReserve > 0;
		});
		GrantRuneFromSource(State, Seat, Source, NowSeconds);
		return EEffectResult::Applied;
	}

	EEffectResult CanWardenTrial(const FMatchState& State, int32 Seat)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Seat);
		// R-EFE-10: como mucho una; sobre quien ya la tiene no es válida (el Warden nunca parece equivocarse, 14/10/2026).
		return Check == EEffectResult::Applied && State.Seats[Seat].bWardenTrialPending ? EEffectResult::AlreadyPending : Check;
	}

	EEffectResult WardenTrial(FMatchState& State, int32 Seat, double NowSeconds)
	{
		const EEffectResult Result = CanWardenTrial(State, Seat);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		State.Seats[Seat].bWardenTrialPending = true;
		LogEvent(State, EMatchEventType::WardenTrialPending, NowSeconds, Seat);
		return EEffectResult::Applied;
	}

	bool ApplyWardenTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		const ESkillDifficulty Difficulty = State.PendingDifficulty;
		State.PendingTrial = ESkillTrial::None;
		if (State.Seats.IsValidIndex(Seat))
		{
			State.Seats[Seat].bWardenTrialPending = false;
		}
		LogEvent(State, bPassed ? EMatchEventType::WardenTrialPassed : EMatchEventType::WardenTrialFailed, NowSeconds, Seat, static_cast<int32>(Difficulty));
		return bPassed;
	}

	EEffectResult CanDecreeTheft(const FMatchState& State, int32 Victim, int32 Thief)
	{
		// R-INT-3: nadie en el Vacío es ladrón ni víctima; nadie se roba a sí mismo.
		const EEffectResult ThiefCheck = CheckSeatOnBoard(State, Thief);
		if (ThiefCheck != EEffectResult::Applied)
		{
			return ThiefCheck;
		}
		const EEffectResult VictimCheck = CheckSeatOnBoard(State, Victim);
		if (VictimCheck != EEffectResult::Applied)
		{
			return VictimCheck;
		}
		return Victim == Thief ? EEffectResult::SelfTarget : EEffectResult::Applied;
	}

	EEffectResult DecreeTheft(FMatchState& State, int32 Victim, int32 Thief, double NowSeconds)
	{
		const EEffectResult Result = CanDecreeTheft(State, Victim, Thief);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		// R-EFE-12: el trilero es el de siempre (R-ROB-1 a R-ROB-6), jugado entre turnos por la máquina.
		BeginEventSteal(State, Thief, Victim, NowSeconds);
		return EEffectResult::Applied;
	}

	EEffectResult CanForceDuel(const FMatchState& State, int32 Challenged, int32 Challenger)
	{
		const EEffectResult ChallengerCheck = CheckSeatOnBoard(State, Challenger);
		if (ChallengerCheck != EEffectResult::Applied)
		{
			return ChallengerCheck;
		}
		if (Challenged == Challenger)
		{
			return EEffectResult::SelfTarget;
		}
		return CanForceDuelTarget(State, Challenged);
	}

	EEffectResult CanForceDuelTarget(const FMatchState& State, int32 Seat)
	{
		const EEffectResult Check = CheckSeatOnBoard(State, Seat);
		// R-EFE-13 (decisión del autor del 15/10/2026): el retado tiene al menos una runa; si no, el duelo se anularía.
		return Check == EEffectResult::Applied && TotalRunes(State.Seats[Seat]) == 0 ? EEffectResult::NoRunes : Check;
	}

	EEffectResult ForceDuel(FMatchState& State, int32 Challenged, int32 Challenger, bool bAllRunes, double NowSeconds)
	{
		const EEffectResult Result = CanForceDuel(State, Challenged, Challenger);
		if (Result != EEffectResult::Applied)
		{
			return Result;
		}
		BeginEventDuel(State, Challenger, Challenged, bAllRunes, NowSeconds);
		return EEffectResult::Applied;
	}

	void StartRound(FMatchState& State, double NowSeconds)
	{
		// Decisión del autor del 10/10/2026: una ronda es cada vuelta completa del orden de turno.
		++State.Round;
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			FSeatState& SeatState = State.Seats[Seat];
			if (SeatState.MarkedUntilRound != 0 && SeatState.MarkedUntilRound <= State.Round)
			{
				// R-EFE-8: caduca sin cobrarse.
				SeatState.MarkedUntilRound = 0;
				LogEvent(State, EMatchEventType::MarkExpired, NowSeconds, Seat);
			}
		}
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			FSeatState& SeatState = State.Seats[Seat];
			if (SeatState.RevealedUntilRound != 0 && SeatState.RevealedUntilRound <= State.Round)
			{
				// R-EFE-2: el Brazal vuelve a ocultarse.
				SeatState.RevealedUntilRound = 0;
				LogEvent(State, EMatchEventType::BracerHidden, NowSeconds, Seat);
			}
		}
		if (State.EchoNightUntilRound != 0 && State.EchoNightUntilRound <= State.Round)
		{
			// R-EFE-18: al tope, los Ecos a casa y confinados, como con la Ley, sin su evento (decisión del autor del 12/10/2026).
			SendEchoesHomeAndEndNight(State, EEchoNightEnd::RoundLimit, NowSeconds);
		}
	}
}
