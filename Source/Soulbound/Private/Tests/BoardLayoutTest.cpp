// Tests de la colocación del tablero en la mesa (S-019, glosario propuesto "BoardLayout"): dónde cae cada losa, cada cuña,
// cada estructura y cada tramo de sendero, como función pura del tablero y de los parámetros. Sin mundo, sin editor y sin
// assets. Cada test cita el criterio (CA-VIS-n) y las reglas (R-VIS-n) que cubre. Filtro: "Soulbound.Game.Presentation.Tablero3D".

#include "../../Presentation/BoardLayout.h"
#include "../../Presentation/BoardViewActor.h"
#include "Board/BoardLoader.h"
#include "Board/BoardState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags LayoutTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Carga Content/Data/Boards/tablero_35.json; falso, con el fallo anotado, si no se puede. */
	bool LoadBoard35(FAutomationTestBase& Test, FBoardState& OutBoard)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_35.json")));
		if (!Test.TestTrue(FString::Printf(TEXT("tablero_35.json se carga (%s)"), *Result.Message), Result.IsSuccess()))
		{
			return false;
		}
		OutBoard = Result.Board;
		return true;
	}

	/** Ángulo de un vector del plano, en grados. */
	float DirectionAngleOf(const FVector2D& Vector)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Vector.Y, Vector.X));
	}

	/** Diferencia entre dos ángulos, llevada a (-180, 180]. */
	float AngleBetween(float A, float B)
	{
		return FMath::UnwindDegrees(A - B);
	}

	/** Media circular de las direcciones desde el Altar de las casillas del Dominio, en grados. */
	float MeanDomainAngle(const FBoardState& Board, const FBoardLayout& Layout, int32 DomainId)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.DomainId == DomainId)
			{
				Sum += FVector2D(Layout.Tiles[Tile.TileId].Location).GetSafeNormal();
			}
		}
		return DirectionAngleOf(Sum);
	}

	/** Mitad del tamaño de la pieza en la dirección de una conexión: la apotema en el Altar, media losa en las demás. */
	float HalfSize(const FBoardState& Board, int32 TileId, const FBoardLayoutParams& Params)
	{
		return Board.IsAltar(TileId) ? Params.AltarApothem : Params.SlabHalfSize;
	}

	/** Da a cada Nodo de Poder un efecto, rotando entre los tres, para que el tablero cargado tenga estructuras de Nodo. */
	void AssignNodeEffects(FBoardState& Board)
	{
		const EPowerNodeEffect Effects[] = {EPowerNodeEffect::Planar, EPowerNodeEffect::Duel, EPowerNodeEffect::Oracle};
		int32 Next = 0;
		for (FTileState& Tile : Board.Tiles)
		{
			if (Tile.Type == ETileType::PowerNode)
			{
				Tile.PowerNodeEffect = Effects[Next++ % UE_ARRAY_COUNT(Effects)];
			}
		}
	}

	bool IsSpecial(const FTileState& Tile)
	{
		return Tile.Type == ETileType::Niche || Tile.Type == ETileType::PowerSource || Tile.Type == ETileType::PowerNode;
	}
}

// CA-VIS-1 (R-VIS-1, R-VIS-2): con tablero_35.json, toda losa dentro del radio de mesa, el Altar en el centro y cada casilla
// en el sector de su Dominio (a menos de medio sector, 30°, del giro de su cuña).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutTableTest, "Soulbound.Game.Presentation.Tablero3D.CasillasEnLaMesaYEnSuSector", LayoutTestFlags)
bool FBoardLayoutTableTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	const FBoardLayoutParams Params;
	const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
	if (!TestEqual(TEXT("Una colocación por casilla"), Layout.Tiles.Num(), Board.Tiles.Num()) || !TestEqual(TEXT("Una cuña por Dominio"), Layout.Wedges.Num(), Board.Domains.Num()))
	{
		return false;
	}
	TestTrue(TEXT("El tablero de 35 es radial: sin aviso"), Layout.bRadial);
	TestTrue(TEXT("Escala positiva"), Layout.Scale > 0.f);
	TestTrue(TEXT("El Altar, en el centro de la mesa"), Layout.Tiles[Board.FindAltar()].Location.IsNearlyZero(0.01));

	float Farthest = 0.f;
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Tile.Type == ETileType::Altar)
		{
			continue;
		}
		const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		const float Edge = FVector2D(Placed.Location).Size() + Params.SlabHalfSize;
		Farthest = FMath::Max(Farthest, Edge);
		TestTrue(FString::Printf(TEXT("La losa de la casilla %d cae dentro del radio de mesa (%.2f de %.2f)"), Tile.TileId, Edge, Params.TableRadius), Edge <= Params.TableRadius + 0.01f);
		TestTrue(FString::Printf(TEXT("La losa de la casilla %d no pisa el Altar"), Tile.TileId), FVector2D(Placed.Location).Size() - Params.SlabHalfSize >= Params.AltarApothem - 0.01f);
		const float Offset = AngleBetween(DirectionAngleOf(FVector2D(Placed.Location)), Layout.Wedges[Tile.DomainId].Yaw);
		TestTrue(FString::Printf(TEXT("La casilla %d está en el sector de su Dominio %d (a %.1f° del centro)"), Tile.TileId, Tile.DomainId, Offset), FMath::Abs(Offset) <= 30.f);
	}
	TestTrue(TEXT("La losa más lejana toca el borde de la mesa: la escala aprovecha el radio"), FMath::IsNearlyEqual(Farthest, Params.TableRadius, 0.01f));
	return true;
}

// CA-VIS-1 (R-VIS-1): el kit se escala (suelo de cada cuña y Altar) con el radio de mesa para el que está hecho el kit: con la mesa de 120 y
// el kit de 74, 120/74; con la mesa igual al radio del kit, 1; y 1 si el radio del kit no es positivo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutKitScaleTest, "Soulbound.Game.Presentation.Tablero3D.EscalaDelKit", LayoutTestFlags)
bool FBoardLayoutKitScaleTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	FBoardLayoutParams Params;
	Params.KitTableRadius = 74.f;
	Params.TableRadius = 120.f;
	TestTrue(TEXT("Mesa de 120 con kit de 74: el suelo se escala 120/74"),
		FMath::IsNearlyEqual(Soulbound::Presentation::ComputeBoardLayout(Board, Params).KitScale, 120.f / 74.f, 0.0001f));

	Params.TableRadius = 74.f;
	TestTrue(TEXT("Mesa igual al radio del kit: el suelo no se escala"),
		FMath::IsNearlyEqual(Soulbound::Presentation::ComputeBoardLayout(Board, Params).KitScale, 1.f, 0.0001f));

	Params.KitTableRadius = 0.f;
	TestTrue(TEXT("Radio del kit no positivo: el suelo no se escala"),
		FMath::IsNearlyEqual(Soulbound::Presentation::ComputeBoardLayout(Board, Params).KitScale, 1.f, 0.0001f));
	return true;
}

