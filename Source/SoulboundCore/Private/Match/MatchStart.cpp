#include "Match/MatchStart.h"

#include "Board/BoardLoader.h"
#include "Elects/ElectsLoader.h"
#include "Match/MatchEvents.h"
#include "Match/WardenEvents.h"
#include "Math/RandomStream.h"
#include "Warden/WardenVoice.h"

namespace
{
	/** Baraja Items con el sorteo de la partida (Fisher-Yates): misma semilla, mismo orden. */
	template <typename ItemType>
	void Shuffle(TArray<ItemType>& Items, FRandomStream& Random)
	{
		for (int32 Last = Items.Num() - 1; Last > 0; --Last)
		{
			Items.Swap(Last, Random.RandRange(0, Last));
		}
	}

	FMatchStartResult Fail(EMatchStartError Error, const FString& Message)
	{
		FMatchStartResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}
}

namespace Soulbound::Rules
{
	FMatchStartResult StartMatch(const FBoardState& Board, const FMatchParams& Params, const FElectsTable& Elects, int32 PlayerCount, FRandomStream& Random,
		const FWardenEventTable& WardenEvents, const FWardenParams& WardenParams, const FWardenLineTable& WardenLines)
	{
		using Soulbound::Board::DomainCount;

		if (PlayerCount < Soulbound::Match::MinPlayers || PlayerCount > Soulbound::Match::MaxPlayers)
		{
			return Fail(EMatchStartError::InvalidPlayerCount, FString::Printf(TEXT("Una partida admite de %d a %d jugadores, no %d."),
				Soulbound::Match::MinPlayers, Soulbound::Match::MaxPlayers, PlayerCount));
		}
		const FBoardLoadResult Validation = ValidateBoard(Board);
		if (!Validation.IsSuccess())
		{
			return Fail(EMatchStartError::InvalidBoard, TEXT("El tablero no es válido: ") + Validation.Message);
		}
		const FMatchParamsLoadResult ParamsValidation = ValidateMatchParams(Params);
		if (!ParamsValidation.IsSuccess())
		{
			return Fail(EMatchStartError::InvalidMatchParams, TEXT("Los parámetros de partida no son válidos: ") + ParamsValidation.Message);
		}
		const FElectsLoadResult ElectsValidation = ValidateElects(Elects);
		if (!ElectsValidation.IsSuccess())
		{
			return Fail(EMatchStartError::InvalidElects, TEXT("Los Elegidos no son válidos: ") + ElectsValidation.Message);
		}
		const FWardenEventsLoadResult EventsValidation = ValidateWardenEvents(WardenEvents);
		if (!EventsValidation.IsSuccess())
		{
			return Fail(EMatchStartError::InvalidWardenEvents, TEXT("Los eventos del Warden no son válidos: ") + EventsValidation.Message);
		}
		if (WardenParams.HasWarden())
		{
			const FWardenParamsLoadResult WardenValidation = ValidateWardenParams(WardenParams);
			if (!WardenValidation.IsSuccess())
			{
				return Fail(EMatchStartError::InvalidWardenParams, TEXT("Los parámetros del Warden no son válidos: ") + WardenValidation.Message);
			}
		}

		FMatchStartResult Result;
		FMatchState& State = Result.State;
		State.Board = Board;
		State.Params = Params;
		State.Elects = Elects;
		// S-014: los eventos del Warden viajan en el estado, como los Elegidos; sin ellos, la partida no tiene eventos.
		State.WardenEvents = WardenEvents;
		// S-016 (R-WAR-10): el Warden viaja en el estado con su sorteo propio, la semilla de la partida más 2 (como el anfitrión y
		// la simulación hasta S-015), sin tocar el sorteo de la partida; los usos de cada evento y la primera evaluación.
		State.WardenParams = WardenParams;
		// S-017 (R-DIS-2): las tablas de los disparadores, enlazadas con los eventos de la partida por su identificador.
		if (WardenParams.HasWarden())
		{
			const FWardenParamsLoadResult Binding = BindWardenTriggerEvents(State.WardenParams, WardenEvents);
			if (!Binding.IsSuccess())
			{
				return Fail(EMatchStartError::InvalidWardenParams, TEXT("Los disparadores del Warden no son válidos: ") + Binding.Message);
			}
		}
		State.Warden.Seed = Random.GetInitialSeed() + 2;
		// S-021 (R-WAR-9): la voz tiene su propio sorteo, la semilla de la partida más 5, y no gasta ningún valor de los demás.
		State.WardenLines = WardenLines;
		State.Warden.VoiceSeed = Random.GetInitialSeed() + 5;
		// S-016 (R-TAB-12): el reparto de hogares al acabar la carrera, con su propia semilla, más 3, tampoco toca los dados.
		State.HomeAssignmentSeed = Random.GetInitialSeed() + 3;
		for (const FWardenEventDef& Event : WardenEvents.Events)
		{
			State.Warden.UsesLeft.Add(Event.MaxUses > 0 ? Event.MaxUses : -1);
		}
		State.Warden.NextEvaluationSeconds = WardenParams.EvaluationIntervalSeconds;

		// R-TAB-5: una bolsa con tantos efectos de cada tipo como piden los datos, barajada y repartida a los Nodos
		// en orden de casilla. ValidateBoard garantiza que hay tantos Nodos como efectos en la bolsa.
		TArray<EPowerNodeEffect> Effects;
		Effects.Reserve(Board.Params.TotalPowerNodeCount());
		const TPair<EPowerNodeEffect, int32> Counts[] = {
			{EPowerNodeEffect::Duel, Board.Params.DuelNodeCount},
			{EPowerNodeEffect::Oracle, Board.Params.OracleNodeCount},
			{EPowerNodeEffect::Planar, Board.Params.PlanarNodeCount},
		};
		for (const TPair<EPowerNodeEffect, int32>& Count : Counts)
		{
			for (int32 i = 0; i < Count.Value; ++i)
			{
				Effects.Add(Count.Key);
			}
		}
		Shuffle(Effects, Random);
		int32 NextEffect = 0;
		for (FTileState& Tile : State.Board.Tiles)
		{
			Tile.PowerNodeEffect = Tile.Type == ETileType::PowerNode ? Effects[NextEffect++] : EPowerNodeEffect::None;
		}

		// R-TAB-6: 6 menos N Dominios Olvidados al azar, con su Nicho Cerrado; el resto con dueño (su Elegido) y con el Nicho Libre.
		TArray<int32> DomainOrder;
		for (int32 D = 0; D < DomainCount; ++D)
		{
			DomainOrder.Add(D);
		}
		Shuffle(DomainOrder, Random);
		const int32 ForgottenCount = DomainCount - PlayerCount;
		for (int32 Index = 0; Index < DomainOrder.Num(); ++Index)
		{
			FDomainState& Domain = State.Board.Domains[DomainOrder[Index]];
			const bool bForgotten = Index < ForgottenCount;
			Domain.State = bForgotten ? EDomainState::Forgotten : EDomainState::Owned;
			Domain.NicheState = bForgotten ? ENicheState::Closed : ENicheState::Free;
		}

		// R-TAB-2, R-CAR-2: siempre cuatro asientos, todos en el Altar y Ecos Perdidos sin Dominio.
		State.Seats.SetNum(Soulbound::Match::SeatCount);
		const int32 Altar = State.Board.FindAltar();
		for (FSeatState& Seat : State.Seats)
		{
			Seat.TileId = Altar;
			Seat.Role = ESeatRole::LostEcho;
			Seat.DomainId = INDEX_NONE;
		}

		// R-CAR-1, R-TAB-8: la carrera empieza con la partida y el tablero, Primordial.
		State.MatchPhase = EMatchPhase::BondRace;
		State.Outcome = EMatchOutcome::Undecided;
		State.Board.Phase = EBoardPhase::Primordial;

		// R-TUR-1: el orden de turno se sortea una vez y no cambia en toda la partida. Va el último, para que el
		// sorteo de Nodos y Olvidados siga dando lo mismo con la misma semilla que antes de existir.
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			State.TurnOrder.Add(Seat);
		}
		Shuffle(State.TurnOrder, Random);
		State.ActiveSeat = State.TurnOrder[0];

