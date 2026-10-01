#pragma once

// Montaje común de los tests de partida: el tablero de pruebas y los parámetros de partida, cargados de sus archivos
// de datos como en el juego, y una partida iniciada con StartMatch. Solo para tests.

#include "Board/BoardLoader.h"
#include "Match/MatchParams.h"
#include "Match/MatchStart.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Soulbound::Tests
{
	inline FString TestBoardPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_pruebas.json"));
	}

	inline FString MatchParamsPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Match/partida.json"));
	}

	/**
	 * Tablero configurado del juego (S-006): el que carga el anfitrión de partida, de BoardRelativePath en la sección
	 * del MatchHost de DefaultGame.ini. Sin la línea, el valor inicial del código (UMatchHostSubsystem), el de 15.
	 */
	inline FString ConfiguredBoardPath()
	{
		FString Relative = TEXT("Data/Boards/tablero_15.json");
		if (GConfig)
		{
			GConfig->GetString(TEXT("/Script/Soulbound.MatchHostSubsystem"), TEXT("BoardRelativePath"), Relative, GGameIni);
		}
		return FPaths::Combine(FPaths::ProjectContentDir(), Relative);
	}

	/** Carga el tablero configurado del juego; si falla, lo apunta como error del test y devuelve false. */
	inline bool LoadConfiguredBoard(FAutomationTestBase& Test, FBoardState& OutBoard)
	{
		const FString Path = ConfiguredBoardPath();
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(Path);
		OutBoard = Result.Board;
		return Test.TestTrue(FString::Printf(TEXT("El tablero configurado %s carga (%s)"), *FPaths::GetCleanFilename(Path), *Result.Message), Result.IsSuccess());
	}

	/** Carga el tablero de pruebas; si falla, lo apunta como error del test y devuelve false. */
	inline bool LoadTestBoard(FAutomationTestBase& Test, FBoardState& OutBoard)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(TestBoardPath());
		OutBoard = Result.Board;
		return Test.TestTrue(FString::Printf(TEXT("El tablero de pruebas carga (%s)"), *Result.Message), Result.IsSuccess());
	}

	/** Carga Content/Data/Match/partida.json; si falla, lo apunta como error del test y devuelve false. */
	inline bool LoadTestMatchParams(FAutomationTestBase& Test, FMatchParams& OutParams)
	{
		const FMatchParamsLoadResult Result = Soulbound::Rules::LoadMatchParamsFromFile(MatchParamsPath());
		OutParams = Result.Params;
		return Test.TestTrue(FString::Printf(TEXT("Los parámetros de partida cargan (%s)"), *Result.Message), Result.IsSuccess());
	}

	/**
	 * Inicia una partida de cuatro jugadores sobre el tablero de pruebas con los parámetros de partida del archivo.
	 * Random queda como lo deja StartMatch: es el sorteo de la partida y los turnos siguen tirando de él.
	 */
	inline bool StartTestMatch(FAutomationTestBase& Test, FRandomStream& Random, FMatchState& OutState)
	{
		FBoardState Board;
		FMatchParams Params;
		if (!LoadTestBoard(Test, Board) || !LoadTestMatchParams(Test, Params))
		{
			return false;
		}
		FMatchStartResult Result = Soulbound::Rules::StartMatch(Board, Params, Soulbound::Match::MaxPlayers, Random);
		OutState = MoveTemp(Result.State);
		return Test.TestTrue(FString::Printf(TEXT("La partida empieza (%s)"), *Result.Message), Result.IsSuccess());
	}

	/**
	 * El asiento Seat termina su movimiento en TileId en el instante NowSeconds, por la máquina de turnos: se prepara
	 * su turno en RollAndChoose con esa casilla como única opción (el montaje que el test necesita, sin depender del
	 * dado), se entrega la intención y se avanza hasta BetweenTurns, que es donde se resuelve el destino. Solo para
	 * tests. Devuelve false si la intención no se acepta o el turno no llega a terminar.
	 */
	inline bool LandOn(FMatchState& State, FRandomStream& Random, int32 Seat, int32 TileId, double NowSeconds)
	{
		State.ActiveSeat = Seat;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.TurnDeadlineSeconds = NowSeconds + State.Params.TurnClockSeconds;
		State.Roll = 1;
		State.ReachableOptions = {TileId};
		if (Soulbound::Rules::SubmitDestinationIntent(State, Seat, TileId, NowSeconds) != EIntentResult::Accepted)
		{
			return false;
		}
		for (int32 Step = 0; Step < 8 && State.TurnPhase != ETurnPhase::BetweenTurns; ++Step)
		{
			Soulbound::Rules::AdvanceTurn(State, NowSeconds, Random);
		}
		return State.TurnPhase == ETurnPhase::BetweenTurns;
	}

	/** Dominios con el Nicho en el estado pedido, en orden de identificador. */
	inline TArray<int32> DomainsWithNiche(const FMatchState& State, ENicheState NicheState)
	{
		TArray<int32> Result;
		for (const FDomainState& Domain : State.Board.Domains)
		{
			if (Domain.NicheState == NicheState)
			{
				Result.Add(Domain.DomainId);
			}
		}
		return Result;
	}

	/** Eventos del registro de un tipo, en orden. */
	inline TArray<FMatchEvent> EventsOfType(const FMatchState& State, EMatchEventType Type)
	{
		return State.EventLog.FilterByPredicate([Type](const FMatchEvent& Event) { return Event.Type == Type; });
	}

	/** Si dos estados de partida son idénticos campo a campo (todos los campos son UPROPERTY). */
	inline bool SameState(const FMatchState& A, const FMatchState& B)
	{
		return FMatchState::StaticStruct()->CompareScriptStruct(&A, &B, PPF_None);
	}

	/**
	 * Casillas no selladas sin camino al Altar pisando solo casillas no selladas (jugador sin Paso Libre, R-TAB-22).
	 * Oráculo propio de los tests: no usa las reglas que comprueba.
	 */
	inline TArray<int32> TilesCutOffFromAltar(const FBoardState& Board)
	{
		TArray<bool> Reached;
		Reached.Init(false, Board.Tiles.Num());
		TArray<int32> Queue;
		const int32 Altar = Board.FindAltar();
		if (Board.IsValidTile(Altar))
		{
			Reached[Altar] = true;
			Queue.Add(Altar);
		}
		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			for (const int32 Next : Board.Tiles[Queue[i]].Neighbours)
			{
				if (Board.IsValidTile(Next) && !Reached[Next] && !Board.Tiles[Next].bSealed)
				{
					Reached[Next] = true;
					Queue.Add(Next);
				}
			}
		}
		TArray<int32> CutOff;
		for (const FTileState& Tile : Board.Tiles)
		{
			if (!Tile.bSealed && !Reached[Tile.TileId])
			{
				CutOff.Add(Tile.TileId);
			}
		}
		return CutOff;
	}

	/**
	 * Invariantes de los Sellos (R-TAB-22): no más del máximo, lista y marcas coherentes, ninguno en casilla prohibida
	 * y ninguna casilla sin camino al Altar. Apunta cada fallo como error del test, con Context delante.
	 * "No ocupada" solo vale al sellar (CanSealTile): hoy una ficha puede entrar después en una casilla que se selló
	 * entre su tirada y su confirmación, porque las opciones no se recalculan (TODO(R-TUR-11) en TurnRules.cpp).
	 */
	inline bool CheckSealInvariants(FAutomationTestBase& Test, const FMatchState& State, const FString& Context)
	{
		const FBoardState& Board = State.Board;
		bool bOk = true;
		auto Fail = [&](const FString& What)
		{
			Test.AddError(FString::Printf(TEXT("%s: %s"), *Context, *What));
			bOk = false;
		};

		if (Board.SealedTiles.Num() > Board.Params.MaxSealedTiles)
		{
			Fail(FString::Printf(TEXT("%d casillas selladas con un máximo de %d"), Board.SealedTiles.Num(), Board.Params.MaxSealedTiles));
		}
		for (const FTileState& Tile : Board.Tiles)
		{
			const int32 InList = Board.SealedTiles.FilterByPredicate([&Tile](int32 Id) { return Id == Tile.TileId; }).Num();
			if (InList > 1 || Tile.bSealed != (InList == 1))
			{
				Fail(FString::Printf(TEXT("la casilla %d tiene bSealed=%d y aparece %d veces en SealedTiles"), Tile.TileId, Tile.bSealed ? 1 : 0, InList));
			}
			if (Tile.bSealed && (Tile.Type == ETileType::Altar || Tile.Type == ETileType::Niche || Tile.Type == ETileType::PowerSource))
			{
				Fail(FString::Printf(TEXT("la casilla %d está sellada y es de un tipo prohibido"), Tile.TileId));
			}
		}
		for (const int32 Id : Board.SealedTiles)
		{
			if (!Board.IsValidTile(Id))
			{
				Fail(FString::Printf(TEXT("SealedTiles tiene una casilla inexistente, la %d"), Id));
			}
		}
		const TArray<int32> CutOff = TilesCutOffFromAltar(Board);
		if (!CutOff.IsEmpty())
		{
			Fail(FString::Printf(TEXT("casillas sin camino al Altar: %s"), *FString::JoinBy(CutOff, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); })));
		}
		return bOk;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
