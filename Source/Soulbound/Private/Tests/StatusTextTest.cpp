// Tests de los paneles de estado y de depuración del HUD (S-027, R-HUD-1, R-HUD-2, R-HUD-3, R-HUD-5): qué líneas van a cada panel, en qué
// orden, y la escala de la letra con la altura de la pantalla. Desde S-045 (R-HUD-14, R-HUD-15), la caja de la partida y qué piezas se ven. Funciones puras sobre un FMatchState armado a mano: sin mundo, sin actor y sin
// disco, como BoardLayoutTest. Filtro: "Soulbound.Game.Presentation.Estado".

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
		TestTrue(TEXT("La primera lleva la tirada"), Lines[0].Text.Contains(TEXT("Dado: 4")));
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
			TestTrue(TEXT("La tirada está detrás"), StAnyContains(Lines, TEXT("Dado: 4")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusResultFirstTest, "Soulbound.Game.Presentation.Estado.ResultadoFueraDelPanel", StatusTestFlags)

bool FStatusResultFirstTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 9/11/2026 (R-HUD-1): las líneas que la story no repartía (votación, aspirantes y resultado) van al panel de
	// estado. CA-HUD-16 (R-HUD-8, añadido del autor del 16/11/2026 en S-041): el resultado grande ya no va aquí, va en el centro, encima del botón
	// (MatchResultLine); el panel de estado conserva la línea pequeña que explica el final, sin letra grande.
	{
		FMatchState State = StMakeState();
		State.MatchPhase = EMatchPhase::MatchOver;
		State.Outcome = EMatchOutcome::PlayerVictory;
		State.WinnerSeat = 2;
		const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, StMakeContext());
		TestFalse(TEXT("Sin el resultado grande"), StAnyContains(Lines, TEXT("GANA EL")));
		TestFalse(TEXT("Sin la línea de volver al menú"), StAnyContains(Lines, TEXT("volver al menú")));
		const int32 Explains = Lines.IndexOfByPredicate([](const FPanelLine& Line) { return Line.Text.StartsWith(TEXT("VICTORIA DEL ASIENTO 2: supera el desafío final.")); });
		if (TestTrue(TEXT("Conserva la línea que explica el final"), Explains != INDEX_NONE))
		{
			TestFalse(TEXT("Pequeña"), Lines[Explains].bLarge);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusResultLineTest, "Soulbound.Game.Presentation.Estado.ResultadoEnElCentro", StatusTestFlags)

bool FStatusResultLineTest::RunTest(const FString& Parameters)
{
	// CA-HUD-16 (R-HUD-8, añadido del autor del 16/11/2026 en S-041): el texto grande del resultado, el que va encima del botón "Volver al menú",
	// con su color de hoy: el del ganador si gana un jugador y rojo si gana el Warden, en los tres finales. Sin final, vacío.
	using Soulbound::Presentation::MatchResultLine;
	FMatchState State = StMakeState();
	TestTrue(TEXT("Partida en curso: vacío"), MatchResultLine(State, StMakeContext()).Text.IsEmpty());
	State.MatchPhase = EMatchPhase::MatchOver;
	State.Outcome = EMatchOutcome::PlayerVictory;
	State.WinnerSeat = 2;
	const FPanelLine Player = MatchResultLine(State, StMakeContext());
	TestEqual(TEXT("Gana un jugador"), Player.Text, FString(TEXT("GANA EL ASIENTO 2")));
	TestEqual(TEXT("Del color del ganador"), Player.Color, FLinearColor::Blue);
	TestTrue(TEXT("Grande"), Player.bLarge);
	State.Outcome = EMatchOutcome::WardenVictory;
	State.WinnerSeat = INDEX_NONE;
	State.EndReason = EMatchEndReason::TimeOut;
	const FPanelLine TimeOut = MatchResultLine(State, StMakeContext());
	TestEqual(TEXT("Gana el Warden por tiempo"), TimeOut.Text, FString(TEXT("GANA EL WARDEN: SE ACABÓ EL TIEMPO")));
	State.EndReason = EMatchEndReason::NobodyBonded;
	const FPanelLine Nobody = MatchResultLine(State, StMakeContext());
	TestEqual(TEXT("Gana el Warden: nadie se vinculó"), Nobody.Text, FString(TEXT("GANA EL WARDEN: NADIE SE VINCULÓ")));
	TestTrue(TEXT("El Warden, en rojo, en los dos"), TimeOut.Color == Nobody.Color && Nobody.Color.R > Nobody.Color.G && Nobody.Color.R > Nobody.Color.B);
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
	// CA-HUD-5 (R-HUD-3): la letra es PanelFontSize a 1080 de alto, la mitad a 540 y nunca menos de PanelMinFontSize. S-045 (R-HUD-16): el ancho
	// de la columna de la derecha (ScaledPanelWidth) ya no se comprueba: la columna y su ancho desaparecen.
	TestEqual(TEXT("A 1080, el tamaño de base"), Soulbound::Presentation::ScaledPanelFontSize(1080.f, 20, 9), 20);
	TestEqual(TEXT("A 540, la mitad"), Soulbound::Presentation::ScaledPanelFontSize(540.f, 20, 9), 10);
	TestEqual(TEXT("A 2160, el doble"), Soulbound::Presentation::ScaledPanelFontSize(2160.f, 20, 9), 40);
	TestEqual(TEXT("A 270, el mínimo y no 5"), Soulbound::Presentation::ScaledPanelFontSize(270.f, 20, 9), 9);
	TestEqual(TEXT("Con los valores iniciales a 768, el mínimo"), Soulbound::Presentation::ScaledPanelFontSize(768.f, 13, 9), 9);
	TestEqual(TEXT("Con los valores iniciales a 1080, 13"), Soulbound::Presentation::ScaledPanelFontSize(1080.f, 13, 9), 13);
	return true;
}

// ---- S-035: lo que se ve de los demás (la pestaña de la columna se fue con la columna en S-045) ----

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

	// Textos de la leyenda revisados por el autor (13/11/2026): "2: 3 runas", "0 (tú): D5×1 | poderes: Vuelo Planar" y "2 (Brazal revelado): ...".
	TestEqual(TEXT("El ajeno oculto, solo el total"), Hidden, FString(TEXT("2: 3 runas")));

	const FString Own = BracerText(State, StHuman, StHuman);
	TestTrue(TEXT("El propio lleva sus tipos"), Own.Contains(TEXT("D5×1")));
	TestTrue(TEXT("Y sus poderes"), Own.Contains(TEXT("Vuelo Planar")));
	TestTrue(TEXT("El propio, con (tú) y los poderes tras dos puntos"), Own.StartsWith(TEXT("0 (tú): D5×1 | poderes: Vuelo Planar")));

	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	const FString Revealed = BracerText(State, StOther, StHuman);
	TestTrue(TEXT("Revelado: sus tipos"), Revealed.Contains(TEXT("D1×2")) && Revealed.Contains(TEXT("D3×1")));
	TestTrue(TEXT("Revelado: lo dice"), Revealed.StartsWith(TEXT("2 (Brazal revelado): D1×2 D3×1")));
	State.Seats[StOther].RevealedUntilRound = 0;

	// Un Fenrir ajeno (Dominio 1 en elegidos.json) conserva el Paso Libre por afinidad, que es público, y sigue sin decir sus runas.
	State.Seats[3].Role = ESeatRole::Elect;
	State.Seats[3].DomainId = 1;
	State.Seats[3].Runes[5] = 1; // la D5 de Fenrir da Vuelo Planar, no Paso Libre: así la afinidad no se confunde con el poder
	const FString Fenrir = BracerText(State, 3, StHuman);
	TestTrue(TEXT("Paso Libre por afinidad, de un Elegido ajeno"), Fenrir.Contains(TEXT("Paso Libre (afinidad)")));
	TestEqual(TEXT("Con el formato nuevo"), Fenrir, FString(TEXT("3: 1 runa | poderes: Paso Libre (afinidad)")));
	TestFalse(TEXT("Pero no sus runas ni sus poderes por runa"), Fenrir.Contains(TEXT("D5")) || Fenrir.Contains(TEXT("Vuelo Planar")));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusBracerEmptyTest, "Soulbound.Game.Presentation.Estado.BrazalSinRunasNiPoderes", StatusTestFlags)

