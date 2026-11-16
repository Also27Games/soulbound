// Tests del aspecto de la ficha (S-029, R-VIS-10, R-VIS-11): su color según el rol y qué asientos llevan el aro del turno. Funciones puras sobre
// un FMatchState armado a mano: sin mundo y sin actor. Filtro: "Soulbound.Game.Presentation.Ficha".

#include "../../Presentation/TokenLook.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TokenLookFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	TArray<FLinearColor> TlDomainColors()
	{
		TArray<FLinearColor> Colors;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Colors.Add(FLinearColor(0.1f * (Index + 1), 0.2f, 0.3f));
		}
		return Colors;
	}

	FMatchState TlMakeState()
	{
		FMatchState State;
		State.Seats.SetNum(4);
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTokenColorTest, "Soulbound.Game.Presentation.Ficha.ColorSegunElRol", TokenLookFlags)

bool FTokenColorTest::RunTest(const FString& Parameters)
{
	// CA-VIS-10 (R-VIS-10): un Eco es gris; vinculado al Dominio 3, el color del Dominio 3; si vuelve a Eco, gris otra vez.
	const FLinearColor Gray(0.4f, 0.4f, 0.4f);
	const TArray<FLinearColor> Colors = TlDomainColors();
	FSeatState Seat;
	TestEqual(TEXT("Eco: gris"), Soulbound::Presentation::TokenColor(Seat.Role, Seat.DomainId, Gray, Colors), Gray);
	Seat.Role = ESeatRole::Elect;
	Seat.DomainId = 3;
	TestEqual(TEXT("Elegido del Dominio 3: su color"), Soulbound::Presentation::TokenColor(Seat.Role, Seat.DomainId, Gray, Colors), Colors[3]);
	Seat.Role = ESeatRole::LostEcho;
	TestEqual(TEXT("De vuelta a Eco: gris"), Soulbound::Presentation::TokenColor(Seat.Role, Seat.DomainId, Gray, Colors), Gray);
	// R-VIS-10 (glosario, rol del asiento): el Dominio de un Eco tras la carrera es su hogar y no le da color.
	Seat.DomainId = 5;
	TestEqual(TEXT("Eco con hogar: gris"), Soulbound::Presentation::TokenColor(Seat.Role, Seat.DomainId, Gray, Colors), Gray);
	TestEqual(TEXT("Elegido de un Dominio sin color: gris"), Soulbound::Presentation::TokenColor(ESeatRole::Elect, 9, Gray, Colors), Gray);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingedSeatsTest, "Soulbound.Game.Presentation.Ficha.AroDelTurno", TokenLookFlags)

