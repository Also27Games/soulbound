// Tests del tablero de 15 casillas por Dominio (S-005): el archivo generado por el editor de tablero sirve para una
// simulación entera. Si falla, el fallo es del archivo (lo generó el editor, cuyo validador es un espejo en
// JavaScript): se avisa al autor, no se retoca el validador. Desde S-006, que el archivo es válido lo comprueba
// Soulbound.Core.Board.Data.TodosLosTablerosValidos para todos los tableros de datos.
// Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Board15".

#include "Board/BoardLoader.h"
#include "Decider/Decider.h"
#include "Match/MatchParams.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "MatchTestSetup.h"
#include "Simulation/MatchSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString Board15Path()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_15.json"));
	}

	bool LoadBoard15(FAutomationTestBase& Test, FBoardState& OutBoard)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(Board15Path());
		OutBoard = Result.Board;
		return Test.TestTrue(FString::Printf(TEXT("tablero_15.json carga y valida (%s)"), *Result.Message), Result.IsSuccess());
	}

	bool MakeParams15(FAutomationTestBase& Test, int32 Seed, FSimulationParams& OutParams)
	{
		OutParams.Seed = Seed;
		return LoadBoard15(Test, OutParams.Board) && Soulbound::Tests::LoadTestMatchParams(Test, OutParams.MatchParams)
			&& Soulbound::Tests::LoadTestElects(Test, OutParams.Elects);
	}

	TArray<TSharedPtr<IDecider>> FourDummies()
	{
		TArray<TSharedPtr<IDecider>> Deciders;
		for (int32 i = 0; i < Soulbound::Match::SeatCount; ++i)
		{
			Deciders.Add(MakeShared<FDummyDecider>());
		}
		return Deciders;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoard15SimulationTest,
	"Soulbound.Core.Board15.SimulacionDeterminista",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoard15SimulationTest::RunTest(const FString& Parameters)
{
	// CA "Simulación sobre el tablero de 15": la partida entera termina, es determinista por semilla y baja de 1 s.
	FSimulationParams Params;
	if (!MakeParams15(*this, 42, Params))
	{
		return false;
	}
	const FSimulationResult A = FMatchSimulation(Params, FourDummies()).Run();
	const FSimulationResult B = FMatchSimulation(Params, FourDummies()).Run();

	TestTrue(TEXT("La partida termina y se jugaron turnos"), A.Turns > 0);
	// Desde S-006, con dummies al azar en un tablero de 91 casillas nadie suele vincularse en la carrera: la partida
	// termina entonces con victoria del Warden (R-CAR-8), antes de su duración.
	TestTrue(TEXT("La partida llega a su duración o termina con victoria del Warden"),
		A.FinalState.ClockSeconds >= Params.DurationSeconds - Params.MatchParams.TurnClockSeconds || A.FinalState.Outcome == EMatchOutcome::WardenVictory);
	TestEqual(TEXT("Misma semilla, mismo número de líneas"), A.Log.Num(), B.Log.Num());
	TestTrue(TEXT("Misma semilla, mismo registro"), A.Log == B.Log);
	TestTrue(TEXT("Misma semilla, mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));
	TestTrue(FString::Printf(TEXT("Una partida tarda menos de un segundo (%.4f s)"), A.WallSeconds), A.WallSeconds < 1.0);

	FSimulationParams Other = Params;
	Other.Seed = 43;
	TestTrue(TEXT("Otra semilla, otro registro"), FMatchSimulation(Other, FourDummies()).Run().Log != A.Log);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
