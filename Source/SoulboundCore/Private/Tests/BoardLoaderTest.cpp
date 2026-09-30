// Tests de carga y validación del tablero desde datos (S-002): CA-TAB-1 (R-TAB-1), CA-TAB-3 (R-TAB-3),
// CA-TAB-4 (R-TAB-4, 4c) y un test por cada regla que rechaza un tablero mal formado.
// Los inválidos se construyen en el test a partir del tablero de pruebas válido, cambiando una sola cosa.
// Se lanzan sin editor con Scripts/test.ps1 (comandlet con -nullrhi). Filtro: "Soulbound.Core.Board".

#include "Board/BoardLoader.h"
#include "Board/BoardState.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** El archivo real del tablero de pruebas, relativo al directorio de contenido del proyecto. */
	FString TestBoardPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_pruebas.json"));
	}

	/** Carga el tablero de pruebas; si no es válido, falla el test y devuelve false para que no siga con un tablero vacío. */
	bool LoadTestBoard(FAutomationTestBase& Test, FBoardState& OutBoard)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(TestBoardPath());
		OutBoard = Result.Board;
		return Test.TestTrue(FString::Printf(TEXT("El tablero de pruebas carga (%s)"), *Result.Message), Result.IsSuccess());
	}

	/** Lee el JSON del tablero de pruebas por su cuenta, sin pasar por el lector, para compararlo con lo cargado. */
	TSharedPtr<FJsonObject> ReadTestBoardJson()
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *TestBoardPath());
		TSharedPtr<FJsonObject> Root;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
		return Root;
	}

	FString ToJsonString(const TSharedPtr<FJsonObject>& Root)
	{
		FString Text;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
		return Text;
	}

	/** Valida y comprueba que el rechazo es por la regla esperada y trae un motivo legible. */
	void ExpectRejected(FAutomationTestBase& Test, const FString& What, const FBoardLoadResult& Result, EBoardError Expected)
	{
		Test.TestFalse(What + TEXT(": se rechaza"), Result.IsSuccess());
		Test.TestEqual(What + TEXT(": por la regla esperada"), static_cast<int32>(Result.Error), static_cast<int32>(Expected));
		Test.TestFalse(What + TEXT(": con un motivo legible"), Result.Message.IsEmpty());
	}

	void ExpectInvalid(FAutomationTestBase& Test, const FString& What, const FBoardState& Board, EBoardError Expected)
	{
		ExpectRejected(Test, What, Soulbound::Rules::ValidateBoard(Board), Expected);
	}

	int32 CountTiles(const FBoardState& Board, int32 DomainId, ETileType Type)
	{
		return Board.Tiles.FilterByPredicate([&](const FTileState& T) { return T.DomainId == DomainId && T.Type == Type; }).Num();
	}

	/** Si alguna casilla del Dominio tiene una conexión con una casilla que cumple Pred. */
	template <typename PredType>
	bool DomainTouches(const FBoardState& Board, int32 DomainId, PredType Pred)
	{
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.DomainId != DomainId)
			{
				continue;
			}
			for (const int32 Other : Tile.Neighbours)
			{
				if (Pred(Board.Tiles[Other]))
				{
					return true;
				}
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLoadsGraphFromFileTest,
	"Soulbound.Core.Board.Loader.GrafoDesdeElArchivo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardLoadsGraphFromFileTest::RunTest(const FString& Parameters)
{
	// CA-TAB-1 (R-TAB-1): el grafo cargado tiene las casillas y conexiones que describe el archivo.
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Json = ReadTestBoardJson();
	if (!TestTrue(TEXT("El test lee el archivo por su cuenta"), Json.IsValid()))
	{
		return false;
	}

	// Copias, no referencias: el final del test sustituye estos arrays en Json y una referencia quedaría colgando.
	const TArray<TSharedPtr<FJsonValue>> JsonTiles = Json->GetArrayField(TEXT("tiles"));
	const TArray<TSharedPtr<FJsonValue>> JsonEdges = Json->GetArrayField(TEXT("edges"));
	TestEqual(TEXT("Tantas casillas como en el archivo"), Board.Tiles.Num(), JsonTiles.Num());

	const TMap<FString, ETileType> Types = {
		{TEXT("normal"), ETileType::Normal}, {TEXT("altar"), ETileType::Altar}, {TEXT("niche"), ETileType::Niche},
		{TEXT("powerSource"), ETileType::PowerSource}, {TEXT("powerNode"), ETileType::PowerNode}};
	for (const TSharedPtr<FJsonValue>& Value : JsonTiles)
	{
		const TSharedPtr<FJsonObject>& Tile = Value->AsObject();
		const int32 Id = Tile->GetIntegerField(TEXT("id"));
		int32 Domain = INDEX_NONE;
		Tile->TryGetNumberField(TEXT("domain"), Domain);
		if (TestTrue(FString::Printf(TEXT("La casilla %d existe"), Id), Board.IsValidTile(Id)))
		{
			TestEqual(FString::Printf(TEXT("Tipo de la casilla %d"), Id), static_cast<int32>(Board.Tiles[Id].Type), static_cast<int32>(Types.FindRef(Tile->GetStringField(TEXT("type")))));
			TestEqual(FString::Printf(TEXT("Dominio de la casilla %d"), Id), Board.Tiles[Id].DomainId, Domain);
		}
	}

	int32 NeighbourEntries = 0;
	for (const FTileState& Tile : Board.Tiles)
	{
		NeighbourEntries += Tile.Neighbours.Num();
	}
	TestEqual(TEXT("Tantas conexiones como en el archivo (cada una aparece en sus dos extremos)"), NeighbourEntries, JsonEdges.Num() * 2);
	for (const TSharedPtr<FJsonValue>& Value : JsonEdges)
	{
		const TArray<TSharedPtr<FJsonValue>>& Pair = Value->AsArray();
		const int32 A = static_cast<int32>(Pair[0]->AsNumber());
		const int32 B = static_cast<int32>(Pair[1]->AsNumber());
		TestTrue(FString::Printf(TEXT("Conexión %d-%d en los dos sentidos"), A, B),
			Board.IsValidTile(A) && Board.IsValidTile(B) && Board.Tiles[A].Neighbours.Contains(B) && Board.Tiles[B].Neighbours.Contains(A));
	}

	// Los parámetros del tablero también salen del archivo.
	const TSharedPtr<FJsonObject>& Params = Json->GetObjectField(TEXT("parameters"));
	const TSharedPtr<FJsonObject>& Effects = Params->GetObjectField(TEXT("powerNodeEffects"));
	TestEqual(TEXT("Fuentes por Dominio"), Board.Params.PowerSourcesPerDomain, Params->GetIntegerField(TEXT("powerSourcesPerDomain")));
	TestEqual(TEXT("Nodos de Duelo"), Board.Params.DuelNodeCount, Effects->GetIntegerField(TEXT("duel")));
	TestEqual(TEXT("Nodos del Oráculo"), Board.Params.OracleNodeCount, Effects->GetIntegerField(TEXT("oracle")));
	TestEqual(TEXT("Nodos Planares"), Board.Params.PlanarNodeCount, Effects->GetIntegerField(TEXT("planar")));
	TestEqual(TEXT("Máximo de casillas selladas"), Board.Params.MaxSealedTiles, Params->GetIntegerField(TEXT("maxSealedTiles")));
	TestEqual(TEXT("Runas en reserva por Fuente"), Board.Params.RuneReservePerSource, Params->GetIntegerField(TEXT("runeReservePerSource")));
	TestEqual(TEXT("Nombre provisional del Dominio 0"), Board.Domains.IsValidIndex(0) ? Board.Domains[0].Name : FString(),
		Json->GetArrayField(TEXT("domains"))[0]->AsObject()->GetStringField(TEXT("name")));

	// Ningún valor de geometría procede del código: una casilla y una conexión más en los datos son una más en el grafo.
	TArray<TSharedPtr<FJsonValue>> MoreTiles = JsonTiles;
	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetNumberField(TEXT("id"), JsonTiles.Num());
	Extra->SetStringField(TEXT("type"), TEXT("normal"));
	Extra->SetNumberField(TEXT("domain"), 3);
	MoreTiles.Add(MakeShared<FJsonValueObject>(Extra));
	TArray<TSharedPtr<FJsonValue>> MoreEdges = JsonEdges;
	MoreEdges.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(18), MakeShared<FJsonValueNumber>(JsonTiles.Num())}));
	Json->SetArrayField(TEXT("tiles"), MoreTiles);
	Json->SetArrayField(TEXT("edges"), MoreEdges);
	const FBoardLoadResult Grown = Soulbound::Rules::LoadBoardFromJsonString(ToJsonString(Json));
	TestTrue(FString::Printf(TEXT("El tablero con una casilla más carga (%s)"), *Grown.Message), Grown.IsSuccess());
	TestEqual(TEXT("Y tiene una casilla más"), Grown.Board.Tiles.Num(), JsonTiles.Num() + 1);
	TestTrue(TEXT("Y la conexión nueva"), Grown.Board.IsValidTile(JsonTiles.Num()) && Grown.Board.Tiles[18].Neighbours.Contains(JsonTiles.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardTestBoardIsValidTest,
	"Soulbound.Core.Board.Validation.TableroDePruebasValido",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardTestBoardIsValidTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	const FBoardLoadResult Validation = Soulbound::Rules::ValidateBoard(Board);
	TestTrue(FString::Printf(TEXT("El tablero de pruebas pasa la validación (%s)"), *Validation.Message), Validation.IsSuccess());

	// CA-TAB-3 (R-TAB-3): seis Dominios, cada uno con al menos una conexión al Altar y a cada uno de sus dos vecinos.
	// Se comprueba aquí por separado de ValidateBoard, para que el test no dependa de la implementación que prueba.
	if (!TestEqual(TEXT("Seis Dominios"), Board.Domains.Num(), Soulbound::Board::DomainCount))
	{
		return false;
	}
	for (int32 D = 0; D < Soulbound::Board::DomainCount; ++D)
	{
		const int32 Next = (D + 1) % Soulbound::Board::DomainCount;
		const int32 Prev = (D + Soulbound::Board::DomainCount - 1) % Soulbound::Board::DomainCount;
		TestEqual(FString::Printf(TEXT("El Dominio %d tiene el identificador de su posición en el anillo"), D), Board.Domains[D].DomainId, D);
		TestTrue(FString::Printf(TEXT("El Dominio %d conecta con el Altar"), D), DomainTouches(Board, D, [](const FTileState& T) { return T.Type == ETileType::Altar; }));
		TestTrue(FString::Printf(TEXT("El Dominio %d conecta con el %d"), D, Next), DomainTouches(Board, D, [Next](const FTileState& T) { return T.DomainId == Next; }));
		TestTrue(FString::Printf(TEXT("El Dominio %d conecta con el %d"), D, Prev), DomainTouches(Board, D, [Prev](const FTileState& T) { return T.DomainId == Prev; }));

		// CA-TAB-4 (R-TAB-4, 4c): exactamente un Nicho con una sola conexión y el número de Fuentes de los datos.
		TestEqual(FString::Printf(TEXT("El Dominio %d tiene un Nicho"), D), CountTiles(Board, D, ETileType::Niche), 1);
		const int32 Niche = Board.FindNiche(D);
		TestTrue(FString::Printf(TEXT("El Nicho del Dominio %d tiene una sola conexión"), D), Board.IsValidTile(Niche) && Board.Tiles[Niche].Neighbours.Num() == 1);
		TestEqual(FString::Printf(TEXT("El Dominio %d tiene las Fuentes de los datos"), D), CountTiles(Board, D, ETileType::PowerSource), Board.Params.PowerSourcesPerDomain);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsAltarCountTest,
	"Soulbound.Core.Board.Validation.RechazaSinAltarOConDos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsAltarCountTest::RunTest(const FString& Parameters)
{
	// R-TAB-2: una única casilla Altar.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState NoAltar = Valid;
	NoAltar.Tiles[0].Type = ETileType::Normal;
	ExpectInvalid(*this, TEXT("Sin Altar"), NoAltar, EBoardError::AltarCount);

	FBoardState TwoAltars = Valid;
	TwoAltars.Tiles[1].Type = ETileType::Altar;
	TwoAltars.Tiles[1].DomainId = INDEX_NONE;
	ExpectInvalid(*this, TEXT("Con dos Altares"), TwoAltars, EBoardError::AltarCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsDomainCountTest,
	"Soulbound.Core.Board.Validation.RechazaSiNoHaySeisDominios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsDomainCountTest::RunTest(const FString& Parameters)
{
	// R-TAB-3: exactamente seis Dominios, con identificadores de 0 a 5.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState Five = Valid;
	Five.Domains.RemoveAt(5);
	ExpectInvalid(*this, TEXT("Cinco Dominios"), Five, EBoardError::DomainCount);

	FBoardState Seven = Valid;
	FDomainState& Extra = Seven.Domains.AddDefaulted_GetRef();
	Extra.DomainId = 6;
	ExpectInvalid(*this, TEXT("Siete Dominios"), Seven, EBoardError::DomainCount);

	FBoardState Repeated = Valid;
	Repeated.Domains[5].DomainId = 4;
	ExpectInvalid(*this, TEXT("Seis Dominios con un identificador repetido"), Repeated, EBoardError::DomainCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsTileWithoutDomainTest,
	"Soulbound.Core.Board.Validation.RechazaCasillaSinDominio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsTileWithoutDomainTest::RunTest(const FString& Parameters)
{
	// R-TAB-3 y decisión del autor (25/09/2026): toda casilla que no es el Altar pertenece a un Dominio.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState NoDomain = Valid;
	NoDomain.Tiles[18].DomainId = INDEX_NONE;
	ExpectInvalid(*this, TEXT("Casilla normal sin Dominio"), NoDomain, EBoardError::TileWithoutDomain);

	FBoardState UnknownDomain = Valid;
	UnknownDomain.Tiles[18].DomainId = 7;
	ExpectInvalid(*this, TEXT("Casilla en un Dominio que no existe"), UnknownDomain, EBoardError::TileWithoutDomain);

	FBoardState AltarInDomain = Valid;
	AltarInDomain.Tiles[0].DomainId = 0;
	ExpectInvalid(*this, TEXT("El Altar dentro de un Dominio"), AltarInDomain, EBoardError::TileWithoutDomain);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsDomainWithoutAltarTest,
	"Soulbound.Core.Board.Validation.RechazaDominioSinConexionAlAltar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsDomainWithoutAltarTest::RunTest(const FString& Parameters)
{
	// R-TAB-3: cada Dominio tiene al menos una conexión al Altar. El Dominio 4 solo la tiene por la casilla 21.
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	Board.Disconnect(0, 21);
	ExpectInvalid(*this, TEXT("Dominio 4 sin conexión al Altar"), Board, EBoardError::DomainNotConnectedToAltar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsBrokenRingTest,
	"Soulbound.Core.Board.Validation.RechazaAnilloRoto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsBrokenRingTest::RunTest(const FString& Parameters)
{
	// R-TAB-3: cada Dominio conecta con sus dos vecinos. Los Dominios 3 y 4 solo se tocan por la conexión 17-23.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState Broken = Valid;
	Broken.Disconnect(17, 23);
	ExpectInvalid(*this, TEXT("Sin conexión entre los Dominios 3 y 4"), Broken, EBoardError::BrokenRing);

	// El cierre del anillo (Dominio 5 con el 0) también cuenta.
	FBoardState Unclosed = Valid;
	Unclosed.Disconnect(29, 2);
	ExpectInvalid(*this, TEXT("Sin conexión entre los Dominios 5 y 0"), Unclosed, EBoardError::BrokenRing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsNicheCountTest,
	"Soulbound.Core.Board.Validation.RechazaDominioSinUnNicho",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsNicheCountTest::RunTest(const FString& Parameters)
{
	// R-TAB-4: cada Dominio contiene exactamente un Nicho.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState NoNiche = Valid;
	NoNiche.Tiles[3].Type = ETileType::Normal;
	ExpectInvalid(*this, TEXT("Dominio 0 sin Nicho"), NoNiche, EBoardError::NicheCount);

	// La 18 pasa a ser Nicho: el Dominio 3 tendría dos (el suyo es la 20).
	FBoardState TwoNiches = Valid;
	TwoNiches.Tiles[18].Type = ETileType::Niche;
	ExpectInvalid(*this, TEXT("Dominio 3 con dos Nichos"), TwoNiches, EBoardError::NicheCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsNicheWithTwoEdgesTest,
	"Soulbound.Core.Board.Validation.RechazaNichoConDosConexiones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsNicheWithTwoEdgesTest::RunTest(const FString& Parameters)
{
	// R-TAB-4c: el Nicho tiene una sola conexión, al final de un ramal.
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	Board.Connect(3, 1);
	ExpectInvalid(*this, TEXT("Nicho del Dominio 0 con dos conexiones"), Board, EBoardError::NicheNotDeadEnd);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsPowerSourceCountTest,
	"Soulbound.Core.Board.Validation.RechazaFuentesQueNoCoincidenConLosDatos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsPowerSourceCountTest::RunTest(const FString& Parameters)
{
	// R-TAB-4: cada Dominio tiene el número de Fuentes de Poder definido en datos.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState MissingSource = Valid;
	MissingSource.Tiles[4].Type = ETileType::Normal;
	ExpectInvalid(*this, TEXT("Dominio 0 sin Fuente con una por Dominio en datos"), MissingSource, EBoardError::PowerSourceCount);

	FBoardState DataAsksTwo = Valid;
	DataAsksTwo.Params.PowerSourcesPerDomain = 2;
	ExpectInvalid(*this, TEXT("Datos que piden dos Fuentes por Dominio"), DataAsksTwo, EBoardError::PowerSourceCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsPowerNodeCountTest,
	"Soulbound.Core.Board.Validation.RechazaNodosQueNoCoincidenConLosDatos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsPowerNodeCountTest::RunTest(const FString& Parameters)
{
	// R-TAB-5: el número de Nodos de Poder es el de los datos; si no coincide con las cantidades por efecto, no se puede sortear.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState MoreInData = Valid;
	MoreInData.Params.DuelNodeCount += 1;
	ExpectInvalid(*this, TEXT("Datos que piden un Nodo más de los que hay"), MoreInData, EBoardError::PowerNodeCount);

	FBoardState MoreOnBoard = Valid;
	MoreOnBoard.Tiles[18].Type = ETileType::PowerNode;
	ExpectInvalid(*this, TEXT("Un Nodo en el tablero que los datos no piden"), MoreOnBoard, EBoardError::PowerNodeCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsInvalidGraphTest,
	"Soulbound.Core.Board.Validation.RechazaGrafoIncoherente",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsInvalidGraphTest::RunTest(const FString& Parameters)
{
	// R-TAB-1: el tablero es un grafo de casillas y conexiones sin dirección.
	FBoardState Valid;
	if (!LoadTestBoard(*this, Valid))
	{
		return false;
	}

	FBoardState Missing = Valid;
	Missing.Tiles[1].Neighbours.Add(99);
	ExpectInvalid(*this, TEXT("Conexión con una casilla que no existe"), Missing, EBoardError::InvalidGraph);

	FBoardState OneWay = Valid;
	OneWay.Tiles[1].Neighbours.Add(6);
	ExpectInvalid(*this, TEXT("Conexión en un solo sentido"), OneWay, EBoardError::InvalidGraph);

	FBoardState SelfLoop = Valid;
	SelfLoop.Tiles[1].Neighbours.Add(1);
	ExpectInvalid(*this, TEXT("Conexión de una casilla consigo misma"), SelfLoop, EBoardError::InvalidGraph);

	FBoardState WrongId = Valid;
	WrongId.Tiles[5].TileId = 7;
	ExpectInvalid(*this, TEXT("Identificador que no coincide con la posición"), WrongId, EBoardError::InvalidGraph);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardRejectsMalformedFileTest,
	"Soulbound.Core.Board.Loader.RechazaArchivoMalFormado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardRejectsMalformedFileTest::RunTest(const FString& Parameters)
{
	// Un tablero se lee entero o se rechaza entero, con un motivo legible.
	ExpectRejected(*this, TEXT("Archivo inexistente"), Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/no_existe.json"))), EBoardError::FileNotFound);
	ExpectRejected(*this, TEXT("Texto que no es JSON"), Soulbound::Rules::LoadBoardFromJsonString(TEXT("esto no es un tablero")), EBoardError::MalformedData);

	auto Mutated = [](TFunctionRef<void(FJsonObject&)> Change)
	{
		const TSharedPtr<FJsonObject> Json = ReadTestBoardJson();
		Change(*Json);
		return Soulbound::Rules::LoadBoardFromJsonString(ToJsonString(Json));
	};

	ExpectRejected(*this, TEXT("Sin bloque de parámetros"), Mutated([](FJsonObject& J) { J.RemoveField(TEXT("parameters")); }), EBoardError::MalformedData);
	ExpectRejected(*this, TEXT("Parámetro negativo"), Mutated([](FJsonObject& J)
	{
		J.GetObjectField(TEXT("parameters"))->SetNumberField(TEXT("maxSealedTiles"), -1);
	}), EBoardError::MalformedData);
	ExpectRejected(*this, TEXT("Tipo de casilla desconocido"), Mutated([](FJsonObject& J)
	{
		J.GetArrayField(TEXT("tiles"))[5]->AsObject()->SetStringField(TEXT("type"), TEXT("lava"));
	}), EBoardError::MalformedData);
	ExpectRejected(*this, TEXT("Conexión que no es un par"), Mutated([](FJsonObject& J)
	{
		TArray<TSharedPtr<FJsonValue>> Edges = J.GetArrayField(TEXT("edges"));
		Edges.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(1)}));
		J.SetArrayField(TEXT("edges"), Edges);
	}), EBoardError::MalformedData);
	ExpectRejected(*this, TEXT("Conexión con una casilla que no existe"), Mutated([](FJsonObject& J)
	{
		TArray<TSharedPtr<FJsonValue>> Edges = J.GetArrayField(TEXT("edges"));
		Edges.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(1), MakeShared<FJsonValueNumber>(99)}));
		J.SetArrayField(TEXT("edges"), Edges);
	}), EBoardError::InvalidGraph);
	ExpectRejected(*this, TEXT("Identificador de casilla repetido"), Mutated([](FJsonObject& J)
	{
		J.GetArrayField(TEXT("tiles"))[5]->AsObject()->SetNumberField(TEXT("id"), 4);
	}), EBoardError::InvalidGraph);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLoadsLayoutPositionsTest,
	"Soulbound.Core.Board.Loader.PosicionesDesdeElArchivo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardLoadsLayoutPositionsTest::RunTest(const FString& Parameters)
{
	// CA de S-005 "Posiciones desde el archivo" (R-TAB-1). Con posiciones: tablero_15.json trae el bloque
	// `editor.positions` (el tablero de pruebas no lo tiene; decisión del autor el 30/09/2026). Se compara con
	// lo que dice el JSON leído por el propio test, sin pasar por el lector.
	const FString Path15 = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_15.json"));
	FString Text;
	if (!TestTrue(TEXT("El test lee tablero_15.json"), FFileHelper::LoadFileToString(Text, *Path15)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
	if (!TestTrue(TEXT("tablero_15.json es JSON"), Root.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Positions = Root->GetObjectField(TEXT("editor"))->GetObjectField(TEXT("positions"));

	const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromJsonString(Text);
	if (!TestTrue(FString::Printf(TEXT("tablero_15.json carga (%s)"), *Result.Message), Result.IsSuccess()))
	{
		return false;
	}
	for (const FTileState& Tile : Result.Board.Tiles)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (!Positions->TryGetArrayField(FString::FromInt(Tile.TileId), Pair) || Pair->Num() != 2)
		{
			AddError(FString::Printf(TEXT("El archivo no tiene posición para la casilla %d"), Tile.TileId));
			continue;
		}
		TestTrue(FString::Printf(TEXT("La casilla %d tiene posición"), Tile.TileId), Tile.bHasLayoutPosition);
		TestEqual(FString::Printf(TEXT("X de la casilla %d"), Tile.TileId), Tile.LayoutPosition.X, (*Pair)[0]->AsNumber());
		TestEqual(FString::Printf(TEXT("Y de la casilla %d"), Tile.TileId), Tile.LayoutPosition.Y, (*Pair)[1]->AsNumber());
	}

	// Sin bloque: el tablero de pruebas no lo tiene, carga igual y las posiciones quedan sin definir.
	FBoardState Plain;
	if (!LoadTestBoard(*this, Plain))
	{
		return false;
	}
	for (const FTileState& Tile : Plain.Tiles)
	{
		TestFalse(FString::Printf(TEXT("La casilla %d del tablero de pruebas no tiene posición"), Tile.TileId), Tile.bHasLayoutPosition);
	}

	// Bloque malformado: se rechaza entero, no se ignora en silencio.
	const TSharedPtr<FJsonObject> Bad = ReadTestBoardJson();
	TSharedPtr<FJsonObject> Editor = MakeShared<FJsonObject>();
	Editor->SetStringField(TEXT("positions"), TEXT("no es un objeto"));
	Bad->SetObjectField(TEXT("editor"), Editor);
	ExpectRejected(*this, TEXT("Bloque editor.positions malformado"), Soulbound::Rules::LoadBoardFromJsonString(ToJsonString(Bad)), EBoardError::MalformedData);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
