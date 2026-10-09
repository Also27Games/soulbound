#include "Match/RuneRules.h"

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
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
		// Glosario "Afinidad": la del Elegido, siempre activa; un Eco no tiene.
		if (!State.Seats.IsValidIndex(Seat) || State.Seats[Seat].Role != ESeatRole::Elect)
		{
			return false;
		}
		const int32 DomainId = State.Seats[Seat].DomainId;
		return State.Elects.IsValidDomain(DomainId) && State.Elects.Elects[DomainId].Affinity == Affinity;
	}

	TArray<int32> PlanarJumpOptions(const FMatchState& State, int32 Seat)
	{
		TArray<int32> Options;
		// R-POD-5, R-TAB-15: termina su movimiento (R-RUN-3: quedarse sin moverse no cuenta) en un Nodo Planar del tablero
		// Despierto y tiene Vuelo Planar. Pasar por el Nodo no cuenta: solo se mira la casilla final.
		if (!State.Seats.IsValidIndex(Seat) || !State.bActiveSeatMoved || State.Board.Phase != EBoardPhase::Awakened
			|| !IsPowerActive(State, Seat, EPower::PlanarFlight))
		{
			return Options;
		}
		const int32 From = State.Seats[Seat].TileId;
		if (!State.Board.IsValidTile(From) || State.Board.Tiles[From].PowerNodeEffect != EPowerNodeEffect::Planar)
		{
			return Options;
		}
		// R-TAB-21: el mismo criterio de transitabilidad que el movimiento; un Nodo sellado solo con Paso Libre (decisión
		// del autor del 04/10/2026). Las fichas no bloquean (R-TAB-19): un Nodo ocupado es una opción.
		const FMoverTraits Mover = MakeMoverTraits(State, Seat);
		for (const FTileState& Tile : State.Board.Tiles)
		{
			if (Tile.TileId != From && Tile.PowerNodeEffect == EPowerNodeEffect::Planar && IsPassable(State.Board, Tile.TileId, Mover))
			{
				Options.Add(Tile.TileId);
			}
		}
		return Options;
	}

	TArray<int32> ExpelTargets(const FMatchState& State, int32 Seat)
	{
		TArray<int32> Targets;
		// R-POD-4: termina su movimiento (o su salto) en una casilla ocupada y tiene Expulsión.
		if (!State.Seats.IsValidIndex(Seat) || !State.bActiveSeatMoved || !IsPowerActive(State, Seat, EPower::Expel))
		{
			return Targets;
		}
		const int32 TileId = State.Seats[Seat].TileId;
		for (int32 Other = 0; Other < State.Seats.Num(); ++Other)
		{
			// Quien ya está en su hogar no es objetivo válido; quien está en el Vacío tampoco (R-INT-3, S-013).
			if (Other != Seat && !State.Seats[Other].IsInVoid() && State.Seats[Other].TileId == TileId && TileId != HomeTile(State, Other))
			{
				Targets.Add(Other);
			}
		}
		return Targets;
	}

	TArray<int32> StealTargets(const FMatchState& State, int32 Seat)
	{
		TArray<int32> Targets;
		// R-ECO-4: un Eco tras la carrera (con hogar asignado) que termina su movimiento; durante la carrera no tiene Dominio.
		// Sin moverse no hay robo (decisión del autor del 05/10/2026, el mismo criterio que Expulsión, salto y Fuente).
		if (!State.Seats.IsValidIndex(Seat) || !State.bActiveSeatMoved || State.MatchPhase == EMatchPhase::BondRace
			|| State.Seats[Seat].Role != ESeatRole::LostEcho || State.Seats[Seat].DomainId == INDEX_NONE)
		{
			return Targets;
		}
		const int32 TileId = State.Seats[Seat].TileId;
		for (int32 Other = 0; Other < State.Seats.Num(); ++Other)
		{
			// Elegido o Eco, tenga runas o no: el ladrón no ve el Brazal ajeno, y robar a quien no tiene es R-ROB-4. R-INT-3
			// (S-013): quien está en el Vacío no comparte casilla con nadie y no puede ser robado.
			if (Other != Seat && !State.Seats[Other].IsInVoid() && State.Seats[Other].TileId == TileId)
			{
				Targets.Add(Other);
			}
		}
		return Targets;
	}

	TArray<FPlayerActionOption> PlayerActionOptions(const FMatchState& State, int32 Seat)
	{
		// R-INT-2: una sola decisión, qué y a quién. Un Elegido puede tener Duelo y Expulsión a la vez (S-012), y un Eco en un
		// Nodo de Duelo, reto y robo. Primero los duelos, como en la vista de la story (decisión del autor del 06/10/2026).
		TArray<FPlayerActionOption> Options;
		for (const int32 Target : DuelTargets(State, Seat))
		{
			Options.Add({EPlayerAction::Duel, Target});
		}
		for (const int32 Target : ExpelTargets(State, Seat))
		{
			Options.Add({EPlayerAction::Expel, Target});
		}
		for (const int32 Target : StealTargets(State, Seat))
		{
			Options.Add({EPlayerAction::Steal, Target});
		}
		return Options;
	}

	bool TransferRune(FMatchState& State, int32 FromSeat, int32 ToSeat, int32 RuneType)
	{
		if (!State.Seats.IsValidIndex(FromSeat) || !State.Seats.IsValidIndex(ToSeat) || FromSeat == ToSeat
			|| State.Seats[FromSeat].RuneCountOf(RuneType) <= 0 || !State.Seats[ToSeat].Runes.IsValidIndex(RuneType))
		{
			return false;
		}
		// R-ROB-3: pasa al Brazal del ladrón aunque ya tenga de ese tipo. Si era la última del tipo, la víctima pierde el
		// poder en el acto (R-ELE-5): IsPowerActive lo calcula del Brazal, sin nada más que actualizar.
		--State.Seats[FromSeat].Runes[RuneType];
		++State.Seats[ToSeat].Runes[RuneType];
		return true;
	}

	void ApplyPlanarJump(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds)
	{
		// La llegada es un final de movimiento (glosario "Nodo Planar"): el destino es otro Nodo Planar, sin Nicho ni
		// Fuente que resolver, y el salto ya se ha usado (un salto por movimiento, decisión del autor del 04/10/2026).
		State.Seats[Seat].TileId = TileId;
		LogEvent(State, EMatchEventType::PlanarJumped, NowSeconds, Seat, TileId);
	}

	void ApplyExpel(FMatchState& State, int32 Seat, int32 Target, double NowSeconds)
	{
		// R-POD-4, R-TAB-13: el objetivo vuelve a su hogar. No es un movimiento suyo: no resuelve casilla, no toca
		// bActiveSeatMoved (que es del asiento activo) ni sus turnos (R-INT-4).
		State.Seats[Target].TileId = HomeTile(State, Target);
		// Seat expulsa y Target es el afectado, en OtherSeat; Value nunca lleva un asiento (decisión del autor del 05/10/2026).
		LogEvent(State, EMatchEventType::Expelled, NowSeconds, Seat, INDEX_NONE, Target);
	}
}
