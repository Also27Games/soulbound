// Tests de las correspondencias del kit de arte del tablero (S-019, glosario propuesto "BoardKit"): qué pieza lleva cada losa y
// cada estructura, y que cada pieza tiene nombre para el registro. Sin mundo, sin editor y sin assets. La colocación de los
// componentes comparte vista de partida y vista previa y necesita mundo; su lógica de posiciones la cubre BoardLayoutTest.
// Cada test cita el criterio (CA-VIS-n) y la regla (R-VIS-n) que cubre. Filtro: "Soulbound.Game.Presentation.Tablero3D.Kit".

#include "../../Presentation/BoardKit.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags KitTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

// R-VIS-3: cada tipo de losa lleva su pieza, y el Altar la suya.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardKitSlabPieceTest, "Soulbound.Game.Presentation.Tablero3D.Kit.PiezaDeCadaLosa", KitTestFlags)
bool FBoardKitSlabPieceTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("normal: losa"), FBoardKit::SlabPiece(EBoardSlab::Normal) == EBoardKitPiece::Slab);
	TestTrue(TEXT("partida: losa partida"), FBoardKit::SlabPiece(EBoardSlab::Cracked) == EBoardKitPiece::CrackedSlab);
	TestTrue(TEXT("Nicho: losa del Nicho"), FBoardKit::SlabPiece(EBoardSlab::Niche) == EBoardKitPiece::NicheSlab);
	TestTrue(TEXT("Fuente: losa de la Fuente"), FBoardKit::SlabPiece(EBoardSlab::PowerSource) == EBoardKitPiece::SourceSlab);
	TestTrue(TEXT("Nodo: losa del Nodo"), FBoardKit::SlabPiece(EBoardSlab::PowerNode) == EBoardKitPiece::NodeSlab);
	TestTrue(TEXT("Altar: Altar"), FBoardKit::SlabPiece(EBoardSlab::Altar) == EBoardKitPiece::Altar);
	return true;
}

// R-VIS-5: cada estructura lleva su pieza; sin estructura no hay pieza.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardKitStructurePieceTest, "Soulbound.Game.Presentation.Tablero3D.Kit.PiezaDeCadaEstructura", KitTestFlags)
bool FBoardKitStructurePieceTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("trono del Nicho"), FBoardKit::StructurePiece(EBoardStructure::NicheThrone) == EBoardKitPiece::NicheThrone);
	TestTrue(TEXT("pilón de la Fuente"), FBoardKit::StructurePiece(EBoardStructure::Pylon) == EBoardKitPiece::Pylon);
	TestTrue(TEXT("arco Planar"), FBoardKit::StructurePiece(EBoardStructure::PlanarArch) == EBoardKitPiece::PlanarArch);
	TestTrue(TEXT("espadas de Duelo"), FBoardKit::StructurePiece(EBoardStructure::DuelSwords) == EBoardKitPiece::DuelSwords);
	TestTrue(TEXT("orbe del Oráculo"), FBoardKit::StructurePiece(EBoardStructure::OracleOrb) == EBoardKitPiece::OracleOrb);
	TestTrue(TEXT("sin estructura, sin pieza"), FBoardKit::StructurePiece(EBoardStructure::None) == EBoardKitPiece::Count);
	return true;
}

// R-VIS-8: el aviso del registro nombra cada pieza que falta; todas tienen un nombre distinto.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardKitPieceNameTest, "Soulbound.Game.Presentation.Tablero3D.Kit.NombreDeCadaPieza", KitTestFlags)
bool FBoardKitPieceNameTest::RunTest(const FString& Parameters)
{
	TSet<FString> Names;
	for (int32 Index = 0; Index < static_cast<int32>(EBoardKitPiece::Count); ++Index)
	{
		const FString Name = FBoardKit::PieceName(static_cast<EBoardKitPiece>(Index));
		TestFalse(FString::Printf(TEXT("la pieza %d tiene nombre"), Index), Name.IsEmpty() || Name == TEXT("?"));
		TestFalse(FString::Printf(TEXT("el nombre de la pieza %d (%s) no se repite"), Index, *Name), Names.Contains(Name));
		Names.Add(Name);
	}
	return true;
}

#endif
