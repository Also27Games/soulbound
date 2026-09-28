#pragma once

// Montaje común de los tests de partida: el tablero de pruebas y los parámetros de partida, cargados de sus archivos
// de datos como en el juego, y una partida iniciada con StartMatch. Solo para tests.

#include "Board/BoardLoader.h"
#include "Match/MatchParams.h"
#include "Match/MatchStart.h"
#include "Match/MatchState.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
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
	 * u ocupada y ninguna casilla sin camino al Altar. Apunta cada fallo como error del test, con Context delante.
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
			if (Tile.bSealed && State.IsTileOccupied(Tile.TileId))
			{
				Fail(FString::Printf(TEXT("la casilla %d está sellada y ocupada"), Tile.TileId));
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
