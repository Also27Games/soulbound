// Tests del cerebro del Warden del spike (paso 4): se instancia y se prueba sin subsystem ni mundo (R-WAR-1).
// Desde S-004 sella y libera con las reglas de Sello del motor (Board/SealRules.h).

#include "Board/SealRules.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "MatchTestSetup.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenBrainPicksValidTilesTest,
	"Soulbound.Core.Warden.EligeSoloCasillasValidas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWardenBrainPicksValidTilesTest::RunTest(const FString& Parameters)
{
	// Criterio "Cerebro del spike" (S-004, R-TAB-22): el cerebro elige entre las casillas que CanSealTile acepta, así
	// que ninguna intervención de sellar se pierde; y después de cada intervención nunca hay más de M Sellos, ninguno
	// en casilla prohibida u ocupada, y toda casilla no sellada tiene camino al Altar. Entre evaluaciones se juegan
	// turnos para que las fichas se muevan y cambien las casillas ocupadas.
	FRandomStream Random(21);
	FMatchState State;
	if (!Soulbound::Tests::StartTestMatch(*this, Random, State))
	{
		return false;
	}
	FWardenBrain::FParams Always;
	Always.InterveneProbability = 1.0;
	const FWardenBrain Brain(Always);
	FRandomStream BrainRandom(22);

	int32 Seals = 0, Releases = 0;
	for (int32 Evaluation = 0; Evaluation < 300; ++Evaluation)
	{
		for (int32 Step = 0; Step < 3; ++Step)
		{
			if (State.TurnPhase == ETurnPhase::RollAndChoose && !State.ReachableOptions.IsEmpty())
			{
				Soulbound::Rules::SubmitDestinationIntent(State, State.ActiveSeat, State.ReachableOptions.Last(), State.ClockSeconds);
			}
			else
			{
				Soulbound::Rules::AdvanceTurn(State, State.ClockSeconds, Random);
			}
		}

		const FWardenDecision Decision = Brain.Evaluate(State, BrainRandom);
		if (Decision.Action == EWardenAction::SealTile)
		{
			++Seals;
			const ESealResult Check = Soulbound::Rules::CanSealTile(State, Decision.TileId);
			if (Check != ESealResult::Accepted)
			{
				AddError(FString::Printf(TEXT("Evaluación %d: el cerebro elige la casilla %d, que se rechaza (%d)"), Evaluation, Decision.TileId, static_cast<int32>(Check)));
				break;
			}
		}
		Releases += Decision.Action == EWardenAction::ReleaseTile ? 1 : 0;
		FWardenBrain::Apply(State, Decision);
		if (Decision.Action == EWardenAction::SealTile && !State.Board.Tiles[Decision.TileId].bSealed)
		{
			AddError(FString::Printf(TEXT("Evaluación %d: la intervención de sellar la %d se pierde"), Evaluation, Decision.TileId));
			break;
		}
		if (!Soulbound::Tests::CheckSealInvariants(*this, State, FString::Printf(TEXT("Evaluación %d"), Evaluation)))
		{
			break;
		}
		State.ClockSeconds += 1.0;
	}
	TestTrue(TEXT("El cerebro sella más veces que el máximo: el tope entra en juego"), Seals > State.Board.Params.MaxSealedTiles);
	TestTrue(TEXT("El cerebro también libera"), Releases > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
