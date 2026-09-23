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
	TestTrue(TEXT("Nunca sella la casilla 0 ni una ocupada"), First.TileId != 0 && !State.IsTileOccupied(First.TileId));

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

#endif // WITH_DEV_AUTOMATION_TESTS
