#include "Match/BondRaceRules.h"

#include "Match/RuneRules.h"
#include "Match/MatchEvents.h"
#include "Math/RandomStream.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** Baraja Items con el sorteo de la partida (Fisher-Yates): misma semilla, mismo orden. Como en StartMatch. */
	void Shuffle(TArray<int32>& Items, FRandomStream& Random)
	{
		for (int32 Last = Items.Num() - 1; Last > 0; --Last)
		{
			Items.Swap(Last, Random.RandRange(0, Last));
		}
	}

	bool HasFreeNiche(const FBoardState& Board)
	{
		return Board.Domains.ContainsByPredicate([](const FDomainState& Domain) { return Domain.NicheState == ENicheState::Free; });
	}

	/** Si la casilla pertenece a un Dominio con dueño (el Altar no pertenece a ninguno). */
	bool IsInOwnedDomain(const FBoardState& Board, int32 TileId)
	{
		if (!Board.IsValidTile(TileId))
		{
			return false;
		}
		const int32 DomainId = Board.Tiles[TileId].DomainId;
		return Board.Domains.IsValidIndex(DomainId) && Board.Domains[DomainId].State == EDomainState::Owned;
	}
}

namespace Soulbound::Rules
{
	void ClaimNiche(FMatchState& State, int32 Seat, int32 DomainId, double NowSeconds, FRandomStream& Random)
	{
		FDomainState& Domain = State.Board.Domains[DomainId];
		FSeatState& SeatState = State.Seats[Seat];
		// R-CAR-4, R-CAR-5: el Nicho pasa a Reclamado, el Dominio tiene dueño y el asiento es su Elegido.
		Domain.NicheState = ENicheState::Claimed;
		Domain.OwnerSeat = Seat;
		SeatState.Role = ESeatRole::Elect;
		SeatState.DomainId = Domain.DomainId;
		LogEvent(State, EMatchEventType::Bonded, NowSeconds, Seat, Domain.DomainId);

		// R-TAB-9: el primer vínculo despierta el tablero y revela los Nodos, sin vuelta atrás.
		if (State.Board.Phase == EBoardPhase::Primordial)
		{
			State.Board.Phase = EBoardPhase::Awakened;
			// R-RUN-4: cada Fuente arranca con la reserva de los datos al Despertar (S-008).
			for (FTileState& BoardTile : State.Board.Tiles)
			{
				BoardTile.RuneReserve = BoardTile.Type == ETileType::PowerSource ? State.Board.Params.RuneReservePerSource : 0;
			}
			LogEvent(State, EMatchEventType::BoardAwakening, NowSeconds, Seat);
		}

		// R-CAR-1: la carrera termina antes si se reclama el último Nicho Libre.
		if (!HasFreeNiche(State.Board))
		{
			EndBondRace(State, NowSeconds, Random);
		}
	}

	bool ResolveNicheLanding(FMatchState& State, int32 Seat, double NowSeconds, FRandomStream& Random)
	{
		if (State.MatchPhase != EMatchPhase::BondRace || !State.Seats.IsValidIndex(Seat))
		{
			return false;
		}
		FSeatState& SeatState = State.Seats[Seat];
		// R-CAR-6: un Elegido no cambia de rol ni de Nicho.
		if (SeatState.Role != ESeatRole::LostEcho || !State.Board.IsValidTile(SeatState.TileId))
		{
			return false;
		}
		const FTileState& Tile = State.Board.Tiles[SeatState.TileId];
		if (Tile.Type != ETileType::Niche || !State.Board.Domains.IsValidIndex(Tile.DomainId))
		{
			return false;
		}
		FDomainState& Domain = State.Board.Domains[Tile.DomainId];
		// R-CAR-4: solo un Nicho Libre se reclama; Reclamado o Cerrado no cambia nada.
		if (Domain.NicheState != ENicheState::Free)
		{
			return false;
		}

		ClaimNiche(State, Seat, Domain.DomainId, NowSeconds, Random);
		return true;
	}

