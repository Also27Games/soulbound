// Tests del panel de decisión (S-028, R-DEC-1 y R-DEC-3): qué decisión del humano se enseña, con qué opciones, teclas y plazo, cuándo se
// enseña y el reloj de su barra. Funciones puras sobre un FMatchState armado a mano: sin mundo, sin actor y sin disco, como StatusTextTest.
// Filtro: "Soulbound.Game.Presentation.Decision".

#include "../../Presentation/DecisionText.h"
#include "../../Presentation/EventText.h"
#include "../../Presentation/StatusText.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags DecisionTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::EDecisionAnswer;
	using Soulbound::Presentation::EDecisionInput;
	using Soulbound::Presentation::FDecisionOption;
	using Soulbound::Presentation::FDecisionView;

	constexpr int32 DtHuman = 0;
	constexpr double DtNow = 100.0;
	/** Posición en la tabla de eventos del dilema y de la votación de prueba. */
	constexpr int32 DtDilemma = 0;
	constexpr int32 DtVote = 1;

	/** Cuatro asientos entre turnos, el humano el 0, sin ninguna decisión abierta; la tabla de eventos con un dilema y una votación. */
	FMatchState DtMakeState()
	{
		FMatchState State;
		State.Seats.SetNum(4);
		State.MatchPhase = EMatchPhase::RuneHunt;
		State.ActiveSeat = 1;
		State.TurnPhase = ETurnPhase::BetweenTurns;
		State.ClockSeconds = DtNow;

		FWardenEventDef Dilemma;
		Dilemma.Id = TEXT("prueba-dilema");
		Dilemma.Name = TEXT("La balanza");
		Dilemma.Text = TEXT("Elige tu castigo, o tu premio.");
		FWardenEventOption Gift;
		Gift.Applications.Add({EEffect::ExtraTurn, EEffectTarget::ActivePlayer});
		FWardenEventOption Harm;
		Harm.Applications.Add({EEffect::LoseTurn, EEffectTarget::NextPlayer});
		Dilemma.Options = {Gift, Harm};
		State.WardenEvents.Events.Add(Dilemma);

		FWardenEventDef Vote;
		Vote.Id = TEXT("prueba-votacion");
		Vote.Name = TEXT("El juicio");
		Vote.Text = TEXT("Votad a quien pierde su próximo turno.");
		Vote.bVote = true;
		FWardenEventOption Verdict;
		Verdict.Applications.Add({EEffect::LoseTurn, EEffectTarget::MostVoted});
		Vote.Options = {Verdict};
		State.WardenEvents.Events.Add(Vote);
		return State;
	}

	/** Abre para Seat una elección de dados con Celeridad: uno o dos, 12 s de plazo. */
	void DtOpenDice(FMatchState& State, int32 Seat)
	{
		State.ActiveSeat = Seat;
		State.TurnPhase = ETurnPhase::PreRoll;
		State.PendingChoice.Kind = EChoiceKind::DiceCount;
		State.PendingChoice.Seat = Seat;
		State.PendingChoice.Options = {1, 2};
		State.PendingChoice.DeadlineSeconds = DtNow + 12.0;
	}

	/** Abre la votación de prueba: se puede votar a los asientos 1, 2 y 3; nadie ha votado. */
	void DtOpenVote(FMatchState& State)
	{
		State.CurrentEvent.EventIndex = DtVote;
		State.CurrentEvent.Option = 0;
		State.Vote.bOpen = true;
		State.Vote.DeadlineSeconds = DtNow + 20.0;
		State.Vote.Votables = {1, 2, 3};
		State.Vote.Votes.Init(INDEX_NONE, State.Seats.Num());
	}

	/** Abre una consulta del Oráculo para el humano, que se puede rechazar o no. */
	void DtOpenOracle(FMatchState& State, bool bRefusable)
	{
		State.Oracle.Stage = EOracleStage::Waiting;
		State.Oracle.Consultant = DtHuman;
		State.Oracle.Origin = EOracleOrigin::Node;
		State.Oracle.DeadlineSeconds = DtNow + 8.0;
		State.Oracle.bRefusable = bRefusable;
	}

	/** El humano en su turno de tirada: elige casilla con 15 s de reloj de turno. */
	void DtOpenTile(FMatchState& State)
	{
		State.ActiveSeat = DtHuman;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.Roll = 4;
		State.ReachableOptions = {1, 2};
		State.TurnDeadlineSeconds = DtNow + 15.0;
	}

	TArray<FString> DtTexts(const FDecisionView& View)
	{
		TArray<FString> Texts;
		for (const FDecisionOption& Option : View.Options)
		{
			Texts.Add(Option.Text);
		}
		return Texts;
	}

	TArray<FString> DtKeys(const FDecisionView& View)
	{
		TArray<FString> Keys;
		for (const FDecisionOption& Option : View.Options)
		{
			Keys.Add(Option.Key);
		}
		return Keys;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionDiceTest, "Soulbound.Game.Presentation.Decision.DadosDosBotones", DecisionTestFlags)

bool FDecisionDiceTest::RunTest(const FString& Parameters)
{
	// CA-DEC-1 (R-DEC-1, R-DEC-2): con una elección DiceCount abierta para el humano, la vista es de botones, con las opciones del motor
	// numeradas desde 1 y el plazo de la elección: "Un dado" con la tecla 1 y "Dos dados" con la tecla 2, sin "no".
	FMatchState State = DtMakeState();
	DtOpenDice(State, DtHuman);
	const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	if (!TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("Con botones"), View->Input == EDecisionInput::Buttons);
	// Corrección del autor del 11/11/2026: el título del panel pregunta; la línea del panel de estado sigue diciendo "ELIGE dados".
	TestEqual(TEXT("El título pregunta"), View->Title, FString(TEXT("¿Cuántos dados tiras?")));
	{
		Soulbound::Presentation::FStatusContext Context;
		Context.HumanSeat = DtHuman;
		const TArray<Soulbound::Presentation::FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, Context);
		TestTrue(TEXT("La línea del panel de estado no cambia"), Lines.Num() > 0 && Lines[0].Text.StartsWith(TEXT("ELIGE dados")));
	}
	TestEqual(TEXT("Las opciones del motor"), DtTexts(*View), TArray<FString>{TEXT("Un dado"), TEXT("Dos dados")});
	TestEqual(TEXT("Numeradas desde 1"), DtKeys(*View), TArray<FString>{TEXT("1"), TEXT("2")});
	if (View->Options.Num() == 2)
	{
		TestTrue(TEXT("Cada una contesta la elección de dados con su número"), View->Options[0].Answer == EDecisionAnswer::Choice
			&& View->Options[0].ChoiceKind == EChoiceKind::DiceCount && View->Options[0].Value == 1 && View->Options[1].Value == 2);
	}
	TestEqual(TEXT("El plazo de la elección"), View->DeadlineSeconds, DtNow + 12.0);
	TestFalse(TEXT("Sin la espera del Warden, no está bloqueada"), View->bBlocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionDilemmaTest, "Soulbound.Game.Presentation.Decision.DilemaTituloYOpciones", DecisionTestFlags)

bool FDecisionDilemmaTest::RunTest(const FString& Parameters)
{
	// CA-DEC-2 (R-DEC-1): con un dilema abierto, el título es el nombre del evento, la línea de contexto su texto, y las opciones son las del
	// dilema en su orden (DescribeOption, el texto de siempre), con las teclas 1 y 2 y sin "no" (desatender al Warden no es contestar).
	FMatchState State = DtMakeState();
	State.CurrentEvent.EventIndex = DtDilemma;
	State.CurrentEvent.Director = DtHuman;
	State.PendingChoice.Kind = EChoiceKind::Dilemma;
	State.PendingChoice.Seat = DtHuman;
	State.PendingChoice.Options = {0, 1};
	State.PendingChoice.DeadlineSeconds = DtNow + 10.0;
	const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	if (!TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		return false;
	}
	const FWardenEventDef& Def = State.WardenEvents.Events[DtDilemma];
	TestEqual(TEXT("El título es el nombre del evento"), View->Title, Def.Name);
	TestEqual(TEXT("El contexto es su texto"), View->Context, Def.Text);
	// Desde R-DEC-11 (S-033, 12/11/2026) el botón lleva el texto para el jugador: sin "al activo" cuando el efecto recae en quien elige.
	TestEqual(TEXT("Las opciones del dilema, en su orden"), DtTexts(*View),
		TArray<FString>{Soulbound::Presentation::DescribeOptionForPlayer(Def.Options[0]), Soulbound::Presentation::DescribeOptionForPlayer(Def.Options[1])});
	TestEqual(TEXT("Con las teclas 1 y 2, sin 0"), DtKeys(*View), TArray<FString>{TEXT("1"), TEXT("2")});
	TestTrue(TEXT("Con botones"), View->Input == EDecisionInput::Buttons);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionVoteTest, "Soulbound.Game.Presentation.Decision.VotoPorFicha", DecisionTestFlags)

