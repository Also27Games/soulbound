// Tests del Grimorio (S-027, R-HUD-4, R-HUD-5): las filas que pinta el panel, una por asiento, y los textos de afinidades y poderes que se
// leen de Content/Data/Textos/grimorio.json. Funciones puras sobre un FMatchState armado a mano con los Elegidos de elegidos.json: sin
// mundo y sin actor. Filtro: "Soulbound.Game.Presentation.Grimorio".

#include "../../Presentation/StatusText.h"
#include "Elects/ElectsLoader.h"
#include "HAL/FileManager.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GrimoireTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::FGrimoireRow;
	using Soulbound::Presentation::FGrimoireTexts;

	constexpr int32 GrFenrirSeat = 0;
	constexpr int32 GrEchoSeat = 1;
	/** Fenrir es el Elegido del Dominio 1 en elegidos.json; su runa del Dominio 5 le da Vuelo Planar. */
	constexpr int32 GrFenrirDomain = 1;
	constexpr int32 GrRuneType = 5;

	/** Cuatro asientos con los Elegidos del repositorio: el 0, Elegido de Fenrir con una runa del Dominio 5; el 1, Eco; los otros, Ecos. */
	bool GrMakeState(FAutomationTestBase& Test, FMatchState& OutState)
	{
		const FElectsLoadResult Elects = Soulbound::Rules::LoadElectsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Elects/elegidos.json")));
		if (!Test.TestTrue(FString::Printf(TEXT("elegidos.json se carga (%s)"), *Elects.Message), Elects.IsSuccess()))
		{
			return false;
		}
		OutState.Elects = Elects.Table;
		OutState.Seats.SetNum(4);
		OutState.Seats[GrFenrirSeat].Role = ESeatRole::Elect;
		OutState.Seats[GrFenrirSeat].DomainId = GrFenrirDomain;
		OutState.Seats[GrFenrirSeat].Runes[GrRuneType] = 1;
		return true;
	}

	/** Un grimorio.json de prueba con una afinidad (la de Fenrir) y un poder (Vuelo Planar); lo demás falta. */
	const TCHAR* const GrTestJson = TEXT(R"({
		"afinidades": {"freePassage": {"nombre": "Nombre de prueba", "frase": "Frase de prueba de la afinidad"}},
		"poderes": {"planarFlight": "Frase de prueba del poder"}
	})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireRowsTest, "Soulbound.Game.Presentation.Grimorio.FilasDeLosAsientos", GrimoireTestFlags)