	bool EndBondRace(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		if (State.MatchPhase != EMatchPhase::BondRace)
		{
			return false;
		}
		const int32 Elects = State.Seats.FilterByPredicate([](const FSeatState& Seat) { return Seat.Role == ESeatRole::Elect; }).Num();

		// R-CAR-8: sin ningún vínculo, victoria del Warden. Nada más cambia (decisión del autor del 01/10/2026): ni
		// Olvidados nuevos ni hogares, porque la partida ya no sigue.
		if (Elects == 0)
		{
			State.MatchPhase = EMatchPhase::MatchOver;
			State.Outcome = EMatchOutcome::WardenVictory;
			LogEvent(State, EMatchEventType::WardenVictory, NowSeconds, INDEX_NONE);
			LogEvent(State, EMatchEventType::BondRaceEnded, NowSeconds, INDEX_NONE, Elects);
			return true;
		}

		// R-TAB-10, R-TAB-11: todo Dominio sin dueño pasa a Olvidado con su Nicho Cerrado.
		for (FDomainState& Domain : State.Board.Domains)
		{
			if (Domain.OwnerSeat == INDEX_NONE)
			{
				Domain.State = EDomainState::Forgotten;
				Domain.NicheState = ENicheState::Closed;
			}
		}

		// R-TAB-12: cada Eco, en orden de asiento, recibe al azar un Dominio Olvidado distinto. Siempre hay bastantes:
		// quedan 4 menos E Ecos y 6 menos E Olvidados. Se barajan los seis Dominios y se toman los Olvidados en ese
		// orden: el sorteo gasta siempre lo mismo del generador de la partida, sea cual sea E, así quién se vincula no
		// cambia los dados que vienen después (grabación y reproducción, GDD 08).
		TArray<int32> Forgotten;
		for (const FDomainState& Domain : State.Board.Domains)
		{
			Forgotten.Add(Domain.DomainId);
		}
		Shuffle(Forgotten, Random);
		Forgotten.RemoveAll([&State](int32 DomainId) { return State.Board.Domains[DomainId].State != EDomainState::Forgotten; });
		int32 NextForgotten = 0;
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			FSeatState& SeatState = State.Seats[Seat];
			if (SeatState.Role == ESeatRole::LostEcho && Forgotten.IsValidIndex(NextForgotten))
			{
				SeatState.DomainId = Forgotten[NextForgotten++];
				LogEvent(State, EMatchEventType::HomeAssigned, NowSeconds, Seat, SeatState.DomainId);
			}
		}

		// Decisión del autor del 01/10/2026: el Eco que acaba la carrera en un Dominio con dueño vuelve a su hogar;
		// con el confinamiento de R-TAB-20 no podría salir.
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			FSeatState& SeatState = State.Seats[Seat];
			if (SeatState.Role == ESeatRole::LostEcho && IsInOwnedDomain(State.Board, SeatState.TileId))
			{
				SeatState.TileId = HomeTile(State, Seat);
				LogEvent(State, EMatchEventType::ReturnedHome, NowSeconds, Seat, SeatState.TileId);
			}
		}

		State.MatchPhase = EMatchPhase::RuneHunt;
		LogEvent(State, EMatchEventType::BondRaceEnded, NowSeconds, INDEX_NONE, Elects);
		return true;
	}

	int32 HomeTile(const FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return INDEX_NONE;
		}
		// R-TAB-13: el Nicho del Dominio del asiento (reclamado o asignado); sin Dominio, el Altar.
		const int32 DomainId = State.Seats[Seat].DomainId;
		return State.Board.Domains.IsValidIndex(DomainId) ? State.Board.FindNiche(DomainId) : State.Board.FindAltar();
	}

	FMoverTraits MakeMoverTraits(const FMatchState& State, int32 Seat)
	{
		FMoverTraits Mover;
		if (!State.Seats.IsValidIndex(Seat))
		{
			return Mover;
		}
		// TODO(R-ECO-2): el permiso temporal de invasión (Noche de los Ecos) lo da un decreto del Warden, en su story.
		Mover.bConfinedToForgotten = State.Seats[Seat].Role == ESeatRole::LostEcho && State.MatchPhase != EMatchPhase::BondRace;
		// R-POD-2, R-ELE-2: Paso Libre por la runa que lo da según el mapa del Elegido, o siempre por la afinidad de Fenrir.
		Mover.bFreePassage = HasAffinity(State, Seat, EAffinity::FreePassage) || IsPowerActive(State, Seat, EPower::FreePassage);
		return Mover;
	}
}
