// Tests del telón (S-024, R-PRO-1 precisión): la partida no empieza el prólogo hasta que la presentación lo sube, o hasta
// que vence el tope. Sin mundo ni editor. Filtro: "Soulbound.Game.Match.Telon".

#include "../../Match/Curtain.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// CA-TEL-1 (espera), R-PRO-1: bajado en el 10 con tope 6, espera; subirlo la primera vez lo dice, la segunda no hace nada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurtainWaitTest, "Soulbound.Game.Match.Telon.EsperaYSubida",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCurtainWaitTest::RunTest(const FString& Parameters)
{
	FCurtain Curtain;
	Curtain.Lower(10.0, 6.0);
	TestTrue(TEXT("Bajado en el 10: está bajado"), Curtain.IsDown(10.0));
	TestFalse(TEXT("En el 12 el tope (6 s) no ha vencido"), Curtain.TimedOut(12.0));
	TestTrue(TEXT("Subirlo estando bajado devuelve verdadero"), Curtain.Raise(12.0));
	TestFalse(TEXT("Y deja de estar bajado"), Curtain.IsDown(12.0));
	TestFalse(TEXT("Subirlo otra vez no hace nada"), Curtain.Raise(13.0));
	return true;
}

// CA-TEL-2 (tope), R-PRO-1: vence a los TimeoutSeconds de bajarlo; con tope cero o negativo, en el acto; subido, nunca.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurtainTimeoutTest, "Soulbound.Game.Match.Telon.TopeVence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCurtainTimeoutTest::RunTest(const FString& Parameters)
{
	FCurtain Curtain;
	Curtain.Lower(10.0, 6.0);
	TestFalse(TEXT("En el 15.9 aún no vence"), Curtain.TimedOut(15.9));
	TestTrue(TEXT("En el 16 vence"), Curtain.TimedOut(16.0));

	for (const double Timeout : {0.0, -1.0})
	{
		FCurtain Immediate;
		Immediate.Lower(10.0, Timeout);
		TestTrue(FString::Printf(TEXT("Con tope %.0f vence en el acto"), Timeout), Immediate.TimedOut(10.0));
	}

	Curtain.Raise(11.0);
	TestFalse(TEXT("Subido, el tope no vence aunque pase el tiempo"), Curtain.TimedOut(100.0));
	return true;
}

// CA-TEL-3 (sin bajar), R-PRO-1: un telón recién creado no espera nada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurtainNeverLoweredTest, "Soulbound.Game.Match.Telon.SinBajarNoHaceNada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCurtainNeverLoweredTest::RunTest(const FString& Parameters)
{
	FCurtain Curtain;
	TestFalse(TEXT("Recién creado no está bajado"), Curtain.IsDown(5.0));
	TestFalse(TEXT("Subirlo no hace nada"), Curtain.Raise(5.0));
	TestFalse(TEXT("El tope no vence"), Curtain.TimedOut(1000.0));
	return true;
}

// CA-TEL-4 (margen), ampliación del 3/11: tras subirse, el telón sigue contando como bajado durante el margen; el tope solo cuenta sin subir.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurtainGraceTest, "Soulbound.Game.Match.Telon.MargenTrasSubir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCurtainGraceTest::RunTest(const FString& Parameters)
{
	FCurtain Curtain;
	Curtain.Lower(10.0, 6.0, 1.5);
	TestTrue(TEXT("Sin subir sigue bajado"), Curtain.IsDown(11.0));
	TestTrue(TEXT("Subirlo estando bajado devuelve verdadero"), Curtain.Raise(12.0));
	TestTrue(TEXT("En el 13, dentro del margen (12 + 1.5), sigue contando como bajado"), Curtain.IsDown(13.0));
	TestFalse(TEXT("En el 13.5 el margen ha vencido"), Curtain.IsDown(13.5));
	TestFalse(TEXT("El tope no cuenta una vez subido"), Curtain.TimedOut(13.0));

	FCurtain NoGrace;
	NoGrace.Lower(10.0, 6.0, 0.0);
	NoGrace.Raise(12.0);
	TestFalse(TEXT("Con margen 0, subido en el 12 ya no está bajado"), NoGrace.IsDown(12.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
