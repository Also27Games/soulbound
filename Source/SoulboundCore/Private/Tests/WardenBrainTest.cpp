// Test del paso 4 del spike: el cerebro del Warden se instancia y se prueba sin subsystem ni mundo (R-WAR-1).

#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenBrain.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenBrainWithoutWorldTest,
	"Soulbound.Core.Warden.CerebroSinMundo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWardenBrainWithoutWorldTest::RunTest(const FString& Parameters)
{
	FMatchState State = FMatchState::MakeSpikeMatch();
	FRandomStream Rng(5);

	FWardenBrain::FParams Never;
	Never.InterveneProbability = 0.0;
	const FWardenDecision Silence = FWardenBrain(Never).Evaluate(State, Rng);
	TestFalse(TEXT("Con probabilidad 0 no interviene"), Silence.IsIntervention());

	FWardenBrain::FParams Always;
	Always.InterveneProbability = 1.0;
	const FWardenDecision First = FWardenBrain(Always).Evaluate(State, Rng);
	TestTrue(TEXT("Con probabilidad 1 interviene"), First.IsIntervention());
	TestEqual(TEXT("Sin Sellos previos, la primera intervencion sella"), static_cast<int32>(First.Action), static_cast<int32>(EWardenAction::SealTile));
	TestTrue(TEXT("Nunca sella el Altar ni una casilla ocupada"), !State.Board.IsAltar(First.TileId) && !State.IsTileOccupied(First.TileId));

	FWardenBrain::Apply(State, First);
	TestTrue(TEXT("Aplicar la decision deja la casilla sellada"), State.Board.Tiles[First.TileId].bSealed);

	// Mismo estado y misma semilla: misma decisión (R-SIM-2).
	FRandomStream RngA(9), RngB(9);
	const FWardenDecision A = FWardenBrain(Always).Evaluate(State, RngA);
	const FWardenDecision B = FWardenBrain(Always).Evaluate(State, RngB);
	TestEqual(TEXT("Misma semilla, misma accion"), static_cast<int32>(A.Action), static_cast<int32>(B.Action));
	TestEqual(TEXT("Misma semilla, misma casilla"), A.TileId, B.TileId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenBrainNeverSealsAltarTest,
	"Soulbound.Core.Warden.NuncaSellaElAltar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWardenBrainNeverSealsAltarTest::RunTest(const FString& Parameters)
{
	// Glosario "Sello": no se sella el Altar. El cerebro lo reconoce por el tipo de casilla, no por su número:
	// la 3 hace de Altar, está libre y, si fuera una casilla normal, alguna semilla la elegiría.
	FMatchState State = FMatchState::MakeSpikeMatch();
	State.Board.Tiles[3].Type = ETileType::Altar;

	FWardenBrain::FParams Always;
	Always.InterveneProbability = 1.0;
	const FWardenBrain Brain(Always);
	for (int32 Seed = 0; Seed < 50; ++Seed)
	{
		FRandomStream Rng(Seed);
		const FWardenDecision Decision = Brain.Evaluate(State, Rng);
		if (Decision.Action == EWardenAction::SealTile && State.Board.IsAltar(Decision.TileId))
		{
			AddError(FString::Printf(TEXT("Con la semilla %d el Warden sella el Altar (casilla %d)"), Seed, Decision.TileId));
			break;
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
