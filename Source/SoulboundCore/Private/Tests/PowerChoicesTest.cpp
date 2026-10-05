// Tests de las elecciones del jugador más allá del destino (S-009): Celeridad y la elección de dados, la repetición de
// Apep, Vuelo Planar y Expulsión, con la elección pendiente común (FPendingChoice, SubmitChoiceIntent) y sus relojes.
// GDD 04 (runas y poderes) y GDD 02 (turnos y dados). Cada test cita el criterio de aceptación (CA-XXX-n) y la regla
// (R-XXX-n) que cubre. Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Powers".
//
// Montaje: tablero de 35 casillas (dos Nodos Planares desde S-009), cuatro asientos y los Elegidos del archivo de datos.
// Los vínculos pasan por ResolveNicheLanding (LandOn en el Nicho); si el Dominio pedido salió Olvidado en el sorteo, el
// montaje lo devuelve a "con dueño" antes. Las runas y las posiciones de las fichas se fijan a mano: es lo que el test
// necesita, no lo que comprueba. Los turnos se juegan con la máquina de turnos (AdvanceTurn y las intenciones). El
// montaje (FChoiceFixture) vive desde S-010 en ChoiceFixture.h, compartido con los tests del trilero. Desde S-010 la
// expulsión es una opción de la acción contra un jugador (PlayerAction), contestada por su índice.

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/MatchStart.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "ChoiceFixture.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::LandOn;
	using Soulbound::Tests::SameState;

	/** Dominio de Apep en Content/Data/Elects/elegidos.json: afinidad rerollOnce (R-ELE-2). */
	constexpr int32 ApepDomain = 2;

	int32 AsInt(EIntentResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EChoiceKind Kind) { return static_cast<int32>(Kind); }
	int32 AsInt(ETurnPhase Phase) { return static_cast<int32>(Phase); }

	using Soulbound::Tests::FChoiceFixture;

	/** Los dos primeros valores de un dado que sacaría Random, sin tocarlo. */
	TPair<int32, int32> PeekDice(const FRandomStream& Random)
	{
		FRandomStream Copy = Random;
		const int32 First = Copy.RandRange(1, Soulbound::Dice::Faces);
		const int32 Second = Copy.RandRange(1, Soulbound::Dice::Faces);
		return {First, Second};
	}

	/** Opciones de expulsión contra los asientos Targets, en ese orden (S-010: la acción contra un jugador es un par). */
	TArray<FPlayerActionOption> ExpelOptions(const TArray<int32>& Targets)
	{
		TArray<FPlayerActionOption> Options;
		for (const int32 Target : Targets)
		{
			Options.Add({EPlayerAction::Expel, Target});
		}
		return Options;
	}

	/** Random tras sacar Count valores de un dado. */
	FRandomStream AfterDraws(const FRandomStream& Random, int32 Count)
	{
		FRandomStream Copy = Random;
		for (int32 Draw = 0; Draw < Count; ++Draw)
		{
			Copy.RandRange(1, Soulbound::Dice::Faces);
		}
		return Copy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersHasteDiceChoiceTest,
	"Soulbound.Core.Powers.CeleridadEligeDados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersHasteDiceChoiceTest::RunTest(const FString& Parameters)
{
	// CA-TUR-4 y CA-TUR-5 (R-POD-1, R-TUR-4, R-TUR-5): dado un asiento con Celeridad, cuando empieza su turno, la máquina
	// espera su elección de dados y tira uno o dos según elija; sin Celeridad no hay espera y tira uno. Dado un asiento que
	// pierde su última runa de Celeridad, en su siguiente turno vuelve a un dado sin elección.
	FChoiceFixture F;
	const int32 Own = F.Start(*this) ? F.BondAny(*this, 0) : INDEX_NONE;
	const int32 Other = Own != INDEX_NONE ? F.BondAny(*this, 1) : INDEX_NONE;
	if (Other == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Haste);
	TestTrue(TEXT("El asiento 0 tiene Celeridad"), Soulbound::Rules::IsPowerActive(F.State, 0, EPower::Haste));

	F.BeginTurn(0);
	TestEqual(TEXT("Con Celeridad: la máquina está en PreRoll"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::PreRoll));
	TestEqual(TEXT("... con la elección de dados pendiente"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::DiceCount));
	TestTrue(TEXT("... entre uno y dos dados"), F.State.PendingChoice.Options == TArray<int32>{1, 2});
	TestEqual(TEXT("... con el plazo del reloj de turno (R-TUR-8)"), F.State.PendingChoice.DeadlineSeconds, F.State.TurnDeadlineSeconds);
	const FMatchState Waiting = F.State;
	TestFalse(TEXT("AdvanceTurn espera la elección"), Soulbound::Rules::AdvanceTurn(F.State, F.Now + 1.0, F.Random));
	TestTrue(TEXT("... sin tirar ni cambiar nada"), SameState(Waiting, F.State));

	TestEqual(TEXT("Otro asiento no puede elegir"), AsInt(F.Choose(1, EChoiceKind::DiceCount, 2)), AsInt(EIntentResult::WrongSeat));
	TestEqual(TEXT("Otro tipo de elección se rechaza por fase"), AsInt(F.Choose(0, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::WrongPhase));
	TestEqual(TEXT("Tres dados no es una opción"), AsInt(F.Choose(0, EChoiceKind::DiceCount, 3)), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("\"No\" no vale en la elección de dados"), AsInt(F.Choose(0, EChoiceKind::DiceCount, INDEX_NONE)), AsInt(EIntentResult::InvalidOption));
	TestTrue(TEXT("Los rechazos no cambian nada"), SameState(Waiting, F.State));

	const FMatchState BeforeChoice = F.State;
	const FRandomStream RandomBeforeChoice = F.Random;
	const TPair<int32, int32> Dice = PeekDice(F.Random);

	TestEqual(TEXT("Elige dos dados: aceptado"), AsInt(F.Choose(0, EChoiceKind::DiceCount, 2)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("... el turno tira dos"), F.State.DiceCount, 2);
	TestFalse(TEXT("... y la elección se cierra"), F.State.PendingChoice.IsOpen());
	TestTrue(TEXT("Tira"), F.Advance());
	TestEqual(TEXT("... y pasa a RollAndChoose"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose));
	TestEqual(TEXT("Con dos dados, la tirada es la suma de los dos"), F.State.Roll, Dice.Key + Dice.Value);

	F.State = BeforeChoice;
	F.Random = RandomBeforeChoice;
	TestEqual(TEXT("Elige un dado: aceptado"), AsInt(F.Choose(0, EChoiceKind::DiceCount, 1)), AsInt(EIntentResult::Accepted));
	F.Advance();
	TestEqual(TEXT("Con un dado, la tirada es el primero"), F.State.Roll, Dice.Key);
	F.FinishTurn();

	// Sin Celeridad: ni espera ni elección, un dado.
	TestFalse(TEXT("El asiento 1 no tiene Celeridad"), Soulbound::Rules::IsPowerActive(F.State, 1, EPower::Haste));
	F.BeginTurn(1);
	TestFalse(TEXT("Sin Celeridad no hay elección"), F.State.PendingChoice.IsOpen());
	const TPair<int32, int32> NoHasteDice = PeekDice(F.Random);
	TestTrue(TEXT("... y tira sin esperar"), F.Advance());
	TestEqual(TEXT("... en RollAndChoose"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose));
	TestEqual(TEXT("... un dado"), F.State.DiceCount, 1);
	TestEqual(TEXT("... la tirada es el primer valor"), F.State.Roll, NoHasteDice.Key);
	F.FinishTurn();
	TestEqual(TEXT("Solo el asiento con Celeridad deja DiceChosen"), EventsOfType(F.State, EMatchEventType::DiceChosen).FilterByPredicate([](const FMatchEvent& Event) { return Event.Seat == 1; }).Num(), 0);

	// Pierde su última runa de Celeridad (la de su propio Dominio, R-ELE-3): en el turno siguiente, un dado sin elección.
	// Montaje: una sola, por si el turno anterior le dio otra.
	F.State.Seats[0].Runes[Own] = 1;
	TestTrue(TEXT("El asiento 0 pierde su única runa de Celeridad"), Soulbound::Rules::ReturnRuneToSource(F.State, 0, Own, F.SourceOf(Own), F.Now));
	F.BeginTurn(0);
	TestFalse(TEXT("Sin Celeridad ya no hay elección"), F.State.PendingChoice.IsOpen());
	F.Advance();
	TestEqual(TEXT("... y tira un dado"), F.State.DiceCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersEchoDiceChoiceTest,
	"Soulbound.Core.Powers.EcoConLiberacionEligeDados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersEchoDiceChoiceTest::RunTest(const FString& Parameters)
{
	// CA-ECO-6 (resto) (R-ECO-7): dado un Eco tras la carrera con su Runa de Liberación, elige uno o dos dados igual que
	// un Elegido.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random));
	constexpr int32 Echo = 1;
	if (!TestEqual(TEXT("El asiento 1 queda Eco"), static_cast<int32>(F.State.Seats[Echo].Role), static_cast<int32>(ESeatRole::LostEcho)))
	{
		return false;
	}
	F.BeginTurn(Echo);
	TestFalse(TEXT("Sin la Runa de Liberación, no hay elección"), F.State.PendingChoice.IsOpen());
	F.FinishTurn();

	F.GivePower(Echo, EPower::Haste);
	F.BeginTurn(Echo);
	TestEqual(TEXT("Con la Runa de Liberación, elige dados"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::DiceCount));
	const TPair<int32, int32> Dice = PeekDice(F.Random);
	TestEqual(TEXT("Elige dos: aceptado"), AsInt(F.Choose(Echo, EChoiceKind::DiceCount, 2)), AsInt(EIntentResult::Accepted));
	F.Advance();
	TestEqual(TEXT("... y tira dos"), F.State.Roll, Dice.Key + Dice.Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersDiceChoiceClockTest,
	"Soulbound.Core.Powers.RelojAgotadoEnLaEleccionDeDados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersDiceChoiceClockTest::RunTest(const FString& Parameters)
{
	// Criterio "Reloj en la elección de dados" (R-TUR-8): dado un asiento con Celeridad que no elige dados, cuando se
	// agota el reloj de turno, el turno termina sin tirar ni mover, con agotamiento registrado e insolencia subida. Y
	// consume del sorteo lo mismo que un turno con tirada (decisión del autor del 04/10/2026): cuatro valores.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Haste);
	F.BeginTurn(0);
	const double Deadline = F.State.TurnDeadlineSeconds;
	const int32 Tile = F.State.Seats[0].TileId;
	const int32 Insolence = F.State.Seats[0].Insolence;
	const FRandomStream Expected = AfterDraws(F.Random, 2 * Soulbound::Dice::MaxCount);

	TestFalse(TEXT("Antes del límite, espera"), Soulbound::Rules::AdvanceTurn(F.State, Deadline - 0.5, F.Random));
	const FMatchState AtLimit = F.State;
	TestEqual(TEXT("Una elección que llega en el límite se rechaza"), AsInt(Soulbound::Rules::SubmitChoiceIntent(F.State, 0, EChoiceKind::DiceCount, 2, Deadline, F.Random)),
		AsInt(EIntentResult::ClockExpired));
	TestTrue(TEXT("... sin cambiar nada"), SameState(AtLimit, F.State));

	TestTrue(TEXT("En el límite, la máquina avanza"), Soulbound::Rules::AdvanceTurn(F.State, Deadline, F.Random));
	const TArray<FMatchEvent> Expired = EventsOfType(F.State, EMatchEventType::TurnClockExpired);
	TestTrue(TEXT("Se registra el agotamiento del asiento 0"), Expired.Num() == 1 && Expired[0].Seat == 0 && Expired[0].ClockSeconds == Deadline);
	TestEqual(TEXT("Sube la insolencia"), F.State.Seats[0].Insolence, Insolence + 1);
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Sin tirar"), EventsOfType(F.State, EMatchEventType::Rolled).Num(), 0);
	TestEqual(TEXT("Sin mover"), F.State.Seats[0].TileId, Tile);
	TestFalse(TEXT("La elección se cierra"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Consume del sorteo cuatro valores, como un turno con tirada"), F.Random.GetCurrentSeed(), Expected.GetCurrentSeed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersApepRerollTest,
	"Soulbound.Core.Powers.ApepRepiteUnaVez",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersApepRerollTest::RunTest(const FString& Parameters)
{
	// CA-ELE-4 (R-TUR-6, R-ELE-2): dado Apep con tirada vista y sin destino confirmado, cuando repite, la tirada nueva
	// sustituye a la anterior, las alcanzables son las de la nueva y un segundo intento de repetir ese turno se rechaza;
	// en el turno siguiente puede otra vez. Otro Elegido no puede repetir. Repetir tras confirmar destino se rechaza.
	FChoiceFixture F;
	if (!F.Start(*this) || !F.Bond(*this, 0, ApepDomain) || F.BondAny(*this, 1) == INDEX_NONE)
	{
		return false;
	}
	TestTrue(TEXT("El asiento 0 es Apep: afinidad de repetir"), Soulbound::Rules::HasAffinity(F.State, 0, EAffinity::RerollOnce));
	TestFalse(TEXT("El asiento 1 no la tiene"), Soulbound::Rules::HasAffinity(F.State, 1, EAffinity::RerollOnce));
	const int32 Altar = F.State.Board.FindAltar();
	F.State.Seats[0].TileId = Altar;
	F.State.Seats[1].TileId = Altar;

	F.BeginTurn(0);
	const FRandomStream BeforeRoll = F.Random;
	const TPair<int32, int32> First = PeekDice(BeforeRoll);
	const TPair<int32, int32> Second = PeekDice(AfterDraws(BeforeRoll, Soulbound::Dice::MaxCount));
	F.Advance();
	TestEqual(TEXT("Tira un dado"), F.State.Roll, First.Key);
	TestEqual(TEXT("Se le ofrece repetir"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Reroll));
	TestTrue(TEXT("... con la opción \"repetir\""), F.State.PendingChoice.Options == TArray<int32>{1});
	TestEqual(TEXT("... dentro del reloj de turno"), F.State.PendingChoice.DeadlineSeconds, F.State.TurnDeadlineSeconds);
	const double Deadline = F.State.TurnDeadlineSeconds;

	F.Now += 1.0;
	TestEqual(TEXT("Repite: aceptado"), AsInt(F.Choose(0, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("La tirada nueva sustituye a la anterior, con los mismos dados"), F.State.Roll, Second.Key);
	const TArray<int32> Expected = Soulbound::Rules::ReachableTiles(F.State.Board, Altar, Second.Key, Soulbound::Rules::MakeMoverTraits(F.State, 0));
	TestTrue(TEXT("Las alcanzables son las de la tirada nueva"), F.State.ReachableOptions == Expected);
	TestEqual(TEXT("El reloj de turno sigue corriendo: mismo límite"), F.State.TurnDeadlineSeconds, Deadline);
	TestFalse(TEXT("La oferta se cierra"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Sigue esperando el destino"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose));

	const FMatchState Rerolled = F.State;
	TestEqual(TEXT("Un segundo intento de repetir se rechaza"), AsInt(F.Choose(0, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("... sin cambiar nada"), SameState(Rerolled, F.State));

	TestEqual(TEXT("Confirma destino"), AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, 0, F.State.ReachableOptions[0], F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Repetir tras confirmar destino se rechaza"), AsInt(F.Choose(0, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::WrongPhase));
	F.FinishTurn();

	// Turno siguiente: puede otra vez. Repetir tras confirmar destino, sin haber contestado la oferta, también se rechaza.
	F.State.Seats[0].TileId = Altar;
	F.BeginTurn(0);
	F.Advance();
	TestEqual(TEXT("En el turno siguiente se le ofrece otra vez"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Reroll));
	TestEqual(TEXT("Confirma destino sin contestar la oferta"), AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, 0, F.State.ReachableOptions[0], F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("... y repetir se rechaza"), AsInt(F.Choose(0, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::WrongPhase));
	F.FinishTurn();

	// Otro Elegido: sin oferta, y su intento se rechaza.
	F.BeginTurn(1);
	F.Advance();
	TestFalse(TEXT("A otro Elegido no se le ofrece repetir"), F.State.PendingChoice.IsOpen());
	const FMatchState Other = F.State;
	TestEqual(TEXT("... y si lo intenta, se rechaza"), AsInt(F.Choose(1, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("... sin cambiar nada"), SameState(Other, F.State));

	const TArray<FMatchEvent> Rerolls = EventsOfType(F.State, EMatchEventType::Rerolled);
	TestTrue(TEXT("Una sola repetición en el registro, la de Apep con la tirada nueva"), Rerolls.Num() == 1 && Rerolls[0].Seat == 0 && Rerolls[0].Value == Second.Key);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersApepNoOptionsTest,
	"Soulbound.Core.Powers.ApepSinAlcanzables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersApepNoOptionsTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 04/10/2026 (R-TUR-6 con R-TAB-18): Apep sin casillas alcanzables recibe la oferta de repetir
	// y la máquina la espera dentro del reloj de turno; "no" o reloj agotado, sin mover y sin insolencia. Si tras repetir
	// sigue sin alcanzables, el turno sigue sin esperar más. Los tres caminos consumen lo mismo del sorteo.
	FChoiceFixture F;
	if (!F.Start(*this) || !F.Bond(*this, 0, ApepDomain))
	{
		return false;
	}
	// Apep en su Nicho, con su única conexión sellada: ninguna tirada le deja moverse (R-TAB-17, R-TAB-18).
	const int32 Niche = F.State.Board.FindNiche(ApepDomain);
	F.State.Seats[0].TileId = Niche;
	F.Seal(F.State.Board.Tiles[Niche].Neighbours[0]);
	F.BeginTurn(0);
	F.Advance();
	TestTrue(TEXT("Sin alcanzables"), F.State.ReachableOptions.IsEmpty());
	TestEqual(TEXT("... se le ofrece repetir"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Reroll));
	const FMatchState Waiting = F.State;
	TestFalse(TEXT("... y la máquina la espera"), F.Advance());
	TestTrue(TEXT("... sin cambiar nada"), SameState(Waiting, F.State));
	const FRandomStream WaitingRandom = F.Random;
	const double Deadline = F.State.TurnDeadlineSeconds;

	auto ExpectNoMoveNoPunishment = [&](const TCHAR* Path, const FChoiceFixture& Done)
	{
		TestEqual(FString::Printf(TEXT("%s: pasa a MoveAndResolve"), Path), AsInt(Done.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
		TestEqual(FString::Printf(TEXT("%s: sin mover"), Path), Done.State.Seats[0].TileId, Niche);
		TestEqual(FString::Printf(TEXT("%s: sin insolencia"), Path), Done.State.Seats[0].Insolence, 0);
		TestEqual(FString::Printf(TEXT("%s: sin agotamiento del reloj de turno"), Path), EventsOfType(Done.State, EMatchEventType::TurnClockExpired).Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: consume cuatro valores del sorteo"), Path), Done.Random.GetCurrentSeed(),
			AfterDraws(WaitingRandom, Soulbound::Dice::MaxCount).GetCurrentSeed());
	};

	// "No".
	FChoiceFixture Declined = F;
	TestEqual(TEXT("\"No\": aceptado"), AsInt(Declined.Choose(0, EChoiceKind::Reroll, INDEX_NONE)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("\"No\": el turno sigue"), Declined.Advance());
	ExpectNoMoveNoPunishment(TEXT("\"No\""), Declined);

	// Reloj agotado: se resuelve como "no" y se registra como elección agotada.
	FChoiceFixture Expired = F;
	TestTrue(TEXT("Reloj agotado: el turno sigue"), Soulbound::Rules::AdvanceTurn(Expired.State, Deadline, Expired.Random));
	ExpectNoMoveNoPunishment(TEXT("Reloj agotado"), Expired);
	const TArray<FMatchEvent> ChoiceExpired = EventsOfType(Expired.State, EMatchEventType::ChoiceExpired);
	TestTrue(TEXT("Reloj agotado: se registra la elección agotada de repetir"), ChoiceExpired.Num() == 1 && ChoiceExpired[0].Value == AsInt(EChoiceKind::Reroll));

	// Repite y sigue sin alcanzables: el turno sigue sin esperar.
	FChoiceFixture Rerolled = F;
	TestEqual(TEXT("Repite: aceptado"), AsInt(Rerolled.Choose(0, EChoiceKind::Reroll, 1)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("Repite: sigue sin alcanzables"), Rerolled.State.ReachableOptions.IsEmpty());
	TestTrue(TEXT("Repite: el turno sigue sin esperar"), Rerolled.Advance());
	TestEqual(TEXT("Repite: pasa a MoveAndResolve"), AsInt(Rerolled.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
	TestEqual(TEXT("Repite: sin insolencia"), Rerolled.State.Seats[0].Insolence, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersFixedDrawsTest,
	"Soulbound.Core.Powers.ConsumoFijoDelSorteo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersFixedDrawsTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 04/10/2026 (R-TUR-4, R-TUR-6, grabación y reproducción del GDD 08): todo turno que llega a
	// PreRoll consume exactamente cuatro valores del sorteo de la partida, dos de la tirada y dos de la repetición, se
	// elijan uno o dos dados, se repita o no, y también si el reloj se agota antes de tirar. Dos copias de la misma
	// partida con decisores distintos (uno que elige siempre un dado y no repite, otro que elige dos y repite) y una
	// tercera que deja agotar la elección de dados y confirma destino sin contestar la oferta de repetir: tras cada turno,
	// el sorteo está en el mismo punto, así que los turnos siguientes tiran los mismos valores.
	FChoiceFixture Base;
	if (!Base.Start(*this) || !Base.Bond(*this, 0, ApepDomain) || Base.BondAny(*this, 1) == INDEX_NONE)
	{
		return false;
	}
	TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(Base.State, Base.Now, Base.Random));
	Base.GivePower(0, EPower::Haste);
	Base.GivePower(1, EPower::Haste);
	Base.GivePower(2, EPower::Haste);
	Base.State.TurnPhase = ETurnPhase::BetweenTurns;

	struct FPolicy
	{
		const TCHAR* Name;
		int32 Dice;
		bool bReroll;
		/** Deja agotar la elección de dados y, con la oferta de repetir abierta, confirma destino sin contestarla. */
		bool bIgnore;
	};
	const FPolicy Policies[] = {
		{TEXT("un dado, no repite"), 1, false, false},
		{TEXT("dos dados, repite"), 2, true, false},
		{TEXT("deja agotar y no contesta"), 1, false, true},
	};
	constexpr int32 Turns = 12;
	TArray<TArray<int32>> Seeds;
	TArray<FChoiceFixture> Played;
	for (const FPolicy& Policy : Policies)
	{
		FChoiceFixture F = Base;
		TArray<int32>& After = Seeds.AddDefaulted_GetRef();
		for (int32 Turn = 0; Turn < Turns; ++Turn)
		{
			F.Now += 1.0;
			F.Advance();
			for (int32 Step = 0; Step < 32 && F.State.TurnPhase != ETurnPhase::BetweenTurns; ++Step)
			{
				const int32 Seat = F.State.ActiveSeat;
				const EChoiceKind Kind = F.State.PendingChoice.Kind;
				if (Kind == EChoiceKind::DiceCount)
				{
					if (Policy.bIgnore)
					{
						F.Now = F.State.PendingChoice.DeadlineSeconds;
					}
					else
					{
						F.Choose(Seat, Kind, Policy.Dice);
					}
				}
				else if (Kind == EChoiceKind::Reroll && !Policy.bIgnore)
				{
					F.Choose(Seat, Kind, Policy.bReroll ? 1 : INDEX_NONE);
				}
				else if (F.State.TurnPhase == ETurnPhase::RollAndChoose && !F.State.ReachableOptions.IsEmpty())
				{
					Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, F.State.ReachableOptions[0], F.Now);
				}
				else if (F.State.TurnPhase == ETurnPhase::RollAndChoose)
				{
					F.Now = F.State.TurnDeadlineSeconds;
				}
				else if (F.State.PendingChoice.IsOpen())
				{
					F.Choose(Seat, Kind, INDEX_NONE);
				}
				else if (F.State.TurnPhase == ETurnPhase::SkillTrial)
				{
					Soulbound::Rules::SubmitSkillTrialResult(F.State, Seat, false, F.Now);
				}
				F.Advance();
			}
			After.Add(F.Random.GetCurrentSeed());
		}
		Played.Add(F);
	}

	TestTrue(TEXT("La política de dos dados eligió dos alguna vez"), EventsOfType(Played[1].State, EMatchEventType::DiceChosen).ContainsByPredicate([](const FMatchEvent& Event) { return Event.Value == 2; }));
	TestTrue(TEXT("... y repitió alguna vez"), EventsOfType(Played[1].State, EMatchEventType::Rerolled).Num() > 0);
	TestTrue(TEXT("La tercera dejó agotar el reloj alguna vez"), EventsOfType(Played[2].State, EMatchEventType::TurnClockExpired).Num() > 0);
	for (int32 Policy = 1; Policy < UE_ARRAY_COUNT(Policies); ++Policy)
	{
		for (int32 Turn = 0; Turn < Turns; ++Turn)
		{
			if (Seeds[Policy][Turn] != Seeds[0][Turn])
			{
				AddError(FString::Printf(TEXT("\"%s\" frente a \"%s\": tras el turno %d el sorteo está en otro punto"), Policies[Policy].Name, Policies[0].Name, Turn + 1));
				break;
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersPlanarJumpOfferTest,
	"Soulbound.Core.Powers.VueloPlanarOfreceLosDemasNodos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersPlanarJumpOfferTest::RunTest(const FString& Parameters)
{
	// CA-POD-3 y CA-TAB-16 (R-POD-5, R-TAB-15): dado un asiento con Vuelo Planar que termina en un Nodo Planar con el
	// tablero Despierto, se le ofrecen los demás Nodos Planares y "no saltar"; si salta, está en el destino y no se le
	// ofrece otro salto. Un Nodo Planar sellado solo se ofrece con Paso Libre (decisión del autor del 04/10/2026, R-TAB-21).
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	TestEqual(TEXT("El tablero de 35 casillas tiene dos Nodos Planares (S-009)"), F.PlanarNodes().Num(), 2);
	F.MakeAllNodesPlanar();
	const TArray<int32> Nodes = F.PlanarNodes();
	if (!TestEqual(TEXT("Tres Nodos Planares en el montaje"), Nodes.Num(), 3))
	{
		return false;
	}
	F.GivePower(0, EPower::PlanarFlight);

	TestTrue(TEXT("Termina en un Nodo Planar"), F.Land(0, Nodes[0]));
	TestEqual(TEXT("Se le ofrece saltar"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlanarJump));
	TestTrue(TEXT("... a los demás Nodos Planares"), F.State.PendingChoice.Options == TArray<int32>{Nodes[1], Nodes[2]});
	TestTrue(TEXT("... y admite \"no saltar\""), F.State.PendingChoice.CanDecline());
	TestEqual(TEXT("... con el reloj de elección"), F.State.PendingChoice.DeadlineSeconds, F.Now + F.State.Params.ChoiceClockSeconds);
	TestEqual(TEXT("... en MoveAndResolve"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
	const FMatchState Waiting = F.State;
	TestFalse(TEXT("La máquina espera"), Soulbound::Rules::AdvanceTurn(F.State, F.Now + 1.0, F.Random));
	TestEqual(TEXT("Su propio Nodo no es una opción"), AsInt(F.Choose(0, EChoiceKind::PlanarJump, Nodes[0])), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("Otro asiento no puede contestar"), AsInt(F.Choose(1, EChoiceKind::PlanarJump, Nodes[1])), AsInt(EIntentResult::WrongSeat));
	TestEqual(TEXT("Otro tipo de elección se rechaza por fase"), AsInt(F.Choose(0, EChoiceKind::PlayerAction, 0)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("Los rechazos no cambian nada"), SameState(Waiting, F.State));

	F.Now += 1.0;
	TestEqual(TEXT("Salta: aceptado"), AsInt(F.Choose(0, EChoiceKind::PlanarJump, Nodes[2])), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Está en el destino"), F.State.Seats[0].TileId, Nodes[2]);
	TestTrue(TEXT("No se le ofrece otro salto"), F.State.PendingChoice.Kind != EChoiceKind::PlanarJump);
	TestEqual(TEXT("Sin nadie a quien expulsar, el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	F.FinishTurn();

	// Decisión del autor del 04/10/2026: un Nodo Planar sellado no se ofrece sin Paso Libre; con él, sí.
	F.Seal(Nodes[1]);
	TestTrue(TEXT("Vuelve a terminar en el primer Nodo"), F.Land(0, Nodes[0]));
	TestTrue(TEXT("Sin Paso Libre, el Nodo sellado no se ofrece"), F.State.PendingChoice.Options == TArray<int32>{Nodes[2]});
	F.FinishTurn();
	F.GivePower(0, EPower::FreePassage);
	TestTrue(TEXT("Con Paso Libre, vuelve al primer Nodo"), F.Land(0, Nodes[0]));
	TestTrue(TEXT("Con Paso Libre, el Nodo sellado sí se ofrece"), F.State.PendingChoice.Options == TArray<int32>{Nodes[1], Nodes[2]});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersPlanarJumpNotOfferedTest,
	"Soulbound.Core.Powers.VueloPlanarNoSeOfrece",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersPlanarJumpNotOfferedTest::RunTest(const FString& Parameters)
{
	// CA-POD-3 y CA-TAB-16 (R-POD-5, R-TAB-15): sin el poder, en Primordial, o pasando por el Nodo sin acabar en él, no
	// se ofrece nada. Tampoco si el asiento no se mueve y se queda en el Nodo (no termina un movimiento, como en R-RUN-3).
	FChoiceFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const TArray<int32> Nodes = F.PlanarNodes();
	if (!TestEqual(TEXT("Dos Nodos Planares"), Nodes.Num(), 2))
	{
		return false;
	}

	// Primordial: un Elegido montado a mano con el poder, sin Despertar.
	FChoiceFixture Primordial = F;
	Primordial.State.Seats[0].Role = ESeatRole::Elect;
	Primordial.State.Seats[0].DomainId = Primordial.DomainOf(EDomainState::Owned);
	Primordial.GivePower(0, EPower::PlanarFlight);
	TestTrue(TEXT("Primordial: el tablero no ha despertado"), Primordial.State.Board.Phase == EBoardPhase::Primordial);
	TestTrue(TEXT("Primordial: termina en un Nodo Planar"), Primordial.Land(0, Nodes[0]));
	TestFalse(TEXT("Primordial: no se ofrece saltar"), Primordial.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Primordial: el turno termina"), AsInt(Primordial.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));

	if (F.BondAny(*this, 0) == INDEX_NONE || F.BondAny(*this, 1) == INDEX_NONE)
	{
		return false;
	}
	// Sin el poder.
	TestTrue(TEXT("Sin el poder: termina en un Nodo Planar"), F.Land(0, Nodes[0]));
	TestFalse(TEXT("Sin el poder: no se ofrece saltar"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Sin el poder: el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));

	// Pasando por el Nodo: el motor solo mira dónde termina el movimiento; una casilla vecina del Nodo no ofrece nada.
	F.GivePower(1, EPower::PlanarFlight);
	const int32 Beyond = F.State.Board.Tiles[Nodes[0]].Neighbours.Last();
	TestTrue(TEXT("Pasando por el Nodo: termina al otro lado"), F.Land(1, Beyond));
	TestFalse(TEXT("Pasando por el Nodo: no se ofrece saltar"), F.State.PendingChoice.IsOpen());

	// Sin moverse, sobre el Nodo: no termina un movimiento.
	F.State.Seats[1].TileId = Nodes[0];
	F.BeginTurn(1);
	F.Advance();
	F.State.ReachableOptions.Reset();
	F.Advance();
	TestEqual(TEXT("Sin moverse: MoveAndResolve"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
	F.Advance();
	TestFalse(TEXT("Sin moverse: no se ofrece saltar"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Sin moverse: el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Nadie ha saltado"), EventsOfType(F.State, EMatchEventType::PlanarJumped).Num(), 0);

	// Control: con el poder, el tablero Despierto y terminando el movimiento en el Nodo, sí se ofrece. Sin él, los "no se
	// ofrece" de arriba pasarían con un motor que nunca ofrece nada.
	F.FinishTurn();
	TestTrue(TEXT("Control: termina en el Nodo con el poder"), F.Land(1, Nodes[0]));
	TestEqual(TEXT("Control: se ofrece saltar"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlanarJump));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersExpelHomeTest,
	"Soulbound.Core.Powers.ExpulsionMandaAlHogar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersExpelHomeTest::RunTest(const FString& Parameters)
{
	// CA-POD-2 (R-POD-4, R-TAB-13): dado un asiento con Expulsión que termina en una casilla con otros, cuando expulsa a
	// un Elegido, este está en su Nicho; a un Eco tras la carrera, en el Nicho Cerrado de su Dominio asignado; a un Eco en
	// carrera, en el Altar. Un ocupante que ya está en su hogar no aparece entre las opciones. Sin el poder, no hay elección.
	FChoiceFixture F;
	const int32 Own = F.Start(*this) ? F.BondAny(*this, 0) : INDEX_NONE;
	const int32 Other = Own != INDEX_NONE ? F.BondAny(*this, 1) : INDEX_NONE;
	if (Other == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Expel);
	const TArray<int32> Normal = F.NormalTiles();
	const int32 Altar = F.State.Board.FindAltar();

	// Elegido → su Nicho.
	F.State.Seats[1].TileId = Normal[0];
	TestTrue(TEXT("Termina donde está el Elegido del asiento 1"), F.Land(0, Normal[0]));
	TestEqual(TEXT("Se le ofrece expulsar"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
	TestTrue(TEXT("... al asiento 1"), F.State.PendingChoice.ActionOptions == ExpelOptions({1}));
	TestEqual(TEXT("... con el reloj de elección"), F.State.PendingChoice.DeadlineSeconds, F.Now + F.State.Params.ChoiceClockSeconds);
	TestEqual(TEXT("Expulsa al Elegido: aceptado"), AsInt(F.Act(0, EPlayerAction::Expel, 1)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El Elegido está en su Nicho"), F.State.Seats[1].TileId, F.State.Board.FindNiche(Other));
	TestEqual(TEXT("Quien expulsa sigue en su casilla"), F.State.Seats[0].TileId, Normal[0]);
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	F.FinishTurn();

	// Eco en carrera → el Altar.
	F.State.Seats[2].TileId = Normal[1];
	TestTrue(TEXT("Termina donde está el Eco en carrera"), F.Land(0, Normal[1]));
	TestTrue(TEXT("Se le ofrece expulsar al asiento 2"), F.State.PendingChoice.ActionOptions == ExpelOptions({2}));
	TestEqual(TEXT("Expulsa al Eco en carrera"), AsInt(F.Act(0, EPlayerAction::Expel, 2)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El Eco en carrera está en el Altar"), F.State.Seats[2].TileId, Altar);
	F.FinishTurn();

	// Quien ya está en su hogar no es objetivo: en el Altar, el Eco en carrera (su hogar) y el Elegido 1 (no lo es).
	F.State.Seats[1].TileId = Altar;
	TestTrue(TEXT("Termina en el Altar"), F.Land(0, Altar));
	TestTrue(TEXT("Solo se ofrece al que no está en su hogar"), F.State.PendingChoice.ActionOptions == ExpelOptions({1}));
	F.FinishTurn();

	// Eco tras la carrera → el Nicho Cerrado de su Dominio asignado.
	TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random));
	const int32 Echo = 3;
	const int32 EchoHome = F.State.Board.FindNiche(F.State.Seats[Echo].DomainId);
	F.State.Seats[Echo].TileId = Normal[2];
	TestTrue(TEXT("Termina donde está el Eco tras la carrera"), F.Land(0, Normal[2]));
	TestTrue(TEXT("Se le ofrece expulsar al asiento 3"), F.State.PendingChoice.ActionOptions == ExpelOptions({Echo}));
	TestEqual(TEXT("Expulsa al Eco"), AsInt(F.Act(0, EPlayerAction::Expel, Echo)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El Eco está en el Nicho Cerrado de su Dominio asignado"), F.State.Seats[Echo].TileId, EchoHome);
	TestEqual(TEXT("... que es su hogar"), F.State.Seats[Echo].TileId, Soulbound::Rules::HomeTile(F.State, Echo));
	F.FinishTurn();

	// Sin el poder: el asiento 1 termina con otro y no hay elección.
	F.State.Seats[Echo].TileId = Normal[3];
	TestTrue(TEXT("Sin el poder: termina con el Eco"), F.Land(1, Normal[3]));
	TestFalse(TEXT("Sin el poder: no hay elección"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Sin el poder: el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersExpelOneOrNoneTest,
	"Soulbound.Core.Powers.ExpulsionAUnoONinguno",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersExpelOneOrNoneTest::RunTest(const FString& Parameters)
{
	// CA-POD-1 (parte) (R-INT-2): dado un asiento con Expulsión en una casilla con dos ocupantes, puede expulsar a uno solo
	// o a ninguno.
	FChoiceFixture F;
	const int32 Own = F.Start(*this) ? F.BondAny(*this, 0) : INDEX_NONE;
	const int32 Other = Own != INDEX_NONE ? F.BondAny(*this, 1) : INDEX_NONE;
	if (Other == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Expel);
	const int32 Tile = F.NormalTiles()[0];
	F.State.Seats[1].TileId = Tile;
	F.State.Seats[2].TileId = Tile;
	TestTrue(TEXT("Termina con dos ocupantes"), F.Land(0, Tile));
	TestTrue(TEXT("Se le ofrecen los dos"), F.State.PendingChoice.ActionOptions == ExpelOptions({1, 2}));
	TestTrue(TEXT("... y \"ninguno\""), F.State.PendingChoice.CanDecline());
	const FChoiceFixture Waiting = F;

	TestEqual(TEXT("Ninguno: aceptado"), AsInt(F.Choose(0, EChoiceKind::PlayerAction, INDEX_NONE)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("Ninguno: los dos siguen ahí"), F.State.Seats[1].TileId == Tile && F.State.Seats[2].TileId == Tile);
	TestEqual(TEXT("Ninguno: el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Ninguno: nadie expulsado"), EventsOfType(F.State, EMatchEventType::Expelled).Num(), 0);

	F = Waiting;
	TestEqual(TEXT("Uno: aceptado"), AsInt(F.Act(0, EPlayerAction::Expel, 2)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Uno: el asiento 2 está en su hogar"), F.State.Seats[2].TileId, Soulbound::Rules::HomeTile(F.State, 2));
	TestEqual(TEXT("Uno: el asiento 1 sigue ahí"), F.State.Seats[1].TileId, Tile);
	TestFalse(TEXT("Uno: no se ofrece otra expulsión"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Uno: el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Uno: el segundo intento se rechaza"), AsInt(F.Act(0, EPlayerAction::Expel, 1)), AsInt(EIntentResult::WrongPhase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersTileThenActionTest,
	"Soulbound.Core.Powers.PrimeroLaCasillaDespuesLaAccion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersTileThenActionTest::RunTest(const FString& Parameters)
{
	// CA-INT-1 (R-INT-1): dado un asiento con Expulsión que termina en una Fuente ocupada, primero obtiene la runa (o
	// juega su minijuego) y después se le ofrece la acción. Tras un salto planar a un Nodo ocupado, se le ofrece la acción
	// allí. El salto va antes que la acción.
	FChoiceFixture F;
	const int32 Own = F.Start(*this) ? F.BondAny(*this, 0) : INDEX_NONE;
	const int32 Other = Own != INDEX_NONE ? F.BondAny(*this, 1) : INDEX_NONE;
	if (Other == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Expel);

	// Fuente con dueño, ocupada: la runa primero.
	const int32 OwnedSource = F.SourceOf(Other);
	F.State.Seats[1].TileId = OwnedSource;
	const int32 RuneBefore = F.State.Seats[0].RuneCountOf(Other);
	TestTrue(TEXT("Termina en una Fuente con dueño ocupada"), F.Land(0, OwnedSource));
	TestEqual(TEXT("Ya ha obtenido la runa"), F.State.Seats[0].RuneCountOf(Other), RuneBefore + 1);
	TestEqual(TEXT("... y después se le ofrece la acción"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
	const TArray<FMatchEvent>& Log = F.State.EventLog;
	TestTrue(TEXT("El último evento es la runa obtenida"), Log.Num() > 0 && Log.Last().Type == EMatchEventType::RuneObtained);
	F.FinishTurn();

	// Fuente de un Olvidado, ocupada: el minijuego primero.
	const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
	if (!TestTrue(TEXT("Hay un Dominio Olvidado"), Forgotten != INDEX_NONE))
	{
		return false;
	}
	const int32 ForgottenSource = F.SourceOf(Forgotten);
	F.State.Seats[2].TileId = ForgottenSource;
	TestTrue(TEXT("Termina en la Fuente de un Olvidado ocupada"), F.Land(0, ForgottenSource));
	TestEqual(TEXT("Primero el minijuego"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
	TestFalse(TEXT("... sin elección todavía"), F.State.PendingChoice.IsOpen());
	F.Now += 1.0;
	TestEqual(TEXT("Supera el minijuego"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, 0, true, F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Obtiene la runa"), F.State.Seats[0].RuneCountOf(Forgotten), 1);
	TestEqual(TEXT("Después se le ofrece la acción"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
	TestTrue(TEXT("... contra el asiento 2"), F.State.PendingChoice.ActionOptions == ExpelOptions({2}));
	TestEqual(TEXT("... y la máquina la espera en MoveAndResolve"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
	F.FinishTurn();

	// Salto planar: primero el salto, después la acción en el destino.
	F.GivePower(0, EPower::PlanarFlight);
	const TArray<int32> Nodes = F.PlanarNodes();
	F.State.Seats[2].TileId = Nodes[0];
	F.State.Seats[1].TileId = Nodes[1];
	const FChoiceFixture BeforeLanding = F;
	TestTrue(TEXT("Termina en un Nodo Planar ocupado"), F.Land(0, Nodes[0]));
	TestEqual(TEXT("Primero se ofrece el salto"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlanarJump));
	F.Now += 1.0;
	TestEqual(TEXT("Salta al otro Nodo, ocupado"), AsInt(F.Choose(0, EChoiceKind::PlanarJump, Nodes[1])), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Tras el salto, se le ofrece la acción"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
	TestTrue(TEXT("... contra quien está en el destino"), F.State.PendingChoice.ActionOptions == ExpelOptions({1}));

	F = BeforeLanding;
	TestTrue(TEXT("Otra vez en el Nodo ocupado"), F.Land(0, Nodes[0]));
	TestEqual(TEXT("No salta"), AsInt(F.Choose(0, EChoiceKind::PlanarJump, INDEX_NONE)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("Sin salto, la acción es contra quien está en su Nodo"), F.State.PendingChoice.Kind == EChoiceKind::PlayerAction && F.State.PendingChoice.ActionOptions == ExpelOptions({2}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersExpelTurnsTest,
	"Soulbound.Core.Powers.ExpulsionNoCambiaTurnos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersExpelTurnsTest::RunTest(const FString& Parameters)
{
	// CA-INT-3 (R-INT-4): tras cualquier expulsión, los turnos extra y perdidos del expulsado no cambian, y su turno le
	// llega en su orden.
	FChoiceFixture F;
	const int32 Own = F.Start(*this) ? F.BondAny(*this, 0) : INDEX_NONE;
	const int32 Other = Own != INDEX_NONE ? F.BondAny(*this, 1) : INDEX_NONE;
	if (Other == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::Expel);
	F.State.TurnOrder = {0, 1, 2, 3};
	F.State.Seats[1].ExtraTurns = 1;
	F.State.Seats[2].LostTurns = 1;
	const TArray<int32> Normal = F.NormalTiles();

	F.State.Seats[2].TileId = Normal[0];
	TestTrue(TEXT("Termina con el asiento 2"), F.Land(0, Normal[0]));
	TestEqual(TEXT("Expulsa al asiento 2"), AsInt(F.Act(0, EPlayerAction::Expel, 2)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El asiento 2 conserva su turno perdido"), F.State.Seats[2].LostTurns, 1);
	TestEqual(TEXT("... y no gana extra"), F.State.Seats[2].ExtraTurns, 0);
	F.FinishTurn();

	F.State.Seats[1].TileId = Normal[1];
	TestTrue(TEXT("Termina con el asiento 1"), F.Land(0, Normal[1]));
	TestEqual(TEXT("Expulsa al asiento 1"), AsInt(F.Act(0, EPlayerAction::Expel, 1)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El asiento 1 conserva su turno extra"), F.State.Seats[1].ExtraTurns, 1);
	TestEqual(TEXT("... y no gana perdidos"), F.State.Seats[1].LostTurns, 0);
	TestEqual(TEXT("Quien expulsa no gana turnos"), F.State.Seats[0].ExtraTurns + F.State.Seats[0].LostTurns, 0);

	F.Advance();
	TestEqual(TEXT("Termina el turno del asiento 0"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	F.Advance();
	const TArray<FMatchEvent> Started = EventsOfType(F.State, EMatchEventType::TurnStarted);
	TestTrue(TEXT("El siguiente turno es el del asiento 1, en su orden"), Started.Num() > 0 && Started.Last().Seat == 1);
	TestEqual(TEXT("... con su turno extra intacto"), F.State.Seats[1].ExtraTurns, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersChoiceClockTest,
	"Soulbound.Core.Powers.EleccionAgotadaEsNo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersChoiceClockTest::RunTest(const FString& Parameters)
{
	// Criterio "Reloj de elección" (decisión del autor del 04/10/2026): dada una elección de salto o de expulsión, cuando
	// pasan choiceClockSeconds sin respuesta, se resuelve como "no", se registra, y el turno sigue; la insolencia no cambia.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	F.GivePower(0, EPower::PlanarFlight);
	F.GivePower(0, EPower::Expel);
	const TArray<int32> Nodes = F.PlanarNodes();
	F.State.Seats[1].TileId = Nodes[0];
	TestTrue(TEXT("Termina en un Nodo Planar ocupado"), F.Land(0, Nodes[0]));
	const double JumpDeadline = F.Now + F.State.Params.ChoiceClockSeconds;
	TestEqual(TEXT("Se le ofrece saltar con el reloj de elección"), F.State.PendingChoice.DeadlineSeconds, JumpDeadline);

	TestFalse(TEXT("Antes del plazo, espera"), Soulbound::Rules::AdvanceTurn(F.State, JumpDeadline - 0.01, F.Random));
	const FMatchState AtLimit = F.State;
	TestEqual(TEXT("Una respuesta en el plazo se rechaza"), AsInt(Soulbound::Rules::SubmitChoiceIntent(F.State, 0, EChoiceKind::PlanarJump, Nodes[1], JumpDeadline, F.Random)),
		AsInt(EIntentResult::ClockExpired));
	TestTrue(TEXT("... sin cambiar nada"), SameState(AtLimit, F.State));

	TestTrue(TEXT("En el plazo, la máquina avanza"), Soulbound::Rules::AdvanceTurn(F.State, JumpDeadline, F.Random));
	TestEqual(TEXT("Salto agotado: no salta"), F.State.Seats[0].TileId, Nodes[0]);
	TestEqual(TEXT("... y el turno sigue con la acción"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
	const double ExpelDeadline = JumpDeadline + F.State.Params.ChoiceClockSeconds;
	TestEqual(TEXT("... con su propio reloj de elección"), F.State.PendingChoice.DeadlineSeconds, ExpelDeadline);

	TestTrue(TEXT("Agota también la acción"), Soulbound::Rules::AdvanceTurn(F.State, ExpelDeadline, F.Random));
	TestEqual(TEXT("Acción agotada: nadie expulsado"), F.State.Seats[1].TileId, Nodes[0]);
	TestEqual(TEXT("... y el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("La insolencia no cambia"), F.State.Seats[0].Insolence, 0);

	const TArray<FMatchEvent> Expired = EventsOfType(F.State, EMatchEventType::ChoiceExpired);
	if (TestEqual(TEXT("Dos elecciones agotadas en el registro"), Expired.Num(), 2))
	{
		TestTrue(TEXT("La del salto, con su asiento y su plazo"), Expired[0].Seat == 0 && Expired[0].Value == AsInt(EChoiceKind::PlanarJump) && Expired[0].ClockSeconds == JumpDeadline);
		TestTrue(TEXT("La de la acción, con su asiento y su plazo"), Expired[1].Seat == 0 && Expired[1].Value == AsInt(EChoiceKind::PlayerAction) && Expired[1].ClockSeconds == ExpelDeadline);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPowersEventLogTest,
	"Soulbound.Core.Powers.RegistroDeLasElecciones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPowersEventLogTest::RunTest(const FString& Parameters)
{
	// Criterio "Registro" (R-TUR-15): los cinco eventos nuevos, con asiento, dato y marca de tiempo: dados elegidos,
	// repetición, salto planar, expulsión y elección agotada.
	FChoiceFixture F;
	if (!F.Start(*this) || !F.Bond(*this, 1, ApepDomain) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	auto Expect = [this, &F](EMatchEventType Type, const TCHAR* Name, int32 Seat, int32 Value, double At, int32 OtherSeat = INDEX_NONE)
	{
		const TArray<FMatchEvent> Events = EventsOfType(F.State, Type);
		if (TestEqual(FString::Printf(TEXT("%s: un evento"), Name), Events.Num(), 1))
		{
			TestEqual(FString::Printf(TEXT("%s: asiento"), Name), Events[0].Seat, Seat);
			TestEqual(FString::Printf(TEXT("%s: dato"), Name), Events[0].Value, Value);
			TestEqual(FString::Printf(TEXT("%s: segundo asiento"), Name), Events[0].OtherSeat, OtherSeat);
			TestEqual(FString::Printf(TEXT("%s: marca de tiempo"), Name), Events[0].ClockSeconds, At);
		}
	};

	// Dados elegidos.
	F.GivePower(0, EPower::Haste);
	F.BeginTurn(0);
	F.Now += 1.0;
	F.Choose(0, EChoiceKind::DiceCount, 2);
	Expect(EMatchEventType::DiceChosen, TEXT("DiceChosen"), 0, 2, F.Now);
	F.FinishTurn();

	// Repetición.
	F.State.Seats[1].TileId = F.State.Board.FindAltar();
	F.BeginTurn(1);
	F.Advance();
	F.Now += 1.0;
	F.Choose(1, EChoiceKind::Reroll, 1);
	Expect(EMatchEventType::Rerolled, TEXT("Rerolled"), 1, F.State.Roll, F.Now);
	F.FinishTurn();

	// Salto planar.
	F.GivePower(0, EPower::PlanarFlight);
	F.GivePower(0, EPower::Expel);
	const TArray<int32> Nodes = F.PlanarNodes();
	F.State.Seats[1].TileId = Nodes[1];
	F.Land(0, Nodes[0]);
	F.Now += 1.0;
	F.Choose(0, EChoiceKind::PlanarJump, Nodes[1]);
	Expect(EMatchEventType::PlanarJumped, TEXT("PlanarJumped"), 0, Nodes[1], F.Now);

	// Expulsión: asiento de quien expulsa y, desde S-010, el expulsado en OtherSeat; sin dato.
	F.Now += 1.0;
	F.Act(0, EPlayerAction::Expel, 1);
	Expect(EMatchEventType::Expelled, TEXT("Expelled"), 0, INDEX_NONE, F.Now, 1);
	F.FinishTurn();

	// Elección agotada: dato el tipo de elección, marca el plazo.
	F.State.Seats[1].TileId = Nodes[0];
	F.Land(0, Nodes[0]);
	const double Deadline = F.State.PendingChoice.DeadlineSeconds;
	Soulbound::Rules::AdvanceTurn(F.State, Deadline, F.Random);
	Expect(EMatchEventType::ChoiceExpired, TEXT("ChoiceExpired"), 0, AsInt(EChoiceKind::PlanarJump), Deadline);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