// CA-VIS-1 (R-VIS-1): el Altar crece con el kit: con la mesa de 120 y el kit de 74, su apotema en la colocación es 15 * 120/74 y la
// casilla más cercana queda a esa apotema más su distancia escalada desde el borde; con la escala 1, a 15.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutAltarScaleTest, "Soulbound.Game.Presentation.Tablero3D.AltarEscaladoConElKit", LayoutTestFlags)
bool FBoardLayoutAltarScaleTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	for (const float TableRadius : {120.f, 74.f})
	{
		FBoardLayoutParams Params;
		Params.KitTableRadius = 74.f;
		Params.TableRadius = TableRadius;
		const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
		const float Apothem = Params.AltarApothem * TableRadius / 74.f;
		TestTrue(FString::Printf(TEXT("Mesa de %.0f: el apotema del Altar es %.3f (%.3f)"), TableRadius, Apothem, Layout.AltarApothem), FMath::IsNearlyEqual(Layout.AltarApothem, Apothem, 0.001f));

		// La casilla más cercana en el editor es la más cercana en la mesa: su borde interior es el apotema escalado más su distancia escalada.
		float NearestEditor = TNumericLimits<float>::Max();
		float NearestTable = TNumericLimits<float>::Max();
		const FVector2D AltarAt = Board.Tiles[Board.FindAltar()].LayoutPosition;
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Board.IsAltar(Tile.TileId))
			{
				continue;
			}
			const float Editor = static_cast<float>((Tile.LayoutPosition - AltarAt).Size());
			if (Editor < NearestEditor)
			{
				NearestEditor = Editor;
				NearestTable = static_cast<float>(FVector2D(Layout.Tiles[Tile.TileId].Location).Size());
			}
		}
		const float Expected = Apothem + Params.AltarGap + NearestEditor * Layout.Scale;
		TestTrue(FString::Printf(TEXT("Mesa de %.0f: la casilla más cercana está a %.3f del centro (%.3f)"), TableRadius, Expected, NearestTable), FMath::IsNearlyEqual(NearestTable, Expected, 0.001f));
	}
	return true;
}

// CA-VIS-1 (R-VIS-3): la cara superior donde se posa la ficha sale de los datos, no de la malla: con la mesa de 120 y el kit de 74,
// la del Altar es AltarTop * 120/74 y la de una losa, KitFloorHeight + SlabTop.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutTileTopTest, "Soulbound.Game.Presentation.Tablero3D.CaraSuperiorDelAltarYLasLosas", LayoutTestFlags)
bool FBoardLayoutTileTopTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	FBoardLayoutParams Params;
	Params.KitTableRadius = 74.f;
	Params.TableRadius = 120.f;
	Params.KitFloorHeight = 3.4f;
	const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
	if (!TestEqual(TEXT("Una colocación por casilla"), Layout.Tiles.Num(), Board.Tiles.Num()))
	{
		return false;
	}
	for (const FTileState& Tile : Board.Tiles)
	{
		const float Expected = Board.IsAltar(Tile.TileId) ? Params.AltarTop * 120.f / 74.f : Params.KitFloorHeight + Params.SlabTop;
		TestTrue(FString::Printf(TEXT("La cara superior de la casilla %d está a %.3f (%.3f)"), Tile.TileId, Expected, Layout.Tiles[Tile.TileId].Top), FMath::IsNearlyEqual(Layout.Tiles[Tile.TileId].Top, Expected, 0.001f));
	}
	return true;
}

// CA-VIS-1 (R-VIS-3): lo que se posa sobre el suelo del kit sube su cota: con KitFloorHeight 3,4, toda losa, toda estructura y
// todo tramo de sendero están a z = 3,4 y el Altar, a 0; con 0, todo a 0.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutKitFloorHeightTest, "Soulbound.Game.Presentation.Tablero3D.CotaDelSueloDelKit", LayoutTestFlags)
bool FBoardLayoutKitFloorHeightTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	AssignNodeEffects(Board);
	for (const float Height : {3.4f, 0.f})
	{
		FBoardLayoutParams Params;
		Params.KitFloorHeight = Height;
		const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
		if (!TestEqual(TEXT("Una colocación por casilla"), Layout.Tiles.Num(), Board.Tiles.Num()))
		{
			return false;
		}
		int32 Structures = 0;
		for (const FTileState& Tile : Board.Tiles)
		{
			const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
			const float Expected = Board.IsAltar(Tile.TileId) ? 0.f : Height;
			TestTrue(FString::Printf(TEXT("Cota %.1f: la losa %d está a z = %.2f (%.2f)"), Height, Tile.TileId, Expected, Placed.Location.Z), FMath::IsNearlyEqual(Placed.Location.Z, Expected, 0.0001f));
			if (Placed.Structure != EBoardStructure::None)
			{
				++Structures;
				TestTrue(FString::Printf(TEXT("Cota %.1f: la estructura de la casilla %d está a z = %.2f (%.2f)"), Height, Tile.TileId, Height, Placed.StructureLocation.Z),
					FMath::IsNearlyEqual(Placed.StructureLocation.Z, Height, 0.0001f));
			}
		}
		TestTrue(TEXT("El tablero tiene estructuras que comprobar"), Structures > 0);
		TestTrue(TEXT("El tablero tiene tramos de sendero que comprobar"), Layout.PathSegments.Num() > 0);
		for (const FPathSegmentLayout& Segment : Layout.PathSegments)
		{
			TestTrue(FString::Printf(TEXT("Cota %.1f: el tramo %d-%d está a z = %.2f (%.2f)"), Height, Segment.TileA, Segment.TileB, Height, Segment.Location.Z), FMath::IsNearlyEqual(Segment.Location.Z, Height, 0.0001f));
		}
	}
	return true;
}