bool FStatusBracerEmptyTest::RunTest(const FString& Parameters)
{
	// Retoque del autor (13/11/2026): cuando se ven los tipos (el propio o un Brazal revelado), sin runas dice "0 runas" en vez de "-", y sin
	// poderes se omite " | poderes: -". Un Fenrir sin runas conserva "Paso Libre (afinidad)".
	using Soulbound::Presentation::BracerText;
	FMatchState State;
	if (!StMakeRuneState(*this, State))
	{
		return false;
	}
	for (FSeatState& Seat : State.Seats)
	{
		Seat.Runes.Init(0, Seat.Runes.Num());
	}
	State.Seats[StHuman].Role = ESeatRole::LostEcho;
	State.Seats[StHuman].DomainId = INDEX_NONE;
	TestEqual(TEXT("El propio, sin runas ni poderes"), BracerText(State, StHuman, StHuman), FString(TEXT("0 (tú): 0 runas")));

	State.Seats[StOther].RevealedUntilRound = State.Round + 2;
	TestEqual(TEXT("Revelado, sin runas ni poderes"), BracerText(State, StOther, StHuman), FString(TEXT("2 (Brazal revelado): 0 runas")));
	State.Seats[StOther].RevealedUntilRound = 0;

	State.Seats[StHuman].Role = ESeatRole::Elect;
	State.Seats[StHuman].DomainId = 1; // Fenrir
	TestEqual(TEXT("Sin runas pero con la afinidad de Fenrir"), BracerText(State, StHuman, StHuman), FString(TEXT("0 (tú): 0 runas | poderes: Paso Libre (afinidad)")));

	State.Seats[StHuman].Runes[5] = 1;
	const FString WithRune = BracerText(State, StHuman, StHuman);
	TestTrue(TEXT("Con runas y poderes, como antes"), WithRune.StartsWith(TEXT("0 (tú): D5×1 | poderes: Vuelo Planar")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusDomainLegendTest, "Soulbound.Game.Presentation.Estado.LeyendaDominios", StatusTestFlags)

bool FStatusDomainLegendTest::RunTest(const FString& Parameters)
{
	// Textos de la leyenda revisados por el autor (13/11/2026), bloque Dominios: "D0 El Banquete · Shuten-dōji: del asiento 0", "...: libre" y
	// "D1 El Fiordo Encadenado · Fenrir: Olvidado".
	using Soulbound::Presentation::DomainLegendText;
	FMatchState State;
	if (!StMakeRuneState(*this, State))
	{
		return false;
	}
	State.Board.Domains.SetNum(6);
	State.Board.Domains[0].Name = TEXT("El Banquete");
	State.Board.Domains[1].Name = TEXT("El Fiordo Encadenado");
	const FString Banquet = Soulbound::Presentation::DomainTitle(State, 0);
	const FString Fjord = Soulbound::Presentation::DomainTitle(State, 1);
	TestEqual(TEXT("El nombre del Dominio y el de su Elegido"), Banquet, FString(TEXT("El Banquete · Shuten-dōji")));
	TestEqual(TEXT("Sin dueño"), DomainLegendText(State, 0, false), FString(TEXT("D0 El Banquete · Shuten-dōji: libre")));
	State.Board.Domains[0].OwnerSeat = 0;
	TestEqual(TEXT("Con dueño: del asiento"), DomainLegendText(State, 0, false), FString(TEXT("D0 El Banquete · Shuten-dōji: del asiento 0")));
	TestEqual(TEXT("Olvidado"), DomainLegendText(State, 1, true), FString::Printf(TEXT("D1 %s: Olvidado"), *Fjord));
	TestEqual(TEXT("Un Dominio fuera de rango, solo su número"), DomainLegendText(State, 9, false), FString(TEXT("D9")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusSeatLegendTest, "Soulbound.Game.Presentation.Estado.LeyendaJugadores", StatusTestFlags)

bool FStatusSeatLegendTest::RunTest(const FString& Parameters)
{
	// Textos de la leyenda revisados por el autor (13/11/2026), bloque Jugadores: "0 (tú): Elegido, Shuten-dōji | insolencia 0 de 5", "1: Elegido,
	// Tifón" y "2: Eco, con hogar en D2". Sin "(humano)" ni "(dummy)". Lo de detrás (Vacío, Marcado, Brazal revelado) no cambia.
	using Soulbound::Presentation::SeatLegendText;
	FMatchState State;
	if (!StMakeRuneState(*this, State))
	{
		return false;
	}
	State.Seats[0].Role = ESeatRole::Elect;
	State.Seats[0].DomainId = 0;
	State.Seats[0].Insolence = 0;
	State.Seats[1].Role = ESeatRole::Elect;
	State.Seats[1].DomainId = 3;
	State.Seats[StOther].Role = ESeatRole::LostEcho;
	State.Seats[StOther].DomainId = 2;
	const FString First = SeatLegendText(State, 0, 0);
	TestTrue(TEXT("El jugador: (tú), Elegido y su nombre"), First.StartsWith(TEXT("0 (tú): Elegido, Shuten-dōji")));
	TestTrue(TEXT("Y su insolencia, lo único que sigue detrás"), First.Contains(TEXT("insolencia 0 de ")));
	TestEqual(TEXT("Otro Elegido: sin (tú) ni (dummy) ni insolencia"), SeatLegendText(State, 1, 0), FString::Printf(TEXT("1: Elegido, %s"), *Soulbound::Presentation::ElectName(State, 3)));
	TestEqual(TEXT("Un Eco: con hogar en su Dominio, sin nombre"), SeatLegendText(State, StOther, 0), FString(TEXT("2: Eco, con hogar en D2")));
	State.Seats[StOther].DomainId = INDEX_NONE;
	TestEqual(TEXT("Un Eco sin hogar"), SeatLegendText(State, StOther, 0), FString(TEXT("2: Eco")));
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		const FString Text = SeatLegendText(State, Seat, 0);
		TestFalse(TEXT("Nunca (humano) ni (dummy)"), Text.Contains(TEXT("humano")) || Text.Contains(TEXT("dummy")));
	}
	State.Seats[StOther].MarkedUntilRound = State.Round + 2;
	TestTrue(TEXT("Marcado sigue yendo detrás"), SeatLegendText(State, StOther, 0).StartsWith(TEXT("2: Eco")) && SeatLegendText(State, StOther, 0).Contains(TEXT("MARCADO")));
	return true;
}

namespace
{
	/** Una prueba de habilidad pendiente del asiento humano, en la dificultad dada. */
	FMatchState StMakeTrialState(ESkillTrial Trial, ESkillDifficulty Difficulty)
	{
		FMatchState State = StMakeState();
		State.TurnPhase = ETurnPhase::SkillTrial;
		State.PendingTrial = Trial;
		State.PendingDifficulty = Difficulty;
		return State;
	}

	const ESkillDifficulty StDifficulties[] = {ESkillDifficulty::Easy, ESkillDifficulty::Normal, ESkillDifficulty::Hard};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusTrialWithoutLevelTest, "Soulbound.Game.Presentation.Estado.PruebaPendienteSinNivel", StatusTestFlags)

bool FStatusTrialWithoutLevelTest::RunTest(const FString& Parameters)
{
	// CA-HUD-11 (S-036, R-HUD-1): con una prueba pendiente de cada tipo en cada dificultad, ninguna línea del panel de estado dice el nivel y la
	// línea de la prueba sigue nombrando al asiento que juega.
	const ESkillTrial Trials[] = {ESkillTrial::FinalChallenge, ESkillTrial::VoidEscape, ESkillTrial::WardenTrial};
	for (const ESkillTrial Trial : Trials)
	{
		for (const ESkillDifficulty Difficulty : StDifficulties)
		{
			FMatchState State = StMakeTrialState(Trial, Difficulty);
			State.ActiveSeat = 2;
			const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, StMakeContext());
			const FString Case = FString::Printf(TEXT("prueba %d, nivel %d"), static_cast<int32>(Trial), static_cast<int32>(Difficulty));
			for (const TCHAR* Level : {TEXT("fácil"), TEXT("normal"), TEXT("difícil")})
			{
				TestFalse(FString::Printf(TEXT("%s: ninguna línea dice «%s»"), *Case, Level), StAnyContains(Lines, Level));
			}
			TestTrue(FString::Printf(TEXT("%s: la línea de la prueba nombra al asiento 2"), *Case), StAnyContains(Lines, TEXT("el asiento 2 juega")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPendingTrialLevelTest, "Soulbound.Game.Presentation.Estado.DepuracionDiceElNivelDeLaPrueba", StatusTestFlags)

bool FDebugPendingTrialLevelTest::RunTest(const FString& Parameters)
{
	// CA-HUD-13 (S-036, R-HUD-1): el panel de depuración lleva "Prueba pendiente: <uso>, nivel <nivel>" mientras hay una prueba pendiente, y
	// ninguna línea así sin ella.
	{
		const TArray<FPanelLine> Lines = Soulbound::Presentation::DebugLines(StMakeTrialState(ESkillTrial::VoidEscape, ESkillDifficulty::Hard), StMakeContext());
		TestTrue(TEXT("Escape del Vacío en difícil"), StAnyContains(Lines, TEXT("Prueba pendiente: ESCAPE DEL VACÍO, nivel difícil")));
	}
	{
		const TArray<FPanelLine> Lines = Soulbound::Presentation::DebugLines(StMakeTrialState(ESkillTrial::WardenTrial, ESkillDifficulty::Normal), StMakeContext());
		TestTrue(TEXT("Prueba del Warden en normal"), StAnyContains(Lines, TEXT("Prueba pendiente: PRUEBA DEL WARDEN, nivel normal")));
	}
	{
		const TArray<FPanelLine> Lines = Soulbound::Presentation::DebugLines(StMakeState(), StMakeContext());
		TestFalse(TEXT("Sin prueba pendiente, ninguna línea empieza por Prueba pendiente"),
			Lines.ContainsByPredicate([](const FPanelLine& Line) { return Line.Text.StartsWith(TEXT("Prueba pendiente")); }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusEchoNightNoRoundTest, "Soulbound.Game.Presentation.Estado.NocheSinRonda", StatusTestFlags)

bool FStatusEchoNightNoRoundTest::RunTest(const FString& Parameters)
{
	// Corrección del autor del 17/11/2026 (S-043): la ronda tope de la Noche de los Ecos no le interesa al jugador. La línea de la Noche del panel
	// de estado dice "NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio", sin "(como mucho hasta la ronda N)".
	FMatchState State = StMakeState();
	State.MatchPhase = EMatchPhase::RuneHunt;
	State.Round = 3;
	State.EchoNightUntilRound = 9;
	const TArray<FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, StMakeContext());
	const FPanelLine* Night = Lines.FindByPredicate([](const FPanelLine& Line) { return Line.Text.StartsWith(TEXT("NOCHE DE LOS ECOS")); });
	if (TestNotNull(TEXT("Con la Noche activa, su línea"), Night))
	{
		TestEqual(TEXT("Sin la ronda"), Night->Text, FString(TEXT("NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio")));
	}
	TestFalse(TEXT("Ninguna línea dice la ronda tope"), StAnyContains(Lines, TEXT("hasta la ronda")));
	return true;
}

// ---- S-045: la caja de la partida y el panel de estado de depuración ----

namespace
{
	/** El color del Warden de las pruebas de la caja de la partida: uno que no es blanco ni el de ningún asiento. */
	const FLinearColor StWardenColor(0.7f, 0.2f, 0.9f);
	constexpr int32 StAltarTile = 0;
	constexpr int32 StPlainTile = 1;

	/**
	 * Tras la carrera, con Warden y el fin de la partida a 2400 s, en el segundo 395 (quedan 2005 s): un tablero de dos casillas (0, el Altar; 1,
	 * normal), los cuatro asientos en la normal y sin runas; sin Noche de los Ecos.
	 */
	FMatchState StMakeMatchPanelState()
	{
		FMatchState State = StMakeState();
		State.MatchPhase = EMatchPhase::RuneHunt;
		State.ClockSeconds = 395.0;
		State.WardenParams.Acts.SetNum(1);
		State.WardenParams.MatchEndSeconds = 2400.0;
		State.Board.Tiles.SetNum(2);
		State.Board.Tiles[StAltarTile].TileId = StAltarTile;
		State.Board.Tiles[StAltarTile].Type = ETileType::Altar;
		State.Board.Tiles[StPlainTile].TileId = StPlainTile;
		for (FSeatState& Seat : State.Seats)
		{
			Seat.TileId = StPlainTile;
		}
		return State;
	}

	/** El asiento Seat, aspirante: en el Altar con los seis tipos de runa (IsAspirant). */
	void StMakeAspirant(FMatchState& State, int32 Seat)
	{
		State.Seats[Seat].TileId = StAltarTile;
		for (int32& Count : State.Seats[Seat].Runes)
		{
			Count = 1;
		}
	}

	FStatusContext StMakeMatchPanelContext()
	{
		FStatusContext Context = StMakeContext();
		Context.WardenLineColor = StWardenColor;
		return Context;
	}

	/** Las líneas de aspirante de la caja, en su orden. */
	TArray<FString> StAspirantLines(const TArray<FPanelLine>& Lines)
	{
		TArray<FString> Out;
		for (const FPanelLine& Line : Lines)
		{
			if (Line.Text.Contains(TEXT("a un turno de ganar")))
			{
				Out.Add(Line.Text);
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchPanelClockTest, "Soulbound.Game.Presentation.Estado.CajaDeLaPartidaReloj", StatusTestFlags)

bool FMatchPanelClockTest::RunTest(const FString& Parameters)
{
	// CA-HUD-25 (R-HUD-14, reloj): "Carrera 4:11" con 251 s de carrera por delante y "Partida 33:25" con 2005 s de partida por delante y la carrera
	// terminada; los segundos, siempre con dos cifras ("Carrera 0:07"); ninguna línea en el prólogo ni con la partida terminada. Precisiones del autor
	// del 17/11/2026: hacia arriba, "Carrera 0:00" con la carrera agotada y el turno sin acabar, y sin Warden no hay "Partida".
	using Soulbound::Presentation::MatchCountdownText;
	using Soulbound::Presentation::MatchPanelLines;
	FMatchState Race = StMakeMatchPanelState();
	Race.MatchPhase = EMatchPhase::BondRace;
	Race.Params.RaceDurationSeconds = 300.0;
	Race.ClockSeconds = 49.0;
	TestEqual(TEXT("251 s de carrera"), MatchCountdownText(Race), FString(TEXT("Carrera 4:11")));
	{
		const TArray<FPanelLine> Lines = MatchPanelLines(Race, StMakeMatchPanelContext());
		if (TestTrue(TEXT("La caja lleva el reloj"), Lines.Num() >= 1))
		{
			TestEqual(TEXT("La primera línea es el reloj"), Lines[0].Text, FString(TEXT("Carrera 4:11")));
			TestEqual(TEXT("En blanco"), Lines[0].Color, FLinearColor::White);
			TestTrue(TEXT("Con la letra grande"), Lines[0].bLarge);
		}
	}
	Race.ClockSeconds = 293.0;
	TestEqual(TEXT("Los segundos con dos cifras"), MatchCountdownText(Race), FString(TEXT("Carrera 0:07")));
	Race.ClockSeconds = 49.6;
	TestEqual(TEXT("250,4 s se redondean hacia arriba"), MatchCountdownText(Race), FString(TEXT("Carrera 4:11")));
	Race.ClockSeconds = 310.0;
	TestEqual(TEXT("Carrera agotada, el turno sin acabar: 0:00"), MatchCountdownText(Race), FString(TEXT("Carrera 0:00")));

	const FMatchState Match = StMakeMatchPanelState();
	TestEqual(TEXT("2005 s de partida tras la carrera"), MatchCountdownText(Match), FString(TEXT("Partida 33:25")));
	{
		const TArray<FPanelLine> Lines = MatchPanelLines(Match, StMakeMatchPanelContext());
		TestTrue(TEXT("La caja lleva el reloj de partida"), Lines.Num() >= 1 && Lines[0].Text == TEXT("Partida 33:25"));
	}

	FMatchState NoWarden = StMakeMatchPanelState();
	NoWarden.WardenParams.Acts.Reset();
	TestEqual(TEXT("Sin Warden, tras la carrera no hay reloj"), MatchCountdownText(NoWarden), FString());
	TestFalse(TEXT("Ni línea de Partida en la caja"), StAnyContains(MatchPanelLines(NoWarden, StMakeMatchPanelContext()), TEXT("Partida")));

	FStatusContext Prologue = StMakeMatchPanelContext();
	Prologue.bPrologue = true;
	TestEqual(TEXT("En el prólogo, ninguna línea"), MatchPanelLines(Race, Prologue).Num(), 0);

	FMatchState Over = StMakeMatchPanelState();
	Over.MatchPhase = EMatchPhase::MatchOver;
	TestEqual(TEXT("Con la partida terminada, sin reloj"), MatchCountdownText(Over), FString());
	TestEqual(TEXT("Ni ninguna línea en la caja"), MatchPanelLines(Over, StMakeMatchPanelContext()).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchPanelNoticesTest, "Soulbound.Game.Presentation.Estado.CajaDeLaPartidaAvisos", StatusTestFlags)

bool FMatchPanelNoticesTest::RunTest(const FString& Parameters)
{
	// CA-HUD-26 (R-HUD-14, avisos de mesa): con la Noche de los Ecos, su línea con el color del Warden; sin Noche, no. Con el asiento 3 de aspirante y
	// el humano en el 0, "El asiento 3 está a un turno de ganar"; si el aspirante es el humano, ninguna línea de aspirante; con dos aspirantes ajenos,
	// dos líneas en orden de asiento. El reloj, siempre primero. Precisión del autor del 17/11/2026: los avisos, también con la letra grande.
	using Soulbound::Presentation::MatchPanelLines;
	const FStatusContext Context = StMakeMatchPanelContext();
	const FString NightText = TEXT("Noche de los Ecos: los Ecos entran en cualquier Dominio");
	{
		const TArray<FPanelLine> Lines = MatchPanelLines(StMakeMatchPanelState(), Context);
		TestEqual(TEXT("Sin Noche ni aspirantes, solo el reloj"), Lines.Num(), 1);
		TestFalse(TEXT("Sin Noche, sin su línea"), StAnyContains(Lines, TEXT("Noche de los Ecos")));
	}
	{
		FMatchState Night = StMakeMatchPanelState();
		Night.Round = 3;
		Night.EchoNightUntilRound = 9;
		const TArray<FPanelLine> Lines = MatchPanelLines(Night, Context);
		if (TestEqual(TEXT("Con la Noche, el reloj y su línea"), Lines.Num(), 2))
		{
			TestTrue(TEXT("El reloj primero"), Lines[0].Text.StartsWith(TEXT("Partida")));
			TestEqual(TEXT("La línea de la Noche"), Lines[1].Text, NightText);
			TestEqual(TEXT("Con el color del Warden"), Lines[1].Color, StWardenColor);
			TestTrue(TEXT("Y grande"), Lines[1].bLarge);
		}
	}
	{
		FMatchState Other = StMakeMatchPanelState();
		StMakeAspirant(Other, 3);
		const TArray<FPanelLine> Lines = MatchPanelLines(Other, Context);
		TestEqual(TEXT("El asiento 3, aspirante"), StAspirantLines(Lines), TArray<FString>{TEXT("El asiento 3 está a un turno de ganar")});
		if (TestEqual(TEXT("El reloj y una línea"), Lines.Num(), 2))
		{
			TestTrue(TEXT("El reloj primero"), Lines[0].Text.StartsWith(TEXT("Partida")));
			TestEqual(TEXT("La del aspirante, con el color del Warden"), Lines[1].Color, StWardenColor);
			TestTrue(TEXT("Y grande"), Lines[1].bLarge);
		}
	}
	{
		FMatchState Own = StMakeMatchPanelState();
		StMakeAspirant(Own, StHuman);
		TestEqual(TEXT("El humano aspirante: ninguna línea de aspirante"), StAspirantLines(MatchPanelLines(Own, Context)).Num(), 0);
	}
	{
		FMatchState Two = StMakeMatchPanelState();
		Two.Round = 3;
		Two.EchoNightUntilRound = 9;
		StMakeAspirant(Two, 3);
		StMakeAspirant(Two, 1);
		StMakeAspirant(Two, StHuman);
		const TArray<FPanelLine> Lines = MatchPanelLines(Two, Context);
		TestEqual(TEXT("Dos aspirantes ajenos, en orden de asiento"), StAspirantLines(Lines),
			TArray<FString>{TEXT("El asiento 1 está a un turno de ganar"), TEXT("El asiento 3 está a un turno de ganar")});
		TestTrue(TEXT("Con todo, el reloj sigue primero"), Lines.Num() == 4 && Lines[0].Text.StartsWith(TEXT("Partida")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudShownTest, "Soulbound.Game.Presentation.Estado.PanelDeEstadoOcultoYSuTecla", StatusTestFlags)

bool FHudShownTest::RunTest(const FString& Parameters)
{
	// CA-HUD-27 (R-HUD-15): al empezar la partida, el panel de estado oculto; con su tecla una vez, visible; otra vez, oculto. La caja de la partida, la
	// del objetivo y el botón del Grimorio no dependen de esa tecla. R-HUD-14: la caja de la partida, ni en el prólogo ni con la partida terminada, y
	// sigue con una frase del Warden en pantalla.
	using Soulbound::Presentation::FHudShown;
	using Soulbound::Presentation::FHudToggles;
	using Soulbound::Presentation::HudShown;
	FHudToggles Toggles;
	const FHudShown Start = HudShown(Toggles, false, false, ESeatGoal::Runes, false);
	TestFalse(TEXT("Al empezar, el panel de estado oculto"), Start.bStatusPanel);
	TestFalse(TEXT("Y el de depuración también"), Start.bDebugPanel);
	TestTrue(TEXT("La caja de la partida se ve"), Start.bMatchPanel);
	TestTrue(TEXT("La del objetivo también"), Start.bGoalPanel);
	TestTrue(TEXT("Y el botón del Grimorio"), Start.bGrimoireButton);

	Toggles.ToggleStatusPanel();
	const FHudShown Once = HudShown(Toggles, false, false, ESeatGoal::Runes, false);
	TestTrue(TEXT("Con la tecla una vez, visible"), Once.bStatusPanel);
	TestTrue(TEXT("Las cajas y el botón no cambian"), Once.bMatchPanel == Start.bMatchPanel && Once.bGoalPanel == Start.bGoalPanel && Once.bGrimoireButton == Start.bGrimoireButton);
	TestFalse(TEXT("Ni el panel de depuración"), Once.bDebugPanel);

	Toggles.ToggleStatusPanel();
	const FHudShown Twice = HudShown(Toggles, false, false, ESeatGoal::Runes, false);
	TestFalse(TEXT("Otra vez, oculto"), Twice.bStatusPanel);
	TestTrue(TEXT("Y lo demás, como al empezar"), Twice == Start);

	Toggles.ToggleDebugPanel();
	TestTrue(TEXT("La tecla de depuración enseña el suyo"), HudShown(Toggles, false, false, ESeatGoal::Runes, false).bDebugPanel);

	// Las tres piezas siguen sus reglas, con el panel de estado a la vista o no.
	for (const bool bStatus : {false, true})
	{
		FHudToggles Status;
		if (bStatus)
		{
			Status.ToggleStatusPanel();
		}
		const FHudShown Prologue = HudShown(Status, true, false, ESeatGoal::Niche, false);
		TestFalse(TEXT("En el prólogo, sin caja de la partida"), Prologue.bMatchPanel);
		TestFalse(TEXT("Ni caja del objetivo"), Prologue.bGoalPanel);
		TestFalse(TEXT("Ni botón"), Prologue.bGrimoireButton);
		const FHudShown Over = HudShown(Status, false, true, ESeatGoal::None, false);
		TestFalse(TEXT("Con la partida terminada, sin caja de la partida"), Over.bMatchPanel);
		TestFalse(TEXT("Ni botón"), Over.bGrimoireButton);
		const FHudShown Speaking = HudShown(Status, false, false, ESeatGoal::Runes, true);
		TestTrue(TEXT("Con frase del Warden, la caja de la partida sigue"), Speaking.bMatchPanel);
		TestFalse(TEXT("La del objetivo, no"), Speaking.bGoalPanel);
		TestFalse(TEXT("Ni el botón"), Speaking.bGrimoireButton);
		TestTrue(TEXT("El panel de estado, solo por su tecla"), Speaking.bStatusPanel == bStatus);
	}
	TestFalse(TEXT("IsMatchPanelShown: no en el prólogo"), Soulbound::Presentation::IsMatchPanelShown(true, false));
	TestFalse(TEXT("IsMatchPanelShown: no con la partida terminada"), Soulbound::Presentation::IsMatchPanelShown(false, true));
	TestTrue(TEXT("IsMatchPanelShown: en partida"), Soulbound::Presentation::IsMatchPanelShown(false, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
