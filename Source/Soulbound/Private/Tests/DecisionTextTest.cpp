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
	TestEqual(TEXT("Las opciones del dilema, en su orden"), DtTexts(*View),
		TArray<FString>{Soulbound::Presentation::DescribeOption(Def.Options[0]), Soulbound::Presentation::DescribeOption(Def.Options[1])});
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
	// iluminada" y el plazo del reloj de turno.
	{
		FMatchState State = DtMakeState();
		DtOpenTile(State);
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, DtHuman, false);
		if (TestTrue(TEXT("Hay vista"), View.IsSet()))
		{
			TestTrue(TEXT("Tipo casilla"), View->Input == EDecisionInput::Tile);
			TestEqual(TEXT("Sin opciones"), View->Options.Num(), 0);
			TestTrue(TEXT("Elige una casilla iluminada"), View->Instruction.Contains(TEXT("Elige una casilla iluminada")));
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
			TestTrue(TEXT("Elige una casilla iluminada"), View->Instruction.Contains(TEXT("Elige una casilla iluminada")));
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

#endif
