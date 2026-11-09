// Tests de los paneles de estado y de depuración del HUD (S-027, R-HUD-1, R-HUD-2, R-HUD-3, R-HUD-5): qué líneas van a cada panel, en qué
// orden, y la escala de la letra con la altura de la pantalla. Funciones puras sobre un FMatchState armado a mano: sin mundo, sin actor y sin
// disco, como BoardLayoutTest. Filtro: "Soulbound.Game.Presentation.Estado".

#include "../../Presentation/StatusText.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

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

#endif
