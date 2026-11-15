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
	// CA-DEC-32 (R-DEC-16, vida del aviso): se ve desde el clic rechazado hasta que pasan sus segundos; un segundo rechazo lo sustituye y vuelve a
	// contar, aunque sea el mismo texto; un clic aceptado o el final del turno de tirada (Dismiss) lo quitan.
	using Soulbound::Presentation::FUnreachableNotice;
	const double Seconds = 4.0;
	FUnreachableNotice Notice;
	TestEqual(TEXT("Antes de ningún rechazo, nada"), Notice.Current(10.0, Seconds), FString());

	Notice.Show(TEXT("A"), 10.0);
	TestEqual(TEXT("Se ve desde el clic"), Notice.Current(10.0, Seconds), FString(TEXT("A")));
	TestEqual(TEXT("... y mientras dura"), Notice.Current(13.9, Seconds), FString(TEXT("A")));
	TestEqual(TEXT("... y se quita cuando pasan sus segundos"), Notice.Current(14.0, Seconds), FString());
	TestEqual(TEXT("... y sigue quitado"), Notice.Current(20.0, Seconds), FString());

	Notice.Show(TEXT("A"), 20.0);
	Notice.Show(TEXT("A"), 22.0);
	TestEqual(TEXT("Un segundo rechazo, con el mismo texto, vuelve a contar"), Notice.Current(25.9, Seconds), FString(TEXT("A")));
	TestEqual(TEXT("... desde el segundo"), Notice.Current(26.0, Seconds), FString());

	Notice.Show(TEXT("B"), 30.0);
	Notice.Show(TEXT("C"), 31.0);
	TestEqual(TEXT("Un rechazo con otro texto sustituye al anterior"), Notice.Current(31.5, Seconds), FString(TEXT("C")));

	Notice.Dismiss();
	TestEqual(TEXT("Un clic aceptado o el final del turno lo quitan"), Notice.Current(31.5, Seconds), FString());
	TestEqual(TEXT("Con duración cero no se ve"), FUnreachableNotice().Current(0.0, 0.0), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoticeKeepsIdentityTest, "Soulbound.Game.Presentation.Decision.ElAvisoNoMueveLaVista", HintsTestFlags)

