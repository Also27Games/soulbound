// Tests de lo que decide el bucle de avance del anfitrión de partida (S-032): cuándo repite la vuelta, cuándo se pasa del tope y
// cuándo se avisa. Sin mundo ni editor. Filtro: "Soulbound.Game.Match.Anfitrion".

#include "../../Match/AdvanceGuard.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using Soulbound::Match::AfterAnswer;
using Soulbound::Match::EAfterAnswer;

// CA-ANF-1 (R-ANF-1): solo Accepted repite la vuelta. Comprueba los siete valores de EIntentResult de hoy, de Accepted a NotNextTile, cada uno
// en su sitio. Un valor nuevo queda decidido por construcción, no por este test: AfterAnswer solo repite con Accepted, así que cualquier otro
// valor sigue. (El test solo se rompe si se inserta un valor en medio y cambia los números; añadido al final, pasa sin enterarse.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdvanceAfterAnswerTest, "Soulbound.Game.Match.Anfitrion.RepiteSoloSiAcepta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FAdvanceAfterAnswerTest::RunTest(const FString& Parameters)
{
	const EIntentResult All[] = {EIntentResult::Accepted, EIntentResult::WrongSeat, EIntentResult::WrongPhase, EIntentResult::Unreachable,
		EIntentResult::ClockExpired, EIntentResult::InvalidOption, EIntentResult::NotNextTile};
	// El último valor conocido más uno tiene que ser el tamaño de la lista: solo cuida que la lista siga alineada con los números del enum.
	TestEqual(TEXT("La lista cubre todo el enum (NotNextTile es el último valor)"), static_cast<int32>(EIntentResult::NotNextTile) + 1, static_cast<int32>(UE_ARRAY_COUNT(All)));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(All); ++Index)
	{
		TestEqual(FString::Printf(TEXT("El valor %d está en su sitio de la lista"), Index), static_cast<int32>(All[Index]), Index);
		const EAfterAnswer Expected = All[Index] == EIntentResult::Accepted ? EAfterAnswer::Repeat : EAfterAnswer::Continue;
		TestEqual(FString::Printf(TEXT("Resultado %d: solo Accepted repite"), Index), static_cast<int32>(AfterAnswer(All[Index])), static_cast<int32>(Expected));
	}
	return true;
}

// CA-ANF-2 (R-ANF-2): con tope 1000 permite 1000 vueltas y niega la 1001; reiniciado, vuelve a permitirlas.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdvanceGuardLimitTest, "Soulbound.Game.Match.Anfitrion.TopeDelBucle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FAdvanceGuardLimitTest::RunTest(const FString& Parameters)
{
	FAdvanceGuard Guard;
	TestEqual(TEXT("El tope inicial es 1000"), Guard.Limit, 1000);
	int32 Allowed = 0;
	for (int32 Lap = 0; Lap < 1000; ++Lap)
	{
		Allowed += Guard.Allow() ? 1 : 0;
	}
	TestEqual(TEXT("Permite las 1000 vueltas"), Allowed, 1000);
	TestFalse(TEXT("Niega la 1001"), Guard.Allow());
	TestFalse(TEXT("... y las que siguen"), Guard.Allow());

	Guard.Reset();
	Allowed = 0;
	for (int32 Lap = 0; Lap < 1000; ++Lap)
	{
		Allowed += Guard.Allow() ? 1 : 0;
	}
	TestEqual(TEXT("Reiniciado, vuelve a permitir las 1000"), Allowed, 1000);
	TestFalse(TEXT("... y vuelve a negar la 1001"), Guard.Allow());
	return true;
}

// R-ANF-2, ajuste del autor: el primer aviso sale en el acto; después, como mucho uno cada 5 s, con la cuenta de las veces que se ha
// superado el tope desde el aviso anterior.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdvanceTripNoticeTest, "Soulbound.Game.Match.Anfitrion.AvisoDelTope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FAdvanceTripNoticeTest::RunTest(const FString& Parameters)
{
	FAdvanceTripNotice Notice;
	TestEqual(TEXT("El intervalo inicial es 5 s"), Notice.MinIntervalSeconds, 5.0);
	int32 Trips = -1;
	TestTrue(TEXT("La primera vez avisa"), Notice.NoteTrip(100.0, Trips));
	TestEqual(TEXT("... con una sola vez"), Trips, 1);

	TestFalse(TEXT("A los 0,5 s no avisa"), Notice.NoteTrip(100.5, Trips));
	TestFalse(TEXT("A los 4,9 s no avisa"), Notice.NoteTrip(104.9, Trips));
	TestTrue(TEXT("A los 5 s avisa"), Notice.NoteTrip(105.0, Trips));
	TestEqual(TEXT("... contando las dos que se callaron y esta"), Trips, 3);

	TestFalse(TEXT("Justo después, otra vez callado"), Notice.NoteTrip(105.1, Trips));
	TestTrue(TEXT("Cinco segundos después del aviso, avisa"), Notice.NoteTrip(110.2, Trips));
	TestEqual(TEXT("... y la cuenta vuelve a empezar: dos veces"), Trips, 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