bool FGrimoireRowsTest::RunTest(const FString& Parameters)
{
	// CA-HUD-3 (R-HUD-4): con un asiento Elegido de Fenrir con una runa del Dominio 5 y otro Eco, las filas dicen: para el primero, "Fenrir", su
	// afinidad, la runa D5 con cantidad 1 y el poder que le da (Vuelo Planar) marcado como activo, y el resto de runas sin cantidad; para el
	// segundo, Eco sin Elegido ni afinidad.
	FMatchState State;
	if (!GrMakeState(*this, State))
	{
		return false;
	}
	FGrimoireTexts Texts;
	Texts.AffinityNames.Add(EAffinity::FreePassage, TEXT("Nombre de prueba"));
	Texts.AffinityTexts.Add(EAffinity::FreePassage, TEXT("Frase de prueba de la afinidad"));
	const TArray<FGrimoireRow> Rows = Soulbound::Presentation::GrimoireRows(State, GrFenrirSeat, Texts, {FLinearColor::Red, FLinearColor::Green});
	if (!TestEqual(TEXT("Una fila por asiento"), Rows.Num(), 4))
	{
		return false;
	}

	const FGrimoireRow& Fenrir = Rows[GrFenrirSeat];
	TestEqual(TEXT("La fila es la del asiento"), Fenrir.Seat, GrFenrirSeat);
	TestTrue(TEXT("Es el humano"), Fenrir.bHuman);
	TestTrue(TEXT("Es Elegido"), Fenrir.bElect);
	TestEqual(TEXT("De Fenrir"), Fenrir.ElectName, FString(TEXT("Fenrir")));
	TestEqual(TEXT("En su Dominio"), Fenrir.DomainId, GrFenrirDomain);
	TestEqual(TEXT("Su afinidad, con el nombre de los textos"), Fenrir.AffinityName, FString(TEXT("Nombre de prueba")));
	TestEqual(TEXT("Y su frase"), Fenrir.AffinityText, FString(TEXT("Frase de prueba de la afinidad")));
	TestEqual(TEXT("Del color del asiento"), Fenrir.Color, FLinearColor::Red);
	if (TestEqual(TEXT("Seis runas"), Fenrir.Runes.Num(), 6))
	{
		for (int32 RuneType = 0; RuneType < Fenrir.Runes.Num(); ++RuneType)
		{
			TestEqual(FString::Printf(TEXT("La runa %d es la D%d"), RuneType, RuneType), Fenrir.Runes[RuneType].RuneType, RuneType);
			TestTrue(FString::Printf(TEXT("La D%d da un poder a un Elegido"), RuneType), Fenrir.Runes[RuneType].bHasPower);
			TestEqual(FString::Printf(TEXT("D%d: cantidad"), RuneType), Fenrir.Runes[RuneType].Count, RuneType == GrRuneType ? 1 : 0);
			TestEqual(FString::Printf(TEXT("D%d: activa solo la que tiene"), RuneType), Fenrir.Runes[RuneType].bActive, RuneType == GrRuneType);
		}
		TestEqual(TEXT("La D5 da Vuelo Planar"), Fenrir.Runes[GrRuneType].PowerName, FString(TEXT("Vuelo Planar")));
		TestEqual(TEXT("Según su mapa"), static_cast<int32>(Fenrir.Runes[GrRuneType].Power), static_cast<int32>(EPower::PlanarFlight));
	}
	TestEqual(TEXT("Sus poderes activos: Vuelo Planar"), Fenrir.ActivePowers, TArray<FString>{TEXT("Vuelo Planar")});

	const FGrimoireRow& Echo = Rows[GrEchoSeat];
	TestFalse(TEXT("El segundo no es el humano"), Echo.bHuman);
	TestFalse(TEXT("Es Eco"), Echo.bElect);
	TestTrue(TEXT("Sin Elegido"), Echo.ElectName.IsEmpty());
	TestTrue(TEXT("Sin afinidad"), Echo.AffinityName.IsEmpty() && Echo.AffinityText.IsEmpty());
	TestEqual(TEXT("Sin color en la lista, blanco"), Rows[2].Color, FLinearColor::White);
	if (TestEqual(TEXT("También seis runas"), Echo.Runes.Num(), 6))
	{
		TestFalse(TEXT("Que no le dan poderes de ningún mapa"), Echo.Runes.ContainsByPredicate([](const Soulbound::Presentation::FGrimoireRune& Rune) { return Rune.bHasPower; }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireTextsTest, "Soulbound.Game.Presentation.Grimorio.TextosDeDatos", GrimoireTestFlags)

bool FGrimoireTextsTest::RunTest(const FString& Parameters)
{
	// CA-HUD-4 (R-HUD-4): con un grimorio.json de prueba, la frase de una afinidad y la de un poder son las del archivo; lo que le falta, y
	// todo sin el archivo, se enseña con el nombre del enumerado y no falla.
	const FString TestFile = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("grimorio-prueba.json"));
	if (!TestTrue(TEXT("Se escribe el archivo de prueba"), FFileHelper::SaveStringToFile(FString(GrTestJson), *TestFile, FFileHelper::EEncodingOptions::ForceUTF8)))
	{
		return false;
	}
	const Soulbound::Presentation::FGrimoireTextsLoadResult Loaded = Soulbound::Presentation::LoadGrimoireTexts(TestFile);
	IFileManager::Get().Delete(*TestFile);
	TestEqual(TEXT("La frase de la afinidad es la del archivo"), Soulbound::Presentation::AffinityText(Loaded.Texts, EAffinity::FreePassage), FString(TEXT("Frase de prueba de la afinidad")));
	TestEqual(TEXT("Y su nombre"), Soulbound::Presentation::AffinityName(Loaded.Texts, EAffinity::FreePassage), FString(TEXT("Nombre de prueba")));
	TestEqual(TEXT("La del poder, también"), Soulbound::Presentation::PowerText(Loaded.Texts, EPower::PlanarFlight), FString(TEXT("Frase de prueba del poder")));
	TestFalse(TEXT("Al archivo de prueba le faltan claves, y lo dice"), Loaded.IsComplete());
	TestTrue(TEXT("Nombrando una que falta"), Loaded.Message.Contains(TEXT("duelAdvantage")));
	TestEqual(TEXT("Una afinidad que falta: el nombre del enumerado"), Soulbound::Presentation::AffinityName(Loaded.Texts, EAffinity::DuelAdvantage), FString(TEXT("DuelAdvantage")));
	TestEqual(TEXT("Un poder que falta: el nombre del enumerado"), Soulbound::Presentation::PowerText(Loaded.Texts, EPower::Haste), FString(TEXT("Haste")));
	const TArray<FString> PowerLines = Soulbound::Presentation::GrimoirePowerLines(Loaded.Texts);
	if (TestEqual(TEXT("Seis líneas de poderes"), PowerLines.Num(), 6))
	{
		TestEqual(TEXT("La de Vuelo Planar, con su frase"), PowerLines[static_cast<int32>(EPower::PlanarFlight)], FString(TEXT("Vuelo Planar: Frase de prueba del poder")));
	}

	// Sin el archivo: textos vacíos, el motivo, y las filas con el nombre del enumerado.
	const Soulbound::Presentation::FGrimoireTextsLoadResult Missing = Soulbound::Presentation::LoadGrimoireTexts(TEXT("no/existe/grimorio.json"));
	TestFalse(TEXT("Sin archivo: lo dice"), Missing.IsComplete());
	TestEqual(TEXT("Sin archivo: la afinidad con el nombre del enumerado"), Soulbound::Presentation::AffinityName(Missing.Texts, EAffinity::FreePassage), FString(TEXT("FreePassage")));
	TestEqual(TEXT("Y la frase del poder"), Soulbound::Presentation::PowerText(Missing.Texts, EPower::PlanarFlight), FString(TEXT("PlanarFlight")));
	FMatchState State;
	if (GrMakeState(*this, State))
	{
		const TArray<FGrimoireRow> Rows = Soulbound::Presentation::GrimoireRows(State, GrFenrirSeat, Missing.Texts, {});
		if (TestEqual(TEXT("Sin archivo, las filas salen igual"), Rows.Num(), 4))
		{
			TestEqual(TEXT("Con la afinidad del enumerado"), Rows[GrFenrirSeat].AffinityName, FString(TEXT("FreePassage")));
		}
	}

	// Un texto que no es JSON tampoco falla.
	TestFalse(TEXT("Un JSON roto: lo dice"), Soulbound::Presentation::ParseGrimoireTexts(TEXT("no es json")).IsComplete());

	// El archivo del repositorio está completo: una frase por afinidad y por poder, y un nombre por afinidad.
	const Soulbound::Presentation::FGrimoireTextsLoadResult Repository =
		Soulbound::Presentation::LoadGrimoireTexts(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::GrimoireRelativePath));
	TestTrue(FString::Printf(TEXT("grimorio.json del repositorio está completo (%s)"), *Repository.Message), Repository.IsComplete());
	TestEqual(TEXT("Seis afinidades con nombre"), Repository.Texts.AffinityNames.Num(), 6);
	TestEqual(TEXT("Seis afinidades con frase"), Repository.Texts.AffinityTexts.Num(), 6);
	TestEqual(TEXT("Seis poderes con frase"), Repository.Texts.PowerTexts.Num(), 6);
	return true;
}

#endif
