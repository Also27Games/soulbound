#include "Match/RuneRules.h"
#include "Match/MatchEvents.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** Si la casilla es una Fuente de Poder de un Dominio válido. */
	bool IsPowerSource(const FBoardState& Board, int32 TileId)
	{
		return Board.IsValidTile(TileId) && Board.Tiles[TileId].Type == ETileType::PowerSource && Board.Domains.IsValidIndex(Board.Tiles[TileId].DomainId);
	}

	/**
	 * El asiento obtiene una runa de la Fuente TileId, si le queda reserva (R-RUN-2): el Brazal sube uno en el tipo de la
	 * Fuente (su Dominio) y la reserva baja uno. Sin reserva, registra SourceDepleted (R-RUN-4). Devuelve true si obtuvo.
	 */
	bool ObtainRune(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds)
	{
		FTileState& Source = State.Board.Tiles[TileId];
		const int32 RuneType = Source.DomainId;
		if (Source.RuneReserve <= 0)
		{
			LogEvent(State, EMatchEventType::SourceDepleted, NowSeconds, Seat, RuneType);
			return false;
		}
		--Source.RuneReserve;
		++State.Seats[Seat].Runes[RuneType];
		LogEvent(State, EMatchEventType::RuneObtained, NowSeconds, Seat, RuneType);
		return true;
	}
}

namespace Soulbound::Rules
{
	ERuneLanding ResolvePowerSourceLanding(FMatchState& State, int32 Seat, double NowSeconds)
	{
		// R-RUN-3: una runa por llegada; quedarse sin casillas alcanzables no es llegar. R-TAB-8: en Primordial, nada.
		if (!State.Seats.IsValidIndex(Seat) || !State.bActiveSeatMoved || State.Board.Phase != EBoardPhase::Awakened)
		{
			return ERuneLanding::None;
		}
		const int32 TileId = State.Seats[Seat].TileId;
		if (!IsPowerSource(State.Board, TileId))
		{
			return ERuneLanding::None;
		}
		const bool bForgotten = State.Board.Domains[State.Board.Tiles[TileId].DomainId].State == EDomainState::Forgotten;
		const bool bElect = State.Seats[Seat].Role == ESeatRole::Elect;
		// R-ECO-3, R-CAR-7b: un Eco solo obtiene en las Fuentes de los Olvidados.
		if (!bElect && !bForgotten)
		{
			return ERuneLanding::None;
		}
		// R-RUN-4: sin reserva no hay runa ni minijuego que jugar.
		if (State.Board.Tiles[TileId].RuneReserve <= 0)
		{
			ObtainRune(State, Seat, TileId, NowSeconds);
			return ERuneLanding::Depleted;
		}
		// R-TAB-7: un Elegido en la Fuente de un Olvidado juega el minijuego; el Eco, no.
		if (bElect && bForgotten)
		{
			State.PendingTrial = ESkillTrial::ForgottenSource;
			State.PendingDifficulty = ESkillDifficulty::Easy;
			return ERuneLanding::TrialPending;
		}
		ObtainRune(State, Seat, TileId, NowSeconds);
		return ERuneLanding::Obtained;
	}

	bool ApplySkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		const int32 TileId = State.Seats.IsValidIndex(Seat) ? State.Seats[Seat].TileId : INDEX_NONE;
		State.PendingTrial = ESkillTrial::None;
		if (!IsPowerSource(State.Board, TileId))
		{
			return false;
		}
		const int32 RuneType = State.Board.Tiles[TileId].DomainId;
		LogEvent(State, bPassed ? EMatchEventType::SkillMinigamePassed : EMatchEventType::SkillMinigameFailed, NowSeconds, Seat, RuneType);
		return bPassed && ObtainRune(State, Seat, TileId, NowSeconds);
	}

	bool ReturnRuneToSource(FMatchState& State, int32 Seat, int32 RuneType, int32 SourceTile, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat) || State.Seats[Seat].RuneCountOf(RuneType) <= 0 || !IsPowerSource(State.Board, SourceTile)
			|| State.Board.Tiles[SourceTile].DomainId != RuneType)
		{
			return false;
		}
		--State.Seats[Seat].Runes[RuneType];
		++State.Board.Tiles[SourceTile].RuneReserve;
		LogEvent(State, EMatchEventType::RuneReturned, NowSeconds, Seat, RuneType);
		return true;
	}

	bool IsPowerActive(const FMatchState& State, int32 Seat, EPower Power)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return false;
		}
		const FSeatState& SeatState = State.Seats[Seat];
		if (SeatState.Role == ESeatRole::Elect)
		{
			// R-ELE-5: activo mientras haya al menos una runa del tipo que se lo da según su mapa.
			if (!State.Elects.IsValidDomain(SeatState.DomainId))
			{
				return false;
			}
			const TArray<EPower>& Map = State.Elects.Elects[SeatState.DomainId].RuneToPower;
			for (int32 RuneType = 0; RuneType < Map.Num(); ++RuneType)
			{
				if (Map[RuneType] == Power && SeatState.RuneCountOf(RuneType) > 0)
				{
					return true;
				}
			}
			return false;
		}
		// R-ECO-1, R-ECO-7: un Eco tras la carrera solo tiene Celeridad, con la Runa de Liberación (la de su Dominio
		// asignado). Durante la carrera no tiene Dominio ni poderes.
		return Power == EPower::Haste && State.MatchPhase != EMatchPhase::BondRace && SeatState.DomainId != INDEX_NONE
			&& SeatState.RuneCountOf(SeatState.DomainId) > 0;
	}

	TArray<EPower> GetActivePowers(const FMatchState& State, int32 Seat)
	{
		TArray<EPower> Active;
		for (int32 Power = static_cast<int32>(EPower::Duel); Power <= static_cast<int32>(EPower::Deflection); ++Power)
		{
			if (IsPowerActive(State, Seat, static_cast<EPower>(Power)))
			{
				Active.Add(static_cast<EPower>(Power));
			}
		}
		return Active;
	}

	bool HasAffinity(const FMatchState& State, int32 Seat, EAffinity Affinity)
	{
		return false;
	}

	TArray<int32> PlanarJumpOptions(const FMatchState& State, int32 Seat)
	{
		return {};
	}

	TArray<int32> ExpelTargets(const FMatchState& State, int32 Seat)
	{
		return {};
	}

	void ApplyPlanarJump(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds)
	{
	}

	void ApplyExpel(FMatchState& State, int32 Seat, int32 Target, double NowSeconds)
	{
	}
}
