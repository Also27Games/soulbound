// Tests de S-039 (se entiende cómo moverse): el latido de los halos, la tirada en dados, el aviso de casilla inalcanzable, la línea de la tirada
// del panel de estado y el motivo en el acta. Funciones puras, sin mundo, sin actor y sin disco: lo que se ve en pantalla (el pulso del halo, la
// cara del dado pintada, el aviso en el panel) solo tiene criterio manual. Filtros: "Soulbound.Game.Presentation.Decision",
// "Soulbound.Game.Presentation.Ficha", "Soulbound.Game.Presentation.Estado" y "Soulbound.Game.Presentation.Acta".

#include "../../Presentation/DecisionText.h"
#include "../../Presentation/DiceFace.h"
#include "../../Presentation/MatchRecord.h"
#include "../../Presentation/StatusText.h"
#include "../../Presentation/TokenLook.h"
#include "../../Presentation/UnreachableNotice.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags HintsTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::EDecisionInput;
	using Soulbound::Presentation::FDecisionView;

	constexpr int32 MhHuman = 0;
	constexpr double MhNow = 100.0;

	/** Cuatro asientos; el humano en su turno de tirada con Roll y los dados dados; 15 s de reloj de turno. */
	FMatchState MhHumanRoll(const TArray<int32>& Dice)
	{
		FMatchState State;
		State.Seats.SetNum(4);
		State.MatchPhase = EMatchPhase::RuneHunt;
		State.ActiveSeat = MhHuman;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.ClockSeconds = MhNow;
		State.TurnDeadlineSeconds = MhNow + 15.0;
		State.ReachableOptions = {1, 2};
		State.DiceCount = FMath::Max(Dice.Num(), 1);
		State.DiceValues = Dice;
		State.Roll = 0;
		for (const int32 Value : Dice)
		{
			State.Roll += Value;
		}
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReachablePulseTest, "Soulbound.Game.Presentation.Ficha.LatidoDelHalo", HintsTestFlags)

