// Tests del cerebro del Warden (S-016, GDD 06): evaluación sin mundo y reproducible (R-WAR-1, R-WAR-3), actos (R-WAR-2), margen
// (R-WAR-4d), aplazamiento (R-WAR-7), garantía (R-WAR-5), canales y registro (R-WAR-4, R-WAR-10), y la selección por sacos con
// pesos, usos y ventanas (R-WAR-8, R-SAC-1). Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre. Sustituye a
// los tres tests del cerebro del spike (Sellos sueltos), que desaparecen con él. Filtro: "Soulbound.Core.Warden".
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos,
// con los eventos y el Warden de los archivos).

#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FWardenFixture;

	constexpr int32 Fenrir = FWardenFixture::Fenrir;
	constexpr int32 Koschei = FWardenFixture::Koschei;
	constexpr int32 EchoA = FWardenFixture::EchoA;
	constexpr int32 EchoB = FWardenFixture::EchoB;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	bool SameDecision(const FWardenDecision& A, const FWardenDecision& B)
	{
		return A.Kind == B.Kind && A.EventIndex == B.EventIndex && A.Act == B.Act && A.bForcedByGuarantee == B.bForcedByGuarantee
			&& A.NextEvaluationSeconds == B.NextEvaluationSeconds;
	}

	/** Da a cada asiento una runa de cada tipo y lo saca de su hogar: así casi todos los efectos tienen objetivo válido. */
	void EveryoneRichAndAway(FWardenFixture& F)
	{
		for (int32 Seat = 0; Seat < F.State.Seats.Num(); ++Seat)
		{
			F.State.Seats[Seat].Runes.Init(1, Soulbound::Board::DomainCount);
			F.MoveAway(Seat);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenSameInputTest, "Soulbound.Core.Warden.Brain.MismaEntradaMismaDecision",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenSameInputTest::RunTest(const FString& Parameters)
{
	// CA-WAR-1 (R-WAR-1, R-WAR-3): mismo estado, mismo reloj y misma semilla, misma decisión y mismo instante siguiente, dos
	// veces, sin mundo. El instante siguiente es el intervalo más o menos su variación. Y no es una constante: con semillas
	// distintas hay intervenciones con efecto y evaluaciones que no hacen nada.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	EveryoneRichAndAway(F);
	int32 WithEffect = 0;
	int32 Nothing = 0;
	for (const double At : {400.0, 900.0, 1700.0})
	{
		for (int32 Seed = 1; Seed <= 40; ++Seed)
		{
			const FWardenDecision A = F.Evaluate(At, Seed);
			const FWardenDecision B = F.Evaluate(At, Seed);
			if (!SameDecision(A, B))
			{
				AddError(FString::Printf(TEXT("t=%.0f, semilla %d: dos evaluaciones iguales deciden distinto"), At, Seed));
			}
			if (A.NextEvaluationSeconds < At + 10.0 || A.NextEvaluationSeconds > At + 20.0)
			{
				AddError(FString::Printf(TEXT("t=%.0f, semilla %d: siguiente evaluación en %.2f, fuera de 15 ± 5"), At, Seed, A.NextEvaluationSeconds));
			}
			WithEffect += A.IsIntervention() ? 1 : 0;
			Nothing += A.Kind == EWardenDecisionKind::None ? 1 : 0;
		}
	}
	TestTrue(TEXT("Hay intervenciones con efecto"), WithEffect > 0);
	TestTrue(TEXT("Y evaluaciones que no hacen nada"), Nothing > 0);

	// R-WAR-10: evaluar y aplicar dos veces desde el mismo estado deja el mismo estado y el mismo registro.
	FMatchState CopyA = F.State;
	FMatchState CopyB = F.State;
	FRandomStream RandomA(5);
	FRandomStream RandomB(5);
	Soulbound::Rules::EvaluateWarden(CopyA, 400.0, RandomA);
	Soulbound::Rules::EvaluateWarden(CopyB, 400.0, RandomB);
	TestTrue(TEXT("Evaluar y aplicar es reproducible"), Soulbound::Tests::SameState(CopyA, CopyB));
	TestEqual(TEXT("El sorteo del Warden avanzó"), CopyA.Warden.Draws, F.State.Warden.Draws + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenActsTest, "Soulbound.Core.Warden.Brain.ActoEsFuncionDelEstado",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenActsTest::RunTest(const FString& Parameters)
{
	// R-WAR-2: tres actos por tiempo de datos (0, 300 y 1620); el fin de la carrera adelanta el Acto II. Es una función del
	// reloj y de la fase, no un contador.
	FWardenFixture F;
	if (!F.StartInRace(*this))
	{
		return false;
	}
	TestEqual(TEXT("En la carrera, al empezar: Acto I"), AsInt(Soulbound::Rules::CurrentAct(F.State, 10.0)), AsInt(EAct::ActOne));
	TestEqual(TEXT("En la carrera, en el 299: Acto I"), AsInt(Soulbound::Rules::CurrentAct(F.State, 299.0)), AsInt(EAct::ActOne));
	TestEqual(TEXT("En la carrera, en el 300: Acto II por tiempo"), AsInt(Soulbound::Rules::CurrentAct(F.State, 300.0)), AsInt(EAct::ActTwo));

	F.State.MatchPhase = EMatchPhase::RuneHunt;
	TestEqual(TEXT("Acabada la carrera antes del 300: Acto II"), AsInt(Soulbound::Rules::CurrentAct(F.State, 200.0)), AsInt(EAct::ActTwo));
	TestEqual(TEXT("En el 1619: Acto II"), AsInt(Soulbound::Rules::CurrentAct(F.State, 1619.0)), AsInt(EAct::ActTwo));
	TestEqual(TEXT("En el 1620: Acto III"), AsInt(Soulbound::Rules::CurrentAct(F.State, 1620.0)), AsInt(EAct::ActThree));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenMarginTest, "Soulbound.Core.Warden.Brain.MargenFinDeActo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenMarginTest::RunTest(const FString& Parameters)
{
	// CA-WAR-1b (R-WAR-4d): una evaluación en el segundo 290 del Acto I no inicia nada, ni un comentario; en el 280, sí. Lo
	// mismo en los últimos 12 s del Acto II y de la partida.
	FWardenFixture F;
	if (!F.StartInRace(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Scene);
	TestTrue(TEXT("Acto I, segundo 280: interviene"), F.Evaluate(280.0).IsIntervention());
	TestEqual(TEXT("Acto I, segundo 290: nada"), AsInt(F.Evaluate(290.0).Kind), AsInt(EWardenDecisionKind::None));
	TestTrue(TEXT("Margen en el 290"), Soulbound::Rules::IsInWardenMargin(F.State, 290.0));

	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::EvaluateWarden(F.State, 290.0, F.Random);
	TestEqual(TEXT("Evaluar en el 290 no lanza nada"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 0);
	TestEqual(TEXT("Ni comenta"), F.Since(Before, EMatchEventType::WardenRemark).Num(), 0);
	TestEqual(TEXT("Pero deja su evaluación"), F.Since(Before, EMatchEventType::WardenEvaluated).Num(), 1);

	F.AlwaysIntervene(EWardenChannel::Remark);
	TestEqual(TEXT("Con solo comentarios, en el 290 tampoco"), AsInt(F.Evaluate(290.0).Kind), AsInt(EWardenDecisionKind::None));

	F.State.MatchPhase = EMatchPhase::RuneHunt;
	TestTrue(TEXT("Fin del Acto II, segundo 1610: margen"), Soulbound::Rules::IsInWardenMargin(F.State, 1610.0));
	TestFalse(TEXT("Segundo 1600: fuera del margen"), Soulbound::Rules::IsInWardenMargin(F.State, 1600.0));
	TestTrue(TEXT("Fin de la partida, segundo 2390: margen"), Soulbound::Rules::IsInWardenMargin(F.State, 2390.0));
	TestEqual(TEXT("En el 2390, nada"), AsInt(F.Evaluate(2390.0).Kind), AsInt(EWardenDecisionKind::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenClockKeepsRunningTest, "Soulbound.Core.Warden.Brain.IntervencionNoDetieneElReloj",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenClockKeepsRunningTest::RunTest(const FString& Parameters)
{
	// CA-WAR-1b (R-WAR-4c): una intervención no detiene el reloj de partida. Lo que lanza lleva la marca de su instante, su
	// dilema corre con el reloj desde ese instante y la siguiente evaluación es la de siempre, 15 ± 5 s después, aunque el
	// evento siga en curso.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Strike);
	F.KeepOnly({TEXT("generosidad-envenenada")});
	EveryoneRichAndAway(F);
	F.State.ClockSeconds = 500.0;
	const int32 Before = F.State.EventLog.Num();
	const FWardenDecision Decision = Soulbound::Rules::EvaluateWarden(F.State, 500.0, F.Random);
	if (!TestEqual(TEXT("Golpe con el dilema"), Decision.EventIndex, F.IndexOf(TEXT("generosidad-envenenada"))))
	{
		return false;
	}
	TestEqual(TEXT("El reloj del estado no se toca"), F.State.ClockSeconds, 500.0);
	for (int32 Index = Before; Index < F.State.EventLog.Num(); ++Index)
	{
		TestEqual(TEXT("Todo lo que deja lleva la marca del instante"), F.State.EventLog[Index].ClockSeconds, 500.0);
	}
	TestEqual(TEXT("El dilema corre desde ese instante"), F.State.PendingChoice.DeadlineSeconds, 500.0 + Soulbound::Rules::DilemmaClockSeconds(F.State.Params, 2));
	TestTrue(TEXT("La siguiente evaluación, 15 ± 5 s después"), F.State.Warden.NextEvaluationSeconds >= 510.0 && F.State.Warden.NextEvaluationSeconds <= 520.0);

	// El reloj sigue: pasado el plazo, el evento se resuelve en contra sin que nadie haya parado nada.
	F.Now = F.State.PendingChoice.DeadlineSeconds;
	F.Advance();
	TestEqual(TEXT("El dilema se agotó con el reloj"), F.Since(Before, EMatchEventType::WardenIgnored).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenDeferTest, "Soulbound.Core.Warden.Brain.AplazaDuranteDueloTrileroMinijuegoYEvento",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenDeferTest::RunTest(const FString& Parameters)
{
	// CA-WAR-4 (R-WAR-7): una evaluación durante un duelo, un trilero, un minijuego o un evento en curso se aplaza: el cerebro
	// devuelve "aplazada", no aplica nada y el estado lo apunta.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Strike);
	EveryoneRichAndAway(F);
	const FMatchState Clean = F.State;

	auto ExpectDeferred = [this, &F](const TCHAR* Context)
	{
		TestEqual(FString::Printf(TEXT("%s: aplazada"), Context), AsInt(F.Evaluate(600.0).Kind), AsInt(EWardenDecisionKind::Deferred));
		TestTrue(FString::Printf(TEXT("%s: hay que aplazar"), Context), Soulbound::Rules::IsWardenDeferred(F.State));
		const int32 Before = F.State.EventLog.Num();
		Soulbound::Rules::EvaluateWarden(F.State, 600.0, F.Random);
		TestTrue(FString::Printf(TEXT("%s: el estado lo apunta"), Context), F.State.Warden.bDeferred);
		TestEqual(FString::Printf(TEXT("%s: no lanza nada"), Context), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: no corta nada"), Context), F.Since(Before, EMatchEventType::TurnInterrupted).Num(), 0);
		TestFalse(FString::Printf(TEXT("%s: mientras dura, no toca evaluar"), Context), Soulbound::Rules::IsWardenEvaluationDue(F.State, 700.0));
	};

	F.State.TurnPhase = ETurnPhase::Duel;
	F.State.Duel.Challenger = Fenrir;
	F.State.Duel.Challenged = Koschei;
	ExpectDeferred(TEXT("Duelo"));

	F.State = Clean;
	F.State.TurnPhase = ETurnPhase::ShellGame;
	F.State.ShellGame.Stage = EShellGameStage::Shuffle;
	ExpectDeferred(TEXT("Trilero"));

	F.State = Clean;
	F.State.TurnPhase = ETurnPhase::SkillTrial;
	F.State.PendingTrial = ESkillTrial::ForgottenSource;
	ExpectDeferred(TEXT("Minijuego"));

	F.State = Clean;
	F.Director(Fenrir);
	if (TestEqual(TEXT("Un dilema en curso"), AsInt(F.Launch(TEXT("dos-turnos-o-casa"))), AsInt(EWardenEventStart::Started)))
	{
		ExpectDeferred(TEXT("Evento en curso"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenDeferredResumesTest, "Soulbound.Core.Warden.Brain.AplazadaIntervieneAlTerminar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenDeferredResumesTest::RunTest(const FString& Parameters)
{
	// CA-WAR-4 (R-WAR-7): al terminar lo que la aplazó, se vuelve a evaluar en ese momento, sin esperar al intervalo, e
	// interviene si procede (aquí, siempre).
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Strike);
	F.KeepOnly({TEXT("prisa")});
	F.Director(Fenrir);
	F.Now = 600.0;
	if (!TestEqual(TEXT("Un dilema en curso"), AsInt(Soulbound::Rules::ApplyEvent(F.State, TEXT("dos-turnos-o-casa"), F.Now, F.WardenRandom)), AsInt(EWardenEventStart::Started)))
	{
		return false;
	}
	Soulbound::Rules::EvaluateWarden(F.State, 605.0, F.Random);
	TestTrue(TEXT("Aplazada"), F.State.Warden.bDeferred);

	// El dilema termina: Fenrir elige los dos turnos extra.
	F.Now = 607.0;
	TestEqual(TEXT("Fenrir elige"), AsInt(F.Answer(0)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("El evento terminó"), F.State.CurrentEvent.IsActive());
	TestTrue(TEXT("Toca evaluar ya, antes del intervalo"), Soulbound::Rules::IsWardenEvaluationDue(F.State, 607.0) && 607.0 < 605.0 + 10.0);

	const int32 Before = F.State.EventLog.Num();
	const FWardenDecision Decision = Soulbound::Rules::EvaluateWarden(F.State, 607.0, F.Random);
	TestEqual(TEXT("Y ahora interviene"), AsInt(Decision.Kind), AsInt(EWardenDecisionKind::Strike));
	TestFalse(TEXT("Sin aplazamiento pendiente"), F.State.Warden.bDeferred);
	TestEqual(TEXT("Lanza Prisa"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 1);
	TestTrue(TEXT("La siguiente, 15 ± 5 s después de esta"), F.State.Warden.NextEvaluationSeconds >= 617.0 && F.State.Warden.NextEvaluationSeconds <= 627.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenGuaranteeTest, "Soulbound.Core.Warden.Brain.GarantiaObligaConEfecto",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenGuaranteeTest::RunTest(const FString& Parameters)
{
	// R-WAR-5, garantía anticipada (decisión del autor del 19/10/2026): si esperar a la siguiente evaluación dejaría pasar más
	// de 120 s sin golpe ni escena, el cerebro interviene ya, con probabilidad 1 y con efecto, nunca con un comentario.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.NeverIntervene();
	EveryoneRichAndAway(F);
	F.State.Warden.LastEffectSeconds = 400.0;
	for (int32 Seed = 1; Seed <= 30; ++Seed)
	{
		const FWardenDecision Early = F.Evaluate(450.0, Seed);
		if (Early.Kind != EWardenDecisionKind::None || Early.bForcedByGuarantee)
		{
			AddError(FString::Printf(TEXT("Semilla %d: a 50 s de la última, sin probabilidad, no debería intervenir"), Seed));
		}
		// Desde el 521, la siguiente evaluación (al menos 10 s después) caería pasados los 120 s.
		const FWardenDecision Late = F.Evaluate(521.0, Seed);
		if (!Late.bForcedByGuarantee || !Late.IsIntervention())
		{
			AddError(FString::Printf(TEXT("Semilla %d: en el 521 la garantía obliga, con efecto (tipo %d)"), Seed, AsInt(Late.Kind)));
		}
		// Entre medias, solo si el instante que sorteó para la siguiente queda fuera de la garantía.
		const FWardenDecision Middle = F.Evaluate(505.0, Seed);
		if (Middle.bForcedByGuarantee != (Middle.NextEvaluationSeconds - 400.0 > 120.0))
		{
			AddError(FString::Printf(TEXT("Semilla %d: en el 505 se fuerza si y solo si la siguiente (%.1f) pasa de la garantía"), Seed, Middle.NextEvaluationSeconds));
		}
	}
	TestTrue(TEXT("Sin nada que ver con el comentario: con su peso al máximo sigue siendo con efecto"), [&F]()
	{
		for (FWardenActParams& Act : F.State.WardenParams.Acts)
		{
			Act.ChannelWeights = {1.0, 1.0, 1000.0};
		}
		return F.Evaluate(521.0).IsIntervention();
	}());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenChannelsTest, "Soulbound.Core.Warden.Brain.CanalesYComentario",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenChannelsTest::RunTest(const FString& Parameters)
{
	// R-WAR-4, R-WAR-8, R-WAR-10: sin candidato en el canal elegido se prueba el otro con efecto; sin ninguno, comentario. El
	// comentario no aplica nada: solo deja WardenRemark. Cada evaluación deja WardenEvaluated y WardenDecided.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Strike);
	F.KeepOnly({TEXT("primer-muro")});
	const FWardenDecision Other = F.Evaluate(600.0);
	TestEqual(TEXT("Sin golpes, la escena"), AsInt(Other.Kind), AsInt(EWardenDecisionKind::Scene));
	TestEqual(TEXT("Primer muro"), Other.EventIndex, F.IndexOf(TEXT("primer-muro")));

	F.KeepOnly({});
	TestEqual(TEXT("Sin candidatos, comentario"), AsInt(F.Evaluate(600.0).Kind), AsInt(EWardenDecisionKind::Remark));

	F.AlwaysIntervene(EWardenChannel::Remark);
	const FMatchState Before = F.State;
	const int32 From = F.State.EventLog.Num();
	Soulbound::Rules::EvaluateWarden(F.State, 600.0, F.Random);
	TestEqual(TEXT("El comentario deja WardenRemark"), F.Since(From, EMatchEventType::WardenRemark).Num(), 1);
	const TArray<FMatchEvent> Evaluated = F.Since(From, EMatchEventType::WardenEvaluated);
	const TArray<FMatchEvent> Decided = F.Since(From, EMatchEventType::WardenDecided);
	if (TestEqual(TEXT("Una evaluación"), Evaluated.Num(), 1) && TestEqual(TEXT("Una decisión"), Decided.Num(), 1))
	{
		TestEqual(TEXT("Evaluada en el Acto II"), Evaluated[0].Value, AsInt(EAct::ActTwo));
		TestEqual(TEXT("Decisión: comentario"), Decided[0].Detail, AsInt(EWardenDecisionKind::Remark));
		TestEqual(TEXT("Sin evento"), Decided[0].Value, INDEX_NONE);
	}
	TestTrue(TEXT("Y no aplica nada a nadie"), Before.Seats.Num() == F.State.Seats.Num() && F.Since(From, EMatchEventType::EffectApplied).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenOnePerGapTest, "Soulbound.Core.Warden.Brain.UnaIntervencionPorHueco",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenOnePerGapTest::RunTest(const FString& Parameters)
{
	// R-WAR-4: como mucho una intervención con efecto por hueco entre turnos. La segunda evaluación del mismo hueco solo puede
	// comentar; empezado el turno siguiente, vuelve a poder.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Strike);
	F.KeepOnly({TEXT("prisa")});
	Soulbound::Rules::EvaluateWarden(F.State, 600.0, F.Random);
	TestTrue(TEXT("El hueco ya tuvo su intervención"), F.State.Warden.bGapUsed);
	TestEqual(TEXT("En el mismo hueco, solo un comentario"), AsInt(F.Evaluate(615.0).Kind), AsInt(EWardenDecisionKind::Remark));

	F.Now = 616.0;
	F.Advance();
	TestNotEqual(TEXT("Empieza el turno siguiente"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestFalse(TEXT("El inicio de turno abre otro hueco"), F.State.Warden.bGapUsed);
	TestEqual(TEXT("Y el golpe vuelve a poder"), AsInt(F.Evaluate(617.0).Kind), AsInt(EWardenDecisionKind::Strike));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenDilemmaCandidateTest, "Soulbound.Core.Warden.Bags.DilemaConUnaSolaOpcionValida",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenDilemmaCandidateTest::RunTest(const FString& Parameters)
{
	// CA-WAR-5 (R-WAR-8, R-DIR-2, R-DIR-3): un dilema con una sola opción válida no es candidato. Elige tu veneno: perder un
	// turno o perder una runa; sin runas, la segunda no vale.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.KeepOnly({TEXT("elige-tu-veneno")});
	const int32 Poison = F.IndexOf(TEXT("elige-tu-veneno"));
	TestFalse(TEXT("Fenrir sin runas: no es candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(Poison));
	F.State.Seats[Fenrir].Runes[0] = 1;
	TestTrue(TEXT("Con una runa, las dos opciones valen: candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(Poison));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenUsesCandidateTest, "Soulbound.Core.Warden.Bags.SinUsosNoEsCandidato",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenUsesCandidateTest::RunTest(const FString& Parameters)
{
	// CA-WAR-6 (R-SAC-1): un evento sin usos no es candidato. Y los que el Warden no puede entregar hasta S-017 (el Oráculo,
	// la Consulta forzada) nunca lo son.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	EveryoneRichAndAway(F);
	const int32 Haste = F.IndexOf(TEXT("prisa"));
	TestTrue(TEXT("Prisa, con usos: candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(Haste));
	F.State.Warden.UsesLeft[Haste] = 0;
	TestFalse(TEXT("Sin usos: no"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(Haste));

	const TArray<int32> Strikes = Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0);
	TestFalse(TEXT("El orbe llama (Consulta forzada): nunca"), Strikes.Contains(F.IndexOf(TEXT("el-orbe-llama"))));
	TestFalse(TEXT("Voluntad ajena (Consulta forzada): nunca"), Strikes.Contains(F.IndexOf(TEXT("voluntad-ajena"))));
	F.State.WardenEvents.Events[F.IndexOf(TEXT("impuesto"))].Delivery = EEventDelivery::Oracle;
	TestFalse(TEXT("Un evento solo por Oráculo: nunca"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(F.IndexOf(TEXT("impuesto"))));
	TestFalse(TEXT("Un evento de prueba, fuera de los sacos: nunca"), Strikes.Contains(F.IndexOf(TEXT("pierdes-un-turno"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenThirdActBagsTest, "Soulbound.Core.Warden.Bags.ActoIIISacaDeLosTresSacos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenThirdActBagsTest::RunTest(const FString& Parameters)
{
	// CA-SAC-2 (R-SAC-1): en el Acto III los candidatos salen de los tres sacos, acumulativos, y se sortean con los pesos del III.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	EveryoneRichAndAway(F);
	TSet<int32> ActsSeen;
	for (const EWardenChannel Channel : {EWardenChannel::Strike, EWardenChannel::Scene})
	{
		for (const int32 Index : Soulbound::Rules::EventCandidates(F.State, Channel, 1700.0))
		{
			ActsSeen.Add(AsInt(F.State.WardenEvents.Events[Index].Act));
		}
	}
	TestEqual(TEXT("Acto III: candidatos de los tres sacos"), ActsSeen.Num(), 3);
	bool bThirdInSecond = false;
	for (const int32 Index : Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 1000.0))
	{
		bThirdInSecond |= F.State.WardenEvents.Events[Index].Act == EAct::ActThree;
	}
	TestFalse(TEXT("Acto II: ninguno del saco del III"), bThirdInSecond);

	// Con los pesos del III: uno sin peso en el III no es candidato aunque lo tenga en el II; entre dos, 3 a 1.
	const int32 OnlySecond = F.AddBagEvent(TEXT("solo-en-el-ii"), EAct::ActTwo, EWardenChannel::Strike, {{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer)}}, true);
	const int32 Heavy = F.AddBagEvent(TEXT("pesado"), EAct::ActTwo, EWardenChannel::Strike, {{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer)}}, true);
	const int32 Light = F.AddBagEvent(TEXT("ligero"), EAct::ActTwo, EWardenChannel::Strike, {{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer)}}, true);
	F.State.WardenEvents.Events[OnlySecond].Weights = {0.0, 1.0, 0.0};
	F.State.WardenEvents.Events[Heavy].Weights = {0.0, 1.0, 3.0};
	F.State.WardenEvents.Events[Light].Weights = {0.0, 1.0, 1.0};
	TestTrue(TEXT("Sin peso en el III, candidato en el II"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 1000.0).Contains(OnlySecond));
	TestFalse(TEXT("Pero no en el III"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 1700.0).Contains(OnlySecond));

	F.KeepOnly({TEXT("solo-en-el-ii"), TEXT("pesado"), TEXT("ligero")});
	F.AlwaysIntervene(EWardenChannel::Strike);
	int32 HeavyCount = 0;
	int32 Total = 0;
	for (int32 Seed = 1; Seed <= 400; ++Seed)
	{
		const FWardenDecision Decision = F.Evaluate(1700.0, Seed);
		HeavyCount += Decision.EventIndex == Heavy ? 1 : 0;
		Total += Decision.EventIndex == Heavy || Decision.EventIndex == Light ? 1 : 0;
		if (Decision.EventIndex == OnlySecond)
		{
			AddError(TEXT("Un evento sin peso en el III sale en el III"));
			break;
		}
	}
	const double Share = Total > 0 ? static_cast<double>(HeavyCount) / Total : 0.0;
	TestTrue(FString::Printf(TEXT("El de peso 3 sale unas tres de cada cuatro veces (%.2f)"), Share), Total == 400 && Share > 0.65 && Share < 0.85);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenWindowTest, "Soulbound.Core.Warden.Bags.VentanaDelEvento",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenWindowTest::RunTest(const FString& Parameters)
{
	// CA-SAC-2 (R-SAC-1, glosario `EventWindow`): un evento con ventana solo es candidato dentro de ella. Pausa, hasta el
	// segundo 150; uno hecho a medida, desde el 1000.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	const int32 Pause = F.IndexOf(TEXT("pausa"));
	TestTrue(TEXT("Pausa en el 100: candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 100.0).Contains(Pause));
	TestTrue(TEXT("Pausa en el 150: candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 150.0).Contains(Pause));
	TestFalse(TEXT("Pausa en el 151: no"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 151.0).Contains(Pause));
	const int32 Late = F.AddBagEvent(TEXT("tardio"), EAct::ActTwo, EWardenChannel::Strike, {{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer)}}, true);
	F.State.WardenEvents.Events[Late].WindowFromSeconds = 1000.0;
	TestFalse(TEXT("Desde el 1000, en el 999: no"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 999.0).Contains(Late));
	TestTrue(TEXT("En el 1000: candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 1000.0).Contains(Late));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenSingleUseTest, "Soulbound.Core.Warden.Bags.UsoUnicoNoRepite",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenSingleUseTest::RunTest(const FString& Parameters)
{
	// CA-SAC-2 (R-SAC-1, glosario `MaxUses`): con usos máximos 1, el evento no repite en la partida. Marcha atrás manda a todos a
	// casa; se lanza una vez, todos vuelven a salir de casa y ya no es candidato.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Scene);
	F.KeepOnly({TEXT("marcha-atras")});
	const int32 Back = F.IndexOf(TEXT("marcha-atras"));
	EveryoneRichAndAway(F);
	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::EvaluateWarden(F.State, 600.0, F.Random);
	TestEqual(TEXT("Marcha atrás se lanza"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 1);
	TestEqual(TEXT("Le quedan 0 usos"), F.State.Warden.UsesLeft[Back], 0);

	EveryoneRichAndAway(F);
	F.State.Warden.bGapUsed = false;
	TestFalse(TEXT("Ya no es candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Scene, 700.0).Contains(Back));
	Soulbound::Rules::EvaluateWarden(F.State, 700.0, F.Random);
	TestEqual(TEXT("Y no se repite"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