bool FNoticeKeepsIdentityTest::RunTest(const FString& Parameters)
{
	// R-DEC-16 (S-039), revisión del autor: el aviso no cambia la identidad de la decisión (la barra del plazo no se reinicia al salir o quitarse) ni
	// toca el título, el contexto, los dados ni la instrucción (nada salta); la decisión de casilla deja siempre el sitio de su línea. Otras
	// decisiones no lo llevan.
	using Soulbound::Presentation::SetNotice;
	const FMatchState State = MhHumanRoll({3, 5});
	const TOptional<FDecisionView> Plain = Soulbound::Presentation::DecisionView(State, MhHuman, false);
	if (!TestTrue(TEXT("Hay vista"), Plain.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("La decisión de casilla deja sitio al aviso"), Plain->bReservesNotice);
	TestTrue(TEXT("Sin aviso, el texto está vacío"), Plain->Notice.IsEmpty());

	FDecisionView WithNotice = *Plain;
	SetNotice(WithNotice, TEXT("Con un 8 no llegas ahí. Qué lástima."));
	TestEqual(TEXT("El aviso se pone"), WithNotice.Notice, FString(TEXT("Con un 8 no llegas ahí. Qué lástima.")));
	TestEqual(TEXT("La identidad es la misma con aviso y sin él"), WithNotice.Identity, Plain->Identity);
	TestFalse(TEXT("La identidad no está vacía"), Plain->Identity.IsEmpty());
	TestEqual(TEXT("El título no cambia"), WithNotice.Title, Plain->Title);
	TestEqual(TEXT("El contexto no cambia"), WithNotice.Context, Plain->Context);
	TestEqual(TEXT("La instrucción no cambia"), WithNotice.Instruction, Plain->Instruction);
	TestTrue(TEXT("Los dados no cambian"), WithNotice.DiceValues == Plain->DiceValues);
	TestEqual(TEXT("El plazo no cambia"), WithNotice.DeadlineSeconds, Plain->DeadlineSeconds);
	TestTrue(TEXT("El sitio sigue reservado"), WithNotice.bReservesNotice);

	SetNotice(WithNotice, FString());
	TestTrue(TEXT("Quitar el aviso deja la vista como estaba"), WithNotice.Notice.IsEmpty() && WithNotice.Identity == Plain->Identity);

	// La tirada repetible sigue siendo la decisión de casilla: misma identidad y sitio reservado.
	FMatchState Reroll = State;
	Reroll.PendingChoice.Kind = EChoiceKind::Reroll;
	Reroll.PendingChoice.Seat = MhHuman;
	Reroll.PendingChoice.Options = {1};
	Reroll.PendingChoice.DeadlineSeconds = MhNow + 15.0;
	const TOptional<FDecisionView> RerollView = Soulbound::Presentation::DecisionView(Reroll, MhHuman, false);
	if (TestTrue(TEXT("Hay vista con la repetición"), RerollView.IsSet()))
	{
		TestTrue(TEXT("Con la repetición también deja sitio"), RerollView->bReservesNotice);
	}

	// Otra decisión no lleva el sitio y el aviso no se le pone.
	FMatchState Dice;
	Dice.Seats.SetNum(4);
	Dice.ActiveSeat = MhHuman;
	Dice.TurnPhase = ETurnPhase::PreRoll;
	Dice.PendingChoice.Kind = EChoiceKind::DiceCount;
	Dice.PendingChoice.Seat = MhHuman;
	Dice.PendingChoice.Options = {1, 2};
	Dice.PendingChoice.DeadlineSeconds = MhNow + 12.0;
	TOptional<FDecisionView> Buttons = Soulbound::Presentation::DecisionView(Dice, MhHuman, false);
	if (TestTrue(TEXT("Hay vista de dados"), Buttons.IsSet()))
	{
		TestFalse(TEXT("La elección de dados no reserva aviso"), Buttons->bReservesNotice);
		SetNotice(*Buttons, TEXT("x"));
		TestTrue(TEXT("... y no se le pone"), Buttons->Notice.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatusDiceLineTest, "Soulbound.Game.Presentation.Estado.LineaDeLosDados", HintsTestFlags)

bool FStatusDiceLineTest::RunTest(const FString& Parameters)
{
	// CA-HUD-14 (R-HUD-1, precisión de S-039): la línea de la tirada dice "Dado: 4" con un dado y "Dados: 8" con dos, y la Ayuda dice "clic en una
	// casilla que late"; ya no dice "Tirada:" ni "blanca".
	Soulbound::Presentation::FStatusContext Context;
	Context.HumanSeat = MhHuman;
	Context.SeatColors = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
	auto FirstLine = [&Context](const FMatchState& State)
	{
		const TArray<Soulbound::Presentation::FPanelLine> Lines = Soulbound::Presentation::StatusLines(State, Context);
		return Lines.IsEmpty() ? FString() : Lines[0].Text;
	};

	FMatchState One = MhHumanRoll({4});
	One.MatchPhase = EMatchPhase::BondRace;
	const FString OneLine = FirstLine(One);
	TestTrue(TEXT("Un dado: \"Dado: 4\""), OneLine.Contains(TEXT("Dado: 4")));
	TestTrue(TEXT("Con el reloj de turno"), OneLine.Contains(TEXT("reloj de turno: 15 s")));
	TestTrue(TEXT("La Ayuda: clic en una casilla que late"), OneLine.Contains(TEXT("clic en una casilla que late")));
	TestFalse(TEXT("Ya no dice Tirada"), OneLine.Contains(TEXT("Tirada")));
	TestFalse(TEXT("Ni casilla blanca"), OneLine.Contains(TEXT("blanca")));

	FMatchState Two = MhHumanRoll({3, 5});
	Two.MatchPhase = EMatchPhase::BondRace;
	const FString TwoLine = FirstLine(Two);
	TestTrue(TEXT("Dos dados: \"Dados: 8\""), TwoLine.Contains(TEXT("Dados: 8")));
	TestFalse(TEXT("Sin el \"(dos dados)\" de antes"), TwoLine.Contains(TEXT("dos dados")));
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
