// Test de los archivos de tablero del proyecto (S-006, criterio "Cualquier tablero de datos"): cada
// Content/Data/Boards/*.json carga, pasa ValidateBoard e inicia una partida. Sustituye a "el tablero de 15 es válido"
// de S-005. Si falla, el fallo es del archivo (los genera el editor de tablero): se avisa al autor, no se retoca el
// validador. Se lanza sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Board.Data".

#include "Match/MatchStart.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardFilesAllValidTest,
	"Soulbound.Core.Board.Data.TodosLosTablerosValidos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardFilesAllValidTest::RunTest(const FString& Parameters)
{
	// Criterio "Cualquier tablero de datos" (R-TAB-1 a 5): dado cada archivo Content/Data/Boards/*.json, cuando se
	// carga, entonces pasa ValidateBoard e inicia una partida de cuatro. Semilla fija 1.
	const TArray<FString> Files = Soulbound::Tests::BoardFileNames();
	if (!TestTrue(TEXT("Hay archivos de tablero"), Files.Num() > 0))
	{
		return false;
	}
	FMatchParams Params;
	if (!Soulbound::Tests::LoadTestMatchParams(*this, Params))
	{
		return false;
	}
	for (const FString& File : Files)
	{
		FBoardState Board;
		if (!Soulbound::Tests::LoadBoardFile(*this, File, Board))
		{
			continue;
		}
		FRandomStream Random(1);
		const FMatchStartResult Start = Soulbound::Rules::StartMatch(Board, Params, Soulbound::Match::MaxPlayers, Random);
		TestTrue(FString::Printf(TEXT("%s inicia una partida (%s)"), *File, *Start.Message), Start.IsSuccess());
		AddInfo(FString::Printf(TEXT("%s: %d casillas"), *File, Board.Tiles.Num()));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