bool FDecisionVoteTest::RunTest(const FString& Parameters)
{
	// CA-DEC-3 (R-DEC-1, R-DEC-2): con una votación abierta en la que el humano puede votar, el tipo es ficha, hay una opción por asiento
	// votable (VoteOptions, sin él) con su tecla de 1 en adelante y el asiento en el texto (decisiones 1 y 2 del autor), y la instrucción dice
	// "haz clic en su ficha". Si no puede votar, o ya ha votado (decisión 5), no hay vista.
	{
		FMatchState State = DtMakeState();
		DtOpenVote(State);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista"), View.IsSet()))
		{
			TestTrue(TEXT("Tipo ficha"), View->Input == EDecisionInput::Token);
			TestEqual(TEXT("El título es el nombre del evento"), View->Title, State.WardenEvents.Events[DtVote].Name);
			TestEqual(TEXT("Una opción por asiento votable"), View->Options.Num(), 3);
			TestEqual(TEXT("Con las teclas 1 a 3"), DtKeys(*View), TArray<FString>{TEXT("1"), TEXT("2"), TEXT("3")});
			for (int32 Index = 0; Index < View->Options.Num(); ++Index)
			{
				const FDecisionOption& Option = View->Options[Index];
				const int32 Seat = State.Vote.Votables[Index];
				TestTrue(FString::Printf(TEXT("La opción %d vota al asiento %d"), Index + 1, Seat), Option.Answer == EDecisionAnswer::Vote && Option.Value == Seat);
				TestTrue(FString::Printf(TEXT("Y lo dice: asiento %d"), Seat), Option.Text.Contains(FString::Printf(TEXT("Asiento %d"), Seat)));
			}
			TestTrue(TEXT("La instrucción dice cómo votar con la ficha"), View->Instruction.Contains(TEXT("haz clic en su ficha")));
			TestEqual(TEXT("El plazo de la votación"), View->DeadlineSeconds, DtNow + 20.0);
		}
	}
	{
		// El humano es el único votable: no tiene a quién votar.
		FMatchState State = DtMakeState();
		DtOpenVote(State);
		State.Vote.Votables = {DtHuman};
		TestFalse(TEXT("Si no puede votar, no hay vista"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
	}
	{
		FMatchState State = DtMakeState();
		DtOpenVote(State);
		State.Vote.Votes[DtHuman] = 2;
		TestFalse(TEXT("Si ya ha votado, no hay vista"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOracleTest, "Soulbound.Game.Presentation.Decision.OraculoRechazarSoloSiPuede", DecisionTestFlags)

bool FDecisionOracleTest::RunTest(const FString& Parameters)
{
	// CA-DEC-4 (R-DEC-1, R-DEC-2): con una consulta del Oráculo abierta para el humano, el tipo es orbe, "Consultar" va con la tecla 1 y
	// "Rechazar" con la 2 (decisión 1 del autor), y "Rechazar" solo aparece si el asiento puede (Tifón: Oracle.bRefusable, que dice el motor).
	{
		FMatchState State = DtMakeState();
		DtOpenOracle(State, true);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista"), View.IsSet()))
		{
			TestTrue(TEXT("Tipo orbe"), View->Input == EDecisionInput::Orb);
			TestEqual(TEXT("Consultar y Rechazar"), DtTexts(*View), TArray<FString>{TEXT("Consultar"), TEXT("Rechazar")});
			TestEqual(TEXT("Con las teclas 1 y 2"), DtKeys(*View), TArray<FString>{TEXT("1"), TEXT("2")});
			if (View->Options.Num() == 2)
			{
				TestTrue(TEXT("Consultar consulta"), View->Options[0].Answer == EDecisionAnswer::OracleConsult);
				TestTrue(TEXT("Rechazar rechaza"), View->Options[1].Answer == EDecisionAnswer::OracleRefuse);
			}
			TestEqual(TEXT("El plazo de la consulta"), View->DeadlineSeconds, DtNow + 8.0);
		}
	}
	{
		FMatchState State = DtMakeState();
		DtOpenOracle(State, false);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista sin Tifón"), View.IsSet()))
		{
			TestEqual(TEXT("Solo Consultar"), DtTexts(*View), TArray<FString>{TEXT("Consultar")});
		}
	}
	{
		FMatchState State = DtMakeState();
		DtOpenOracle(State, true);
		State.Oracle.Consultant = 2;
		TestFalse(TEXT("La consulta de otro asiento no es del humano"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionTileTest, "Soulbound.Game.Presentation.Decision.CasillaSinOpciones", DecisionTestFlags)

bool FDecisionTileTest::RunTest(const FString& Parameters)
{
	// CA-DEC-5 (R-DEC-1, R-DEC-2): con el humano en fase de elegir destino, el tipo es casilla, sin opciones, con el texto "Elige una casilla
	// que late." (S-039) y el plazo del reloj de turno.
	{
		FMatchState State = DtMakeState();
		DtOpenTile(State);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista"), View.IsSet()))
		{
			TestTrue(TEXT("Tipo casilla"), View->Input == EDecisionInput::Tile);
			TestEqual(TEXT("Sin opciones"), View->Options.Num(), 0);
			TestTrue(TEXT("Elige una casilla que late"), View->Instruction.Contains(TEXT("Elige una casilla que late")));
			TestEqual(TEXT("El plazo del reloj de turno"), View->DeadlineSeconds, DtNow + 15.0);
		}
	}
	{
		// Decisión 4 del autor: con la tirada repetible, sigue siendo casilla, con un solo botón "R  Repetir la tirada".
		FMatchState State = DtMakeState();
		DtOpenTile(State);
		State.PendingChoice.Kind = EChoiceKind::Reroll;
		State.PendingChoice.Seat = DtHuman;
		State.PendingChoice.Options = {1};
		State.PendingChoice.DeadlineSeconds = DtNow + 15.0;
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista con la repetición"), View.IsSet()))
		{
			TestTrue(TEXT("Tipo casilla"), View->Input == EDecisionInput::Tile);
			TestTrue(TEXT("Elige una casilla que late"), View->Instruction.Contains(TEXT("Elige una casilla que late")));
			if (TestEqual(TEXT("Un solo botón"), View->Options.Num(), 1))
			{
				TestEqual(TEXT("Repetir la tirada"), View->Options[0].Text, FString(TEXT("Repetir la tirada")));
				TestEqual(TEXT("Con la tecla R"), View->Options[0].Key, FString(TEXT("R")));
				TestTrue(TEXT("Que repite"), View->Options[0].ChoiceKind == EChoiceKind::Reroll && View->Options[0].Value == 1);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionPlanarJumpTest, "Soulbound.Game.Presentation.Decision.SaltoPlanarCasillaConNo", DecisionTestFlags)

bool FDecisionPlanarJumpTest::RunTest(const FString& Parameters)
{
	// Añadido a del autor (10/11/2026): el salto planar se contesta con clic en casilla y admite "no": tipo casilla, con su texto y un solo
	// botón, "0  No", que contesta "no" (INDEX_NONE).
	FMatchState State = DtMakeState();
	State.ActiveSeat = DtHuman;
	State.TurnPhase = ETurnPhase::MoveAndResolve;
	State.PendingChoice.Kind = EChoiceKind::PlanarJump;
	State.PendingChoice.Seat = DtHuman;
	State.PendingChoice.Options = {12, 30};
	State.PendingChoice.DeadlineSeconds = DtNow + 9.0;
	const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	if (!TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("Tipo casilla"), View->Input == EDecisionInput::Tile);
	TestFalse(TEXT("Con su texto"), View->Instruction.IsEmpty());
	if (TestEqual(TEXT("Un solo botón"), View->Options.Num(), 1))
	{
		TestEqual(TEXT("No"), View->Options[0].Text, FString(TEXT("No")));
		TestEqual(TEXT("Con la tecla 0"), View->Options[0].Key, FString(TEXT("0")));
		TestTrue(TEXT("Que contesta no al salto"), View->Options[0].ChoiceKind == EChoiceKind::PlanarJump && View->Options[0].Value == INDEX_NONE);
	}
	TestEqual(TEXT("El plazo de la elección"), View->DeadlineSeconds, DtNow + 9.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionDeclineTest, "Soulbound.Game.Presentation.Decision.NoAlFinalYTeclaDeLaDecima", DecisionTestFlags)

bool FDecisionDeclineTest::RunTest(const FString& Parameters)
{
	// Decisión 3 del autor: en las elecciones que admiten "no", un botón "0  No" al final; en la esfera del trilero, que no lo admite, el 0
	// es la décima opción (OptionKey) y la undécima no tiene tecla.
	{
		FMatchState State = DtMakeState();
		State.ActiveSeat = DtHuman;
		State.TurnPhase = ETurnPhase::MoveAndResolve;
		State.PendingChoice.Kind = EChoiceKind::PlayerAction;
		State.PendingChoice.Seat = DtHuman;
		State.PendingChoice.ActionOptions = {{EPlayerAction::Expel, 1}, {EPlayerAction::Steal, 2}};
		State.PendingChoice.DeadlineSeconds = DtNow + 9.0;
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista de la acción"), View.IsSet()))
		{
			TestEqual(TEXT("1, 2 y el 0 al final"), DtKeys(*View), TArray<FString>{TEXT("1"), TEXT("2"), TEXT("0")});
			if (View->Options.Num() == 3)
			{
				TestEqual(TEXT("El último es No"), View->Options[2].Text, FString(TEXT("No")));
				TestEqual(TEXT("Que contesta no"), View->Options[2].Value, static_cast<int32>(INDEX_NONE));
				TestEqual(TEXT("La primera contesta su posición"), View->Options[0].Value, 0);
			}
		}
	}
	{
		FMatchState State = DtMakeState();
		State.ShellGame.Stage = EShellGameStage::Pick;
		State.ShellGame.Thief = DtHuman;
		State.ShellGame.Victim = 1;
		State.PendingChoice.Kind = EChoiceKind::ShellPick;
		State.PendingChoice.Seat = DtHuman;
		for (int32 Orb = 0; Orb < 11; ++Orb)
		{
			State.PendingChoice.Options.Add(Orb);
		}
		State.PendingChoice.DeadlineSeconds = DtNow + 6.0;
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista del trilero"), View.IsSet()) && TestEqual(TEXT("Once esferas, sin No"), View->Options.Num(), 11))
		{
			TestEqual(TEXT("La primera, tecla 1"), View->Options[0].Key, FString(TEXT("1")));
			TestEqual(TEXT("La décima, tecla 0"), View->Options[9].Key, FString(TEXT("0")));
			TestEqual(TEXT("La undécima, sin tecla"), View->Options[10].Key, FString());
			TestEqual(TEXT("La décima contesta su esfera"), View->Options[9].Value, 9);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionNoneTest, "Soulbound.Game.Presentation.Decision.NadaSinDecisionOParaDummy", DecisionTestFlags)

bool FDecisionNoneTest::RunTest(const FString& Parameters)
{
	// CA-DEC-6 (R-DEC-1): sin decisión abierta para el humano, o con la decisión abierta para un dummy, no hay vista. Cada caso lleva su
	// control: la misma decisión, abierta para el humano, sí tiene vista; sin él, el test pasaría con una función que nunca devuelve nada.
	TestFalse(TEXT("Sin decisión, no hay vista"), Soulbound::Presentation::DecisionView(DtMakeState(), DtHuman, false).IsSet());
	{
		FMatchState State = DtMakeState();
		DtOpenDice(State, 2);
		TestFalse(TEXT("Elección de un dummy"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
		DtOpenDice(State, DtHuman);
		TestTrue(TEXT("Control: la misma elección del humano sí tiene vista"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
	}
	{
		FMatchState State = DtMakeState();
		DtOpenTile(State);
		State.ActiveSeat = 3;
		TestFalse(TEXT("Destino de un dummy"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
		State.ActiveSeat = DtHuman;
		TestTrue(TEXT("Control: el destino del humano sí tiene vista"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
	}
	{
		FMatchState State = DtMakeState();
		DtOpenDice(State, DtHuman);
		State.MatchPhase = EMatchPhase::MatchOver;
		State.Outcome = EMatchOutcome::WardenVictory;
		TestFalse(TEXT("Con la partida terminada"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
		State.MatchPhase = EMatchPhase::RuneHunt;
		State.Outcome = EMatchOutcome::Undecided;
		TestTrue(TEXT("Control: sin terminar, sí tiene vista"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionRingTest, "Soulbound.Game.Presentation.Decision.AnilloOculta", DecisionTestFlags)

bool FDecisionRingTest::RunTest(const FString& Parameters)
{
	// CA-DEC-7 (R-DEC-3): con el anillo del humano abierto, la regla de visibilidad devuelve oculto aunque haya decisión; sin anillo, visible.
	FMatchState State = DtMakeState();
	DtOpenDice(State, DtHuman);
	const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	TestTrue(TEXT("Hay decisión"), View.IsSet());
	TestFalse(TEXT("Con el anillo abierto, oculto"), Soulbound::Presentation::IsDecisionPanelShown(View, true));
	TestTrue(TEXT("Sin anillo, visible"), Soulbound::Presentation::IsDecisionPanelShown(View, false));
	TestFalse(TEXT("Sin decisión, oculto"), Soulbound::Presentation::IsDecisionPanelShown(TOptional<FDecisionView>(), false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionWardenSpeaksTest, "Soulbound.Game.Presentation.Decision.WardenHablaBloquea", DecisionTestFlags)

bool FDecisionWardenSpeaksTest::RunTest(const FString& Parameters)
{
	// CA-DEC-8 (R-DEC-3): con una elección abierta para el humano y la mesa esperando al Warden, hay vista, bloqueada y con las mismas
	// opciones que sin la espera; sin la espera, no está bloqueada. La regla de visibilidad devuelve visible en los dos casos.
	FMatchState State = DtMakeState();
	DtOpenDice(State, DtHuman);
	const TOptional<FDecisionView> Waiting = Soulbound::Presentation::DecisionView(State, DtHuman, true);
	const TOptional<FDecisionView> Free = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	if (!TestTrue(TEXT("Hay vista con la espera"), Waiting.IsSet()) || !TestTrue(TEXT("Y sin ella"), Free.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("Con la espera, bloqueada"), Waiting->bBlocked);
	TestFalse(TEXT("Sin la espera, no"), Free->bBlocked);
	TestTrue(TEXT("Las mismas opciones"), Waiting->Options == Free->Options);
	TestFalse(TEXT("Que no son ninguna"), Waiting->Options.IsEmpty());
	TestTrue(TEXT("Visible con la espera"), Soulbound::Presentation::IsDecisionPanelShown(Waiting, false));
	TestTrue(TEXT("Visible sin ella"), Soulbound::Presentation::IsDecisionPanelShown(Free, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOrderTest, "Soulbound.Game.Presentation.Decision.DosALaVezComoElEstado", DecisionTestFlags)

bool FDecisionOrderTest::RunTest(const FString& Parameters)
{
	// Añadido b del autor (10/11/2026): con dos decisiones del humano abiertas a la vez, el panel enseña la que el panel de estado pone
	// primero: la elección pendiente antes que la votación y que el Oráculo, y el Oráculo antes que la votación.
	{
		FMatchState State = DtMakeState();
		DtOpenDice(State, DtHuman);
		DtOpenVote(State);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista con elección y votación"), View.IsSet()))
		{
			TestTrue(TEXT("La elección primero"), View->Input == EDecisionInput::Buttons && View->Options.Num() == 2);
		}
		const TArray<Soulbound::Presentation::FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, Soulbound::Presentation::FStatusContext{DtHuman});
		TestTrue(TEXT("Como en el panel de estado"), Lines.Num() > 0 && Lines[0].Text.StartsWith(TEXT("ELIGE dados")));
	}
	{
		FMatchState State = DtMakeState();
		DtOpenOracle(State, false);
		DtOpenVote(State);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista con Oráculo y votación"), View.IsSet()))
		{
			TestTrue(TEXT("El Oráculo primero"), View->Input == EDecisionInput::Orb);
		}
		const TArray<Soulbound::Presentation::FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, Soulbound::Presentation::FStatusContext{DtHuman});
		const int32 Oracle = Lines.IndexOfByPredicate([](const Soulbound::Presentation::FPanelLine& Line) { return Line.Text.StartsWith(TEXT("ORÁCULO")); });
		const int32 Vote = Lines.IndexOfByPredicate([](const Soulbound::Presentation::FPanelLine& Line) { return Line.Text.StartsWith(TEXT("VOTACIÓN")); });
		TestTrue(TEXT("Como en el panel de estado"), Oracle != INDEX_NONE && Vote != INDEX_NONE && Oracle < Vote);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionKeyTest, "Soulbound.Game.Presentation.Decision.TeclaYBotonLoMismo", DecisionTestFlags)

bool FDecisionKeyTest::RunTest(const FString& Parameters)
{
	// R-DEC-2: la tecla y el botón entregan lo mismo: la tecla busca en la vista la opción que lleva su número.
	FMatchState State = DtMakeState();
	DtOpenVote(State);
	const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	if (!TestTrue(TEXT("Hay vista"), View.IsSet()) || !TestEqual(TEXT("Tres opciones"), View->Options.Num(), 3))
	{
		return false;
	}
	const FDecisionOption* Second = Soulbound::Presentation::FindDecisionOption(*View, TEXT("2"));
	if (TestNotNull(TEXT("La tecla 2 tiene opción"), Second))
	{
		TestTrue(TEXT("Y es la del segundo botón"), *Second == View->Options[1]);
	}
	TestNull(TEXT("La tecla 7 no tiene"), Soulbound::Presentation::FindDecisionOption(*View, TEXT("7")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionClockTest, "Soulbound.Game.Presentation.Decision.RelojDeLaBarra", DecisionTestFlags)

bool FDecisionClockTest::RunTest(const FString& Parameters)
{
	// Decisión 6 del autor: el total de la barra es lo que queda cuando la vista ve aparecer la decisión; la barra se vacía con el reloj de
	// partida; si el plazo se alarga (la espera del Warden lo desplaza), el total pasa a lo que queda; otra decisión, otro total; sin vista, 0.
	using Soulbound::Presentation::FDecisionClock;
	FMatchState State = DtMakeState();
	DtOpenDice(State, DtHuman);
	FDecisionClock Clock;
	TestEqual(TEXT("Al aparecer, llena"), Soulbound::Presentation::AdvanceDecisionClock(Clock, Soulbound::Presentation::DecisionView(State, DtHuman, false)), 1.0);
	TestEqual(TEXT("El total es lo que quedaba"), Clock.TotalSeconds, 12.0);
	State.ClockSeconds = DtNow + 3.0;
	TestEqual(TEXT("A los 3 s de 12, tres cuartos"), Soulbound::Presentation::AdvanceDecisionClock(Clock, Soulbound::Presentation::DecisionView(State, DtHuman, false)), 0.75);
	State.PendingChoice.DeadlineSeconds += 10.0;
	TestEqual(TEXT("Plazo alargado: llena otra vez"), Soulbound::Presentation::AdvanceDecisionClock(Clock, Soulbound::Presentation::DecisionView(State, DtHuman, false)), 1.0);
	TestEqual(TEXT("Con el nuevo total"), Clock.TotalSeconds, 19.0);
	State.ClockSeconds = DtNow + 40.0;
	TestEqual(TEXT("Vencido, vacía"), Soulbound::Presentation::AdvanceDecisionClock(Clock, Soulbound::Presentation::DecisionView(State, DtHuman, false)), 0.0);
	TestEqual(TEXT("Sin vista, vacía"), Soulbound::Presentation::AdvanceDecisionClock(Clock, TOptional<FDecisionView>()), 0.0);
	TestEqual(TEXT("Y el reloj se olvida"), Clock.TotalSeconds, 0.0);

	// Otra decisión con el reloj lleno de la anterior: su propio total.
	FMatchState Other = DtMakeState();
	DtOpenDice(Other, DtHuman);
	FDecisionClock Reused;
	Soulbound::Presentation::AdvanceDecisionClock(Reused, Soulbound::Presentation::DecisionView(Other, DtHuman, false));
	DtOpenOracle(Other, false);
	Other.PendingChoice = FPendingChoice();
	TestEqual(TEXT("Otra decisión, llena"), Soulbound::Presentation::AdvanceDecisionClock(Reused, Soulbound::Presentation::DecisionView(Other, DtHuman, false)), 1.0);
	TestEqual(TEXT("Con su total"), Reused.TotalSeconds, 8.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionButtonRowTest, "Soulbound.Game.Presentation.Decision.BotonesEnFilaOColumna", DecisionTestFlags)

bool FDecisionButtonRowTest::RunTest(const FString& Parameters)
{
	// Corrección del autor del 11/11/2026: los botones de una decisión tienen todos el ancho del más largo; si así no caben en una fila, van
	// en columna. Caben si el número de botones por el más ancho, más el hueco de cada uno, no pasa del ancho disponible.
	using Soulbound::Presentation::DecisionButtonsInRow;
	TestTrue(TEXT("Tres de 100 con 6 de hueco caben justos en 318"), DecisionButtonsInRow(3, 100.f, 6.f, 318.f));
	TestFalse(TEXT("En 317, no"), DecisionButtonsInRow(3, 100.f, 6.f, 317.f));
	TestTrue(TEXT("Uno ancho cabe si cabe él"), DecisionButtonsInRow(1, 300.f, 6.f, 306.f));
	TestFalse(TEXT("Uno más ancho que el hueco: en columna, que es lo mismo"), DecisionButtonsInRow(1, 400.f, 6.f, 306.f));
	TestTrue(TEXT("Sin botones, nada que repartir: en fila"), DecisionButtonsInRow(0, 0.f, 6.f, 100.f));
	TestFalse(TEXT("Sin ancho disponible, en columna"), DecisionButtonsInRow(2, 50.f, 6.f, 0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionStatusTest, "Soulbound.Game.Presentation.Decision.EstadoNoRepitePeticion", DecisionTestFlags)

bool FDecisionStatusTest::RunTest(const FString& Parameters)
{
	// R-DEC-3 (decisión 7 del autor): con el panel de decisión a la vista, el panel de estado no repite la línea de petición del humano; el
	// resto de líneas, la tirada incluida, se quedan. Sin el panel, la petición sigue la primera, como en S-027.
	FMatchState State = DtMakeState();
	DtOpenDice(State, DtHuman);
	Soulbound::Presentation::FStatusContext Context;
	Context.HumanSeat = DtHuman;
	const TArray<Soulbound::Presentation::FPanelLine> Without = Soulbound::Presentation::StatusLines(State, Context);
	TestTrue(TEXT("Sin panel, la petición va la primera"), Without.Num() > 0 && Without[0].Text.StartsWith(TEXT("ELIGE dados")));
	Context.bDecisionPanelShown = true;
	const TArray<Soulbound::Presentation::FPanelLine> With = Soulbound::Presentation::StatusLines(State, Context);
	TestFalse(TEXT("Con el panel, no se repite"), With.ContainsByPredicate([](const Soulbound::Presentation::FPanelLine& Line) { return Line.Text.StartsWith(TEXT("ELIGE")); }));
	TestEqual(TEXT("Solo falta esa línea"), With.Num(), Without.Num() - 1);
	return true;
}

// ---- S-033: se ve a quién se espera (R-DEC-5 a R-DEC-8) ----

namespace
{
	/** Cuánto se enseña el resultado de una votación en los tests (VoteResultSeconds, valor inicial 5). */
	constexpr double DtResultSeconds = 5.0;
	/** Cuánto se enseña un desenlace en los tests (OutcomeSeconds, valor inicial 4). */
	constexpr double DtOutcomeSeconds = 4.0;

	/** Añade un evento al registro a mano. */
	void DtLog(FMatchState& State, EMatchEventType Type, double Clock, int32 Seat, int32 OtherSeat = INDEX_NONE, int32 Value = INDEX_NONE, int32 Detail = INDEX_NONE)
	{
		FMatchEvent Event;
		Event.Type = Type;
		Event.ClockSeconds = Clock;
		Event.Seat = Seat;
		Event.OtherSeat = OtherSeat;
		Event.Value = Value;
		Event.Detail = Detail;
		State.EventLog.Add(Event);
	}

	/** Una votación resuelta en el registro: el 0 y el 1 votan al 2, el 3 vota al 1 al azar; sale el 2 con 2 votos, en el segundo Clock. */
	void DtLogVoteOutcome(FMatchState& State, double Clock, bool bWardenBrokeTie = false)
	{
		DtLog(State, EMatchEventType::VoteStarted, Clock - 10.0, INDEX_NONE, INDEX_NONE, DtVote, 3);
		DtLog(State, EMatchEventType::VoteCast, Clock, 0, 2, INDEX_NONE, 0);
		DtLog(State, EMatchEventType::VoteCast, Clock, 1, 2, INDEX_NONE, 0);
		DtLog(State, EMatchEventType::VoteCast, Clock, 3, 1, INDEX_NONE, 1);
		DtLog(State, EMatchEventType::VoteResolved, Clock, 2, INDEX_NONE, 2, bWardenBrokeTie ? 1 : 0);
	}

	/** Hace de Seat un Elegido de nombre Name (Dominio 1), para que su etiqueta sea "Asiento n · Nombre". */
	void DtMakeElect(FMatchState& State, int32 Seat, const TCHAR* Name)
	{
		FElectDef Def;
		Def.Name = Name;
		State.Elects.Elects.SetNum(2);
		State.Elects.Elects[1] = Def;
		State.Seats[Seat].Role = ESeatRole::Elect;
		State.Seats[Seat].DomainId = 1;
	}

	/** Si dos vistas son la misma en todo lo que enseñan. */
	bool DtSameView(const FDecisionView& A, const FDecisionView& B)
	{
		return A.Input == B.Input && A.Title == B.Title && A.Context == B.Context && A.Instruction == B.Instruction && A.Options == B.Options
			&& A.DeadlineSeconds == B.DeadlineSeconds && A.bBlocked == B.bBlocked && A.Identity == B.Identity && A.Lines == B.Lines && A.bHasDeadline == B.bHasDeadline;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionWaitChoiceTest, "Soulbound.Game.Presentation.Decision.EsperaEleccionDeOtro", DecisionTestFlags)

bool FDecisionWaitChoiceTest::RunTest(const FString& Parameters)
{
	// CA-DEC-9 (R-DEC-5, caso 1): con una elección DiceCount abierta para el asiento 2 y el humano en el 0, no hay DecisionView; WaitView existe,
	// es de tipo espera, sin opciones, nombra al asiento 2 y lleva el plazo de la elección. Con un dilema del Warden, el título es el evento.
	{
		FMatchState State = DtMakeState();
		DtOpenDice(State, 2);
		TestFalse(TEXT("El humano no tiene nada que decidir"), Soulbound::Presentation::DecisionView(State, DtHuman, false).IsSet());
		const TOptional<FDecisionView> View = Soulbound::Presentation::WaitView(State, DtHuman);
		if (TestTrue(TEXT("Hay vista de espera"), View.IsSet()))
		{
			TestTrue(TEXT("Es de tipo espera"), View->Input == EDecisionInput::Wait);
			TestTrue(TEXT("Sin opciones"), View->Options.IsEmpty());
			// Corrección del autor del 11/11/2026: una sola línea de título, en tercera persona, sin contexto.
			TestEqual(TEXT("Nombra al asiento 2 y qué decide, en tercera persona"), View->Title, FString(TEXT("Asiento 2 (Eco) decide cuántos dados tira")));
			TestTrue(TEXT("Sin línea de contexto"), View->Context.IsEmpty());
			TestTrue(TEXT("Ni instrucción"), View->Instruction.IsEmpty());
			TestEqual(TEXT("Plazo de la elección"), View->DeadlineSeconds, DtNow + 12.0);
			TestTrue(TEXT("Con barra"), View->bHasDeadline);
			TestFalse(TEXT("Nunca bloqueada: no hay nada que pulsar"), View->bBlocked);
			TestFalse(TEXT("Con identidad, para el reloj de la barra"), View->Identity.IsEmpty());
		}
		// Control: si quien elige es el humano, no es una espera.
		FMatchState Own = DtMakeState();
		DtOpenDice(Own, DtHuman);
		TestFalse(TEXT("Control: la elección propia no es espera"), Soulbound::Presentation::WaitView(Own, DtHuman).IsSet());
		// El nombre del Elegido, si lo es.
		DtMakeElect(State, 2, TEXT("Fenrir"));
		const TOptional<FDecisionView> Elect = Soulbound::Presentation::WaitView(State, DtHuman);
		if (TestTrue(TEXT("Hay vista con Elegido"), Elect.IsSet()))
		{
			TestEqual(TEXT("Con el nombre del Elegido"), Elect->Title, FString(TEXT("Asiento 2 · Fenrir decide cuántos dados tira")));
		}
		// Un texto por clase de elección, todos en tercera persona (ChoiceWhat, en segunda, no se usa aquí).
		struct FKindCase { EChoiceKind Kind; const TCHAR* Text; };
		const FKindCase Cases[] = {
			{EChoiceKind::PlanarJump, TEXT("Asiento 2 (Eco) decide si hace un salto planar")},
			{EChoiceKind::SafeRune, TEXT("Asiento 2 (Eco) pone una runa a salvo del trilero")},
			{EChoiceKind::ShellPick, TEXT("Asiento 2 (Eco) elige una esfera del trilero")},
			{EChoiceKind::Deflection, TEXT("Asiento 2 (Eco) decide si sale del Vacío con Desvío")},
			{EChoiceKind::PlayerAction, TEXT("Asiento 2 (Eco) decide si actúa contra un jugador")},
		};
		for (const FKindCase& Case : Cases)
		{
			FMatchState Other = DtMakeState();
			Other.ActiveSeat = 2;
			Other.PendingChoice.Kind = Case.Kind;
			Other.PendingChoice.Seat = 2;
			Other.PendingChoice.Options = {0, 1};
			Other.PendingChoice.DeadlineSeconds = DtNow + 12.0;
			const TOptional<FDecisionView> CaseView = Soulbound::Presentation::WaitView(Other, DtHuman);
			if (TestTrue(*FString::Printf(TEXT("Hay vista para la clase %d"), static_cast<int32>(Case.Kind)), CaseView.IsSet()))
			{
				TestEqual(*FString::Printf(TEXT("Texto de la clase %d"), static_cast<int32>(Case.Kind)), CaseView->Title, FString(Case.Text));
				TestTrue(TEXT("Sin contexto"), CaseView->Context.IsEmpty());
			}
		}
	}
	{
		FMatchState State = DtMakeState();
		State.CurrentEvent.EventIndex = DtDilemma;
		State.CurrentEvent.Option = 0;
		State.ActiveSeat = 2;
		State.PendingChoice.Kind = EChoiceKind::Dilemma;
		State.PendingChoice.Seat = 2;
		State.PendingChoice.Options = {0, 1};
		State.PendingChoice.DeadlineSeconds = DtNow + 8.0;
		const TOptional<FDecisionView> View = Soulbound::Presentation::WaitView(State, DtHuman);
		if (TestTrue(TEXT("Hay vista con el dilema de otro"), View.IsSet()))
		{
			TestEqual(TEXT("El título es el evento"), View->Title, FString(TEXT("La balanza")));
			TestEqual(TEXT("El contexto, su texto"), View->Context, FString(TEXT("Elige tu castigo, o tu premio.")));
			TestEqual(TEXT("Y la instrucción dice quién elige"), View->Instruction, FString(TEXT("Le toca a Asiento 2 (Eco)")));
			TestTrue(TEXT("Sin opciones"), View->Options.IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionFreeWidthTest, "Soulbound.Game.Presentation.Decision.AnchoLibreDelTablero", DecisionTestFlags)

bool FDecisionFreeWidthTest::RunTest(const FString& Parameters)
{
	// R-HUD-3 (corrección del autor del 11/11/2026): el ancho en que se parte la frase del Warden y el máximo del panel de decisión salen del mismo
	// cálculo: el hueco del tablero (el ancho de la ventana menos la columna de paneles y los 16 de cada lado), con un tope.
	using Soulbound::Presentation::BoardFreeWidth;
	TestEqual(TEXT("1920 de ventana, columna de 420: sobran 1468, topados en 1000"), BoardFreeWidth(1920.f, 420.f, 1000.f), 1000.f);
	TestEqual(TEXT("Topado en 900 para el panel de decisión"), BoardFreeWidth(1920.f, 420.f, 900.f), 900.f);
	TestEqual(TEXT("1024 de ventana: 1024 menos 420 menos 32"), BoardFreeWidth(1024.f, 420.f, 1000.f), 572.f);
	TestEqual(TEXT("Nunca negativo"), BoardFreeWidth(300.f, 420.f, 1000.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionWaitTurnTest, "Soulbound.Game.Presentation.Decision.EsperaTurnoDeOtro", DecisionTestFlags)

bool FDecisionWaitTurnTest::RunTest(const FString& Parameters)
{
	// CA-DEC-10 (R-DEC-5, caso 2): con el asiento 1 en fase de tirada y una tirada de 5, la vista nombra al asiento 1 y la tirada, y su plazo es el
	// del reloj de turno.
	FMatchState State = DtMakeState();
	State.ActiveSeat = 1;
	State.TurnPhase = ETurnPhase::RollAndChoose;
	State.Roll = 5;
	State.TurnDeadlineSeconds = DtNow + 15.0;
	const TOptional<FDecisionView> View = Soulbound::Presentation::WaitView(State, DtHuman);
	if (TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		TestTrue(TEXT("De tipo espera"), View->Input == EDecisionInput::Wait);
		TestEqual(TEXT("Nombra el turno del 1"), View->Title, FString(TEXT("Turno de Asiento 1 (Eco)")));
		TestEqual(TEXT("Y la tirada, sin dados que enseñar (S-039: \"Ha sacado un 5\")"), View->Context, FString(TEXT("Ha sacado un 5")));
		TestEqual(TEXT("Plazo del reloj de turno"), View->DeadlineSeconds, DtNow + 15.0);
		TestTrue(TEXT("Sin opciones"), View->Options.IsEmpty());
	}
	// Controles: el turno del humano y un turno que no está en la tirada no son espera.
	FMatchState Own = State;
	Own.ActiveSeat = DtHuman;
	TestFalse(TEXT("Control: el turno propio no es espera"), Soulbound::Presentation::WaitView(Own, DtHuman).IsSet());
	FMatchState Between = State;
	Between.TurnPhase = ETurnPhase::BetweenTurns;
	TestFalse(TEXT("Control: sin fase de tirada, no hay espera"), Soulbound::Presentation::WaitView(Between, DtHuman).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionWaitOracleTest, "Soulbound.Game.Presentation.Decision.EsperaOraculoDeOtro", DecisionTestFlags)

bool FDecisionWaitOracleTest::RunTest(const FString& Parameters)
{
	// CA-DEC-11 (R-DEC-5, caso 3): con una consulta del Oráculo abierta para el asiento 2, la vista nombra al asiento 2 y no tiene opciones.
	FMatchState State = DtMakeState();
	DtOpenOracle(State, false);
	State.Oracle.Consultant = 2;
	const TOptional<FDecisionView> View = Soulbound::Presentation::WaitView(State, DtHuman);
	if (TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		TestTrue(TEXT("De tipo espera"), View->Input == EDecisionInput::Wait);
		TestEqual(TEXT("Nombra al asiento 2 ante el Oráculo"), View->Title, FString(TEXT("Asiento 2 (Eco) ante el Oráculo")));
		TestTrue(TEXT("Sin opciones"), View->Options.IsEmpty());
		TestEqual(TEXT("Plazo de la consulta"), View->DeadlineSeconds, DtNow + 8.0);
	}
	FMatchState Own = DtMakeState();
	DtOpenOracle(Own, false);
	TestFalse(TEXT("Control: la consulta propia no es espera"), Soulbound::Presentation::WaitView(Own, DtHuman).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionWaitVoteTest, "Soulbound.Game.Presentation.Decision.EsperaVotacionQuienFalta", DecisionTestFlags)

bool FDecisionWaitVoteTest::RunTest(const FString& Parameters)
{
	// CA-DEC-12 (R-DEC-5, caso 4): con una votación abierta, el humano ya votado y los asientos 1 y 3 sin votar, la vista dice que faltan el 1 y el
	// 3; cuando vota el 1, solo el 3. Un asiento que no puede votar no aparece entre los que faltan.
	FMatchState State = DtMakeState();
	DtOpenVote(State);
	State.Vote.Votes[DtHuman] = 1;
	State.Vote.Votes[2] = 1;
	const TOptional<FDecisionView> Both = Soulbound::Presentation::WaitView(State, DtHuman);
	if (TestTrue(TEXT("Hay vista"), Both.IsSet()))
	{
		TestTrue(TEXT("De tipo espera"), Both->Input == EDecisionInput::Wait);
		TestEqual(TEXT("El título es el evento"), Both->Title, FString(TEXT("El juicio")));
		TestEqual(TEXT("Faltan el 1 y el 3"), Both->Context, FString(TEXT("Faltan por votar: Asiento 1 (Eco), Asiento 3 (Eco)")));
		TestEqual(TEXT("Plazo de la votación"), Both->DeadlineSeconds, DtNow + 20.0);
		TestTrue(TEXT("Sin opciones"), Both->Options.IsEmpty());
	}
	State.Vote.Votes[1] = 2;
	const TOptional<FDecisionView> Last = Soulbound::Presentation::WaitView(State, DtHuman);
	if (TestTrue(TEXT("Sigue habiendo vista"), Last.IsSet()))
	{
		TestEqual(TEXT("Solo el 3"), Last->Context, FString(TEXT("Faltan por votar: Asiento 3 (Eco)")));
	}
	// Quien no puede votar (solo se puede votar al 3, y el 3 no se vota a sí mismo) no falta.
	FMatchState Cannot = DtMakeState();
	DtOpenVote(Cannot);
	Cannot.Vote.Votables = {3};
	Cannot.Vote.Votes[DtHuman] = 3;
	Cannot.Vote.Votes[1] = 3;
	const TOptional<FDecisionView> OnlyTwo = Soulbound::Presentation::WaitView(Cannot, DtHuman);
	if (TestTrue(TEXT("Hay vista con un asiento sin voto posible"), OnlyTwo.IsSet()))
	{
		TestEqual(TEXT("Falta el 2, no el 3"), OnlyTwo->Context, FString(TEXT("Faltan por votar: Asiento 2 (Eco)")));
	}
	// El humano que no puede votar espera igual, y no figura entre los que faltan.
	FMatchState HumanCannot = DtMakeState();
	DtOpenVote(HumanCannot);
	HumanCannot.Vote.Votables = {DtHuman};
	TestFalse(TEXT("El humano no tiene a quién votar: no hay decisión"), Soulbound::Presentation::DecisionView(HumanCannot, DtHuman, false).IsSet());
	const TOptional<FDecisionView> Waiting = Soulbound::Presentation::WaitView(HumanCannot, DtHuman);
	if (TestTrue(TEXT("Y espera"), Waiting.IsSet()))
	{
		TestEqual(TEXT("Faltan los otros tres, no él"), Waiting->Context, FString(TEXT("Faltan por votar: Asiento 1 (Eco), Asiento 2 (Eco), Asiento 3 (Eco)")));
	}
	// Control: si el humano aún puede votar, esa es su decisión y no una espera.
	FMatchState Pending = DtMakeState();
	DtOpenVote(Pending);
	TestFalse(TEXT("Control: con el voto del humano pendiente no hay espera"), Soulbound::Presentation::WaitView(Pending, DtHuman).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionWaitTrialTest, "Soulbound.Game.Presentation.Decision.EsperaPruebaYDueloDeOtros", DecisionTestFlags)

bool FDecisionWaitTrialTest::RunTest(const FString& Parameters)
{
	// CA-DEC-13 (R-DEC-5, caso 5): con el asiento 2 en una prueba de habilidad, la vista lo nombra y dice qué prueba; con un duelo entre el 1 y el 3,
	// nombra a los dos. En los dos casos no hay plazo.
	struct FCase
	{
		ESkillTrial Trial;
		const TCHAR* Context;
	};
	const FCase Cases[] = {
		{ESkillTrial::ForgottenSource, TEXT("Prueba de la Fuente")},
		{ESkillTrial::WardenTrial, TEXT("Prueba del Warden")},
		{ESkillTrial::VoidEscape, TEXT("Escape del Vacío")},
		{ESkillTrial::FinalChallenge, TEXT("Desafío final")},
	};
	for (const FCase& Case : Cases)
	{
		FMatchState State = DtMakeState();
		State.ActiveSeat = 2;
		State.TurnPhase = ETurnPhase::SkillTrial;
		State.PendingTrial = Case.Trial;
		const TOptional<FDecisionView> View = Soulbound::Presentation::WaitView(State, DtHuman);
		if (TestTrue(TEXT("Hay vista de la prueba"), View.IsSet()))
		{
			TestTrue(TEXT("De tipo espera"), View->Input == EDecisionInput::Wait);
			TestEqual(TEXT("Nombra al asiento 2"), View->Title, FString(TEXT("Asiento 2 (Eco) juega")));
			TestEqual(TEXT("Y qué prueba"), View->Context, FString(Case.Context));
			TestFalse(TEXT("Sin plazo"), View->bHasDeadline);
			TestTrue(TEXT("Sin opciones"), View->Options.IsEmpty());
		}
	}
	FMatchState Duel = DtMakeState();
	Duel.TurnPhase = ETurnPhase::Duel;
	Duel.Duel.Challenger = 1;
	Duel.Duel.Challenged = 3;
	const TOptional<FDecisionView> DuelView = Soulbound::Presentation::WaitView(Duel, DtHuman);
	if (TestTrue(TEXT("Hay vista del duelo"), DuelView.IsSet()))
	{
		TestEqual(TEXT("Nombra a los dos"), DuelView->Title, FString(TEXT("Duelo: Asiento 1 (Eco) contra Asiento 3 (Eco)")));
		TestFalse(TEXT("Sin plazo"), DuelView->bHasDeadline);
		TestTrue(TEXT("Sin opciones"), DuelView->Options.IsEmpty());
	}
	// Controles: la prueba y el duelo del humano no son espera de otros.
	FMatchState OwnTrial = DtMakeState();
	OwnTrial.ActiveSeat = DtHuman;
	OwnTrial.TurnPhase = ETurnPhase::SkillTrial;
	OwnTrial.PendingTrial = ESkillTrial::ForgottenSource;
	TestFalse(TEXT("Control: la prueba propia no es espera"), Soulbound::Presentation::WaitView(OwnTrial, DtHuman).IsSet());
	FMatchState OwnDuel = Duel;
	OwnDuel.Duel.Challenger = DtHuman;
	TestFalse(TEXT("Control: un duelo en que juega el humano no es espera"), Soulbound::Presentation::WaitView(OwnDuel, DtHuman).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionPanelOwnTest, "Soulbound.Game.Presentation.Decision.PanelLoPropioManda", DecisionTestFlags)

bool FDecisionPanelOwnTest::RunTest(const FString& Parameters)
{
	// CA-DEC-14 (R-DEC-6): en cada estado de los tests de S-028 en que DecisionView existe, PanelView devuelve esa misma vista. Con una votación
	// abierta en la que el humano aún puede votar, PanelView es su voto, no la espera.
	TArray<TFunction<void(FMatchState&)>> Setups;
	Setups.Add([](FMatchState& State) { DtOpenDice(State, DtHuman); });
	Setups.Add([](FMatchState& State) { DtOpenTile(State); });
	Setups.Add([](FMatchState& State) { DtOpenOracle(State, true); });
	Setups.Add([](FMatchState& State) { DtOpenVote(State); });
	Setups.Add([](FMatchState& State)
	{
		State.CurrentEvent.EventIndex = DtDilemma;
		State.PendingChoice.Kind = EChoiceKind::Dilemma;
		State.PendingChoice.Seat = DtHuman;
		State.PendingChoice.Options = {0, 1};
		State.PendingChoice.DeadlineSeconds = DtNow + 8.0;
	});
	for (int32 Index = 0; Index < Setups.Num(); ++Index)
	{
		for (const bool bSpeaking : {false, true})
		{
			FMatchState State = DtMakeState();
			Setups[Index](State);
			const TOptional<FDecisionView> Own = Soulbound::Presentation::DecisionView(State, DtHuman, bSpeaking);
			const TOptional<FDecisionView> Panel = Soulbound::Presentation::PanelView(State, DtHuman, bSpeaking, DtResultSeconds, DtOutcomeSeconds);
			if (TestTrue(*FString::Printf(TEXT("Caso %d: hay decisión propia"), Index), Own.IsSet()) && TestTrue(*FString::Printf(TEXT("Caso %d: el panel la enseña"), Index), Panel.IsSet()))
			{
				TestTrue(*FString::Printf(TEXT("Caso %d: es la misma vista"), Index), DtSameView(*Own, *Panel));
				TestFalse(*FString::Printf(TEXT("Caso %d: no es una espera"), Index), Panel->Input == EDecisionInput::Wait);
			}
		}
	}
	// Con otros asientos esperando a la vez (el 2 elige), el voto pendiente del humano sigue mandando.
	FMatchState Both = DtMakeState();
	DtOpenVote(Both);
	DtOpenDice(Both, 2);
	const TOptional<FDecisionView> Vote = Soulbound::Presentation::PanelView(Both, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista con votación y elección ajena"), Vote.IsSet()))
	{
		TestTrue(TEXT("Es su voto, no la espera"), Vote->Input == EDecisionInput::Token);
	}
	// Control que puede fallar: sin decisión propia, el mismo estado ajeno sí es una espera (si PanelView solo devolviera DecisionView, no lo sería).
	FMatchState Others = DtMakeState();
	DtOpenDice(Others, 2);
	const TOptional<FDecisionView> Wait = Soulbound::Presentation::PanelView(Others, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
	if (TestTrue(TEXT("Control: sin decisión propia hay espera"), Wait.IsSet()))
	{
		TestTrue(TEXT("Y es de tipo espera"), Wait->Input == EDecisionInput::Wait);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionPanelWardenTest, "Soulbound.Game.Presentation.Decision.PanelElWardenHabla", DecisionTestFlags)

bool FDecisionPanelWardenTest::RunTest(const FString& Parameters)
{
	// CA-DEC-15 (R-DEC-6): con una elección abierta para un dummy y la mesa esperando al Warden, PanelView no devuelve nada; sin la espera,
	// devuelve la vista de espera. La decisión propia sí se sigue enseñando, bloqueada.
	FMatchState State = DtMakeState();
	DtOpenDice(State, 2);
	TestFalse(TEXT("Con el Warden hablando, nada"), Soulbound::Presentation::PanelView(State, DtHuman, true, DtResultSeconds, DtOutcomeSeconds).IsSet());
	const TOptional<FDecisionView> Free = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
	if (TestTrue(TEXT("Sin la espera del Warden, la vista de espera"), Free.IsSet()))
	{
		TestTrue(TEXT("Es de tipo espera"), Free->Input == EDecisionInput::Wait);
	}
	FMatchState Own = DtMakeState();
	DtOpenDice(Own, DtHuman);
	const TOptional<FDecisionView> Blocked = Soulbound::Presentation::PanelView(Own, DtHuman, true, DtResultSeconds, DtOutcomeSeconds);
	if (TestTrue(TEXT("La decisión propia se enseña con el Warden hablando"), Blocked.IsSet()))
	{
		TestTrue(TEXT("Bloqueada"), Blocked->bBlocked);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionPanelNothingTest, "Soulbound.Game.Presentation.Decision.PanelNada", DecisionTestFlags)

bool FDecisionPanelNothingTest::RunTest(const FString& Parameters)
{
	// CA-DEC-16 (R-DEC-6): entre turnos sin nada abierto, con una ficha moviéndose y con la partida terminada, PanelView no devuelve nada.
	FMatchState Between = DtMakeState();
	TestFalse(TEXT("Entre turnos sin nada abierto"), Soulbound::Presentation::PanelView(Between, DtHuman, false, DtResultSeconds, DtOutcomeSeconds).IsSet());
	FMatchState Moving = DtMakeState();
	Moving.ActiveSeat = 1;
	Moving.TurnPhase = ETurnPhase::Moving;
	TestFalse(TEXT("Con una ficha moviéndose"), Soulbound::Presentation::PanelView(Moving, DtHuman, false, DtResultSeconds, DtOutcomeSeconds).IsSet());
	FMatchState Over = DtMakeState();
	DtOpenDice(Over, 2);
	TestTrue(TEXT("Control: con la elección ajena abierta, hay espera"), Soulbound::Presentation::PanelView(Over, DtHuman, false, DtResultSeconds, DtOutcomeSeconds).IsSet());
	Over.MatchPhase = EMatchPhase::MatchOver;
	DtLogVoteOutcome(Over, DtNow - 1.0);
	TestFalse(TEXT("Con la partida terminada, ni espera ni resultado"), Soulbound::Presentation::PanelView(Over, DtHuman, false, DtResultSeconds, DtOutcomeSeconds).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionVoteResultTest, "Soulbound.Game.Presentation.Decision.ResultadoVotacion", DecisionTestFlags)

bool FDecisionVoteResultTest::RunTest(const FString& Parameters)
{
	// CA-DEC-17 (R-DEC-8): con un registro en que el 0 y el 1 votan al 2, el 3 vota al 1 al azar y VoteResolved da al 2 con 2 votos en el segundo
	// 100: en el 102 hay vista, nombra al asiento 2 con sus 2 votos, lleva tres líneas de voto y marca la del 3 como al azar; pasado
	// VoteResultSeconds, no hay vista. Con desempate del Warden, la vista lo dice. Con dos votaciones en el registro, solo cuentan las de la última.
	FMatchState State = DtMakeState();
	DtLogVoteOutcome(State, 100.0);
	State.ClockSeconds = 102.0;
	const TOptional<FDecisionView> View = Soulbound::Presentation::VoteResultView(State, DtResultSeconds);
	if (TestTrue(TEXT("En el segundo 102 hay vista"), View.IsSet()))
	{
		TestTrue(TEXT("De tipo resultado"), View->Input == EDecisionInput::Result);
		TestEqual(TEXT("El título es el evento, sacado del registro"), View->Title, FString(TEXT("El juicio")));
		TestEqual(TEXT("El más votado con sus votos"), View->Context, FString(TEXT("El más votado: Asiento 2 (Eco), con 2 votos")));
		TestTrue(TEXT("Sin botones"), View->Options.IsEmpty());
		TestFalse(TEXT("Sin barra"), View->bHasDeadline);
		if (TestEqual(TEXT("Tres líneas de voto"), View->Lines.Num(), 3))
		{
			TestEqual(TEXT("Primer voto"), View->Lines[0], FString(TEXT("Asiento 0 (Eco) votó a Asiento 2 (Eco)")));
			TestEqual(TEXT("Segundo voto"), View->Lines[1], FString(TEXT("Asiento 1 (Eco) votó a Asiento 2 (Eco)")));
			TestEqual(TEXT("El del 3 va marcado como al azar"), View->Lines[2], FString(TEXT("Asiento 3 (Eco) votó a Asiento 1 (Eco) (al azar: no votó a tiempo)")));
		}
	}
	State.ClockSeconds = 104.9;
	TestTrue(TEXT("Justo antes de caducar, sigue"), Soulbound::Presentation::VoteResultView(State, DtResultSeconds).IsSet());
	State.ClockSeconds = 105.0;
	TestFalse(TEXT("Pasado VoteResultSeconds, no hay vista"), Soulbound::Presentation::VoteResultView(State, DtResultSeconds).IsSet());
	FMatchState Empty = DtMakeState();
	TestFalse(TEXT("Control: sin votación en el registro, no hay vista"), Soulbound::Presentation::VoteResultView(Empty, DtResultSeconds).IsSet());

	FMatchState Tie = DtMakeState();
	DtLogVoteOutcome(Tie, 100.0, true);
	Tie.ClockSeconds = 101.0;
	const TOptional<FDecisionView> TieView = Soulbound::Presentation::VoteResultView(Tie, DtResultSeconds);
	if (TestTrue(TEXT("Con desempate hay vista"), TieView.IsSet()))
	{
		TestEqual(TEXT("Y lo dice"), TieView->Context, FString(TEXT("El más votado: Asiento 2 (Eco), con 2 votos (desempate del Warden)")));
	}

	// Dos votaciones: solo cuentan los votos de la última.
	FMatchState Two = DtMakeState();
	DtLog(Two, EMatchEventType::VoteStarted, 40.0, INDEX_NONE, INDEX_NONE, DtVote, 3);
	DtLog(Two, EMatchEventType::VoteCast, 50.0, 0, 1, INDEX_NONE, 0);
	DtLog(Two, EMatchEventType::VoteCast, 50.0, 1, 0, INDEX_NONE, 0);
	DtLog(Two, EMatchEventType::VoteResolved, 50.0, 1, INDEX_NONE, 1, 1);
	DtLogVoteOutcome(Two, 100.0);
	Two.ClockSeconds = 101.0;
	const TOptional<FDecisionView> TwoView = Soulbound::Presentation::VoteResultView(Two, DtResultSeconds);
	if (TestTrue(TEXT("Con dos votaciones hay vista"), TwoView.IsSet()))
	{
		TestEqual(TEXT("Solo las tres líneas de la última"), TwoView->Lines.Num(), 3);
		TestEqual(TEXT("Y su resultado, no el del desempate anterior"), TwoView->Context, FString(TEXT("El más votado: Asiento 2 (Eco), con 2 votos")));
	}

	// Un solo voto: en singular.
	FMatchState One = DtMakeState();
	DtLog(One, EMatchEventType::VoteCast, 100.0, 0, 2, INDEX_NONE, 0);
	DtLog(One, EMatchEventType::VoteResolved, 100.0, 2, INDEX_NONE, 1, 0);
	One.ClockSeconds = 101.0;
	const TOptional<FDecisionView> OneView = Soulbound::Presentation::VoteResultView(One, DtResultSeconds);
	if (TestTrue(TEXT("Con un voto hay vista"), OneView.IsSet()))
	{
		TestEqual(TEXT("En singular"), OneView->Context, FString(TEXT("El más votado: Asiento 2 (Eco), con 1 voto")));
		TestEqual(TEXT("Sin evento que lo nombre, de título Votación"), OneView->Title, FString(TEXT("Votación")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionPanelOrderTest, "Soulbound.Game.Presentation.Decision.PanelOrdenConResultado", DecisionTestFlags)

bool FDecisionPanelOrderTest::RunTest(const FString& Parameters)
{
	// CA-DEC-18 (R-DEC-6 y R-DEC-8): con un resultado reciente y una decisión abierta para el humano, PanelView es su decisión; con un resultado
	// reciente y una elección abierta para un dummy, es el resultado; con la mesa esperando al Warden, el resultado se sigue enseñando.
	{
		FMatchState State = DtMakeState();
		DtLogVoteOutcome(State, DtNow - 1.0);
		DtOpenDice(State, DtHuman);
		const TOptional<FDecisionView> View = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Hay vista con decisión propia y resultado"), View.IsSet()))
		{
			TestTrue(TEXT("Es la decisión propia"), View->Input == EDecisionInput::Buttons);
		}
	}
	{
		FMatchState State = DtMakeState();
		DtLogVoteOutcome(State, DtNow - 1.0);
		DtOpenDice(State, 2);
		const TOptional<FDecisionView> View = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Hay vista con elección ajena y resultado"), View.IsSet()))
		{
			TestTrue(TEXT("Es el resultado, no la espera"), View->Input == EDecisionInput::Result);
		}
		const TOptional<FDecisionView> Speaking = Soulbound::Presentation::PanelView(State, DtHuman, true, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Con el Warden hablando, el resultado se sigue enseñando"), Speaking.IsSet()))
		{
			TestTrue(TEXT("Sigue siendo el resultado"), Speaking->Input == EDecisionInput::Result);
		}
	}
	{
		// Caducado, vuelve la espera: el resultado no tapa para siempre.
		FMatchState State = DtMakeState();
		DtLogVoteOutcome(State, DtNow - 10.0);
		DtOpenDice(State, 2);
		const TOptional<FDecisionView> View = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Hay vista con el resultado caducado"), View.IsSet()))
		{
			TestTrue(TEXT("Es la espera"), View->Input == EDecisionInput::Wait);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOwnShownTest, "Soulbound.Game.Presentation.Decision.EstadoSoloConDecisionPropia", DecisionTestFlags)

bool FDecisionOwnShownTest::RunTest(const FString& Parameters)
{
	// Adición a del autor (11/11/2026): FStatusContext::bDecisionPanelShown solo se activa cuando el panel enseña una decisión del humano, nunca
	// con la espera ni con el resultado ni con el anillo abierto; si no, el panel de estado dejaría de decir lo que el panel no dice.
	FMatchState Own = DtMakeState();
	DtOpenDice(Own, DtHuman);
	const TOptional<FDecisionView> OwnView = Soulbound::Presentation::PanelView(Own, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
	TestTrue(TEXT("Con una decisión propia, sí"), Soulbound::Presentation::IsOwnDecisionShown(OwnView, false));
	TestFalse(TEXT("Con el anillo abierto, no"), Soulbound::Presentation::IsOwnDecisionShown(OwnView, true));

	FMatchState Wait = DtMakeState();
	DtOpenDice(Wait, 2);
	const TOptional<FDecisionView> WaitPanel = Soulbound::Presentation::PanelView(Wait, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay espera"), WaitPanel.IsSet()))
	{
		TestFalse(TEXT("Con la espera, no"), Soulbound::Presentation::IsOwnDecisionShown(WaitPanel, false));
		TestTrue(TEXT("Pero el panel sí se enseña"), Soulbound::Presentation::IsDecisionPanelShown(WaitPanel, false));
	}
	FMatchState Result = DtMakeState();
	DtLogVoteOutcome(Result, DtNow - 1.0);
	const TOptional<FDecisionView> ResultPanel = Soulbound::Presentation::PanelView(Result, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay resultado"), ResultPanel.IsSet()))
	{
		TestFalse(TEXT("Con el resultado, no"), Soulbound::Presentation::IsOwnDecisionShown(ResultPanel, false));
		TestTrue(TEXT("Pero el panel sí se enseña"), Soulbound::Presentation::IsDecisionPanelShown(ResultPanel, false));
	}
	TestFalse(TEXT("Sin vista, no"), Soulbound::Presentation::IsOwnDecisionShown(TOptional<FDecisionView>(), false));
	return true;
}

// ---- S-033, R-DEC-10: el desenlace (añadido del autor tras la prueba en PIE) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOutcomeDuelTest, "Soulbound.Game.Presentation.Decision.DesenlaceDuelo", DecisionTestFlags)

bool FDecisionOutcomeDuelTest::RunTest(const FString& Parameters)
{
	// CA-DEC-19 (R-DEC-10, caso 1): un DuelWon del 2 sobre el 0 en el segundo 100, con el humano en el 0.
	FMatchState State = DtMakeState();
	DtLog(State, EMatchEventType::DuelWon, 100.0, 2, 0, INDEX_NONE, INDEX_NONE);
	State.ClockSeconds = 101.0;
	const TOptional<FDecisionView> View = Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista, aunque el humano sea el perdedor"), View.IsSet()))
	{
		TestTrue(TEXT("Es de tipo resultado"), View->Input == EDecisionInput::Result);
		TestTrue(TEXT("Sin opciones"), View->Options.IsEmpty());
		TestFalse(TEXT("Sin barra"), View->bHasDeadline);
		TestEqual(TEXT("Título"), View->Title, FString(TEXT("Duelo")));
		TestEqual(TEXT("Gana el 2 al 0"), View->Context, FString(TEXT("Gana Asiento 2 (Eco) a Asiento 0 (Eco)")));
	}
	State.ClockSeconds = 103.9;
	TestTrue(TEXT("Justo antes de caducar, sigue"), Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds).IsSet());
	State.ClockSeconds = 104.0;
	TestFalse(TEXT("Pasado OutcomeSeconds, no hay vista"), Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds).IsSet());
	TestFalse(TEXT("Control: sin nada en el registro, no hay vista"), Soulbound::Presentation::OutcomeView(DtMakeState(), DtHuman, DtOutcomeSeconds).IsSet());

	// Contra el Warden.
	FMatchState Won = DtMakeState();
	DtLog(Won, EMatchEventType::WardenDuelWon, 100.0, 1);
	const TOptional<FDecisionView> WonView = Soulbound::Presentation::OutcomeView(Won, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista si gana el Warden"), WonView.IsSet()))
	{
		TestEqual(TEXT("Título"), WonView->Title, FString(TEXT("Duelo contra el Warden")));
		TestEqual(TEXT("Gana el Warden"), WonView->Context, FString(TEXT("Gana el Warden: Asiento 1 (Eco) pierde lo que estaba en juego")));
	}
	FMatchState Lost = DtMakeState();
	DtLog(Lost, EMatchEventType::WardenDuelLost, 100.0, 1);
	const TOptional<FDecisionView> LostView = Soulbound::Presentation::OutcomeView(Lost, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista si pierde el Warden"), LostView.IsSet()))
	{
		TestEqual(TEXT("Título"), LostView->Title, FString(TEXT("Duelo contra el Warden")));
		TestEqual(TEXT("Gana el jugador"), LostView->Context, FString(TEXT("Gana Asiento 1 (Eco): no pierde nada")));
	}
	// Con el humano como jugador, también.
	FMatchState Own = DtMakeState();
	DtLog(Own, EMatchEventType::WardenDuelLost, 100.0, DtHuman);
	TestTrue(TEXT("Se enseña también si el humano es el jugador"), Soulbound::Presentation::OutcomeView(Own, DtHuman, DtOutcomeSeconds).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOutcomeDilemmaTest, "Soulbound.Game.Presentation.Decision.DesenlaceDilema", DecisionTestFlags)

bool FDecisionOutcomeDilemmaTest::RunTest(const FString& Parameters)
{
	// CA-DEC-20 (R-DEC-10, caso 2): un DilemmaChosen del asiento 2, opción 1, tras el WardenEventStarted del dilema.
	const FMatchState Table = DtMakeState();
	FString OptionText = Soulbound::Presentation::DescribeOption(Table.WardenEvents.Events[DtDilemma].Options[1]);
	TestFalse(TEXT("Control del test: la opción tiene texto"), OptionText.IsEmpty());
	OptionText = OptionText.Left(1).ToUpper() + OptionText.Mid(1);

	FMatchState State = DtMakeState();
	DtLog(State, EMatchEventType::WardenEventStarted, 90.0, 2, INDEX_NONE, DtDilemma, 2);
	DtLog(State, EMatchEventType::DilemmaChosen, 100.0, 2, INDEX_NONE, 1, 0);
	const TOptional<FDecisionView> View = Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		TestEqual(TEXT("El título es el evento"), View->Title, FString(TEXT("La balanza")));
		TestEqual(TEXT("La línea lleva el texto de la opción"), View->Context, FString::Printf(TEXT("Asiento 2 (Eco) elige: %s"), *OptionText));
	}
	// Sorteada por el Warden al agotarse el reloj.
	FMatchState Drawn = DtMakeState();
	DtLog(Drawn, EMatchEventType::WardenEventStarted, 90.0, 2, INDEX_NONE, DtDilemma, 2);
	DtLog(Drawn, EMatchEventType::DilemmaChosen, 100.0, 2, INDEX_NONE, 1, 1);
	const TOptional<FDecisionView> DrawnView = Soulbound::Presentation::OutcomeView(Drawn, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista con la opción sorteada"), DrawnView.IsSet()))
	{
		TestEqual(TEXT("Elige el Warden"), DrawnView->Context, FString::Printf(TEXT("El Warden elige por Asiento 2 (Eco): %s"), *OptionText));
	}
	// El humano que eligió: nada. Al humano al que se le sorteó: sí.
	FMatchState Human = DtMakeState();
	DtLog(Human, EMatchEventType::WardenEventStarted, 90.0, DtHuman, INDEX_NONE, DtDilemma, 2);
	DtLog(Human, EMatchEventType::DilemmaChosen, 100.0, DtHuman, INDEX_NONE, 1, 0);
	TestFalse(TEXT("La opción que eligió el humano no se enseña"), Soulbound::Presentation::OutcomeView(Human, DtHuman, DtOutcomeSeconds).IsSet());
	FMatchState HumanDrawn = DtMakeState();
	DtLog(HumanDrawn, EMatchEventType::WardenEventStarted, 90.0, DtHuman, INDEX_NONE, DtDilemma, 2);
	DtLog(HumanDrawn, EMatchEventType::DilemmaChosen, 100.0, DtHuman, INDEX_NONE, 1, 1);
	const TOptional<FDecisionView> HumanDrawnView = Soulbound::Presentation::OutcomeView(HumanDrawn, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Si se le sorteó al humano, sí"), HumanDrawnView.IsSet()))
	{
		TestEqual(TEXT("Elige el Warden por el asiento 0"), HumanDrawnView->Context, FString::Printf(TEXT("El Warden elige por Asiento 0 (Eco): %s"), *OptionText));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOutcomeShellTest, "Soulbound.Game.Presentation.Decision.DesenlaceTrilero", DecisionTestFlags)

bool FDecisionOutcomeShellTest::RunTest(const FString& Parameters)
{
	// CA-DEC-21 (R-DEC-10, caso 3): un RuneStolen del asiento 1 al 3 con la runa de tipo 2 (decisión del autor: "la runa D2", como los botones).
	FMatchState State = DtMakeState();
	DtLog(State, EMatchEventType::RuneStolen, 100.0, 1, 3, 2);
	const TOptional<FDecisionView> View = Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		TestEqual(TEXT("Título"), View->Title, FString(TEXT("Trilero")));
		TestEqual(TEXT("Ladrón, víctima y runa"), View->Context, FString(TEXT("Asiento 1 (Eco) roba a Asiento 3 (Eco) la runa D2")));
	}
	FMatchState Expired = DtMakeState();
	DtLog(Expired, EMatchEventType::ShellGameExpired, 100.0, 1, 3);
	const TOptional<FDecisionView> ExpiredView = Soulbound::Presentation::OutcomeView(Expired, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista si no elige a tiempo"), ExpiredView.IsSet()))
	{
		TestEqual(TEXT("Título"), ExpiredView->Title, FString(TEXT("Trilero")));
		TestEqual(TEXT("No roba nada"), ExpiredView->Context, FString(TEXT("Asiento 1 (Eco) no elige a tiempo y no roba nada")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOutcomeTrialTest, "Soulbound.Game.Presentation.Decision.DesenlacePrueba", DecisionTestFlags)

bool FDecisionOutcomeTrialTest::RunTest(const FString& Parameters)
{
	// CA-DEC-22 (R-DEC-10, caso 4): la prueba de otro asiento, con el nombre de la prueba (el de la espera) y si la supera o falla.
	struct FTrialCase { EMatchEventType Type; const TCHAR* Title; const TCHAR* Line; };
	const FTrialCase Cases[] = {
		{EMatchEventType::SkillMinigamePassed, TEXT("Prueba de la Fuente"), TEXT("Asiento 2 (Eco) la supera")},
		{EMatchEventType::SkillMinigameFailed, TEXT("Prueba de la Fuente"), TEXT("Asiento 2 (Eco) falla")},
		{EMatchEventType::WardenTrialPassed, TEXT("Prueba del Warden"), TEXT("Asiento 2 (Eco) la supera")},
		{EMatchEventType::WardenTrialFailed, TEXT("Prueba del Warden"), TEXT("Asiento 2 (Eco) falla")},
		{EMatchEventType::VoidEscapePassed, TEXT("Escape del Vacío"), TEXT("Asiento 2 (Eco) la supera")},
		{EMatchEventType::VoidEscapeFailed, TEXT("Escape del Vacío"), TEXT("Asiento 2 (Eco) falla")},
		{EMatchEventType::FinalChallengeFailed, TEXT("Desafío final"), TEXT("Asiento 2 (Eco) falla")},
	};
	for (const FTrialCase& Case : Cases)
	{
		FMatchState State = DtMakeState();
		DtLog(State, Case.Type, 100.0, 2, INDEX_NONE, 1);
		const TOptional<FDecisionView> View = Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds);
		if (TestTrue(*FString::Printf(TEXT("Hay vista para %s"), Case.Line), View.IsSet()))
		{
			TestEqual(TEXT("Nombre de la prueba"), View->Title, FString(Case.Title));
			TestEqual(TEXT("Resultado"), View->Context, FString(Case.Line));
		}
		// Lo mismo del humano: su resultado ya lo enseña el anillo.
		FMatchState Own = DtMakeState();
		DtLog(Own, Case.Type, 100.0, DtHuman, INDEX_NONE, 1);
		TestFalse(TEXT("Las pruebas del humano no"), Soulbound::Presentation::OutcomeView(Own, DtHuman, DtOutcomeSeconds).IsSet());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOutcomeOrderTest, "Soulbound.Game.Presentation.Decision.DesenlaceOrden", DecisionTestFlags)

bool FDecisionOutcomeOrderTest::RunTest(const FString& Parameters)
{
	// CA-DEC-23 (R-DEC-6 y R-DEC-10): el desenlace va después de lo propio y antes de la espera, también con el Warden hablando; entre una
	// votación y un desenlace manda el suceso más reciente; lo que no está en la lista no da vista.
	{
		FMatchState State = DtMakeState();
		DtLog(State, EMatchEventType::DuelWon, DtNow - 1.0, 2, 3);
		DtOpenDice(State, DtHuman);
		const TOptional<FDecisionView> View = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		const TOptional<FDecisionView> Own = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Con una decisión propia, hay panel"), View.IsSet() && Own.IsSet()))
		{
			TestTrue(TEXT("Y es la decisión propia, no el desenlace"), DtSameView(*View, *Own));
		}
	}
	{
		FMatchState State = DtMakeState();
		DtLog(State, EMatchEventType::DuelWon, DtNow - 1.0, 2, 3);
		DtOpenDice(State, 1);
		const TOptional<FDecisionView> View = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Con la elección de un dummy y un desenlace, hay panel"), View.IsSet()))
		{
			TestTrue(TEXT("Es el desenlace, no la espera"), View->Input == EDecisionInput::Result && View->Title == TEXT("Duelo"));
		}
		const TOptional<FDecisionView> Speaking = Soulbound::Presentation::PanelView(State, DtHuman, true, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Con el Warden hablando, el desenlace se sigue enseñando"), Speaking.IsSet()))
		{
			TestTrue(TEXT("Es el desenlace"), Speaking->Input == EDecisionInput::Result);
		}
		// Control: sin el desenlace (caducado), vuelve la espera.
		State.ClockSeconds = DtNow + 10.0;
		const TOptional<FDecisionView> Later = Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Caducado el desenlace, hay espera"), Later.IsSet()))
		{
			TestTrue(TEXT("Es la espera"), Later->Input == EDecisionInput::Wait);
		}
	}
	{
		// El más nuevo gana: un desenlace posterior a la votación, y al revés.
		FMatchState NewerOutcome = DtMakeState();
		DtLogVoteOutcome(NewerOutcome, DtNow - 2.0);
		DtLog(NewerOutcome, EMatchEventType::DuelWon, DtNow - 1.0, 2, 3);
		const TOptional<FDecisionView> A = Soulbound::Presentation::PanelView(NewerOutcome, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Hay panel con votación y desenlace"), A.IsSet()))
		{
			TestEqual(TEXT("Manda el desenlace, más nuevo"), A->Title, FString(TEXT("Duelo")));
		}
		FMatchState NewerVote = DtMakeState();
		DtLog(NewerVote, EMatchEventType::DuelWon, DtNow - 2.0, 2, 3);
		DtLogVoteOutcome(NewerVote, DtNow - 1.0);
		const TOptional<FDecisionView> B = Soulbound::Presentation::PanelView(NewerVote, DtHuman, false, DtResultSeconds, DtOutcomeSeconds);
		if (TestTrue(TEXT("Hay panel con desenlace y votación"), B.IsSet()))
		{
			TestEqual(TEXT("Manda la votación, más nueva"), B->Title, FString(TEXT("El juicio")));
		}
	}
	{
		// Sucesos fuera de la lista: nada.
		FMatchState State = DtMakeState();
		DtLog(State, EMatchEventType::DiceChosen, DtNow - 1.0, 2, INDEX_NONE, 1);
		DtLog(State, EMatchEventType::PlanarJumped, DtNow - 1.0, 2);
		DtLog(State, EMatchEventType::Expelled, DtNow - 1.0, 2, 3);
		TestFalse(TEXT("DiceChosen, PlanarJumped y Expelled no dan vista"), Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds).IsSet());
		TestFalse(TEXT("Ni panel"), Soulbound::Presentation::PanelView(State, DtHuman, false, DtResultSeconds, DtOutcomeSeconds).IsSet());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOutcomePointTest, "Soulbound.Game.Presentation.Decision.DesenlaceSenalar", DecisionTestFlags)

bool FDecisionOutcomePointTest::RunTest(const FString& Parameters)
{
	// CA-DEC-24 (R-DEC-10, caso 5): un EffectApplied de Revelar Brazal sobre el 3 (Seat), señalado por el 1 (OtherSeat). Value es el efecto.
	const int32 Reveal = static_cast<int32>(EEffect::RevealBracer);
	FMatchState State = DtMakeState();
	DtLog(State, EMatchEventType::EffectApplied, 100.0, 3, 1, Reveal, static_cast<int32>(EEffectTarget::ChosenRival));
	const TOptional<FDecisionView> View = Soulbound::Presentation::OutcomeView(State, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		TestEqual(TEXT("El título es el nombre del efecto"), View->Title, FString(TEXT("Revelar Brazal")));
		TestEqual(TEXT("Quién señala a quién"), View->Context, FString(TEXT("Asiento 1 (Eco) señala a Asiento 3 (Eco)")));
		TestTrue(TEXT("Es un resultado, sin barra"), View->Input == EDecisionInput::Result && !View->bHasDeadline);
	}
	// Señalado por el humano: no se enseña. Con el humano de señalado, sí.
	FMatchState ByHuman = DtMakeState();
	DtLog(ByHuman, EMatchEventType::EffectApplied, 100.0, 3, DtHuman, Reveal, static_cast<int32>(EEffectTarget::ChosenRival));
	TestFalse(TEXT("Si señala el humano, no"), Soulbound::Presentation::OutcomeView(ByHuman, DtHuman, DtOutcomeSeconds).IsSet());
	FMatchState OnHuman = DtMakeState();
	DtLog(OnHuman, EMatchEventType::EffectApplied, 100.0, DtHuman, 2, Reveal, static_cast<int32>(EEffectTarget::ChosenRival));
	const TOptional<FDecisionView> OnHumanView = Soulbound::Presentation::OutcomeView(OnHuman, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Si el señalado es el humano, sí"), OnHumanView.IsSet()))
	{
		TestEqual(TEXT("Línea"), OnHumanView->Context, FString(TEXT("Asiento 2 (Eco) señala a Asiento 0 (Eco)")));
	}
	// Sin señalador (efecto sin rival señalado), o señalándose a sí mismo: nada.
	FMatchState NoPointer = DtMakeState();
	DtLog(NoPointer, EMatchEventType::EffectApplied, 100.0, 3, INDEX_NONE, Reveal, static_cast<int32>(EEffectTarget::Leader));
	TestFalse(TEXT("Sin señalador, no"), Soulbound::Presentation::OutcomeView(NoPointer, DtHuman, DtOutcomeSeconds).IsSet());
	FMatchState Self = DtMakeState();
	DtLog(Self, EMatchEventType::EffectApplied, 100.0, 3, 3, Reveal, static_cast<int32>(EEffectTarget::ChosenRival));
	TestFalse(TEXT("Un asiento que se señala a sí mismo, no"), Soulbound::Presentation::OutcomeView(Self, DtHuman, DtOutcomeSeconds).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionOptionForPlayerTest, "Soulbound.Game.Presentation.Decision.TextoDeOpcionParaElJugador", DecisionTestFlags)

bool FDecisionOptionForPlayerTest::RunTest(const FString& Parameters)
{
	// CA-DEC-25 (R-DEC-11): sin la dirección cuando recae en quien elige; las demás se conservan; DescribeOption no cambia.
	FWardenEventOption Lose;
	FEffectApplication LoseRune{EEffect::LoseRune, EEffectTarget::ActivePlayer};
	LoseRune.Variant = EEffectVariant::Chosen;
	Lose.Applications.Add(LoseRune);
	TestEqual(TEXT("Para el jugador, sin \"al activo\""), Soulbound::Presentation::DescribeOptionForPlayer(Lose), FString(TEXT("Perder una runa (a elección)")));
	TestEqual(TEXT("DescribeOption no cambia"), Soulbound::Presentation::DescribeOption(Lose), FString(TEXT("Perder una runa (a elección) al activo")));

	FWardenEventOption Leader;
	Leader.Applications.Add({EEffect::LoseTurn, EEffectTarget::Leader});
	TestEqual(TEXT("Al líder se conserva"), Soulbound::Presentation::DescribeOptionForPlayer(Leader), FString(TEXT("Turno perdido al líder")));

	FWardenEventOption Two;
	Two.Applications.Add({EEffect::ExtraTurn, EEffectTarget::ActivePlayer});
	Two.Applications.Add({EEffect::LoseTurn, EEffectTarget::NextPlayer});
	TestEqual(TEXT("Varias aplicaciones, cada una con lo suyo"), Soulbound::Presentation::DescribeOptionForPlayer(Two), FString(TEXT("Turno extra y luego Turno perdido al siguiente")));

	// El botón del dilema y su desenlace usan el texto para el jugador. La opción 0 de la tabla de prueba es Turno extra al activo.
	FMatchState State = DtMakeState();
	State.ActiveSeat = 2;
	State.CurrentEvent.EventIndex = DtDilemma;
	State.CurrentEvent.Option = 0;
	State.PendingChoice.Kind = EChoiceKind::Dilemma;
	State.PendingChoice.Seat = DtHuman;
	State.PendingChoice.Options = {0, 1};
	State.PendingChoice.DeadlineSeconds = DtNow + 8.0;
	const TOptional<FDecisionView> Buttons = Soulbound::Presentation::DecisionView(State, DtHuman, false);
	if (TestTrue(TEXT("Hay botones del dilema"), Buttons.IsSet() && Buttons->Options.Num() >= 2))
	{
		TestEqual(TEXT("Botón 1 sin dirección"), Buttons->Options[0].Text, FString(TEXT("Turno extra")));
		TestEqual(TEXT("Botón 2 con su dirección"), Buttons->Options[1].Text, FString(TEXT("Turno perdido al siguiente")));
	}
	FMatchState Outcome = DtMakeState();
	DtLog(Outcome, EMatchEventType::WardenEventStarted, 90.0, 2, INDEX_NONE, DtDilemma, 2);
	DtLog(Outcome, EMatchEventType::DilemmaChosen, 100.0, 2, INDEX_NONE, 0, 0);
	const TOptional<FDecisionView> OutcomeLine = Soulbound::Presentation::OutcomeView(Outcome, DtHuman, DtOutcomeSeconds);
	if (TestTrue(TEXT("Hay desenlace del dilema"), OutcomeLine.IsSet()))
	{
		TestEqual(TEXT("Sin dirección en el desenlace"), OutcomeLine->Context, FString(TEXT("Asiento 2 (Eco) elige: Turno extra")));
	}
	return true;
}


#endif
