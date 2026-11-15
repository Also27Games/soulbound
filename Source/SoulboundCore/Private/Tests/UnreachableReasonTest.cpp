// Tests del motivo de una casilla inalcanzable (S-039, R-TAB-29): CA-TAB-30, CA-TAB-31 y CA-TAB-32.
// Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Board.UnreachableReason".

#include "ChoiceFixture.h"
#include "Match/UnreachableRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Rules::UnreachableReason;
	using Soulbound::Tests::FChoiceFixture;

	/** Una casilla normal del Dominio, o INDEX_NONE. */
	int32 AnyTileOf(const FChoiceFixture& F, int32 DomainId)
	{
		const TArray<int32> Tiles = F.NormalTiles(DomainId);
		return Tiles.IsEmpty() ? INDEX_NONE : Tiles[0];
	}

	/** Deja a Seat como Eco con la carrera terminada: confinado a los Dominios Olvidados (R-TAB-20). */
	void MakeConfinedEcho(FChoiceFixture& F, int32 Seat)
	{
		F.State.MatchPhase = EMatchPhase::RuneHunt;
		F.State.Seats[Seat].Role = ESeatRole::LostEcho;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnreachableSealedTest,
	"Soulbound.Core.Board.UnreachableReason.Sello",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnreachableSealedTest::RunTest(const FString& Parameters)
{
	// CA-TAB-30 (R-TAB-29, sello): una casilla sellada que el asiento no alcanza da Sealed; la misma casilla, para un asiento con Paso
	// Libre que no la alcanza con su tirada, da Roll.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE || F.BondAny(*this, 1) == INDEX_NONE)
	{
		return false;
	}
	const int32 Domain = F.State.Seats[0].DomainId;
	const int32 Sealed = AnyTileOf(F, Domain);
	if (!TestTrue(TEXT("Hay una casilla que sellar"), Sealed != INDEX_NONE))
	{
		return false;
	}
	F.Seal(Sealed);
	F.GivePower(1, EPower::FreePassage);
	TestTrue(TEXT("El asiento 1 tiene Paso Libre"), Soulbound::Rules::MakeMoverTraits(F.State, 1).bFreePassage);
	TestFalse(TEXT("El asiento 0 no"), Soulbound::Rules::MakeMoverTraits(F.State, 0).bFreePassage);

	TestEqual(TEXT("Sin Paso Libre: Sealed"), UnreachableReason(F.State, 0, Sealed), EUnreachableReason::Sealed);
	TestEqual(TEXT("Con Paso Libre: Roll"), UnreachableReason(F.State, 1, Sealed), EUnreachableReason::Roll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnreachableConfinedTest,
	"Soulbound.Core.Board.UnreachableReason.Eco",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnreachableConfinedTest::RunTest(const FString& Parameters)
{
	// CA-TAB-31 (R-TAB-29, Eco): para un Eco con la carrera terminada, una casilla de un Dominio con Elegido da Confined y una de un
	// Dominio Olvidado fuera de su tirada da Roll; durante la carrera y durante la Noche de los Ecos, la del Dominio con Elegido da
	// Roll; para un Elegido, siempre Roll.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	const int32 OwnedDomain = F.DomainOf(EDomainState::Owned);
	const int32 ForgottenDomain = F.DomainOf(EDomainState::Forgotten);
	if (!TestTrue(TEXT("Hay un Dominio con dueño y uno Olvidado"), OwnedDomain != INDEX_NONE && ForgottenDomain != INDEX_NONE))
	{
		return false;
	}
	const int32 OwnedTile = AnyTileOf(F, OwnedDomain);
	const int32 ForgottenTile = AnyTileOf(F, ForgottenDomain);
	if (!TestTrue(TEXT("Con casillas normales en los dos"), OwnedTile != INDEX_NONE && ForgottenTile != INDEX_NONE))
	{
		return false;
	}

	// El asiento 1 sigue siendo Eco (no se ha vinculado): en la carrera no está confinado.
	TestEqual(TEXT("El asiento 1 es Eco"), F.State.Seats[1].Role, ESeatRole::LostEcho);
	TestEqual(TEXT("Durante la carrera: Roll"), UnreachableReason(F.State, 1, OwnedTile), EUnreachableReason::Roll);

	MakeConfinedEcho(F, 1);
	TestEqual(TEXT("Eco con la carrera terminada, Dominio con Elegido: Confined"), UnreachableReason(F.State, 1, OwnedTile), EUnreachableReason::Confined);
	TestEqual(TEXT("... Dominio Olvidado fuera de su tirada: Roll"), UnreachableReason(F.State, 1, ForgottenTile), EUnreachableReason::Roll);

	F.State.EchoNightUntilRound = F.State.Round + 3;
	TestTrue(TEXT("Es la Noche de los Ecos"), F.State.IsEchoNight());
	TestEqual(TEXT("Durante la Noche de los Ecos: Roll"), UnreachableReason(F.State, 1, OwnedTile), EUnreachableReason::Roll);

	F.State.EchoNightUntilRound = 0;
	TestEqual(TEXT("Un Elegido, siempre Roll"), UnreachableReason(F.State, 0, OwnedTile), EUnreachableReason::Roll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnreachableOrderTest,
	"Soulbound.Core.Board.UnreachableReason.ElSelloVaPrimero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnreachableOrderTest::RunTest(const FString& Parameters)
{
	// CA-TAB-32 (R-TAB-29, orden): una casilla sellada en un Dominio con Elegido, para un Eco confinado sin Paso Libre, da Sealed.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	const int32 OwnedDomain = F.DomainOf(EDomainState::Owned);
	const int32 Tile = OwnedDomain != INDEX_NONE ? AnyTileOf(F, OwnedDomain) : INDEX_NONE;
	if (!TestTrue(TEXT("Hay una casilla en un Dominio con dueño"), Tile != INDEX_NONE))
	{
		return false;
	}
	MakeConfinedEcho(F, 1);
	F.Seal(Tile);
	const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(F.State, 1);
	TestTrue(TEXT("El Eco está confinado y sin Paso Libre"), Mover.bConfinedToForgotten && !Mover.bFreePassage);
	TestEqual(TEXT("Sellada y confinado: Sealed"), UnreachableReason(F.State, 1, Tile), EUnreachableReason::Sealed);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