bool FRingedSeatsTest::RunTest(const FString& Parameters)
{
	// CA-VIS-11 (R-VIS-11): con el asiento 2 activo en fase de tirada, solo el 2 lleva aro.
	FMatchState State = TlMakeState();
	State.ActiveSeat = 2;
	State.TurnPhase = ETurnPhase::RollAndChoose;
	TestEqual(TEXT("En su tirada, solo el 2"), Soulbound::Presentation::RingedSeats(State), TArray<int32>{2});
	// Sus fases de elección también son su turno.
	State.TurnPhase = ETurnPhase::SkillTrial;
	TestEqual(TEXT("En una fase de elección, el 2"), Soulbound::Presentation::RingedSeats(State), TArray<int32>{2});
	// Entre turnos, ninguno, aunque ActiveSeat guarde el del último turno.
	State.TurnPhase = ETurnPhase::BetweenTurns;
	TestTrue(TEXT("Entre turnos, ninguno"), Soulbound::Presentation::RingedSeats(State).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingedSeatsEventActionTest, "Soulbound.Game.Presentation.Ficha.AroEnDueloYTrileroEntreTurnos", TokenLookFlags)

bool FRingedSeatsEventActionTest::RunTest(const FString& Parameters)
{
	// CA-VIS-11 (R-VIS-11), lo que encuentro en las reglas: un duelo de un evento del Warden entre turnos (S-015) corre en la fase Duel con
	// CurrentEvent.bWaitsForAction, y ActiveSeat conserva el asiento del último turno (aquí el 0); quien juega son Challenger y Challenged.
	// El aro va a los dos participantes y no al ActiveSeat obsoleto.
	FMatchState State = TlMakeState();
	State.ActiveSeat = 0;
	State.CurrentEvent.EventIndex = 0;
	State.CurrentEvent.bWaitsForAction = true;
	State.TurnPhase = ETurnPhase::Duel;
	State.Duel.Challenger = 1;
	State.Duel.Challenged = 3;
	TestEqual(TEXT("Duelo entre turnos entre el 1 y el 3: los dos"), Soulbound::Presentation::RingedSeats(State), TArray<int32>{1, 3});
	// Duelo contra el Warden: solo el jugador.
	State.Duel = FDuelState();
	State.Duel.bAgainstWarden = true;
	State.Duel.Challenged = 2;
	TestEqual(TEXT("Duelo contra el Warden: solo el jugador"), Soulbound::Presentation::RingedSeats(State), TArray<int32>{2});
	// Trilero entre turnos: ladrón y víctima.
	State.Duel = FDuelState();
	State.TurnPhase = ETurnPhase::ShellGame;
	State.ShellGame.Stage = EShellGameStage::Pick;
	State.ShellGame.Thief = 3;
	State.ShellGame.Victim = 0;
	TestEqual(TEXT("Trilero entre turnos: los dos"), Soulbound::Presentation::RingedSeats(State), TArray<int32>{0, 3});
	// El mismo duelo dentro del turno del 1 (sin evento que espere): solo el activo.
	State.CurrentEvent = FCurrentWardenEvent();
	State.ActiveSeat = 1;
	State.TurnPhase = ETurnPhase::Duel;
	State.ShellGame = FShellGameState();
	State.Duel.Challenger = 1;
	State.Duel.Challenged = 3;
	TestEqual(TEXT("Duelo en el turno del 1: solo el 1"), Soulbound::Presentation::RingedSeats(State), TArray<int32>{1});
	// Con la partida terminada, ninguno.
	State.MatchPhase = EMatchPhase::MatchOver;
	TestTrue(TEXT("Partida terminada: ninguno"), Soulbound::Presentation::RingedSeats(State).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingPulseTest, "Soulbound.Game.Presentation.Ficha.PulsoDelAro", TokenLookFlags)

bool FRingPulseTest::RunTest(const FString& Parameters)
{
	// R-VIS-11: pulso lento de intensidad con el periodo de Config: mínimo al empezar, máximo a medio periodo y otra vez el mínimo al cerrarlo.
	TestEqual(TEXT("Empieza en el mínimo"), Soulbound::Presentation::RingPulse(0.0, 1.5f, 0.4f), 0.4f, 0.001f);
	TestEqual(TEXT("A medio periodo, el máximo"), Soulbound::Presentation::RingPulse(0.75, 1.5f, 0.4f), 1.f, 0.001f);
	TestEqual(TEXT("Al cerrar el periodo, el mínimo"), Soulbound::Presentation::RingPulse(1.5, 1.5f, 0.4f), 0.4f, 0.001f);
	TestEqual(TEXT("Se repite"), Soulbound::Presentation::RingPulse(3.75, 1.5f, 0.4f), 1.f, 0.001f);
	TestEqual(TEXT("Sin periodo válido, fijo en 1"), Soulbound::Presentation::RingPulse(0.3, 0.f, 0.4f), 1.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleOrbPulseTest, "Soulbound.Game.Presentation.Ficha.LatidoDelOrbe", TokenLookFlags)

bool FOracleOrbPulseTest::RunTest(const FString& Parameters)
{
	// CA-VIS-30 (R-VIS-29, S-041): la escala del orbe del Oráculo es el mínimo al empezar el periodo y 1 a mitad de periodo; con periodo cero o
	// negativo, 1 fijo.
	TestEqual(TEXT("Empieza en el mínimo"), Soulbound::Presentation::OracleOrbScale(0.0, 0.9f, 0.8f), 0.8f, 0.001f);
	TestEqual(TEXT("A mitad de periodo, su tamaño entero"), Soulbound::Presentation::OracleOrbScale(0.45, 0.9f, 0.8f), 1.f, 0.001f);
	TestEqual(TEXT("Al cerrar el periodo, otra vez el mínimo"), Soulbound::Presentation::OracleOrbScale(0.9, 0.9f, 0.8f), 0.8f, 0.001f);
	TestEqual(TEXT("Periodo cero, 1 fijo"), Soulbound::Presentation::OracleOrbScale(0.3, 0.f, 0.8f), 1.f, 0.001f);
	TestEqual(TEXT("Periodo negativo, 1 fijo"), Soulbound::Presentation::OracleOrbScale(0.3, -1.f, 0.8f), 1.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleDiscTest, "Soulbound.Game.Presentation.Ficha.DiscoDeRol", TokenLookFlags)

bool FRoleDiscTest::RunTest(const FString& Parameters)
{
	// R-VIS-10 (corrección del autor del 13/11/2026): el color del rol va en un disco plano bajo la ficha, más ancho que la peana de la carta y sin
	// pasar del borde interior del aro del turno.
	using Soulbound::Presentation::RoleDiscDiameter;
	using Soulbound::Presentation::RoleDiscLift;
	using Soulbound::Presentation::RoleDiscMaxDiameter;
	// Tope: diámetro del aro menos dos grosores y el margen (8,5 - 2 * 0,5 - 0,1).
	TestEqual(TEXT("Tope: el borde interior del aro del turno, con margen"), RoleDiscMaxDiameter(8.5f, 0.5f), 7.4f, 0.001f);
	TestEqual(TEXT("Un aro absurdo no da un tope negativo"), RoleDiscMaxDiameter(0.5f, 2.f), 0.f, 0.001f);
	// Diámetro: huella por escala.
	TestEqual(TEXT("Huella 3 por 1,8"), RoleDiscDiameter(3.f, 1.8f, 7.4f), 5.4f, 0.001f);
	TestEqual(TEXT("Sin pasar del tope"), RoleDiscDiameter(5.f, 1.8f, 7.4f), 7.4f, 0.001f);
	// Siempre más ancho que la peana: nunca menos de 1,1 veces la huella, aunque la escala sea 1 o menos...
	TestEqual(TEXT("Escala 1: 1,1 veces la huella"), RoleDiscDiameter(3.f, 1.f, 7.4f), 3.3f, 0.001f);
	// ...ni siquiera con un tope más corto que eso (el tope manda sobre la escala, el suelo sobre el tope).
	TestEqual(TEXT("Tope menor que el suelo: el suelo"), RoleDiscDiameter(3.f, 1.8f, 2.f), 3.3f, 0.001f);
	// Cota: cada asiento un poco más alto que el anterior.
	TestEqual(TEXT("Asiento 0: la cota base"), RoleDiscLift(0, 0.1f), 0.1f, 0.0001f);
	TestTrue(TEXT("Cada asiento más alto que el anterior"), RoleDiscLift(1, 0.1f) > RoleDiscLift(0, 0.1f) && RoleDiscLift(2, 0.1f) > RoleDiscLift(1, 0.1f)
		&& RoleDiscLift(3, 0.1f) > RoleDiscLift(2, 0.1f));
	TestEqual(TEXT("Por RoleDiscSeatStep"), RoleDiscLift(3, 0.1f), 0.1f + 3.f * Soulbound::Presentation::RoleDiscSeatStep, 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLabelFacingTest, "Soulbound.Game.Presentation.Ficha.NumeroMiraALaCamara", TokenLookFlags)

bool FLabelFacingTest::RunTest(const FString& Parameters)
{
	// CA-VIS-22 (R-VIS-23): la rotación del número sale de la de la cámara. Ejes del texto de UTextRenderComponent: frente +X, arriba +Z, y se lee hacia -Y
	// (el texto tumbado del tablero, FRotator(90, 180, 0), tiene su derecha en -Y). El frente mira a la cámara y su arriba y su derecha son los de ella.
	using Soulbound::Presentation::LabelFacingRotation;
	auto CheckFacing = [this](const TCHAR* What, const FRotator& Camera)
	{
		const FQuat Text = LabelFacingRotation(Camera).Quaternion();
		const FQuat CameraQuat = Camera.Quaternion();
		TestTrue(FString::Printf(TEXT("%s: el frente mira a la cámara"), What), Text.GetForwardVector().Equals(-CameraQuat.GetForwardVector(), 0.001));
		TestTrue(FString::Printf(TEXT("%s: su arriba es el de la cámara (no boca abajo)"), What), Text.GetUpVector().Equals(CameraQuat.GetUpVector(), 0.001));
		TestTrue(FString::Printf(TEXT("%s: su derecha es la de la cámara (no en espejo)"), What), (-Text.GetRightVector()).Equals(CameraQuat.GetRightVector(), 0.001));
	};
	const FRotator Down(-90.f, 0.f, 0.f);
	CheckFacing(TEXT("Cámara recta hacia abajo"), Down);
	TestTrue(TEXT("Cámara recta hacia abajo: el número mira hacia arriba"), LabelFacingRotation(Down).Quaternion().GetForwardVector().Equals(FVector::UpVector, 0.001));
	const FRotator Level(0.f, 0.f, 0.f);
	CheckFacing(TEXT("Cámara horizontal"), Level);
	TestTrue(TEXT("Cámara horizontal: el número está vertical"), FMath::IsNearlyZero(LabelFacingRotation(Level).Quaternion().GetForwardVector().Z, 0.001));
	const FRotator Turned(0.f, 180.f, 0.f);
	CheckFacing(TEXT("Cámara girada media vuelta"), Turned);
	TestTrue(TEXT("Cámara girada media vuelta: de frente al otro lado"),
		!LabelFacingRotation(Turned).Quaternion().GetForwardVector().Equals(LabelFacingRotation(Level).Quaternion().GetForwardVector(), 0.001));
	CheckFacing(TEXT("Cámara sentada"), FRotator(-35.f, 200.f, 0.f));
	CheckFacing(TEXT("Cámara con alabeo"), FRotator(-20.f, 40.f, 15.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashScaleTest, "Soulbound.Game.Presentation.Ficha.CrecimientoDelDestello", TokenLookFlags)

bool FFlashScaleTest::RunTest(const FString& Parameters)
{
	// R-VIS-12 (corrección del autor del 13/11/2026): el aro del destello y el halo de la Fuente crecen hacia fuera mientras dura el destello.
	using Soulbound::Presentation::FlashScale;
	TestEqual(TEXT("Al empezar, su tamaño"), FlashScale(0.f, 0.8f), 1.f, 0.0001f);
	TestEqual(TEXT("A la mitad, la mitad del crecimiento"), FlashScale(0.5f, 0.8f), 1.4f, 0.0001f);
	TestEqual(TEXT("Al acabar, todo el crecimiento"), FlashScale(1.f, 0.8f), 1.8f, 0.0001f);
	TestEqual(TEXT("Antes de empezar, el extremo"), FlashScale(-0.3f, 0.8f), 1.f, 0.0001f);
	TestEqual(TEXT("Pasado el final, el extremo"), FlashScale(1.7f, 0.8f), 1.8f, 0.0001f);
	TestEqual(TEXT("Sin crecimiento, siempre 1"), FlashScale(0.6f, 0.f), 1.f, 0.0001f);
	TestEqual(TEXT("Un crecimiento negativo no encoge"), FlashScale(1.f, -2.f), 1.f, 0.0001f);
	// Punto 8: hacia dentro hace el camino contrario, se cierra de 1 + crecimiento a 1.
	TestEqual(TEXT("Hacia dentro, al empezar: todo el tamaño"), FlashScale(0.f, 0.8f, true), 1.8f, 0.0001f);
	TestEqual(TEXT("Hacia dentro, a la mitad"), FlashScale(0.5f, 0.8f, true), 1.4f, 0.0001f);
	TestEqual(TEXT("Hacia dentro, al acabar: el tamaño base"), FlashScale(1.f, 0.8f, true), 1.f, 0.0001f);
	TestEqual(TEXT("Hacia dentro, fuera de rango: el extremo"), FlashScale(-1.f, 0.8f, true), 1.8f, 0.0001f);
	return true;
}

#endif
