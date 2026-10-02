// Tests del cargador de los Elegidos (S-008): CA-ELE-1 y CA-ELE-10 (R-ELE-1, R-ELE-3) y el rechazo de StartMatch sin
// Elegidos válidos. Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Elects".

#include "Elects/ElectsLoader.h"
#include "Match/MatchStart.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TCHAR* PowerNames[] = {TEXT("duel"), TEXT("haste"), TEXT("freePassage"), TEXT("expel"), TEXT("planarFlight"), TEXT("deflection")};
	const TCHAR* AffinityNames[] = {TEXT("duelAdvantage"), TEXT("freePassage"), TEXT("rerollOnce"), TEXT("voidEscape"), TEXT("refuseOracle"), TEXT("safeRune")};

	using FMap = TArray<TArray<int32>>;

	/** El mapa del GDD 04 como matriz de EPower (fila: Elegido por Dominio; columna: runa por Dominio), el de elegidos.json. */
	FMap GddMap()
	{
		return {
			{1, 2, 3, 5, 4, 0},
			{0, 1, 2, 3, 5, 4},
			{4, 0, 1, 2, 3, 5},
			{5, 4, 0, 1, 2, 3},
			{3, 5, 4, 0, 1, 2},
			{2, 3, 5, 4, 0, 1},
		};
	}

	/** JSON con el esquema de elegidos.json a partir de una matriz de poderes (índices de EPower). */
	FString MakeJson(const FMap& Map)
	{
		TArray<FString> Rows;
		for (int32 Domain = 0; Domain < Map.Num(); ++Domain)
		{
			TArray<FString> Cells;
			for (int32 Rune = 0; Rune < Map[Domain].Num(); ++Rune)
			{
				Cells.Add(FString::Printf(TEXT("\"%d\": \"%s\""), Rune, PowerNames[Map[Domain][Rune]]));
			}
			Rows.Add(FString::Printf(TEXT("{\"domain\": %d, \"name\": \"Elegido %d\", \"affinity\": \"%s\", \"powers\": {%s}}"),
				Domain, Domain, AffinityNames[Domain], *FString::Join(Cells, TEXT(", "))));
		}
		return FString::Printf(TEXT("{\"elects\": [%s]}"), *FString::Join(Rows, TEXT(", ")));
	}

	void ExpectRejected(FAutomationTestBase& Test, const FString& Context, const FString& Json, EElectsError Expected)
	{
		const FElectsLoadResult Result = Soulbound::Rules::LoadElectsFromJsonString(Json);
		Test.TestEqual(Context + TEXT(": error"), static_cast<int32>(Result.Error), static_cast<int32>(Expected));
		Test.TestFalse(Context + TEXT(": con un motivo legible"), Result.Message.IsEmpty());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectsFileLoadsTest,
	"Soulbound.Core.Elects.ElArchivoDeDatosCarga",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FElectsFileLoadsTest::RunTest(const FString& Parameters)
{
	// CA-ELE-1 y CA-ELE-10 (R-ELE-1, R-ELE-3): dado elegidos.json, cuando se carga y valida, hay seis Elegidos con el
	// nombre y la afinidad de la tabla del GDD, cada fila y cada columna del mapa contienen los seis poderes una vez y la
	// diagonal es Celeridad.
	const FElectsLoadResult Result = Soulbound::Rules::LoadElectsFromFile(Soulbound::Tests::ElectsPath());
	if (!TestTrue(FString::Printf(TEXT("El archivo carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Seis Elegidos"), Result.Table.Elects.Num(), 6))
	{
		return false;
	}
	const TCHAR* Names[] = {TEXT("Shuten-dōji"), TEXT("Fenrir"), TEXT("Apep"), TEXT("Mictlantecuhtli"), TEXT("Tifón"), TEXT("Koschei")};
	const EAffinity Affinities[] = {EAffinity::DuelAdvantage, EAffinity::FreePassage, EAffinity::RerollOnce, EAffinity::VoidEscape, EAffinity::RefuseOracle, EAffinity::SafeRune};
	const FMap Gdd = GddMap();
	for (int32 Domain = 0; Domain < 6; ++Domain)
	{
		const FElectDef& Elect = Result.Table.Elects[Domain];
		TestEqual(FString::Printf(TEXT("Dominio %d: nombre"), Domain), Elect.Name, FString(Names[Domain]));
		TestEqual(FString::Printf(TEXT("Dominio %d: afinidad"), Domain), static_cast<int32>(Elect.Affinity), static_cast<int32>(Affinities[Domain]));
		if (!TestEqual(FString::Printf(TEXT("Dominio %d: seis runas"), Domain), Elect.RuneToPower.Num(), 6))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("Dominio %d: la runa propia da Celeridad"), Domain), static_cast<int32>(Elect.RuneToPower[Domain]), static_cast<int32>(EPower::Haste));
		for (int32 Rune = 0; Rune < 6; ++Rune)
		{
			TestEqual(FString::Printf(TEXT("Dominio %d, runa %d: el mapa del GDD"), Domain, Rune), static_cast<int32>(Elect.RuneToPower[Rune]), Gdd[Domain][Rune]);
		}
	}
	TestTrue(TEXT("La tabla cargada pasa ValidateElects"), Soulbound::Rules::ValidateElects(Result.Table).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElectsRejectedTest,
	"Soulbound.Core.Elects.UnArchivoQueIncumpleSeRechazaConMotivo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FElectsRejectedTest::RunTest(const FString& Parameters)
{
	// CA-ELE-1 y CA-ELE-10 (R-ELE-3): un archivo que incumpla el cuadrado latino o la diagonal se rechaza con motivo, y
	// cada incumplimiento por su regla. Se parte del mapa válido y se cambia una sola cosa.
	TestTrue(TEXT("El mapa del GDD pasa"), Soulbound::Rules::LoadElectsFromJsonString(MakeJson(GddMap())).IsSuccess());

	// Una fila con un poder repetido.
	FMap RepeatedInRow = GddMap();
	RepeatedInRow[2][3] = RepeatedInRow[2][0];
	ExpectRejected(*this, TEXT("Fila con un poder repetido"), MakeJson(RepeatedInRow), EElectsError::NotLatinSquare);

	// Dos celdas de una fila intercambiadas: la fila sigue completa, pero las columnas repiten poder.
	FMap RepeatedInColumn = GddMap();
	RepeatedInColumn[0].Swap(1, 2);
	ExpectRejected(*this, TEXT("Columna con un poder repetido"), MakeJson(RepeatedInColumn), EElectsError::NotLatinSquare);

	// Dos poderes intercambiados en todo el mapa: sigue siendo un cuadrado latino, pero la diagonal ya no es Celeridad.
	FMap WrongDiagonal = GddMap();
	for (TArray<int32>& Row : WrongDiagonal)
	{
		for (int32& Power : Row)
		{
			Power = Power == static_cast<int32>(EPower::Haste) ? static_cast<int32>(EPower::Duel) : Power == static_cast<int32>(EPower::Duel) ? static_cast<int32>(EPower::Haste) : Power;
		}
	}
	ExpectRejected(*this, TEXT("Diagonal que no es Celeridad"), MakeJson(WrongDiagonal), EElectsError::DiagonalNotHaste);

	// Datos mal formados: cada uno es MalformedData.
	ExpectRejected(*this, TEXT("No es JSON"), TEXT("esto no es json"), EElectsError::MalformedData);
	ExpectRejected(*this, TEXT("Poder desconocido"), MakeJson(GddMap()).Replace(TEXT("\"expel\""), TEXT("\"teleport\"")), EElectsError::MalformedData);
	ExpectRejected(*this, TEXT("Afinidad desconocida"), MakeJson(GddMap()).Replace(TEXT("\"rerollOnce\""), TEXT("\"flight\"")), EElectsError::MalformedData);
	ExpectRejected(*this, TEXT("Falta una runa del mapa"), MakeJson(GddMap()).Replace(TEXT(", \"5\": \"duel\"}"), TEXT("}")), EElectsError::MalformedData);
	ExpectRejected(*this, TEXT("Dominio repetido"), MakeJson(GddMap()).Replace(TEXT("\"domain\": 5"), TEXT("\"domain\": 4")), EElectsError::MalformedData);
	ExpectRejected(*this, TEXT("Falta el nombre"), MakeJson(GddMap()).Replace(TEXT("\"name\": \"Elegido 3\", "), TEXT("")), EElectsError::MalformedData);
	FMap FiveRows = GddMap();
	FiveRows.RemoveAt(5);
	ExpectRejected(*this, TEXT("Cinco Elegidos"), MakeJson(FiveRows), EElectsError::MalformedData);

	const FElectsLoadResult Missing = Soulbound::Rules::LoadElectsFromFile(TEXT("Z:/no/existe/elegidos.json"));
	TestEqual(TEXT("Archivo que no existe: error"), static_cast<int32>(Missing.Error), static_cast<int32>(EElectsError::FileNotFound));
	TestFalse(TEXT("Archivo que no existe: con un motivo legible"), Missing.Message.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