// CA-VIS-1 (R-VIS-2): cada cuña gira al ángulo medio de sus casillas vistas desde el Altar; seis ángulos distintos separados
// unos 60° (entre 45 y 75 de una a la siguiente).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutWedgesTest, "Soulbound.Game.Presentation.Tablero3D.CunasAlAnguloMedio", LayoutTestFlags)
bool FBoardLayoutWedgesTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, FBoardLayoutParams());
	if (!TestEqual(TEXT("Seis cuñas"), Layout.Wedges.Num(), Soulbound::Board::DomainCount) || !TestEqual(TEXT("Una colocación por casilla"), Layout.Tiles.Num(), Board.Tiles.Num()))
	{
		return false;
	}
	TArray<float> Yaws;
	for (const FDomainWedgeLayout& Wedge : Layout.Wedges)
	{
		const float Mean = MeanDomainAngle(Board, Layout, Wedge.DomainId);
		TestTrue(FString::Printf(TEXT("La cuña del Dominio %d gira al ángulo medio de sus casillas (%.1f frente a %.1f)"), Wedge.DomainId, Wedge.Yaw, Mean),
			FMath::Abs(AngleBetween(Wedge.Yaw, Mean)) < 0.1f);
		Yaws.Add(FMath::UnwindDegrees(Wedge.Yaw) < 0.f ? FMath::UnwindDegrees(Wedge.Yaw) + 360.f : FMath::UnwindDegrees(Wedge.Yaw));
	}
	Yaws.Sort();
	for (int32 Index = 0; Index < Yaws.Num(); ++Index)
	{
		const float Next = Index + 1 < Yaws.Num() ? Yaws[Index + 1] : Yaws[0] + 360.f;
		const float Step = Next - Yaws[Index];
		TestTrue(FString::Printf(TEXT("De una cuña a la siguiente hay unos 60° (%.1f)"), Step), Step >= 45.f && Step <= 75.f);
	}
	return true;
}

// CA-VIS-1 (R-VIS-2): con posiciones no radiales (dos Dominios en el mismo sector, o un Dominio repartido en más de 60°), la
// colocación lo avisa y pone las cuñas de 60 en 60 grados por identificador de Dominio.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutNotRadialTest, "Soulbound.Game.Presentation.Tablero3D.NoRadialAvisaYCunasDe60", LayoutTestFlags)
bool FBoardLayoutNotRadialTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	const FVector2D AltarAt = Board.Tiles[Board.FindAltar()].LayoutPosition;

	// Dos Dominios en el mismo sector: las casillas del Dominio 1 pasan al sitio de las del Dominio 0, un poco más cerca.
	FBoardState SameSector = Board;
	TArray<FVector2D> DomainZero;
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Tile.DomainId == 0)
		{
			DomainZero.Add(Tile.LayoutPosition);
		}
	}
	int32 Next = 0;
	for (FTileState& Tile : SameSector.Tiles)
	{
		if (Tile.DomainId == 1)
		{
			Tile.LayoutPosition = AltarAt + (DomainZero[Next++ % DomainZero.Num()] - AltarAt) * 0.9;
		}
	}
	const FBoardLayout Shared = Soulbound::Presentation::ComputeBoardLayout(SameSector, FBoardLayoutParams());
	TestFalse(TEXT("Dos Dominios en el mismo sector: la colocación avisa de que no es radial"), Shared.bRadial);
	if (TestEqual(TEXT("Seis cuñas"), Shared.Wedges.Num(), Soulbound::Board::DomainCount))
	{
		for (const FDomainWedgeLayout& Wedge : Shared.Wedges)
		{
			TestTrue(FString::Printf(TEXT("Sin trazado radial, la cuña %d va a %d°"), Wedge.DomainId, Wedge.DomainId * 60),
				FMath::Abs(AngleBetween(Wedge.Yaw, Wedge.DomainId * 60.f)) < 0.01f);
		}
	}

	// Un Dominio repartido en más de 60°: una casilla del Dominio 2 se gira 90° alrededor del Altar.
	FBoardState Spread = Board;
	for (FTileState& Tile : Spread.Tiles)
	{
		if (Tile.DomainId == 2)
		{
			Tile.LayoutPosition = AltarAt + (Tile.LayoutPosition - AltarAt).GetRotated(90.0);
			break;
		}
	}
	TestFalse(TEXT("Un Dominio repartido en más de 60°: la colocación avisa de que no es radial"), Soulbound::Presentation::ComputeBoardLayout(Spread, FBoardLayoutParams()).bRadial);
	return true;
}