		// R-ROB-6 (S-010): el valor base de las semillas de mezcla del trilero, una sola vez y el último, para que los Nodos,
		// los Olvidados y el orden de turno sigan dando lo mismo con la misma semilla. Desplaza un valor los dados de toda
		// semilla respecto a S-009 (decisión del autor del 05/10/2026: no hay partidas grabadas). Los robos no vuelven a
		// tocar el sorteo de la partida.
		State.ShellSeedBase = Random.RandRange(0, MAX_int32 - 1);
		State.ShellGameCount = 0;

		// S-021 (R-PRO-1, R-WAR-10): el Warden abre la partida, sin sacar nada del sorteo: el inicio, una frase por Dominio corrompido y una por
		// posición del orden de turno, dicha al asiento que la ocupa. Sin frases, no dice nada.
		// S-023 (R-WAR-10): cada Dominio corrompido deja su DomainForgotten justo antes de su frase, hable o no el Warden, y la apertura termina
		// con la frase de comienzo (R-PRO-1), a la mesa, con la que el anfitrión arranca el reloj de partida.
		SpeakWardenLine(State, Soulbound::Warden::LineStart, 0.0);
		for (const FDomainState& Domain : State.Board.Domains)
		{
			if (Domain.State == EDomainState::Forgotten)
			{
				LogEvent(State, EMatchEventType::DomainForgotten, 0.0, INDEX_NONE, Domain.DomainId);
				SpeakWardenLine(State, Soulbound::Warden::LineCorruption, 0.0);
			}
		}
		for (int32 Position = 0; Position < State.TurnOrder.Num() && Position < Soulbound::Warden::OrderLineCount; ++Position)
		{
			SpeakWardenLine(State, ScheduledOrderKey(Position + 1), 0.0, State.TurnOrder[Position]);
		}
		SpeakWardenLine(State, Soulbound::Warden::LineBegin, 0.0);
		return Result;
	}
}