bool FReachablePulseTest::RunTest(const FString& Parameters)
{
	// CA-VIS-27 (R-VIS-26): el color del halo es ReachableColor por la intensidad del pulso: el mínimo al empezar el periodo, el color entero a
	// mitad de periodo y otra vez el mínimo al acabarlo; con periodo cero o negativo, el color entero, fijo. El alfa no cambia.
	using Soulbound::Presentation::ReachableHaloColor;
	const FLinearColor Base(0.8f, 0.6f, 0.4f, 0.5f);
	const float Period = 1.2f;
	const float Min = 0.35f;
	auto Matches = [this](const TCHAR* What, const FLinearColor& Got, float Scale, float Alpha)
	{
		const FLinearColor Expected(0.8f * Scale, 0.6f * Scale, 0.4f * Scale, Alpha);
		TestTrue(FString::Printf(TEXT("%s: (%.3f, %.3f, %.3f, %.3f)"), What, Got.R, Got.G, Got.B, Got.A),
			FMath::IsNearlyEqual(Got.R, Expected.R, 0.001f) && FMath::IsNearlyEqual(Got.G, Expected.G, 0.001f)
			&& FMath::IsNearlyEqual(Got.B, Expected.B, 0.001f) && FMath::IsNearlyEqual(Got.A, Expected.A, 0.001f));
	};
	Matches(TEXT("Al empezar el periodo, el mínimo"), ReachableHaloColor(Base, 0.0, Period, Min), Min, 0.5f);
	Matches(TEXT("A mitad de periodo, el color entero"), ReachableHaloColor(Base, 0.6, Period, Min), 1.f, 0.5f);
	Matches(TEXT("Al acabar el periodo, el mínimo"), ReachableHaloColor(Base, 1.2, Period, Min), Min, 0.5f);
	Matches(TEXT("Sin periodo, el color entero"), ReachableHaloColor(Base, 0.3, 0.f, Min), 1.f, 0.5f);
	Matches(TEXT("Con periodo negativo, el color entero"), ReachableHaloColor(Base, 0.3, -2.f, Min), 1.f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionDiceHumanTest, "Soulbound.Game.Presentation.Decision.TiradaEnDadosDelHumano", HintsTestFlags)

bool FDecisionDiceHumanTest::RunTest(const FString& Parameters)
{
	// CA-DEC-27 (R-DEC-15, humano): con un dado que ha sacado 4, la vista lleva el título "Tu turno", el contexto "Has sacado", un dado de valor
	// 4 y la instrucción "Elige una casilla que late."; con dos dados, 3 y 5, los dos en ese orden; con la repetición ofrecida, la instrucción es
	// "Elige una casilla que late o repite la tirada.".
	{
		const FMatchState State = MhHumanRoll({4});
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, MhHuman, false);
		if (TestTrue(TEXT("Hay vista"), View.IsSet()))
		{
			TestTrue(TEXT("Tipo casilla"), View->Input == EDecisionInput::Tile);
			TestEqual(TEXT("Título"), View->Title, FString(TEXT("Tu turno")));
			TestEqual(TEXT("Contexto"), View->Context, FString(TEXT("Has sacado")));
			TestTrue(TEXT("Un dado, de valor 4"), View->DiceValues == TArray<int32>{4});
			TestEqual(TEXT("Instrucción"), View->Instruction, FString(TEXT("Elige una casilla que late.")));
		}
	}
	{
		const FMatchState State = MhHumanRoll({3, 5});
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, MhHuman, false);
		if (TestTrue(TEXT("Hay vista con dos dados"), View.IsSet()))
		{
			TestTrue(TEXT("Dos dados, 3 y 5, en ese orden"), View->DiceValues == TArray<int32>{3, 5});
			TestEqual(TEXT("Contexto"), View->Context, FString(TEXT("Has sacado")));
		}
	}
	{
		FMatchState State = MhHumanRoll({4});
		State.PendingChoice.Kind = EChoiceKind::Reroll;
		State.PendingChoice.Seat = MhHuman;
		State.PendingChoice.Options = {1};
		State.PendingChoice.DeadlineSeconds = MhNow + 15.0;
		const TOptional<FDecisionView> View = Soulbound::Presentation::DecisionView(State, MhHuman, false);
		if (TestTrue(TEXT("Hay vista con la repetición"), View.IsSet()))
		{
			TestEqual(TEXT("Instrucción con la repetición"), View->Instruction, FString(TEXT("Elige una casilla que late o repite la tirada.")));
			TestEqual(TEXT("Título"), View->Title, FString(TEXT("Tu turno")));
			TestTrue(TEXT("Los dados siguen"), View->DiceValues == TArray<int32>{4});
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionDiceWaitTest, "Soulbound.Game.Presentation.Decision.TiradaEnDadosDeLaEspera", HintsTestFlags)

bool FDecisionDiceWaitTest::RunTest(const FString& Parameters)
{
	// CA-DEC-28 (R-DEC-15, espera): en el turno de tirada de otro asiento, la vista de espera lleva "Ha sacado" y los dados de ese asiento.
	FMatchState State = MhHumanRoll({2, 6});
	State.ActiveSeat = 1;
	const TOptional<FDecisionView> View = Soulbound::Presentation::WaitView(State, MhHuman);
	if (TestTrue(TEXT("Hay vista"), View.IsSet()))
	{
		TestTrue(TEXT("De tipo espera"), View->Input == EDecisionInput::Wait);
		TestEqual(TEXT("Nombra el turno del 1"), View->Title, FString(TEXT("Turno de Asiento 1 (Eco)")));
		TestEqual(TEXT("Contexto"), View->Context, FString(TEXT("Ha sacado")));
		TestTrue(TEXT("Los dados de ese asiento"), View->DiceValues == TArray<int32>{2, 6});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionDiceForcedTest, "Soulbound.Game.Presentation.Decision.TiradaFijadaSinDados", HintsTestFlags)

bool FDecisionDiceForcedTest::RunTest(const FString& Parameters)
{
	// CA-DEC-29 (R-DEC-15, depuración): con la tirada fijada en 9 y sin valores de dados, la vista no lleva dados y el contexto es "Has sacado un 9"
	// (en la espera, "Ha sacado un 9").
	FMatchState State = MhHumanRoll({});
	State.Roll = 9;
	const TOptional<FDecisionView> Own = Soulbound::Presentation::DecisionView(State, MhHuman, false);
	if (TestTrue(TEXT("Hay vista"), Own.IsSet()))
	{
		TestTrue(TEXT("Sin dados"), Own->DiceValues.IsEmpty());
		TestEqual(TEXT("Contexto"), Own->Context, FString(TEXT("Has sacado un 9")));
		TestEqual(TEXT("Título"), Own->Title, FString(TEXT("Tu turno")));
	}
	State.ActiveSeat = 1;
	const TOptional<FDecisionView> Other = Soulbound::Presentation::WaitView(State, MhHuman);
	if (TestTrue(TEXT("Hay vista de espera"), Other.IsSet()))
	{
		TestTrue(TEXT("Sin dados"), Other->DiceValues.IsEmpty());
		TestEqual(TEXT("Contexto"), Other->Context, FString(TEXT("Ha sacado un 9")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceFaceTest, "Soulbound.Game.Presentation.Decision.CaraDelDado", HintsTestFlags)

bool FDiceFaceTest::RunTest(const FString& Parameters)
{
	// CA-DEC-30 (R-DEC-15, la cara): los puntos de cada valor, del 1 al 6, son tantos como el valor, todos dentro de la cara y sin repetir
	// posición; para un valor fuera de 1 a 6, ninguno. El trozo de la imagen de la cara N es el sexto N del ancho y todo el alto.
	using Soulbound::Presentation::DiceFacePips;
	using Soulbound::Presentation::DiceFaceUV;
	for (int32 Value = 1; Value <= Soulbound::Dice::Faces; ++Value)
	{
		const TArray<FVector2f> Pips = DiceFacePips(Value);
		TestEqual(FString::Printf(TEXT("La cara %d lleva %d puntos"), Value, Value), Pips.Num(), Value);
		bool bInside = true;
		bool bDistinct = true;
		for (int32 Index = 0; Index < Pips.Num(); ++Index)
		{
			bInside &= Pips[Index].X > 0.f && Pips[Index].X < 1.f && Pips[Index].Y > 0.f && Pips[Index].Y < 1.f;
			for (int32 Other = Index + 1; Other < Pips.Num(); ++Other)
			{
				bDistinct &= !Pips[Index].Equals(Pips[Other], 0.001f);
			}
		}
		TestTrue(FString::Printf(TEXT("La cara %d: todos dentro de la cara"), Value), bInside);
		TestTrue(FString::Printf(TEXT("La cara %d: sin repetir posición"), Value), bDistinct);

		// Corrección del autor del 15/11/2026: los puntos, más pequeños y más separados (se leía un damero). Por proporciones de la cara: el punto no pasa
		// de 0,16 del lado, entre dos puntos cabe al menos un diámetro libre, y ninguno toca el borde (queda al menos un diámetro entre el punto y el borde).
		const float Diameter = Soulbound::Presentation::DicePipDiameterRatio;
		TestTrue(FString::Printf(TEXT("La cara %d: el punto no pasa de 0,16 del lado (%.3f)"), Value, Diameter), Diameter <= 0.16f);
		bool bSeparated = true;
		bool bFromEdge = true;
		for (int32 Index = 0; Index < Pips.Num(); ++Index)
		{
			bFromEdge &= Pips[Index].X >= Diameter * 1.5f && Pips[Index].X <= 1.f - Diameter * 1.5f && Pips[Index].Y >= Diameter * 1.5f && Pips[Index].Y <= 1.f - Diameter * 1.5f;
			for (int32 Other = Index + 1; Other < Pips.Num(); ++Other)
			{
				bSeparated &= FVector2f::Distance(Pips[Index], Pips[Other]) >= Diameter * 2.f - 0.0001f;
			}
		}
		TestTrue(FString::Printf(TEXT("La cara %d: un diámetro libre entre dos puntos"), Value), bSeparated);
		TestTrue(FString::Printf(TEXT("La cara %d: los puntos no se pegan al borde"), Value), bFromEdge);

		const FBox2f UV = DiceFaceUV(Value);
		TestTrue(FString::Printf(TEXT("La cara %d: su sexto del ancho"), Value),
			FMath::IsNearlyEqual(UV.Min.X, (Value - 1) / 6.f, 0.0001f) && FMath::IsNearlyEqual(UV.Max.X, Value / 6.f, 0.0001f));
		TestTrue(FString::Printf(TEXT("La cara %d: todo el alto"), Value), FMath::IsNearlyEqual(UV.Min.Y, 0.f) && FMath::IsNearlyEqual(UV.Max.Y, 1.f));
	}
	TestEqual(TEXT("La cara 1 empieza en 0"), DiceFaceUV(1).Min.X, 0.f);
	TestEqual(TEXT("La cara 6 acaba en 1"), DiceFaceUV(6).Max.X, 1.f);
	for (const int32 Bad : {-1, 0, 7, 100})
	{
		TestEqual(FString::Printf(TEXT("Valor %d: ningún punto"), Bad), DiceFacePips(Bad).Num(), 0);
		TestFalse(FString::Printf(TEXT("Valor %d: ningún trozo"), Bad), DiceFaceUV(Bad).bIsValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnreachableNoticeTextTest, "Soulbound.Game.Presentation.Decision.TextoDelAviso", HintsTestFlags)

bool FUnreachableNoticeTextTest::RunTest(const FString& Parameters)
{
	// CA-DEC-31 (R-DEC-16, textos): los tres textos del autor, letra por letra, y la tirada del turno en el de Roll.
	using Soulbound::Presentation::UnreachableNoticeText;
	TestEqual(TEXT("Sealed"), UnreachableNoticeText(EUnreachableReason::Sealed, 3), FString(TEXT("Esa casilla la he sellado yo. Busca otro camino.")));
	TestEqual(TEXT("Confined"), UnreachableNoticeText(EUnreachableReason::Confined, 3), FString(TEXT("Los Ecos no pisan el Dominio de un Elegido. Por ahora.")));
	TestEqual(TEXT("Roll con 3"), UnreachableNoticeText(EUnreachableReason::Roll, 3), FString(TEXT("Con un 3 no llegas ahí. Qué lástima.")));
	TestEqual(TEXT("Roll con 7"), UnreachableNoticeText(EUnreachableReason::Roll, 7), FString(TEXT("Con un 7 no llegas ahí. Qué lástima.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnreachableNoticeLifeTest, "Soulbound.Game.Presentation.Decision.VidaDelAviso", HintsTestFlags)

bool FUnreachableNoticeLifeTest::RunTest(const FString& Parameters)
{
	// CA-DEC-32 (R-DEC-16, vida del aviso; corrección del autor del 15/11/2026): se ve desde el clic rechazado, en el punto del clic, hasta que pasan
	// sus segundos; un segundo rechazo lo sustituye, lo lleva al punto nuevo y vuelve a contar, aunque sea el mismo texto; un clic aceptado o el final
	// del turno de tirada (Dismiss) lo quitan. Desde S-040 (corrección tras PIE de un defecto de S-039) el punto es el absoluto del clic, sin convertir:
	// el aviso lo guarda tal como se le da, y la vista lo pasa a su capa al colocarlo.
	using Soulbound::Presentation::FUnreachableNotice;
	const double Seconds = 4.0;
	const FVector2f PointA(120.f, 340.f);
	const FVector2f PointB(600.f, 80.f);
	FUnreachableNotice Notice;
	TestEqual(TEXT("Antes de ningún rechazo, nada"), Notice.Current(10.0, Seconds), FString());

	Notice.Show(TEXT("A"), PointA, 10.0);
	TestEqual(TEXT("Se ve desde el clic"), Notice.Current(10.0, Seconds), FString(TEXT("A")));
	TestEqual(TEXT("... en el punto del clic"), Notice.Position(), PointA);
	TestEqual(TEXT("... y mientras dura"), Notice.Current(13.9, Seconds), FString(TEXT("A")));
	TestEqual(TEXT("... y se queda quieto"), Notice.Position(), PointA);
	TestEqual(TEXT("... y se quita cuando pasan sus segundos"), Notice.Current(14.0, Seconds), FString());
	TestEqual(TEXT("... y sigue quitado"), Notice.Current(20.0, Seconds), FString());

	Notice.Show(TEXT("A"), PointA, 20.0);
	Notice.Show(TEXT("A"), PointB, 22.0);
	TestEqual(TEXT("Un segundo rechazo, con el mismo texto, vuelve a contar"), Notice.Current(25.9, Seconds), FString(TEXT("A")));
	TestEqual(TEXT("... lo lleva al punto nuevo"), Notice.Position(), PointB);
	TestEqual(TEXT("... desde el segundo"), Notice.Current(26.0, Seconds), FString());

	Notice.Show(TEXT("B"), PointA, 30.0);
	Notice.Show(TEXT("C"), PointB, 31.0);
	TestEqual(TEXT("Un rechazo con otro texto sustituye al anterior"), Notice.Current(31.5, Seconds), FString(TEXT("C")));
	TestEqual(TEXT("... y se pone en su punto"), Notice.Position(), PointB);

	Notice.Dismiss();
	TestEqual(TEXT("Un clic aceptado o el final del turno lo quitan"), Notice.Current(31.5, Seconds), FString());
	TestEqual(TEXT("Con duración cero no se ve"), FUnreachableNotice().Current(0.0, 0.0), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoticePlacementTest, "Soulbound.Game.Presentation.Decision.ColocacionDelAviso", HintsTestFlags)

bool FNoticePlacementTest::RunTest(const FString& Parameters)
{
	// R-DEC-16 (corrección del autor del 15/11/2026): el aviso sale junto al punto del clic y se recoloca para verse entero: dentro del viewport y
	// sin quedar debajo de la columna de paneles ni del panel de estado. Función pura de la posición, el tamaño, el viewport y las zonas a evitar.
	using Soulbound::Presentation::PlaceNotice;
	const FBox2f Bounds(FVector2f(0.f, 0.f), FVector2f(1000.f, 700.f));
	const FBox2f Column(FVector2f(800.f, 0.f), FVector2f(1000.f, 700.f));
	const FBox2f Status(FVector2f(0.f, 0.f), FVector2f(350.f, 200.f));
	const TArray<FBox2f> Avoid = {Column, Status};
	const FVector2f Size(300.f, 40.f);
	const float Gap = 12.f;
	auto Overlaps = [](const FBox2f& A, const FBox2f& B)
	{
		return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y;
	};

	// Con sitio de sobra: centrado encima del clic, a Gap de distancia.
	TestEqual(TEXT("Centrado encima del clic"), PlaceNotice(FVector2f(500.f, 400.f), Size, Bounds, Avoid, Gap), FVector2f(350.f, 348.f));
	// Sin sitio encima: debajo.
	TestEqual(TEXT("Sin sitio arriba, debajo"), PlaceNotice(FVector2f(500.f, 20.f), Size, Bounds, Avoid, Gap), FVector2f(350.f, 32.f));
	// Pegado al borde izquierdo: recortado al viewport (a una altura que no toca el panel de estado).
	TestEqual(TEXT("Recortado al borde izquierdo"), PlaceNotice(FVector2f(30.f, 400.f), Size, Bounds, Avoid, Gap), FVector2f(0.f, 348.f));
	// Contra la columna: no la toca.
	{
		const FVector2f At = PlaceNotice(FVector2f(790.f, 400.f), Size, Bounds, Avoid, Gap);
		TestFalse(TEXT("Junto a la columna no se mete debajo"), Overlaps(FBox2f(At, At + Size), Column));
		TestTrue(TEXT("... y se ve entero"), At.X >= 0.f && At.Y >= 0.f && At.X + Size.X <= 1000.f && At.Y + Size.Y <= 700.f);
	}

	// Propiedad, en una rejilla de clics por todo el viewport: siempre dentro, y nunca sobre la columna ni el panel de estado.
	int32 Checked = 0;
	int32 Bad = 0;
	for (float X = 0.f; X <= 1000.f; X += 50.f)
	{
		for (float Y = 0.f; Y <= 700.f; Y += 50.f)
		{
			const FVector2f At = PlaceNotice(FVector2f(X, Y), Size, Bounds, Avoid, Gap);
			const FBox2f Box(At, At + Size);
			const bool bInside = Box.Min.X >= 0.f && Box.Min.Y >= 0.f && Box.Max.X <= 1000.f && Box.Max.Y <= 700.f;
			if (!bInside || Overlaps(Box, Column) || Overlaps(Box, Status))
			{
				++Bad;
			}
			++Checked;
		}
	}
	TestEqual(FString::Printf(TEXT("De %d clics en rejilla, ninguno deja el aviso fuera o tapando una zona"), Checked), Bad, 0);

	// Mayor que el viewport: la esquina del viewport, sin romper nada.
	TestEqual(TEXT("Más grande que el viewport: su esquina"), PlaceNotice(FVector2f(500.f, 300.f), FVector2f(2000.f, 40.f), Bounds, TArray<FBox2f>(), Gap).X, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoticeZonesTest, "Soulbound.Game.Presentation.Decision.ZonasQueEsquivaElAviso", HintsTestFlags)

bool FNoticeZonesTest::RunTest(const FString& Parameters)
{
	// CA-HUD-29 (S-045, lo que se recoloca): el aviso de casilla inalcanzable esquiva la caja de la partida, la caja del objetivo, el panel de decisión y
	// el botón del Grimorio, cada uno mientras se ve; el panel de estado no, salvo que su tecla lo enseñe. La columna no está nunca: no hay zona de
	// columna, y el área en que se coloca es la capa entera (NoticeBounds), sin recortar a la derecha (R-HUD-16).
	using Soulbound::Presentation::EHudZone;
	using Soulbound::Presentation::FHudShown;
	using Soulbound::Presentation::NoticeAvoidZones;
	FHudShown Shown;
	Shown.bMatchPanel = true;
	Shown.bGoalPanel = true;
	Shown.bGrimoireButton = true;
	const TArray<EHudZone> Normal = NoticeAvoidZones(Shown, true);
	TestTrue(TEXT("Con el panel de estado oculto: las cuatro de la regla, en su orden"),
		Normal == TArray<EHudZone>{EHudZone::MatchPanel, EHudZone::GoalPanel, EHudZone::DecisionPanel, EHudZone::GrimoireButton});
	TestFalse(TEXT("Sin el panel de estado"), Normal.Contains(EHudZone::StatusPanel));

	FHudShown Status = Shown;
	Status.bStatusPanel = true;
	TestTrue(TEXT("Con su tecla, el panel de estado sí"), NoticeAvoidZones(Status, true).Contains(EHudZone::StatusPanel));
	FHudShown Debug = Shown;
	Debug.bDebugPanel = true;
	TestTrue(TEXT("Y el de depuración, con la suya"), NoticeAvoidZones(Debug, true).Contains(EHudZone::DebugPanel));

	TestFalse(TEXT("Sin panel de decisión a la vista, no se esquiva"), NoticeAvoidZones(Shown, false).Contains(EHudZone::DecisionPanel));
	FHudShown Speaking = Shown;
	Speaking.bGoalPanel = false;
	Speaking.bGrimoireButton = false;
	TestTrue(TEXT("Con frase del Warden: la caja de la partida y el panel de decisión"),
		NoticeAvoidZones(Speaking, true) == TArray<EHudZone>{EHudZone::MatchPanel, EHudZone::DecisionPanel});

	const FBox2f Bounds = Soulbound::Presentation::NoticeBounds(FVector2f(1920.f, 1080.f));
	TestEqual(TEXT("El área es la capa entera: desde la esquina"), Bounds.Min, FVector2f(0.f, 0.f));
	TestEqual(TEXT("Hasta el borde derecho y el de abajo, sin columna"), Bounds.Max, FVector2f(1920.f, 1080.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusDiceLineTest, "Soulbound.Game.Presentation.Estado.LineaDeLosDados", HintsTestFlags)

bool FStatusDiceLineTest::RunTest(const FString& Parameters)
{
	// CA-HUD-14 (R-HUD-1, precisión de S-039; corrección del autor del 15/11/2026): la línea de la tirada del panel de estado dice "Dado: 4" con un dado y
	// "Dados: 8" con dos, con la Ayuda "clic en una casilla que late"; pero en el turno de tirada del humano, con el panel de decisión a la vista, no se
	// repite (R-DEC-3, S-028: la petición ya está allí). La Ayuda solo sigue saliendo cuando el panel de decisión no está a la vista; en el turno de
	// otro asiento, la línea sigue, sin Ayuda.
	Soulbound::Presentation::FStatusContext Context;
	Context.HumanSeat = MhHuman;
	Context.SeatColors = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
	auto Lines = [&Context](const FMatchState& State)
	{
		return Soulbound::Presentation::StatusLines(State, Context);
	};
	auto AnyContains = [](const TArray<Soulbound::Presentation::FPanelLine>& All, const TCHAR* Text)
	{
		return All.ContainsByPredicate([Text](const Soulbound::Presentation::FPanelLine& Line) { return Line.Text.Contains(Text); });
	};

	// Sin el panel de decisión a la vista: la línea de siempre, con la Ayuda.
	FMatchState One = MhHumanRoll({4});
	One.MatchPhase = EMatchPhase::BondRace;
	Context.bDecisionPanelShown = false;
	const TArray<Soulbound::Presentation::FPanelLine> NoPanel = Lines(One);
	const FString OneLine = NoPanel.IsEmpty() ? FString() : NoPanel[0].Text;
	TestTrue(TEXT("Un dado: \"Dado: 4\""), OneLine.Contains(TEXT("Dado: 4")));
	TestTrue(TEXT("Con el reloj de turno"), OneLine.Contains(TEXT("reloj de turno: 15 s")));
	TestTrue(TEXT("La Ayuda: clic en una casilla que late"), OneLine.Contains(TEXT("clic en una casilla que late")));
	TestFalse(TEXT("Ya no dice Tirada"), OneLine.Contains(TEXT("Tirada")));
	TestFalse(TEXT("Ni casilla blanca"), OneLine.Contains(TEXT("blanca")));

	FMatchState Two = MhHumanRoll({3, 5});
	Two.MatchPhase = EMatchPhase::BondRace;
	const TArray<Soulbound::Presentation::FPanelLine> TwoLines = Lines(Two);
	TestTrue(TEXT("Dos dados: \"Dados: 8\""), AnyContains(TwoLines, TEXT("Dados: 8")));
	TestFalse(TEXT("Sin el \"(dos dados)\" de antes"), AnyContains(TwoLines, TEXT("dos dados")));

	// Con el panel de decisión a la vista, la línea del humano no sale, ni con un dado ni con dos, ni su Ayuda.
	Context.bDecisionPanelShown = true;
	const TArray<Soulbound::Presentation::FPanelLine> WithPanel = Lines(One);
	TestFalse(TEXT("Con el panel de decisión, no se repite la tirada (un dado)"), AnyContains(WithPanel, TEXT("Dado:")));
	TestFalse(TEXT("... ni su Ayuda"), AnyContains(WithPanel, TEXT("clic en una casilla que late")));
	TestFalse(TEXT("... ni con dos dados"), AnyContains(Lines(Two), TEXT("Dados:")));
	TestTrue(TEXT("El resto de líneas sigue ahí"), WithPanel.Num() >= 2);

	// En el turno de otro asiento la línea sigue, con el panel a la vista o no, y sin la Ayuda.
	FMatchState Other = MhHumanRoll({4});
	Other.MatchPhase = EMatchPhase::BondRace;
	Other.ActiveSeat = 1;
	FMatchState OtherTwo = MhHumanRoll({3, 5});
	OtherTwo.MatchPhase = EMatchPhase::BondRace;
	OtherTwo.ActiveSeat = 1;
	for (const bool bShown : {true, false})
	{
		Context.bDecisionPanelShown = bShown;
		const TArray<Soulbound::Presentation::FPanelLine> OtherLines = Lines(Other);
		TestTrue(TEXT("Otro asiento: \"Dado: 4\" sigue"), AnyContains(OtherLines, TEXT("Dado: 4")));
		TestFalse(TEXT("... sin la Ayuda"), AnyContains(OtherLines, TEXT("clic en una casilla que late")));
		TestTrue(TEXT("Otro asiento, dos dados: \"Dados: 8\" sigue"), AnyContains(Lines(OtherTwo), TEXT("Dados: 8")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordUnreachableReasonTest, "Soulbound.Game.Presentation.Acta.MotivoDelRechazo", HintsTestFlags)

bool FRecordUnreachableReasonTest::RunTest(const FString& Parameters)
{
	// CA-ACT-8 (R-ACT, precisión de R-ACT-8): el texto del motivo para la línea "humano" es "dado", "Eco" y "sello" para Roll, Confined y Sealed.
	using Soulbound::Presentation::UnreachableReasonText;
	TestEqual(TEXT("Roll"), UnreachableReasonText(EUnreachableReason::Roll), FString(TEXT("dado")));
	TestEqual(TEXT("Confined"), UnreachableReasonText(EUnreachableReason::Confined), FString(TEXT("Eco")));
	TestEqual(TEXT("Sealed"), UnreachableReasonText(EUnreachableReason::Sealed), FString(TEXT("sello")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