// CA-VIS-2 (R-VIS-5): la estructura de cada casilla especial sale por el lado contrario al camino (producto escalar con cada
// conexión negativo; con dos conexiones, fuera de la bisectriz) y a la distancia de datos del borde de la losa. Con una y
// dos conexiones (decisión del autor del 20/10/2026; con tres o más, el centro del mayor hueco, sin garantía de signo); y
// con dos en línea recta, en perpendicular hacia el lado contrario al Altar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutStructureSideTest, "Soulbound.Game.Presentation.Tablero3D.EstructuraFueraDelCamino", LayoutTestFlags)
bool FBoardLayoutStructureSideTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	AssignNodeEffects(Board);
	const FBoardLayoutParams Params;
	const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
	if (!TestEqual(TEXT("Una colocación por casilla"), Layout.Tiles.Num(), Board.Tiles.Num()))
	{
		return false;
	}
	int32 Checked = 0;
	for (const FTileState& Tile : Board.Tiles)
	{
		if (!IsSpecial(Tile))
		{
			continue;
		}
		const FTileLayout& Placed = Layout.Tiles[Tile.TileId];
		TestTrue(FString::Printf(TEXT("La casilla %d lleva la estructura de su tipo"), Tile.TileId), Placed.Structure == Soulbound::Presentation::StructureFor(Tile));
		if (Tile.Neighbours.Num() > 2)
		{
			continue; // Tres o más: el centro del mayor hueco; el tablero definitivo no los tendrá (regla para el editor).
		}
		++Checked;
		const FVector2D Out = Placed.OutwardDirection;
		TestTrue(FString::Printf(TEXT("El lado de la estructura de la casilla %d es unitario"), Tile.TileId), FMath::IsNearlyEqual(Out.Size(), 1.f, 0.001f));
		FVector2D Bisector = FVector2D::ZeroVector;
		for (const int32 Other : Tile.Neighbours)
		{
			const FVector2D Along = FVector2D(Layout.Tiles[Other].Location - Placed.Location).GetSafeNormal();
			Bisector += Along;
			TestTrue(FString::Printf(TEXT("La estructura de la casilla %d sale por el lado contrario a su conexión con %d (producto %.3f)"), Tile.TileId, Other, Out | Along), (Out | Along) < 0.f);
		}
		if (Tile.Neighbours.Num() == 2)
		{
			TestTrue(FString::Printf(TEXT("Con dos conexiones, la estructura de la casilla %d queda fuera de la bisectriz"), Tile.TileId), (Out | Bisector) < 0.f);
		}
		const float Distance = FVector2D(Placed.StructureLocation - Placed.Location).Size() - Params.SlabHalfSize;
		TestTrue(FString::Printf(TEXT("La estructura de la casilla %d está a la distancia de datos del borde de la losa (%.2f frente a %.2f)"), Tile.TileId, Distance,
			Params.StructureDistance(Placed.Structure)), FMath::IsNearlyEqual(Distance, Params.StructureDistance(Placed.Structure), 0.001f));
		TestTrue(FString::Printf(TEXT("La estructura de la casilla %d está en su lado"), Tile.TileId),
			FVector2D(Placed.StructureLocation - Placed.Location).GetSafeNormal().Equals(Out, 0.001f));
		if (Tile.Type == ETileType::Niche)
		{
			TestTrue(FString::Printf(TEXT("La cabecera del Nicho %d (+x) mira a su estructura"), Tile.TileId), FMath::Abs(AngleBetween(Placed.Yaw, DirectionAngleOf(Out))) < 0.01f);
		}
		if (Tile.Type == ETileType::PowerNode)
		{
			TestTrue(FString::Printf(TEXT("Las puntas del Nodo %d (±x) siguen el camino, en perpendicular al lado de la estructura"), Tile.TileId),
				FMath::Abs(FMath::Cos(FMath::DegreesToRadians(AngleBetween(Placed.Yaw, DirectionAngleOf(Out))))) < 0.001f);
		}
	}
	TestTrue(TEXT("Se han comprobado las casillas especiales con una y dos conexiones (al menos los seis Nichos)"), Checked >= 6);
	TestTrue(TEXT("Las distancias por estructura son las de los parámetros"), Params.StructureDistance(EBoardStructure::NicheThrone) == Params.NicheThroneDistance
		&& Params.StructureDistance(EBoardStructure::Pylon) == Params.PylonDistance && Params.StructureDistance(EBoardStructure::PlanarArch) == Params.PlanarArchDistance
		&& Params.StructureDistance(EBoardStructure::DuelSwords) == Params.DuelSwordsDistance && Params.StructureDistance(EBoardStructure::OracleOrb) == Params.OracleOrbDistance);

	// Dos conexiones en línea recta: una Fuente encima del Altar en la pantalla del editor, entre dos casillas a izquierda y
	// derecha. El lado es perpendicular al camino, hacia fuera del Altar.
	FBoardState Line;
	const FVector2D EditorPositions[] = {{500.0, 500.0}, {400.0, 300.0}, {500.0, 300.0}, {600.0, 300.0}};
	const ETileType Types[] = {ETileType::Altar, ETileType::Normal, ETileType::PowerSource, ETileType::Normal};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(EditorPositions); ++Index)
	{
		const int32 Id = Line.AddTile();
		Line.Tiles[Id].Type = Types[Index];
		Line.Tiles[Id].DomainId = Index == 0 ? INDEX_NONE : 0;
		Line.Tiles[Id].LayoutPosition = EditorPositions[Index];
		Line.Tiles[Id].bHasLayoutPosition = true;
	}
	Line.Connect(0, 1);
	Line.Connect(1, 2);
	Line.Connect(2, 3);
	const FBoardLayout LineLayout = Soulbound::Presentation::ComputeBoardLayout(Line, Params);
	if (TestEqual(TEXT("Una colocación por casilla en línea"), LineLayout.Tiles.Num(), 4))
	{
		const FTileLayout& Source = LineLayout.Tiles[2];
		const FVector2D Path = FVector2D(LineLayout.Tiles[3].Location - LineLayout.Tiles[1].Location).GetSafeNormal();
		TestTrue(TEXT("En línea recta, la estructura sale en perpendicular al camino"), FMath::Abs(Source.OutwardDirection | Path) < 0.001f);
		TestTrue(TEXT("En línea recta, hacia el lado contrario al Altar"), (Source.OutwardDirection | FVector2D(Source.Location).GetSafeNormal()) > 0.f);
	}
	return true;
}

// CA-VIS-3 (R-VIS-4): por cada conexión, los tramos de sendero que caben en el hueco entre las dos piezas (longitud menos las
// dos mitades, entre el largo del tramo), nunca cero en el tablero de 35 (con el Altar, la apotema hace de mitad);
// orientados con la conexión y repartidos a partes iguales a lo largo del hueco.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutPathTest, "Soulbound.Game.Presentation.Tablero3D.TramosDeSendero", LayoutTestFlags)
bool FBoardLayoutPathTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	const FBoardLayoutParams Params;
	const FBoardLayout Layout = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
	if (!TestEqual(TEXT("Una colocación por casilla"), Layout.Tiles.Num(), Board.Tiles.Num()))
	{
		return false;
	}
	int32 Expected = 0;
	int32 Edges = 0;
	for (const FTileState& Tile : Board.Tiles)
	{
		for (const int32 Other : Tile.Neighbours)
		{
			if (Other <= Tile.TileId)
			{
				continue;
			}
			++Edges;
			const FVector2D A(Layout.Tiles[Tile.TileId].Location);
			const FVector2D B(Layout.Tiles[Other].Location);
			const float HalfA = HalfSize(Board, Tile.TileId, Params);
			const float HalfB = HalfSize(Board, Other, Params);
			const float Gap = FVector2D::Distance(A, B) - HalfA - HalfB;
			const int32 Fits = FMath::Max(0, FMath::FloorToInt(Gap / Params.PathSegmentLength));
			Expected += Fits;
			TestTrue(FString::Printf(TEXT("La conexión %d-%d tiene al menos un tramo (hueco %.2f)"), Tile.TileId, Other, Gap), Fits > 0);

			const FVector2D Along = (B - A).GetSafeNormal();
			TArray<float> Projections;
			for (const FPathSegmentLayout& Segment : Layout.PathSegments)
			{
				if (Segment.TileA != Tile.TileId || Segment.TileB != Other)
				{
					continue;
				}
				const FVector2D Offset = FVector2D(Segment.Location) - A;
				Projections.Add(Offset | Along);
				TestTrue(FString::Printf(TEXT("El tramo de %d-%d está sobre la conexión"), Tile.TileId, Other), FMath::Abs(Offset ^ Along) < 0.001f);
				TestTrue(FString::Printf(TEXT("El tramo de %d-%d está orientado con la conexión"), Tile.TileId, Other),
					FMath::Abs(FMath::Sin(FMath::DegreesToRadians(AngleBetween(Segment.Yaw, DirectionAngleOf(Along))))) < 0.001f);
			}
			if (!TestEqual(FString::Printf(TEXT("La conexión %d-%d tiene los tramos que caben"), Tile.TileId, Other), Projections.Num(), Fits) || Fits == 0)
			{
				continue;
			}
			Projections.Sort();
			const float Spacing = Gap / Fits;
			for (int32 Index = 0; Index < Projections.Num(); ++Index)
			{
				TestTrue(FString::Printf(TEXT("El tramo %d de %d-%d está en su sitio del reparto a lo largo del hueco"), Index, Tile.TileId, Other),
					FMath::IsNearlyEqual(Projections[Index], HalfA + (Index + 0.5f) * Spacing, 0.001f));
			}
		}
	}
	TestEqual(TEXT("Ningún tramo de más: los tramos son los de las conexiones"), Layout.PathSegments.Num(), Expected);
	TestTrue(TEXT("El tablero de 35 tiene conexiones"), Edges > 0);
	return true;
}

