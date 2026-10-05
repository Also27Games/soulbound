#pragma once

// Montaje de los tests de elecciones (S-009) y del trilero (S-010): una partida de cuatro sobre el tablero de 35 casillas,
// con ayudantes para vincular asientos, dar poderes, dejar una ficha en una casilla y contestar elecciones por la máquina
// de turnos. Las runas y las posiciones de las fichas se fijan a mano: es lo que el test necesita, no lo que comprueba.
// Solo para tests. Sacado de PowerChoicesTest.cpp en S-010 para no duplicarlo.

#include "Match/BondRaceRules.h"
#include "Match/MatchStart.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Soulbound::Tests
{
	/** Partida de cuatro sobre el tablero de 35 casillas, con los Elegidos del archivo de datos. */
	struct FChoiceFixture
	{
		FMatchState State;
		FRandomStream Random{1};
		double Now = 1.0;

		bool Start(FAutomationTestBase& Test)
		{
			FBoardState Board;
			FMatchParams Params;
			FElectsTable Elects;
			if (!Soulbound::Tests::LoadBoardFile(Test, Soulbound::Tests::RaceBoardFileName(), Board) || !Soulbound::Tests::LoadTestMatchParams(Test, Params)
				|| !Soulbound::Tests::LoadTestElects(Test, Elects))
			{
				return false;
			}
			FMatchStartResult Result = Soulbound::Rules::StartMatch(Board, Params, Elects, Soulbound::Match::MaxPlayers, Random);
			State = MoveTemp(Result.State);
			return Test.TestTrue(FString::Printf(TEXT("La partida empieza (%s)"), *Result.Message), Result.IsSuccess());
		}

		/** Seat reclama el Nicho de DomainId y se vincula a su Elegido; el primer vínculo despierta el tablero. */
		bool Bond(FAutomationTestBase& Test, int32 Seat, int32 DomainId)
		{
			FDomainState& Domain = State.Board.Domains[DomainId];
			if (Domain.State == EDomainState::Forgotten)
			{
				Domain.State = EDomainState::Owned;
				Domain.NicheState = ENicheState::Free;
			}
			Now += 1.0;
			return Test.TestTrue(FString::Printf(TEXT("El asiento %d se vincula al Dominio %d"), Seat, DomainId),
				LandOn(State, Random, Seat, State.Board.FindNiche(DomainId), Now) && State.Seats[Seat].Role == ESeatRole::Elect);
		}

		/**
		 * Seat se vincula al primer Dominio con dueño y Nicho Libre, sin tocar el sorteo de Olvidados: para los tests en
		 * los que el Elegido concreto no importa. Devuelve el Dominio, o INDEX_NONE si no pudo.
		 */
		int32 BondAny(FAutomationTestBase& Test, int32 Seat)
		{
			const int32 DomainId = State.Board.Domains.IndexOfByPredicate([](const FDomainState& Domain)
			{
				return Domain.State == EDomainState::Owned && Domain.NicheState == ENicheState::Free;
			});
			return DomainId != INDEX_NONE && Bond(Test, Seat, DomainId) ? DomainId : INDEX_NONE;
		}

		/** Da a Seat una runa del tipo que le da Power: a un Elegido, según su mapa; a un Eco, la de Liberación (Celeridad). */
		void GivePower(int32 Seat, EPower Power)
		{
			FSeatState& Seat_ = State.Seats[Seat];
			if (Seat_.Role == ESeatRole::LostEcho)
			{
				Seat_.Runes[Seat_.DomainId] = 1;
				return;
			}
			const TArray<EPower>& Map = State.Elects.Elects[Seat_.DomainId].RuneToPower;
			Seat_.Runes[Map.IndexOfByKey(Power)] = 1;
		}

		/** El turno de Seat empieza y llega a PreRoll (BetweenTurns → Start → PreRoll), un segundo después del anterior. */
		void BeginTurn(int32 Seat)
		{
			Now += 1.0;
			State.ActiveSeat = Seat;
			State.TurnPhase = ETurnPhase::BetweenTurns;
			Advance();
			Advance();
		}

		bool Advance() { return Soulbound::Rules::AdvanceTurn(State, Now, Random); }

		EIntentResult Choose(int32 Seat, EChoiceKind Kind, int32 Option) { return Soulbound::Rules::SubmitChoiceIntent(State, Seat, Kind, Option, Now, Random); }

		/**
		 * Seat contesta la acción contra un jugador con Action contra Target (S-010): por el índice de esa opción. Si no está
		 * entre las opciones, entrega un índice que no existe, que el motor rechaza.
		 */
		EIntentResult Act(int32 Seat, EPlayerAction Action, int32 Target)
		{
			const TArray<FPlayerActionOption>& Options = State.PendingChoice.ActionOptions;
			const int32 Index = Options.IndexOfByKey(FPlayerActionOption{Action, Target});
			return Choose(Seat, EChoiceKind::PlayerAction, Index != INDEX_NONE ? Index : Options.Num());
		}

		/**
		 * Termina el turno en curso: "no" a lo que lo admita, la primera opción en lo que no (dados, runa de Koschei, esfera),
		 * siempre por el asiento al que se le pide (S-010); primer destino, minijuego fallado, y la muestra y la mezcla del
		 * trilero dejando pasar su tiempo.
		 */
		void FinishTurn()
		{
			for (int32 Step = 0; Step < 32 && State.TurnPhase != ETurnPhase::BetweenTurns; ++Step)
			{
				if (State.TurnPhase == ETurnPhase::SkillTrial)
				{
					Soulbound::Rules::SubmitSkillTrialResult(State, State.ActiveSeat, false, Now);
				}
				else if (State.PendingChoice.IsOpen())
				{
					const FPendingChoice& Choice = State.PendingChoice;
					Choose(Choice.Seat, Choice.Kind, Choice.CanDecline() ? INDEX_NONE : Choice.AnswerAt(0));
				}
				else if (State.ShellGame.Stage == EShellGameStage::Reveal || State.ShellGame.Stage == EShellGameStage::Shuffle)
				{
					Now = FMath::Max(Now, State.ShellGame.StageDeadlineSeconds);
				}
				else if (State.TurnPhase == ETurnPhase::RollAndChoose && !State.ReachableOptions.IsEmpty())
				{
					Soulbound::Rules::SubmitDestinationIntent(State, State.ActiveSeat, State.ReachableOptions[0], Now);
				}
				Advance();
			}
		}

		/** Seat termina su movimiento en TileId, un segundo después del anterior. Devuelve LandOn. */
		bool Land(int32 Seat, int32 TileId)
		{
			Now += 1.0;
			return LandOn(State, Random, Seat, TileId, Now);
		}

		/** Nodos Planares del tablero, en orden ascendente. */
		TArray<int32> PlanarNodes() const
		{
			TArray<int32> Nodes;
			for (const FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.PowerNodeEffect == EPowerNodeEffect::Planar)
				{
					Nodes.Add(Tile.TileId);
				}
			}
			return Nodes;
		}

		/** Todos los Nodos de Poder pasan a Planares: así "los demás" son más de uno. */
		void MakeAllNodesPlanar()
		{
			for (FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.Type == ETileType::PowerNode)
				{
					Tile.PowerNodeEffect = EPowerNodeEffect::Planar;
				}
			}
		}

		/** Casillas normales (sin tipo) del Dominio, en orden ascendente; de cualquier Dominio con INDEX_NONE. */
		TArray<int32> NormalTiles(int32 DomainId = INDEX_NONE) const
		{
			TArray<int32> Tiles;
			for (const FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.Type == ETileType::Normal && (DomainId == INDEX_NONE || Tile.DomainId == DomainId))
				{
					Tiles.Add(Tile.TileId);
				}
			}
			return Tiles;
		}

		int32 SourceOf(int32 DomainId) const
		{
			for (const FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.Type == ETileType::PowerSource && Tile.DomainId == DomainId)
				{
					return Tile.TileId;
				}
			}
			return INDEX_NONE;
		}

		int32 DomainOf(EDomainState DomainState, int32 Except = INDEX_NONE) const
		{
			for (const FDomainState& Domain : State.Board.Domains)
			{
				if (Domain.State == DomainState && Domain.DomainId != Except)
				{
					return Domain.DomainId;
				}
			}
			return INDEX_NONE;
		}

		/** Sella TileId a mano (el test no comprueba las reglas de Sello). */
		void Seal(int32 TileId)
		{
			State.Board.Tiles[TileId].bSealed = true;
			State.Board.SealedTiles.Add(TileId);
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
