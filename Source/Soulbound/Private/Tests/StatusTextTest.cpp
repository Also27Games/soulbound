// Tests de los paneles de estado y de depuración del HUD (S-027, R-HUD-1, R-HUD-2, R-HUD-3, R-HUD-5): qué líneas van a cada panel, en qué
// orden, y la escala de la letra con la altura de la pantalla. Funciones puras sobre un FMatchState armado a mano: sin mundo, sin actor y sin
// disco, como BoardLayoutTest. Filtro: "Soulbound.Game.Presentation.Estado".

#include "../../Presentation/ColumnTab.h"
#include "../../Presentation/StatusText.h"
#include "Elects/ElectsLoader.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags StatusTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::FPanelLine;
	using Soulbound::Presentation::FStatusContext;

	constexpr int32 StHuman = 0;
	/** Una semilla que no se confunde con ningún otro número de las líneas. */
	constexpr int32 StSeed = 987654;
	const TCHAR* const StViewKeys = TEXT("TECLAS-DE-LA-VISTA");

	/** Cuatro asientos, el humano en su turno de tirada: tirada 4, quedan 15 s de reloj de turno, la carrera en marcha. */
	FMatchState StMakeState()
	{
		FMatchState State;
		State.Seats.SetNum(4);
		State.MatchPhase = EMatchPhase::BondRace;
		State.ActiveSeat = StHuman;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.Roll = 4;
		State.ClockSeconds = 25.0;
		State.TurnDeadlineSeconds = 40.0;
		State.ReachableOptions = {1, 2};
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			State.Seats[Seat].Insolence = Seat + 1;
		}
		return State;
	}

	FStatusContext StMakeContext()
	{
		FStatusContext Context;
		Context.HumanSeat = StHuman;
		Context.Seed = StSeed;
		Context.ViewKeys = StViewKeys;
		Context.ReturnKey = TEXT("Intro");
		Context.SeatColors = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
		return Context;
	}

	bool StAnyContains(const TArray<FPanelLine>& Lines, const FString& Text)
	{
		return Lines.ContainsByPredicate([&Text](const FPanelLine& Line) { return Line.Text.Contains(Text); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusRequestFirstTest, "Soulbound.Game.Presentation.Estado.PeticionPrimeroSinSemilla", StatusTestFlags)

bool FStatusRequestFirstTest::RunTest(const FString& Parameters)
{
	// CA-HUD-1 (R-HUD-1, R-HUD-5): en el turno del humano con la elección de destino abierta, las líneas del panel de estado tienen la tirada,
	// el reloj y la petición al jugador en primera posición, con la letra grande; ninguna lleva la semilla ni la palabra "Semilla".
	{
		const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(StMakeState(), StMakeContext());
		if (!TestTrue(TEXT("Hay líneas"), Lines.Num() > 0))
		{
			return false;
		}
		TestTrue(TEXT("La primera lleva la tirada"), Lines[0].Text.Contains(TEXT("Tirada: 4")));
		TestTrue(TEXT("Y el reloj de turno"), Lines[0].Text.Contains(TEXT("reloj de turno: 15 s")));
		TestTrue(TEXT("Y la petición al jugador"), Lines[0].Text.Contains(TEXT("tu turno")));
		TestTrue(TEXT("Con la letra grande"), Lines[0].bLarge);
		TestFalse(TEXT("Ninguna dice Semilla"), StAnyContains(Lines, TEXT("Semilla")));
		TestFalse(TEXT("Ninguna lleva la semilla"), StAnyContains(Lines, FString::FromInt(StSeed)));
		TestFalse(TEXT("Ninguna lleva las teclas de la vista"), StAnyContains(Lines, StViewKeys));
	}
	{
		// Una elección pendiente del humano es la petición: va la primera y grande; la tirada sigue, detrás.
		FMatchState State = StMakeState();
		State.TurnPhase = ETurnPhase::PreRoll;
		State.PendingChoice.Kind = EChoiceKind::DiceCount;
		State.PendingChoice.Seat = StHuman;
		State.PendingChoice.Options = {1, 2};
		State.PendingChoice.DeadlineSeconds = 35.0;
		const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, StMakeContext());
		if (TestTrue(TEXT("Hay líneas con la elección"), Lines.Num() > 1))
		{
			TestTrue(TEXT("La primera es la elección del humano"), Lines[0].Text.StartsWith(TEXT("ELIGE dados")));
			TestTrue(TEXT("Con la letra grande"), Lines[0].bLarge);
			TestTrue(TEXT("La tirada está detrás"), StAnyContains(Lines, TEXT("Tirada: 4")));
		}
	}
	{
		// La elección de un dummy no es la petición al jugador: ni primera ni grande.
		FMatchState State = StMakeState();
		State.ActiveSeat = 1;
		State.TurnPhase = ETurnPhase::PreRoll;
		State.PendingChoice.Kind = EChoiceKind::DiceCount;
		State.PendingChoice.Seat = 1;
		State.PendingChoice.Options = {1, 2};
		const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, StMakeContext());
		const int32 Prompt = Lines.IndexOfByPredicate([](const FPanelLine& Line) { return Line.Text.StartsWith(TEXT("el asiento 1 elige dados")); });
		if (TestTrue(TEXT("La elección del dummy está"), Prompt != INDEX_NONE))
		{
			TestTrue(TEXT("Pero no la primera"), Prompt > 0);
			TestFalse(TEXT("Ni grande"), Lines[Prompt].bLarge);
		}
	}
	{
		// En el prólogo, la línea de la fase no lleva las teclas de la vista (S-027: fuera las teclas de depuración del panel del jugador).
		FStatusContext Context = StMakeContext();
		Context.bPrologue = true;
		const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(StMakeState(), Context);
		TestTrue(TEXT("El prólogo se anuncia"), StAnyContains(Lines, TEXT("Prólogo")));
		TestFalse(TEXT("Sin las teclas de la vista"), StAnyContains(Lines, StViewKeys));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusResultFirstTest, "Soulbound.Game.Presentation.Estado.ResultadoArribaYGrande", StatusTestFlags)

bool FStatusResultFirstTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 9/11/2026 (R-HUD-1): las líneas que la story no repartía (votación, aspirantes y resultado) van al panel de
	// estado, con el resultado de la partida arriba y con la letra grande.
	{
		FMatchState State = StMakeState();
		State.MatchPhase = EMatchPhase::MatchOver;
		State.Outcome = EMatchOutcome::PlayerVictory;
		State.WinnerSeat = 2;
		const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, StMakeContext());
		if (TestTrue(TEXT("Hay líneas"), Lines.Num() > 0))
		{
			TestEqual(TEXT("La primera es el resultado"), Lines[0].Text, FString(TEXT("GANA EL ASIENTO 2\nIntro o el botón: volver al menú")));
			TestTrue(TEXT("Con la letra grande"), Lines[0].bLarge);
			TestEqual(TEXT("Del color del ganador"), Lines[0].Color, FLinearColor::Blue);
		}
	}
	{
		FMatchState State = StMakeState();
		State.Vote.bOpen = true;
		State.Vote.Votables = {1, 2};
		State.Vote.Votes.Init(INDEX_NONE, 4);
		State.Vote.DeadlineSeconds = 40.0;
		TestTrue(TEXT("La votación abierta va al panel de estado"), StAnyContains(Soulbound::Presentation::StatusLines(State, StMakeContext()), TEXT("VOTACIÓN")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusDebugTest, "Soulbound.Game.Presentation.Estado.DepuracionAparte", StatusTestFlags)

bool FStatusDebugTest::RunTest(const FString& Parameters)
{
	// CA-HUD-2 (R-HUD-2): las líneas del panel de depuración tienen la semilla, el tiempo y la insolencia de los cuatro asientos, y ninguna de
	// las del panel de estado.
	const FMatchState State = StMakeState();
	FStatusContext Context = StMakeContext();
	Context.bStepMode = true;
	const TArray<FPanelLine> Debug = Soulbound::Presentation::DebugLines(State, Context);
	const TArray<FPanelLine> Status = Soulbound::Presentation::StatusLines(State, Context);
	TestTrue(TEXT("La semilla"), StAnyContains(Debug, FString::Printf(TEXT("Semilla: %d"), StSeed)));
	TestTrue(TEXT("El tiempo"), StAnyContains(Debug, TEXT("t=25 s")));
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		TestTrue(FString::Printf(TEXT("La insolencia del asiento %d"), Seat), StAnyContains(Debug, FString::Printf(TEXT("insolencia %d de"), Seat + 1)));
	}
	TestTrue(TEXT("El paso a paso va aquí"), StAnyContains(Debug, TEXT("PASO A PASO")));
	TestFalse(TEXT("Y no en el de estado"), StAnyContains(Status, TEXT("PASO A PASO")));
	for (const FPanelLine& Line : Debug)
	{
		TestFalse(FString::Printf(TEXT("\"%s\" no está en el panel de estado"), *Line.Text),
			Status.ContainsByPredicate([&Line](const FPanelLine& Other) { return Other.Text == Line.Text; }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusScaleTest, "Soulbound.Game.Presentation.Estado.EscalaConLaAltura", StatusTestFlags)

bool FStatusScaleTest::RunTest(const FString& Parameters)
{
	// CA-HUD-5 (R-HUD-3): la letra es PanelFontSize a 1080 de alto, la mitad a 540 y nunca menos de PanelMinFontSize; el ancho de la columna
	// de la derecha escala igual.
	TestEqual(TEXT("A 1080, el tamaño de base"), Soulbound::Presentation::ScaledPanelFontSize(1080.f, 20, 9), 20);
	TestEqual(TEXT("A 540, la mitad"), Soulbound::Presentation::ScaledPanelFontSize(540.f, 20, 9), 10);
	TestEqual(TEXT("A 2160, el doble"), Soulbound::Presentation::ScaledPanelFontSize(2160.f, 20, 9), 40);
	TestEqual(TEXT("A 270, el mínimo y no 5"), Soulbound::Presentation::ScaledPanelFontSize(270.f, 20, 9), 9);
	TestEqual(TEXT("Con los valores iniciales a 768, el mínimo"), Soulbound::Presentation::ScaledPanelFontSize(768.f, 13, 9), 9);
	TestEqual(TEXT("Con los valores iniciales a 1080, 13"), Soulbound::Presentation::ScaledPanelFontSize(1080.f, 13, 9), 13);
	TestEqual(TEXT("Ancho a 1080"), Soulbound::Presentation::ScaledPanelWidth(1080.f, 460.f), 460.f);
	TestEqual(TEXT("Ancho a 540, la mitad"), Soulbound::Presentation::ScaledPanelWidth(540.f, 460.f), 230.f);
	TestEqual(TEXT("Sin altura, 0"), Soulbound::Presentation::ScaledPanelWidth(0.f, 460.f), 0.f);
	return true;
}

// ---- S-035: la pestaña de la columna y lo que se ve de los demás ----

namespace
{
	/** El asiento ajeno en las pruebas de S-035; quien mira es el humano de StMakeState (el 0). */
	constexpr int32 StOther = 2;

	/** Los nombres de poder que BracerText no puede decir de un Brazal oculto. */
	bool StNamesAPower(const FString& Text)
	{
		for (const TCHAR* Name : {TEXT("Duelo"), TEXT("Celeridad"), TEXT("Paso Libre"), TEXT("Expulsión"), TEXT("Vuelo Planar"), TEXT("Desvío")})
		{
			if (Text.Contains(Name))
			{
				return true;
			}
		}
		return false;
	}

	/** El asiento 2 con dos runas del Dominio 1 y una del 3 (CA-HUD-8); el 0, Elegido de Fenrir con una del Dominio 5 (Vuelo Planar). */
	bool StMakeRuneState(FAutomationTestBase& Test, FMatchState& OutState)
	{
		const FElectsLoadResult Elects = Soulbound::Rules::LoadElectsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Elects/elegidos.json")));
		if (!Test.TestTrue(FString::Printf(TEXT("elegidos.json se carga (%s)"), *Elects.Message), Elects.IsSuccess()))
		{
			return false;
		}
		OutState = StMakeState();
		OutState.Elects = Elects.Table;
		OutState.Round = 3;
		OutState.Seats[StHuman].Role = ESeatRole::Elect;
		OutState.Seats[StHuman].DomainId = 1;
		OutState.Seats[StHuman].Runes[5] = 1;
		OutState.Seats[StOther].Runes[1] = 2;
		OutState.Seats[StOther].Runes[3] = 1;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusColumnTabTest, "Soulbound.Game.Presentation.Estado.PestanaInicialYCambio", StatusTestFlags)

bool FStatusColumnTabTest::RunTest(const FString& Parameters)
{
	// CA-HUD-6 (R-HUD-4): la pestaña inicial es Leyenda; cambiar da Crónica y volver a cambiar, Leyenda; una partida nueva (Reset) empieza otra
	// vez en Leyenda aunque se hubiera dejado en Crónica.
	using Soulbound::Presentation::EColumnTab;
	Soulbound::Presentation::FColumnTabState Tab;
	TestTrue(TEXT("Empieza en Leyenda"), Tab.Active == EColumnTab::Legend);
	Tab.Toggle();
	TestTrue(TEXT("Cambiar da Crónica"), Tab.Active == EColumnTab::Chronicle);
	Tab.Toggle();
	TestTrue(TEXT("Y volver a cambiar, Leyenda"), Tab.Active == EColumnTab::Legend);
	Tab.Toggle();
	Tab.Reset();
	TestTrue(TEXT("Una partida nueva empieza otra vez en Leyenda"), Tab.Active == EColumnTab::Legend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusCanSeeBracerTest, "Soulbound.Game.Presentation.Estado.VerElBrazalAjeno", StatusTestFlags)

bool FStatusCanSeeBracerTest::RunTest(const FString& Parameters)
{
	// CA-HUD-7 (R-RUN-7): CanSeeBracer es verdadero para el asiento propio; falso para otro; verdadero para otro con el Brazal revelado y falso
	// otra vez cuando la revelación caduca. Con un asiento que mira no válido, solo lo revelado (nunca todo).
	using Soulbound::Presentation::CanSeeBracer;
	FMatchState State = StMakeState();
	State.Round = 3;
	TestTrue(TEXT("El propio"), CanSeeBracer(State, StHuman, StHuman));
	TestFalse(TEXT("Otro asiento"), CanSeeBracer(State, StOther, StHuman));
	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	TestTrue(TEXT("Otro asiento con el Brazal revelado"), CanSeeBracer(State, StOther, StHuman));
	TestFalse(TEXT("Y el de un tercero, no"), CanSeeBracer(State, 3, StHuman));
	State.Round = State.Seats[StOther].RevealedUntilRound;
	TestFalse(TEXT("Caduca la revelación: otra vez no"), CanSeeBracer(State, StOther, StHuman));
	TestFalse(TEXT("Sin asiento que mira, nadie ve el de otro"), CanSeeBracer(State, StOther, INDEX_NONE));
	TestFalse(TEXT("Ni con un asiento que mira fuera de rango"), CanSeeBracer(State, StHuman, 9));
	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	TestTrue(TEXT("Lo revelado lo ve cualquiera, incluso sin asiento que mira"), CanSeeBracer(State, StOther, INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusBracerTextTest, "Soulbound.Game.Presentation.Estado.BrazalDeLosDemas", StatusTestFlags)

bool FStatusBracerTextTest::RunTest(const FString& Parameters)
{
	// CA-HUD-8 (R-RUN-7): con el asiento 2 con dos runas del Dominio 1 y una del 3 y el humano en el 0, el texto del Brazal del 2 dice que tiene
	// 3 runas y no nombra ni D1, ni D3, ni ningún poder; el del 0 lleva sus tipos y sus poderes; con el Brazal del 2 revelado, el del 2 lleva
	// los suyos. Un Fenrir ajeno conserva "Paso Libre (afinidad)".
	using Soulbound::Presentation::BracerText;
	FMatchState State;
	if (!StMakeRuneState(*this, State))
	{
		return false;
	}
	const FString Hidden = BracerText(State, StOther, StHuman);
	TestTrue(TEXT("El 2 dice que tiene 3 runas"), Hidden.Contains(TEXT("3 runas")));
	TestFalse(TEXT("Sin D1"), Hidden.Contains(TEXT("D1")));
	TestFalse(TEXT("Sin D3"), Hidden.Contains(TEXT("D3")));
	TestFalse(TEXT("Sin ningún poder"), StNamesAPower(Hidden));

	const FString Own = BracerText(State, StHuman, StHuman);
	TestTrue(TEXT("El propio lleva sus tipos"), Own.Contains(TEXT("D5×1")));
	TestTrue(TEXT("Y sus poderes"), Own.Contains(TEXT("Vuelo Planar")));

	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	const FString Revealed = BracerText(State, StOther, StHuman);
	TestTrue(TEXT("Revelado: sus tipos"), Revealed.Contains(TEXT("D1×2")) && Revealed.Contains(TEXT("D3×1")));
	State.Seats[StOther].RevealedUntilRound = 0;

	// Un Fenrir ajeno (Dominio 1 en elegidos.json) conserva el Paso Libre por afinidad, que es público, y sigue sin decir sus runas.
	State.Seats[3].Role = ESeatRole::Elect;
	State.Seats[3].DomainId = 1;
	State.Seats[3].Runes[2] = 1;
	const FString Fenrir = BracerText(State, 3, StHuman);
	TestTrue(TEXT("Paso Libre por afinidad, de un Elegido ajeno"), Fenrir.Contains(TEXT("Paso Libre (afinidad)")));
	TestFalse(TEXT("Pero no sus runas"), Fenrir.Contains(TEXT("D2")));

	// Sin asiento que mira válido, tampoco se enseña nada oculto.
	const FString NoViewer = BracerText(State, StOther, INDEX_NONE);
	TestFalse(TEXT("Con un asiento que mira no válido, sin tipos"), NoViewer.Contains(TEXT("D1")) || NoViewer.Contains(TEXT("D3")));
	TestFalse(TEXT("Ni los del humano"), BracerText(State, StHuman, INDEX_NONE).Contains(TEXT("D5")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusInsolenceTest, "Soulbound.Game.Presentation.Estado.InsolenciaAjena", StatusTestFlags)

bool FStatusInsolenceTest::RunTest(const FString& Parameters)
{
	// CA-HUD-9 (R-HUD-6): con el asiento 2 en insolencia 3 y el humano en el 1, el estado del 2 no dice su insolencia ni sus castigos; el del 1
	// dice la suya. El 2 Marcado se sigue diciendo. La depuración (EffectStatusTextAll) lo lleva todo.
	// La story dice "el del 0 dice la suya" con el humano en el 1; se entiende que cada uno ve la suya, la del asiento que mira.
	using Soulbound::Presentation::EffectStatusText;
	using Soulbound::Presentation::EffectStatusTextAll;
	FMatchState State = StMakeState();
	State.Round = 3;
	constexpr int32 Viewer = 1;
	State.Seats[StOther].Insolence = 3;
	State.Seats[Viewer].Insolence = 2;
	State.Seats[StOther].MarkedUntilRound = State.Round + 2;
	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	State.Seats[StOther].PendingPunishments.SetNum(1);

	const FString Other = EffectStatusText(State, StOther, Viewer);
	TestFalse(TEXT("El 2 no dice su insolencia"), Other.Contains(TEXT("insolencia")));
	TestFalse(TEXT("Ni sus castigos pendientes"), Other.Contains(TEXT("castigo")));
	TestTrue(TEXT("Marcado se sigue diciendo"), Other.Contains(TEXT("MARCADO")));
	TestTrue(TEXT("Y el Brazal revelado"), Other.Contains(TEXT("BRAZAL REVELADO")));

	TestTrue(TEXT("El del asiento que mira dice la suya"), EffectStatusText(State, Viewer, Viewer).Contains(TEXT("insolencia 2 de")));
	TestFalse(TEXT("Sin asiento que mira no se dice la de nadie"), EffectStatusText(State, StOther, INDEX_NONE).Contains(TEXT("insolencia")));
	TestFalse(TEXT("Ni con uno fuera de rango"), EffectStatusText(State, Viewer, 9).Contains(TEXT("insolencia")));

	const FString All = EffectStatusTextAll(State, StOther);
	TestTrue(TEXT("La depuración sí: insolencia"), All.Contains(TEXT("insolencia 3 de")));
	TestTrue(TEXT("Y castigos"), All.Contains(TEXT("castigo")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusChoiceOptionsTest, "Soulbound.Game.Presentation.Estado.ElegirRunaAjenaNoDiceTipos", StatusTestFlags)

bool FStatusChoiceOptionsTest::RunTest(const FString& Parameters)
{
	// Añadido del autor (R-RUN-7): el panel de estado enseña las opciones de la elección de un dummy; si son tipos de runa suyos (la que pierde),
	// no se dicen mientras su Brazal esté oculto. La elección del humano, entera. Con el Brazal revelado, entera también.
	FMatchState State = StMakeState();
	State.Round = 3;
	State.PendingChoice.Kind = EChoiceKind::LoseRune;
	State.PendingChoice.Seat = StOther;
	State.PendingChoice.Options = {1, 3};
	State.PendingChoice.DeadlineSeconds = 40.0;
	const FStatusContext Context = StMakeContext();

	const TArray<FPanelLine> Hidden = Soulbound::Presentation::StatusLines(State, Context);
	TestTrue(TEXT("Se dice que el 2 elige"), StAnyContains(Hidden, TEXT("el asiento 2 elige")));
	TestFalse(TEXT("Pero no sus tipos de runa"), StAnyContains(Hidden, TEXT("D1")) || StAnyContains(Hidden, TEXT("D3")));

	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	TestTrue(TEXT("Con el Brazal revelado, sí"), StAnyContains(Soulbound::Presentation::StatusLines(State, Context), TEXT("D1")));
	State.Seats[StOther].RevealedUntilRound = 0;

	State.PendingChoice.Seat = StHuman;
	TestTrue(TEXT("La elección del humano, entera"), StAnyContains(Soulbound::Presentation::StatusLines(State, Context), TEXT("D1")));

	// Sin asiento humano válido, la de un dummy sigue sin decir tipos.
	State.PendingChoice.Seat = StOther;
	FStatusContext NoHuman = Context;
	NoHuman.HumanSeat = INDEX_NONE;
	TestFalse(TEXT("Sin humano válido, nada oculto"), StAnyContains(Soulbound::Presentation::StatusLines(State, NoHuman), TEXT("D1")));
	return true;
}

#endif