// CA-VIS-4 (R-VIS-3): cada casilla lleva la losa de su tipo; las partidas son la fracción de datos de las normales, las mismas
// en dos cálculos con el mismo tablero, y no siguen un patrón (no todas comparten resto entre 2 y 6).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutCrackedTest, "Soulbound.Game.Presentation.Tablero3D.LosasPartidasFijasYNoPeriodicas", LayoutTestFlags)
bool FBoardLayoutCrackedTest::RunTest(const FString& Parameters)
{
	FBoardState Board;
	if (!LoadBoard35(*this, Board))
	{
		return false;
	}
	const FBoardLayoutParams Params;
	const FBoardLayout First = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
	const FBoardLayout Second = Soulbound::Presentation::ComputeBoardLayout(Board, Params);
	if (!TestEqual(TEXT("Una colocación por casilla"), First.Tiles.Num(), Board.Tiles.Num()) || !TestEqual(TEXT("Una colocación por casilla, otra vez"), Second.Tiles.Num(), Board.Tiles.Num()))
	{
		return false;
	}
	TArray<int32> Cracked;
	int32 Normals = 0;
	for (const FTileState& Tile : Board.Tiles)
	{
		const EBoardSlab Slab = First.Tiles[Tile.TileId].Slab;
		TestTrue(FString::Printf(TEXT("La casilla %d lleva la misma losa en los dos cálculos"), Tile.TileId), Slab == Second.Tiles[Tile.TileId].Slab);
		switch (Tile.Type)
		{
		case ETileType::Normal:
			++Normals;
			TestTrue(FString::Printf(TEXT("La casilla normal %d lleva losa normal o partida"), Tile.TileId), Slab == EBoardSlab::Normal || Slab == EBoardSlab::Cracked);
			if (Slab == EBoardSlab::Cracked)
			{
				Cracked.Add(Tile.TileId);
			}
			break;
		case ETileType::Niche: TestTrue(FString::Printf(TEXT("El Nicho %d lleva su losa"), Tile.TileId), Slab == EBoardSlab::Niche); break;
		case ETileType::PowerSource: TestTrue(FString::Printf(TEXT("La Fuente %d lleva su losa"), Tile.TileId), Slab == EBoardSlab::PowerSource); break;
		case ETileType::PowerNode: TestTrue(FString::Printf(TEXT("El Nodo %d lleva su losa"), Tile.TileId), Slab == EBoardSlab::PowerNode); break;
		case ETileType::Altar: TestTrue(TEXT("El Altar es su pieza propia"), Slab == EBoardSlab::Altar); break;
		}
	}
	TestEqual(TEXT("Las partidas son la fracción de datos de las normales"), Cracked.Num(), FMath::RoundToInt(Normals * Params.CrackedSlabFraction));
	for (int32 Period = 2; Period <= 6 && Cracked.Num() > 1; ++Period)
	{
		const bool bAllSame = Cracked.FindByPredicate([&Cracked, Period](int32 Id) { return Id % Period != Cracked[0] % Period; }) == nullptr;
		TestFalse(FString::Printf(TEXT("Las partidas no siguen un patrón de %d en %d"), Period, Period), bAllSame);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardLayoutHaloTest, "Soulbound.Game.Presentation.Tablero3D.HaloDelAltarAsoma", LayoutTestFlags)

bool FBoardLayoutHaloTest::RunTest(const FString& Parameters)
{
	// R-DEC-12 (S-033): el halo de una casilla alcanzable asoma por su borde. Sin pieza, el de siempre (diámetro por HaloScale); con una pieza
	// mayor (el Altar es un hexágono: la mayor de sus medidas X o Y), un margen por encima de ella para que asome también por las esquinas.
	// El margen es un parámetro (AltarHaloMargin, Config, valor inicial 1,1), no un número fijo (corrección del autor del 12/11/2026).
	using Soulbound::Presentation::HaloDiameter;
	TestEqual(TEXT("Sin pieza, el de siempre"), HaloDiameter(30.f, 1.35f, 0.f, 0.f, 1.1f), 40.5f);
	TestEqual(TEXT("Con una pieza menor que el halo, el de siempre"), HaloDiameter(30.f, 1.35f, 30.f, 28.f, 1.1f), 40.5f);
	TestEqual(TEXT("Con una pieza mayor, la mayor de X e Y con el margen"), HaloDiameter(30.f, 1.35f, 40.f, 50.f, 1.1f), 55.f);
	TestEqual(TEXT("Y al revés, con X la mayor"), HaloDiameter(30.f, 1.35f, 50.f, 40.f, 1.1f), 55.f);
	TestEqual(TEXT("Con otro margen, el que se pasa"), HaloDiameter(30.f, 1.35f, 40.f, 50.f, 1.2f), 60.f);
	TestEqual(TEXT("Un margen menor que 1 no encoge la pieza: se toma 1"), HaloDiameter(30.f, 1.35f, 40.f, 50.f, 0.5f), 50.f);
	return true;
}


namespace
{
	/** Carga Content/Data/Boards/<Name>.json; falso, con el fallo anotado, si no se puede. */
	bool LoadNamedBoard(FAutomationTestBase& Test, const FString& Name, FBoardState& OutBoard)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards"), Name + TEXT(".json")));
		if (!Test.TestTrue(FString::Printf(TEXT("%s.json se carga (%s)"), *Name, *Result.Message), Result.IsSuccess()))
		{
			return false;
		}
		OutBoard = Result.Board;
		return true;
	}

	/** Los parámetros de colocación del juego (DefaultGame.ini): la mesa de 120 y el suelo del kit a 3,1. */
	FBoardLayoutParams GameLayoutParams()
	{
		FBoardLayoutParams Params;
		Params.TableRadius = 120.f;
		Params.KitFloorHeight = 3.1f;
		return Params;
	}

	/** El sitio preferido de un rótulo, calculado aquí sin la función de sitio: radio y lado en fracciones de la cuña (FDomainNameplateParams). */
	FVector2D PreferredNameplatePoint(const FBoardLayout& Layout, const FBoardLayoutParams& LayoutParams, int32 DomainId,
		const Soulbound::Presentation::FDomainNameplateParams& Params)
	{
		const float Inner = Layout.AltarApothem * 2.f / FMath::Sqrt(3.f);
		const float Angle = FMath::DegreesToRadians(Layout.Wedges[DomainId].Yaw + Params.AngleFraction * 30.f);
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Inner + Params.RadiusFraction * (LayoutParams.TableRadius - Inner));
	}

	/**
	 * Comprobación independiente de la función de sitio (CA-VIS-37): recorre el rectángulo del rótulo punto a punto (cada 0,25 cm y sus bordes) y dice
	 * el primer problema: fuera de la cuña (sector de su eje ±30° y radio de mesa), bajo el Altar (su radio circunscrito) o sobre una losa (el cuadrado
	 * girado de cualquier casilla). Los senderos no cuentan: el rótulo puede cruzarlos (cambio de regla del autor, 19/11/2026). Vacío si no hay ninguno.
	 */
	FString NameplateProblem(const FBoardState& Board, const FBoardLayout& Layout, const FBoardLayoutParams& LayoutParams, int32 DomainId,
		const FVector2D& Center, const FVector2D& Right, const FVector2D& Up, const FVector2D& Size)
	{
		const float AltarRadius = Layout.AltarApothem * 2.f / FMath::Sqrt(3.f);
		const float WedgeYaw = Layout.Wedges[DomainId].Yaw;
		constexpr float Step = 0.25f;
		const int32 StepsX = FMath::CeilToInt(Size.X / Step);
		const int32 StepsY = FMath::CeilToInt(Size.Y / Step);
		for (int32 IndexX = 0; IndexX <= StepsX; ++IndexX)
		{
			for (int32 IndexY = 0; IndexY <= StepsY; ++IndexY)
			{
				const float U = -Size.X / 2.f + Size.X * IndexX / FMath::Max(StepsX, 1);
				const float V = -Size.Y / 2.f + Size.Y * IndexY / FMath::Max(StepsY, 1);
				const FVector2D Point = Center + Right * U + Up * V;
				if (Point.Size() > LayoutParams.TableRadius + KINDA_SMALL_NUMBER)
				{
					return FString::Printf(TEXT("fuera de la mesa en (%.1f, %.1f)"), Point.X, Point.Y);
				}
				if (FMath::Abs(AngleBetween(DirectionAngleOf(Point), WedgeYaw)) > 30.f + 0.01f)
				{
					return FString::Printf(TEXT("fuera de la cuña en (%.1f, %.1f)"), Point.X, Point.Y);
				}
				if (Point.Size() < AltarRadius)
				{
					return FString::Printf(TEXT("bajo el Altar en (%.1f, %.1f)"), Point.X, Point.Y);
				}
				for (const FTileState& Tile : Board.Tiles)
				{
					if (Board.IsAltar(Tile.TileId))
					{
						continue;
					}
					const FTileLayout& Slab = Layout.Tiles[Tile.TileId];
					const FVector2D Local = (Point - FVector2D(Slab.Location)).GetRotated(-Slab.Yaw);
					if (FMath::Abs(Local.X) <= LayoutParams.SlabHalfSize && FMath::Abs(Local.Y) <= LayoutParams.SlabHalfSize)
					{
						return FString::Printf(TEXT("sobre la losa %d en (%.1f, %.1f)"), Tile.TileId, Point.X, Point.Y);
					}
				}
			}
		}
		return FString();
	}

	/** La tabla para el autor de un cálculo de rótulos: letra por cuña y forma, letra común y forma que sale. */
	void LogNameplateTable(FAutomationTestBase& Test, const TCHAR* Label, const FBoardState& Board,
		const Soulbound::Presentation::FDomainNameplateLayout& Nameplates)
	{
		for (int32 Domain = 0; Domain < Board.Domains.Num() && Domain < Nameplates.MaxLetterOneLine.Num() && Domain < Nameplates.MaxLetterSplit.Num(); ++Domain)
		{
			Test.AddInfo(FString::Printf(TEXT("%s | D%d %s | todos en una línea %.2f cm | partidos donde se puede %.2f cm"), Label, Domain, *Board.Domains[Domain].Name,
				Nameplates.MaxLetterOneLine[Domain], Nameplates.MaxLetterSplit[Domain]));
		}
		Test.AddInfo(FString::Printf(TEXT("%s | forma: %s | letra común %.2f cm"), Label, Nameplates.bSplit ? TEXT("partidos donde se puede") : TEXT("todos en una línea"),
			Nameplates.LetterSize));
	}
}

