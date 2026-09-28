#pragma once

// Montaje común de los tests de partida: el tablero de pruebas y los parámetros de partida, cargados de sus archivos
// de datos como en el juego, y una partida iniciada con StartMatch. Solo para tests.

#include "Board/BoardLoader.h"
#include "Match/MatchParams.h"
#include "Match/MatchStart.h"
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
