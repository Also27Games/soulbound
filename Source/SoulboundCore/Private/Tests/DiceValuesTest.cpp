// Tests del valor de cada dado de la tirada (S-039, R-TUR-16): CA-TUR-16, CA-TUR-17 y CA-TUR-18.
// Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Turn.DiceValues".

#include "ChoiceFixture.h"
#include "EventFixture.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::FChoiceFixture;

	/** Dominio de Apep en Content/Data/Elects/elegidos.json: afinidad rerollOnce (R-ELE-2). */
	constexpr int32 ApepDomain = 2;

	/** Los primeros Count valores de dado que sacaría Random, sin tocarlo. */
	TArray<int32> PeekValues(const FRandomStream& Random, int32 Count)
	{
		FRandomStream Copy = Random;
		TArray<int32> Values;
		for (int32 Die = 0; Die < Count; ++Die)
		{
			Values.Add(Copy.RandRange(1, Soulbound::Dice::Faces));
		}
		return Values;
	}

	/** Random tras sacar Count valores de dado. */
	FRandomStream AfterDraws(const FRandomStream& Random, int32 Count)
	{
		FRandomStream Copy = Random;
		for (int32 Draw = 0; Draw < Count; ++Draw)
		{
			Copy.RandRange(1, Soulbound::Dice::Faces);
		}
		return Copy;
	}

	int32 Sum(const TArray<int32>& Values)
	{
		int32 Total = 0;
		for (const int32 Value : Values)
		{
			Total += Value;
		}
		return Total;
	}

	bool AllInFaces(const TArray<int32>& Values)
	{
		return Values.FilterByPredicate([](int32 Value) { return Value < 1 || Value > Soulbound::Dice::Faces; }).IsEmpty();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceValuesStoredTest,
	"Soulbound.Core.Turn.DiceValues.LaTiradaGuardaCadaDado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDiceValuesStoredTest::RunTest(const FString& Parameters)
{
	// CA-TUR-16 (R-TUR-16): con un dado, el estado guarda un valor de 1 a 6 igual a Roll; con dos (Celeridad), dos valores de 1 a 6,
	// los del sorteo en su orden, y su suma es Roll.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Haste);

	F.BeginTurn(0);
	TestEqual(TEXT("Antes de tirar no hay valores"), F.State.DiceValues.Num(), 0);
	const FMatchState BeforeChoice = F.State;
	const FRandomStream RandomBeforeChoice = F.Random;
	const TArray<int32> Drawn = PeekValues(F.Random, Soulbound::Dice::MaxCount);

	TestEqual(TEXT("Elige dos dados"), F.Choose(0, EChoiceKind::DiceCount, 2), EIntentResult::Accepted);
	F.Advance();
	TestEqual(TEXT("Dos dados: dos valores"), F.State.DiceValues.Num(), 2);
	TestTrue(TEXT("... de 1 a 6"), AllInFaces(F.State.DiceValues));
	TestTrue(TEXT("... los del sorteo, en su orden"), F.State.DiceValues == Drawn);
	TestEqual(TEXT("... y su suma es la tirada"), Sum(F.State.DiceValues), F.State.Roll);

	F.State = BeforeChoice;
	F.Random = RandomBeforeChoice;
	TestEqual(TEXT("Elige un dado"), F.Choose(0, EChoiceKind::DiceCount, 1), EIntentResult::Accepted);
	F.Advance();
	TestEqual(TEXT("Un dado: un valor"), F.State.DiceValues.Num(), 1);
	TestTrue(TEXT("... de 1 a 6"), AllInFaces(F.State.DiceValues));
	if (F.State.DiceValues.Num() == 1)
	{
		TestEqual(TEXT("... igual a la tirada"), F.State.DiceValues[0], F.State.Roll);
		TestEqual(TEXT("... y es el primero del sorteo"), F.State.DiceValues[0], Drawn[0]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceValuesRerollAndClearTest,
	"Soulbound.Core.Turn.DiceValues.LaRepeticionLosSustituyeYElTurnoLosVacia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDiceValuesRerollAndClearTest::RunTest(const FString& Parameters)
{
	// CA-TUR-17 (R-TUR-16): tras repetir la tirada (Apep), los valores son los de la tirada nueva y suman el Roll nuevo; al terminar
	// el turno y en un turno cortado, ninguno.
	FChoiceFixture F;
	if (!F.Start(*this) || !F.Bond(*this, 0, ApepDomain) || F.BondAny(*this, 1) == INDEX_NONE)
	{
		return false;
	}
	const int32 Altar = F.State.Board.FindAltar();
	F.State.Seats[0].TileId = Altar;
	F.State.Seats[1].TileId = Altar;

	F.BeginTurn(0);
	const FRandomStream BeforeRoll = F.Random;
	const TArray<int32> First = PeekValues(BeforeRoll, 1);
	const TArray<int32> Second = PeekValues(AfterDraws(BeforeRoll, Soulbound::Dice::MaxCount), 1);
	F.Advance();
	TestTrue(TEXT("Tras la tirada, el valor es el del primer sorteo"), F.State.DiceValues == First);
	TestEqual(TEXT("... y se le ofrece repetir"), F.State.PendingChoice.Kind, EChoiceKind::Reroll);

	F.Now += 1.0;
	TestEqual(TEXT("Repite: aceptado"), F.Choose(0, EChoiceKind::Reroll, 1), EIntentResult::Accepted);
	TestTrue(TEXT("Los valores son los de la tirada nueva"), F.State.DiceValues == Second);
	TestEqual(TEXT("... y suman el Roll nuevo"), Sum(F.State.DiceValues), F.State.Roll);

	F.FinishTurn();
	TestEqual(TEXT("Al terminar el turno no queda ningún valor"), F.State.DiceValues.Num(), 0);

	// Un turno cortado después de tirar (R-TUR-11): no queda ningún valor.
	F.BeginTurn(1);
	F.Advance();
	TestEqual(TEXT("El asiento 1 tira"), F.State.TurnPhase, ETurnPhase::RollAndChoose);
	TestEqual(TEXT("... y tiene su valor"), F.State.DiceValues.Num(), 1);
	TestTrue(TEXT("El turno se corta"), Soulbound::Rules::InterruptTurn(F.State, F.Now, F.Random));
	TestEqual(TEXT("Un turno cortado no deja valores"), F.State.DiceValues.Num(), 0);
	TestEqual(TEXT("... ni tirada"), F.State.Roll, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceValuesForcedRollTest,
	"Soulbound.Core.Turn.DiceValues.ConTiradaFijadaNoHayValores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDiceValuesForcedRollTest::RunTest(const FString& Parameters)
{
	// CA-TUR-18 (R-TUR-16), primera mitad: con ForcedRoll fijado, Roll es el valor fijado y no se guarda ningún valor de dado. La
	// segunda mitad (el sorteo se consume igual: mismo registro de eventos con la misma semilla) la cubre el test de determinismo
	// que ya existe, que es una guarda y no nace en rojo; aquí solo se comprueba que el sorteo queda en el mismo punto con y sin
	// tirada fijada.
	FChoiceFixture Fixed;
	FChoiceFixture Free;
	if (!Fixed.Start(*this) || !Free.Start(*this))
	{
		return false;
	}
	Fixed.State.ForcedRoll = 9;
	for (FChoiceFixture* F : {&Fixed, &Free})
	{
		F->BeginTurn(0);
		F->Advance();
		TestEqual(TEXT("El turno espera destino"), F->State.TurnPhase, ETurnPhase::RollAndChoose);
	}
	TestEqual(TEXT("La tirada es la fijada"), Fixed.State.Roll, 9);
	TestEqual(TEXT("No se guarda ningún valor de dado"), Fixed.State.DiceValues.Num(), 0);
	TestEqual(TEXT("Sin fijar, sí se guarda"), Free.State.DiceValues.Num(), 1);
	TestEqual(TEXT("El sorteo queda en el mismo punto con y sin fijar"), Fixed.Random.GetCurrentSeed(), Free.Random.GetCurrentSeed());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