// CA-VIS-38 (R-VIS-34, forma; decisiones del autor del 19/11/2026): el nombre partido en dos líneas por el espacio más cercano a la mitad; sin espacio, o si alguna de
// las dos líneas quedaría con menos de 3 caracteres ("El" suelto), una línea.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameplateSplitTest, "Soulbound.Game.Presentation.Tablero3D.RotuloPartidoPorElEspacioDelMedio", LayoutTestFlags)
bool FNameplateSplitTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::SplitNameplateName;
	constexpr int32 MinChars = 3;
	TestEqual(TEXT("El Fiordo Encadenado"), SplitNameplateName(TEXT("El Fiordo Encadenado"), MinChars), TArray<FString>({TEXT("El Fiordo"), TEXT("Encadenado")}));
	TestEqual(TEXT("La Ruina Tormentosa"), SplitNameplateName(TEXT("La Ruina Tormentosa"), MinChars), TArray<FString>({TEXT("La Ruina"), TEXT("Tormentosa")}));
	TestEqual(TEXT("El Río de la Noche: el espacio justo en la mitad"), SplitNameplateName(TEXT("El Río de la Noche"), MinChars), TArray<FString>({TEXT("El Río de"), TEXT("la Noche")}));
	TestEqual(TEXT("El Bosque Negro"), SplitNameplateName(TEXT("El Bosque Negro"), MinChars), TArray<FString>({TEXT("El Bosque"), TEXT("Negro")}));
	TestEqual(TEXT("El Banquete: \"El\" no va suelto, una línea"), SplitNameplateName(TEXT("El Banquete"), MinChars), TArray<FString>({TEXT("El Banquete")}));
	TestEqual(TEXT("El Descenso: \"El\" no va suelto, una línea"), SplitNameplateName(TEXT("El Descenso"), MinChars), TArray<FString>({TEXT("El Descenso")}));
	TestEqual(TEXT("Con mínimo 2, El Banquete sí se parte"), SplitNameplateName(TEXT("El Banquete"), 2), TArray<FString>({TEXT("El"), TEXT("Banquete")}));
	TestEqual(TEXT("Sin espacio: una línea"), SplitNameplateName(TEXT("Abismo"), MinChars), TArray<FString>({TEXT("Abismo")}));
	// "abc de fgh": partido por el primer espacio, "abc" y "de fgh"; por el segundo, "abc de" y "fgh": tres y seis letras en los dos casos.
	TestEqual(TEXT("A igual distancia, el primero"), SplitNameplateName(TEXT("abc de fgh"), MinChars), TArray<FString>({TEXT("abc"), TEXT("de fgh")}));
	TestEqual(TEXT("Vacío: nada"), SplitNameplateName(FString(), MinChars), TArray<FString>());
	return true;
}

