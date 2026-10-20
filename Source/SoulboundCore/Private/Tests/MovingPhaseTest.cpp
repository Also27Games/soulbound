// Tests del movimiento en curso (S-020): la fase Moving entre confirmar destino y resolver la casilla. CA-MOV-2 (R-TUR-7): la
// máquina guarda el camino, cada intención de avance en orden mueve la ficha y deja TileEntered, las demás se rechazan sin
// tocar nada, y al entrar en la última la casilla resuelve como siempre y queda Moved. CA-MOV-3 (salvaguarda): sin intenciones,
// la máquina recorre el camino por su cuenta, una casilla por FMatchParams::MoveStepSeconds.
// Montaje: ChoiceFixture.h (tablero de 35, partida de cuatro en la carrera, todos en el Altar). La tirada se fija con la de
// depuración (ForcedRoll), que consume el sorteo igual: lo que el test necesita es la longitud del camino, no el dado.
// Filtro: "Soulbound.Core.Match.Moving".

#include "Board/MovementRules.h"
#include "ChoiceFixture.h"
#include "Match/BondRaceRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::FChoiceFixture;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** El turno de Seat empieza en Origin con la tirada Roll fijada y llega a la elección de destino. Devuelve si llegó con opciones. */
	bool RollFrom(FChoiceFixture& F, int32 Seat, int32 Origin, int32 Roll)
	{
		F.State.Seats[Seat].TileId = Origin;
		F.BeginTurn(Seat);
		F.State.ForcedRoll = Roll;
		F.Advance();
		return F.State.TurnPhase == ETurnPhase::RollAndChoose && !F.State.ReachableOptions.IsEmpty();
	}

	/** Camino del asiento activo hasta Tile con la tirada del turno, con las reglas (el que el motor debe guardar). */
	TArray<int32> PathTo(const FChoiceFixture& F, int32 Tile)
	{
		const int32 Seat = F.State.ActiveSeat;
		return Soulbound::Rules::MovementPath(F.State.Board, F.State.Seats[Seat].TileId, F.State.Roll, Soulbound::Rules::MakeMoverTraits(F.State, Seat), Tile);
	}

	/** Primera casilla alcanzable cuyo camino mide Length; INDEX_NONE si no hay. */
	int32 DestinationWithPath(const FChoiceFixture& F, int32 Length)
	{
		for (const int32 Tile : F.State.ReachableOptions)
		{
			if (PathTo(F, Tile).Num() == Length)
			{
				return Tile;
			}
		}
		return INDEX_NONE;
	}

	int32 CountSince(const FChoiceFixture& F, int32 From, EMatchEventType Type)
	{
		int32 Count = 0;
		for (int32 Index = From; Index < F.State.EventLog.Num(); ++Index)
		{
			Count += F.State.EventLog[Index].Type == Type ? 1 : 0;
		}
		return Count;
	}

	/** Una casilla a dos pasos de Tile por un vecino suyo, que no sea el Altar ni la propia Tile; INDEX_NONE si no hay. */
	int32 TwoStepsFrom(const FBoardState& Board, int32 Tile)
	{
		for (const int32 Near : Board.Tiles[Tile].Neighbours)
		{
			for (const int32 Far : Board.Tiles[Near].Neighbours)
			{
				if (Far != Tile && !Board.IsAltar(Far) && !Board.IsAltar(Near))
				{
					return Far;
				}
			}
		}
		return INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovingProgressInOrderTest,
	"Soulbound.Core.Match.Moving.AvanceEnOrden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMovingProgressInOrderTest::RunTest(const FString& Parameters)
{
	// CA-MOV-2 (R-TUR-7, fase de movimiento): al confirmar destino la máquina pasa a Moving con el camino de las reglas y la ficha
	// sigue en el origen; cada intención de avance en orden la mueve y deja TileEntered; una de otro asiento, fuera de orden o de
	// otra casilla se rechaza sin cambiar nada; al entrar en la última, Moved y MoveAndResolve.
	FChoiceFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const int32 Seat = F.State.TurnOrder[0];
	const int32 Other = F.State.TurnOrder[1];
	const int32 Origin = F.State.Board.FindAltar();
	if (!TestTrue(TEXT("Llega a elegir destino con tirada 4"), RollFrom(F, Seat, Origin, 4)))
	{
		return false;
	}
	const int32 Destination = DestinationWithPath(F, 4);
	if (!TestTrue(TEXT("Hay un destino a cuatro casillas"), Destination != INDEX_NONE))
	{
		return false;
	}
	const TArray<int32> Path = PathTo(F, Destination);

	FMatchState Before = F.State;
	TestEqual(TEXT("Avance antes de confirmar destino: fase equivocada"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Path[0], F.Now)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("... sin cambiar nada"), Soulbound::Tests::SameState(Before, F.State));

	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("Confirma destino"), AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, Destination, F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Pasa a movimiento en curso"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Moving));
	TestEqual(TEXT("Guarda el camino de las reglas"), F.State.Movement.Path, Path);
	TestEqual(TEXT("Sin casillas entradas"), F.State.Movement.EnteredCount, 0);
	TestEqual(TEXT("La ficha sigue en el origen"), F.State.Seats[Seat].TileId, Origin);
	TestEqual(TEXT("Aún sin Moved"), CountSince(F, From, EMatchEventType::Moved), 0);
	TestFalse(TEXT("Aún no se ha movido"), F.State.bActiveSeatMoved);

	// Intenciones que no valen: el estado no cambia.
	Before = F.State;
	TestEqual(TEXT("De otro asiento: se rechaza"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Other, Path[0], F.Now)), AsInt(EIntentResult::WrongSeat));
	TestEqual(TEXT("Fuera de orden (la segunda): se rechaza"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Path[1], F.Now)), AsInt(EIntentResult::NotNextTile));
	TestEqual(TEXT("El destino de golpe: se rechaza"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Destination, F.Now)), AsInt(EIntentResult::NotNextTile));
	TestEqual(TEXT("El origen: se rechaza"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Origin, F.Now)), AsInt(EIntentResult::NotNextTile));
	TestEqual(TEXT("Una casilla inexistente: se rechaza"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, INDEX_NONE, F.Now)), AsInt(EIntentResult::NotNextTile));
	TestTrue(TEXT("Las rechazadas no cambian nada"), Soulbound::Tests::SameState(Before, F.State));

	for (int32 Index = 0; Index < Path.Num(); ++Index)
	{
		F.Now += 0.3;
		const FString Step = FString::Printf(TEXT("Casilla %d de %d"), Index + 1, Path.Num());
		TestEqual(FString::Printf(TEXT("%s: se acepta"), *Step), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Path[Index], F.Now)), AsInt(EIntentResult::Accepted));
		TestEqual(FString::Printf(TEXT("%s: la ficha está en ella"), *Step), F.State.Seats[Seat].TileId, Path[Index]);
		TestTrue(FString::Printf(TEXT("%s: se ha movido"), *Step), F.State.bActiveSeatMoved);
		const int32 Entered = F.State.EventLog.FindLastByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::TileEntered; });
		if (TestTrue(FString::Printf(TEXT("%s: deja TileEntered"), *Step), Entered != INDEX_NONE))
		{
			const FMatchEvent& Event = F.State.EventLog[Entered];
			TestTrue(FString::Printf(TEXT("%s: del asiento, con la casilla y la hora"), *Step), Event.Seat == Seat && Event.Value == Path[Index] && Event.ClockSeconds == F.Now);
		}
		if (Index < Path.Num() - 1)
		{
			TestEqual(FString::Printf(TEXT("%s: sigue en movimiento"), *Step), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Moving));
			TestEqual(FString::Printf(TEXT("%s: sin Moved"), *Step), CountSince(F, From, EMatchEventType::Moved), 0);
		}
	}
	TestEqual(TEXT("Un TileEntered por casilla"), CountSince(F, From, EMatchEventType::TileEntered), Path.Num());
	if (TestTrue(TEXT("Al final, TileEntered y Moved"), F.State.EventLog.Num() >= 2))
	{
		const FMatchEvent& Last = F.State.EventLog.Last();
		const FMatchEvent& Previous = F.State.EventLog[F.State.EventLog.Num() - 2];
		TestEqual(TEXT("El último es Moved"), AsInt(Last.Type), AsInt(EMatchEventType::Moved));
		TestTrue(TEXT("Moved con el asiento y el destino"), Last.Seat == Seat && Last.Value == Destination);
		TestTrue(TEXT("Justo detrás del TileEntered del destino"), Previous.Type == EMatchEventType::TileEntered && Previous.Value == Destination);
	}
	TestEqual(TEXT("Pasa a resolver la casilla"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
	TestFalse(TEXT("El movimiento termina"), F.State.Movement.IsActive());
	TestEqual(TEXT("Otro avance ya no vale"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Destination, F.Now)), AsInt(EIntentResult::WrongPhase));
	TestEqual(TEXT("Otro destino ya no vale"), AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, Destination, F.Now)), AsInt(EIntentResult::WrongPhase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovingLastTileResolvesTest,
	"Soulbound.Core.Match.Moving.LaUltimaCasillaResuelve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMovingLastTileResolvesTest::RunTest(const FString& Parameters)
{
	// CA-MOV-2 (R-TUR-7, R-CAR-4, R-RUN-2, R-TAB-7): al entrar en la última casilla del camino, resuelve como hoy: un Nicho Libre
	// se reclama, la Fuente de un Dominio con dueño da runa a un Elegido y la de un Olvidado le abre el minijuego.
	FChoiceFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	auto Walk = [&](int32 Seat, int32 Origin, int32 Roll, int32 Destination, const TCHAR* Context) -> bool
	{
		if (!TestTrue(FString::Printf(TEXT("%s: llega a elegir destino"), Context), RollFrom(F, Seat, Origin, Roll))
			|| !TestTrue(FString::Printf(TEXT("%s: el destino es alcanzable"), Context), F.State.ReachableOptions.Contains(Destination)))
		{
			return false;
		}
		// Nada resuelve al confirmar: la ficha aún no ha entrado en el destino.
		const bool bConfirmed = Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, Destination, F.Now) == EIntentResult::Accepted
			&& F.State.TurnPhase == ETurnPhase::Moving && F.State.Seats[Seat].TileId == Origin;
		const bool bWalked = bConfirmed && Soulbound::Tests::CompleteMovement(F.State, F.Now) && F.State.TurnPhase == ETurnPhase::MoveAndResolve;
		F.Advance();
		return TestTrue(FString::Printf(TEXT("%s: recorre el camino y resuelve"), Context), bWalked);
	};

	// Nicho: un Eco a dos casillas del Nicho de un Dominio con dueño se vincula al llegar.
	const int32 Seat = F.State.TurnOrder[0];
	const int32 Domain = F.DomainOf(EDomainState::Owned);
	const int32 Niche = F.State.Board.FindNiche(Domain);
	int32 From = F.State.EventLog.Num();
	if (Walk(Seat, TwoStepsFrom(F.State.Board, Niche), 2, Niche, TEXT("Nicho")))
	{
		TestEqual(TEXT("Nicho: se reclama"), CountSince(F, From, EMatchEventType::Bonded), 1);
		TestEqual(TEXT("Nicho: es Elegido"), AsInt(F.State.Seats[Seat].Role), AsInt(ESeatRole::Elect));
	}
	F.FinishTurn();

	// Fuente de un Olvidado: el Elegido juega el minijuego (antes que la otra Fuente: sin runas, ningún poder abre elecciones
	// antes de tirar).
	const int32 ForgottenSource = F.SourceOf(F.DomainOf(EDomainState::Forgotten));
	if (Walk(Seat, TwoStepsFrom(F.State.Board, ForgottenSource), 2, ForgottenSource, TEXT("Fuente de Olvidado")))
	{
		TestEqual(TEXT("Fuente de Olvidado: el minijuego"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
	}
	F.FinishTurn();

	// Fuente con dueño: el Elegido obtiene la runa al llegar.
	const int32 Source = F.SourceOf(Domain);
	From = F.State.EventLog.Num();
	if (Walk(Seat, TwoStepsFrom(F.State.Board, Source), 2, Source, TEXT("Fuente")))
	{
		TestEqual(TEXT("Fuente: obtiene la runa"), CountSince(F, From, EMatchEventType::RuneObtained), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovingSafeguardTest,
	"Soulbound.Core.Match.Moving.SalvaguardaSinIntenciones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMovingSafeguardTest::RunTest(const FString& Parameters)
{
	// CA-MOV-3 (salvaguarda): sin intenciones de avance, la máquina entra en cada casilla del camino por su cuenta cuando pasan
	// MoveStepSeconds desde la anterior, y llega igual: TileEntered en cada plazo, Moved y MoveAndResolve. Antes del plazo, nada.
	// Con una intención a tiempo, el plazo de la siguiente cuenta desde ella.
	FChoiceFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const double Step = F.State.Params.MoveStepSeconds;
	if (!TestTrue(TEXT("El paso de la salvaguarda es positivo"), Step > 0.0))
	{
		return false;
	}
	const int32 Seat = F.State.TurnOrder[0];
	if (!TestTrue(TEXT("Llega a elegir destino con tirada 3"), RollFrom(F, Seat, F.State.Board.FindAltar(), 3)))
	{
		return false;
	}
	const int32 Destination = DestinationWithPath(F, 3);
	const TArray<int32> Path = PathTo(F, Destination);
	const double Confirmed = F.Now;
	if (!TestEqual(TEXT("Confirma destino"), AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, Destination, Confirmed)), AsInt(EIntentResult::Accepted)))
	{
		return false;
	}
	const int32 From = F.State.EventLog.Num();
	for (int32 Index = 0; Index < Path.Num(); ++Index)
	{
		const double Due = Confirmed + Step * (Index + 1);
		const FMatchState Before = F.State;
		TestFalse(FString::Printf(TEXT("Casilla %d: antes del plazo no avanza"), Index + 1), Soulbound::Rules::AdvanceTurn(F.State, Due - 0.01, F.Random));
		TestTrue(FString::Printf(TEXT("Casilla %d: ... ni cambia nada"), Index + 1), Soulbound::Tests::SameState(Before, F.State));
		TestTrue(FString::Printf(TEXT("Casilla %d: en el plazo, avanza"), Index + 1), Soulbound::Rules::AdvanceTurn(F.State, Due, F.Random));
		TestEqual(FString::Printf(TEXT("Casilla %d: la ficha entra"), Index + 1), F.State.Seats[Seat].TileId, Path[Index]);
	}
	const TArray<FMatchEvent> Entered = F.State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::TileEntered; });
	if (TestEqual(TEXT("Un TileEntered por casilla"), Entered.Num(), Path.Num()))
	{
		for (int32 Index = 0; Index < Path.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("TileEntered %d: la casilla, a su hora"), Index + 1),
				Entered[Index].Value == Path[Index] && FMath::IsNearlyEqual(Entered[Index].ClockSeconds, Confirmed + Step * (Index + 1)));
		}
	}
	TestEqual(TEXT("Llega: Moved"), CountSince(F, From, EMatchEventType::Moved), 1);
	TestEqual(TEXT("Llega: a resolver la casilla"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::MoveAndResolve));
	F.Now = Confirmed + Step * Path.Num();
	F.FinishTurn();

	// Una intención a tiempo y el resto por la salvaguarda: el plazo de la siguiente cuenta desde la intención.
	if (!TestTrue(TEXT("Segundo turno: llega a elegir destino"), RollFrom(F, Seat, F.State.Board.FindAltar(), 3)))
	{
		return false;
	}
	const int32 Second = DestinationWithPath(F, 3);
	const TArray<int32> SecondPath = PathTo(F, Second);
	Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, Second, F.Now);
	const double First = F.Now + 0.1;
	TestEqual(TEXT("Segundo turno: el avance de la primera, a tiempo"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Seat, SecondPath[0], First)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("Segundo turno: la segunda no entra antes de su plazo"), Soulbound::Rules::AdvanceTurn(F.State, First + Step - 0.01, F.Random));
	TestTrue(TEXT("Segundo turno: entra en su plazo"), Soulbound::Rules::AdvanceTurn(F.State, First + Step, F.Random));
	TestEqual(TEXT("Segundo turno: la ficha en la segunda"), F.State.Seats[Seat].TileId, SecondPath[1]);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