// CA-VIS-39 (R-VIS-34, calibración; ronda del 19/11/2026): los parámetros del rótulo son los mismos en los tres sitios (la estructura, el actor y Config/DefaultGame.ini), con la
// calibración medida por el autor sobre una captura cenital: 0,5 de ancho por carácter y 1,0 de alto de línea; la zona, 0,2; partir, con 3 caracteres.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameplateParamsTest, "Soulbound.Game.Presentation.Tablero3D.RotuloParametrosEnLosTresSitios", LayoutTestFlags)
bool FNameplateParamsTest::RunTest(const FString& Parameters)
{
	const Soulbound::Presentation::FDomainNameplateParams Params;
	TestEqual(TEXT("Ancho por carácter, el calibrado"), Params.CharWidthFraction, 0.5f);
	TestEqual(TEXT("Alto de línea, el calibrado"), Params.LineHeightFraction, 1.f);
	TestEqual(TEXT("Zona: 0,2 de la profundidad de la cuña"), Params.MaxOffsetFraction, 0.2f);
	TestEqual(TEXT("Partir con 3 caracteres por línea"), Params.MinSplitLineChars, 3);
	// El actor, con la Config del juego ya aplicada (el objeto por defecto lee DefaultGame.ini).
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	TestEqual(TEXT("Actor y Config: sitio, radio"), View->DomainNameplateRadiusFraction, Params.RadiusFraction);
	TestEqual(TEXT("Actor y Config: sitio, lado"), View->DomainNameplateAngleFraction, Params.AngleFraction);
	TestEqual(TEXT("Actor y Config: tope de la letra"), View->DomainNameplateMaxLetterSizeFraction, Params.MaxLetterSizeFraction);
	TestEqual(TEXT("Actor y Config: ancho por carácter"), View->DomainNameplateCharWidthFraction, Params.CharWidthFraction);
	TestEqual(TEXT("Actor y Config: alto de línea"), View->DomainNameplateLineHeightFraction, Params.LineHeightFraction);
	TestEqual(TEXT("Actor y Config: letra de Olvidado"), View->DomainNameplateForgottenScale, Params.ForgottenScale);
	TestEqual(TEXT("Actor y Config: holgura"), View->DomainNameplateMarginFraction, Params.MarginFraction);
	TestEqual(TEXT("Actor y Config: paso de la búsqueda"), View->DomainNameplateSearchStepFraction, Params.SearchStepFraction);
	TestEqual(TEXT("Actor y Config: zona"), View->DomainNameplateMaxOffsetFraction, Params.MaxOffsetFraction);
	TestEqual(TEXT("Actor y Config: caracteres para partir"), View->DomainNameplateMinSplitLineChars, Params.MinSplitLineChars);
	return true;
}

// CA-VIS-37 (R-VIS-34, sitio; ronda del 19/11/2026): con tablero_86 y tablero_35, cada rótulo reserva el rectángulo de sus propias líneas más la de
// "Olvidado", con la misma letra en los seis; queda entero dentro de la cuña de su Dominio, sin tocar losas ni el Altar (los senderos sí se pueden
// cruzar), con su centro a no más de MaxOffsetFraction de la profundidad de la cuña del sitio preferido, y con el giro de la vista sentada. La
// comprobación recorre el rectángulo punto a punto, sin usar la función de sitio. Imprime la tabla de formas y letras, con y sin el límite de zona.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameplateSpotTest, "Soulbound.Game.Presentation.Tablero3D.RotuloDentroDeSuCunaSinTaparCasillas", LayoutTestFlags)
bool FNameplateSpotTest::RunTest(const FString& Parameters)
{
	using namespace Soulbound::Presentation;
	const FDomainNameplateParams Params;
	const FBoardLayoutParams LayoutParams = GameLayoutParams();
	constexpr float SeatedYaw = 180.f;
	for (const TCHAR* BoardName : {TEXT("tablero_86"), TEXT("tablero_35")})
	{
		FBoardState Board;
		if (!LoadNamedBoard(*this, BoardName, Board))
		{
			return false;
		}
		const FBoardLayout Layout = ComputeBoardLayout(Board, LayoutParams);
		const FDomainNameplateLayout Nameplates = ComputeDomainNameplates(Board, Layout, LayoutParams, SeatedYaw, Params);
		if (!TestEqual(FString::Printf(TEXT("%s: un sitio por Dominio"), BoardName), Nameplates.Spots.Num(), Board.Domains.Num())
			|| !TestEqual(FString::Printf(TEXT("%s: unas líneas por Dominio"), BoardName), Nameplates.NameLines.Num(), Board.Domains.Num())
			|| !TestEqual(FString::Printf(TEXT("%s: una letra por Dominio, en una línea"), BoardName), Nameplates.MaxLetterOneLine.Num(), Board.Domains.Num())
			|| !TestEqual(FString::Printf(TEXT("%s: una letra por Dominio, partidos"), BoardName), Nameplates.MaxLetterSplit.Num(), Board.Domains.Num()))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s: cabe con alguna letra"), BoardName), Nameplates.LetterSize > 0.f);
		LogNameplateTable(*this, BoardName, Board, Nameplates);

		// Para el autor: la letra que saldría sin el límite de zona (el centro, en cualquier sitio libre de la cuña).
		FDomainNameplateParams Unlimited = Params;
		Unlimited.MaxOffsetFraction = 10.f;
		const FDomainNameplateLayout Free = ComputeDomainNameplates(Board, Layout, LayoutParams, SeatedYaw, Unlimited);
		LogNameplateTable(*this, *FString::Printf(TEXT("%s sin zona"), BoardName), Board, Free);

		const float Depth = LayoutParams.TableRadius - Layout.AltarApothem;
		for (int32 Domain = 0; Domain < Board.Domains.Num(); ++Domain)
		{
			const FDomainNameplateSpot& Spot = Nameplates.Spots[Domain];
			const FString What = FString::Printf(TEXT("%s, D%d"), BoardName, Domain);
			// Cada nombre, su forma: partido donde se puede, si sale esa forma; si no, en una línea.
			const FString& Name = Board.Domains[Domain].Name;
			const TArray<FString> Lines = Nameplates.bSplit ? SplitNameplateName(Name, Params.MinSplitLineChars) : TArray<FString>({Name});
			TestEqual(What + TEXT(": sus líneas"), Nameplates.NameLines[Domain], Lines);
			TestEqual(What + TEXT(": una posición por línea"), Spot.NameLineCenters.Num(), Lines.Num());
			if (!TestTrue(What + TEXT(": hay sitio"), Spot.bFound))
			{
				continue;
			}
			TestEqual(What + TEXT(": el giro de la vista sentada"), Spot.Yaw, SeatedYaw);
			// Con la vista sentada en 180 (en -X, mirando a +X), el texto sube hacia +X y se lee hacia +Y, como las etiquetas de casilla.
			TestTrue(What + TEXT(": arriba del texto, +X"), Spot.Up.Equals(FVector2D(1.f, 0.f), 1e-3f));
			TestTrue(What + TEXT(": lectura hacia +Y"), Spot.Right.Equals(FVector2D(0.f, 1.f), 1e-3f));
			// Su propio rectángulo: sus líneas más la de "Olvidado", con la letra común.
			const FVector2D Size = NameplateSize(Lines, Nameplates.LetterSize, Params);
			TestTrue(What + TEXT(": reserva el rectángulo de sus líneas y el de Olvidado"), Spot.HalfExtent.Equals(Size / 2.f, 1e-3f));
			const FString Problem = NameplateProblem(Board, Layout, LayoutParams, Domain, Spot.Center, FVector2D(0.f, 1.f), FVector2D(1.f, 0.f), Size);
			TestTrue(FString::Printf(TEXT("%s: el rectángulo, libre (%s)"), *What, *Problem), Problem.IsEmpty());
			const FVector2D Preferred = PreferredNameplatePoint(Layout, LayoutParams, Domain, Params);
			const float Offset = FVector2D::Distance(Spot.Center, Preferred);
			TestTrue(FString::Printf(TEXT("%s: en la zona, a %.1f cm del sitio preferido (máximo %.1f)"), *What, Offset, Params.MaxOffsetFraction * Depth),
				Offset <= Params.MaxOffsetFraction * Depth + 0.01f);
			AddInfo(FString::Printf(TEXT("%s | %s | centro (%.1f, %.1f): a %.1f cm del centro de la mesa, %.1f° del eje de la cuña, %.1f cm del preferido | rectángulo %.1f x %.1f cm"),
				*What, *FString::Join(Lines, TEXT(" / ")), Spot.Center.X, Spot.Center.Y, Spot.Center.Size(),
				AngleBetween(DirectionAngleOf(Spot.Center), Layout.Wedges[Domain].Yaw), Offset, Size.X, Size.Y));
		}
	}

	// El sitio preferido encima de una losa: la función busca otro, libre, en la misma cuña y dentro de la zona.
	FBoardState Board;
	if (!LoadNamedBoard(*this, TEXT("tablero_35"), Board))
	{
		return false;
	}
	const FBoardLayout Layout = ComputeBoardLayout(Board, LayoutParams);
	const int32 Domain = 0;
	int32 Target = INDEX_NONE;
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Tile.DomainId == Domain && Tile.Type == ETileType::Normal)
		{
			Target = Tile.TileId;
			break;
		}
	}
	if (!TestTrue(TEXT("Una casilla normal del Dominio 0"), Target != INDEX_NONE))
	{
		return false;
	}
	const FVector2D TargetLocation(Layout.Tiles[Target].Location);
	const float Inner = Layout.AltarApothem * 2.f / FMath::Sqrt(3.f);
	FDomainNameplateParams OnSlab = Params;
	OnSlab.RadiusFraction = (TargetLocation.Size() - Inner) / (LayoutParams.TableRadius - Inner);
	OnSlab.AngleFraction = AngleBetween(DirectionAngleOf(TargetLocation), Layout.Wedges[Domain].Yaw) / 30.f;
	const FVector2D Size = NameplateSize({TEXT("El Banquete")}, 2.f, OnSlab);
	const FDomainNameplateSpot Spot = DomainNameplateSpot(Board, Layout, LayoutParams, Domain, Size, SeatedYaw, OnSlab);
	if (TestTrue(TEXT("Preferido sobre una losa: encuentra sitio"), Spot.bFound))
	{
		TestTrue(TEXT("Preferido sobre una losa: se aparta de ella"), !Spot.Center.Equals(TargetLocation, 0.5f));
		TestTrue(TEXT("Preferido sobre una losa: dentro de la zona"),
			FVector2D::Distance(Spot.Center, TargetLocation) <= OnSlab.MaxOffsetFraction * (LayoutParams.TableRadius - Layout.AltarApothem) + 0.01f);
		const FString Problem = NameplateProblem(Board, Layout, LayoutParams, Domain, Spot.Center, Spot.Right, Spot.Up, Size);
		TestTrue(FString::Printf(TEXT("Preferido sobre una losa: el rectángulo, libre (%s)"), *Problem), Problem.IsEmpty());
	}
	return true;
}

#endif
