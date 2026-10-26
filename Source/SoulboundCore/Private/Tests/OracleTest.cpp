// Tests del Oráculo del Caos (S-017, GDD 06): la consulta con su plazo y la insolencia (R-ORA-3), el sesgo de la sentencia
// (R-ORA-4), la entrega por el Oráculo y el cerebro con "any" (R-ORA-1), el rechazo de Tifón (R-ORA-5), el Nodo del Oráculo dentro
// del turno y la Consulta forzada entre turnos (R-ORA-2), y la Consulta forzada como castigo leve (R-INS-2). Cada test cita el
// criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro: "Soulbound.Core.Oracle".
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos,
// con los eventos y el Warden de los archivos). La sentencia que interesa se deja como única posible (OnlySentence): el test mira
// la consulta, no el sorteo, salvo en el del sesgo.

#include "Match/EffectRules.h"
#include "Match/InsolenceRules.h"
#include "Match/OracleRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenTriggers.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FWardenFixture;

	constexpr int32 Fenrir = FWardenFixture::Fenrir;
	constexpr int32 Koschei = FWardenFixture::Koschei;
	constexpr int32 EchoA = FWardenFixture::EchoA;

	/** Dominio de Tifón en elegidos.json: su afinidad es rechazar el Oráculo (RefuseOracle). */
	constexpr int32 TifonDomain = 4;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** Seat pasa a ser el Elegido de Tifón, en su hogar (el Nicho de su Dominio): lo que el test necesita para el rechazo. */
	void MakeTifon(FWardenFixture& F, int32 Seat)
	{
		FDomainState& Domain = F.State.Board.Domains[TifonDomain];
		Domain.State = EDomainState::Owned;
		Domain.NicheState = ENicheState::Claimed;
		Domain.OwnerSeat = Seat;
		F.State.Seats[Seat].Role = ESeatRole::Elect;
		F.State.Seats[Seat].DomainId = TifonDomain;
		F.State.Seats[Seat].TileId = Soulbound::Rules::HomeTile(F.State, Seat);
	}

	/** Deja Id como la única sentencia posible: el único evento de los sacos, con entrega "any". */
	void OnlySentence(FWardenFixture& F, const FString& Id)
	{
		F.KeepOnly({Id});
		F.State.WardenEvents.Events[F.IndexOf(Id)].Delivery = EEventDelivery::Any;
	}

	/**
	 * Un Nodo del Oráculo: el tablero de 35 casillas no tiene ninguno en sus datos, así que el primer Nodo de Poder pasa a serlo
	 * (lo que el test necesita, no lo que comprueba). INDEX_NONE si no hay Nodos.
	 */
	int32 FirstOracleNode(FWardenFixture& F)
	{
		FTileState* Node = F.State.Board.Tiles.FindByPredicate([](const FTileState& Tile) { return Tile.Type == ETileType::PowerNode; });
		if (!Node)
		{
			return INDEX_NONE;
		}
		Node->PowerNodeEffect = EPowerNodeEffect::Oracle;
		return Node->TileId;
	}

	/** Abre una consulta forzada para Seat un segundo después del anterior. */
	bool OpenForced(FWardenFixture& F, int32 Seat)
	{
		F.Now += 1.0;
		return Soulbound::Rules::OpenOracle(F.State, Seat, EOracleOrigin::Forced, INDEX_NONE, false, F.Now);
	}

	EIntentResult Consult(FWardenFixture& F, int32 Seat) { return Soulbound::Rules::SubmitOracleConsult(F.State, Seat, F.Now); }

	EIntentResult Refuse(FWardenFixture& F, int32 Seat) { return Soulbound::Rules::SubmitOracleRefuse(F.State, Seat, F.Now); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleConsultTest, "Soulbound.Core.Oracle.ConsultaEnPlazo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleConsultTest::RunTest(const FString& Parameters)
{
	// CA-ORA-1 (R-ORA-3): abierta la consulta, la intención del consultante dentro del plazo revela la sentencia y la aplica, sin
	// insolencia, y gasta un uso del evento. La intención de otro asiento, o la que llega con el plazo agotado, se rechaza sin
	// tocar nada. La sentencia se sortea al abrir, pero no se revela hasta consultar.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	OnlySentence(F, TEXT("prisa"));
	const int32 Haste = F.IndexOf(TEXT("prisa"));
	F.State.Warden.UsesLeft[Haste] = 3;
	const int32 Before = F.State.EventLog.Num();
	if (!TestTrue(TEXT("Se abre la consulta"), OpenForced(F, Koschei)))
	{
		return false;
	}
	TestTrue(TEXT("Espera la consulta"), F.State.Oracle.IsWaiting());
	TestEqual(TEXT("De Koschei"), F.State.Oracle.Consultant, Koschei);
	TestEqual(TEXT("Con la sentencia sorteada"), F.State.Oracle.Sentence, Haste);
	TestEqual(TEXT("Plazo de los datos: 7 s"), F.State.Oracle.DeadlineSeconds, F.Now + 7.0);
	const TArray<FMatchEvent> Opened = F.Since(Before, EMatchEventType::OracleOpened);
	TestTrue(TEXT("OracleOpened de Koschei, forzada"), Opened.Num() == 1 && Opened[0].Seat == Koschei && Opened[0].Value == AsInt(EOracleOrigin::Forced));
	TestEqual(TEXT("La sentencia no se revela al abrir"), F.Since(Before, EMatchEventType::OracleSentence).Num(), 0);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));

	FMatchState Copy = F.State;
	TestEqual(TEXT("La intención de otro asiento se rechaza"), AsInt(Consult(F, Fenrir)), AsInt(EIntentResult::WrongSeat));
	TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));

	F.Now += 2.0;
	TestEqual(TEXT("Koschei consulta dentro del plazo"), AsInt(Consult(F, Koschei)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("OracleConsulted"), F.Since(Before, EMatchEventType::OracleConsulted).Num(), 1);
	const TArray<FMatchEvent> Sentence = F.Since(Before, EMatchEventType::OracleSentence);
	TestTrue(TEXT("OracleSentence: Prisa, primera"), Sentence.Num() == 1 && Sentence[0].Seat == Koschei && Sentence[0].Value == Haste && Sentence[0].Detail == 0);
	TestEqual(TEXT("La sentencia se aplica: un turno extra para Koschei"), F.State.Seats[Koschei].ExtraTurns, 1);
	TestEqual(TEXT("Sin insolencia"), F.State.Seats[Koschei].Insolence, 0);
	TestEqual(TEXT("Gasta un uso"), F.State.Warden.UsesLeft[Haste], 2);
	TestFalse(TEXT("La consulta termina"), F.State.Oracle.IsActive());
	TestFalse(TEXT("Y el evento de la sentencia también"), F.State.CurrentEvent.IsActive());

	// Fuera de plazo: en el límite ya no vale; la aplica AdvanceTurn (siguiente test).
	if (TestTrue(TEXT("Se abre otra"), OpenForced(F, Koschei)))
	{
		F.Now = F.State.Oracle.DeadlineSeconds;
		Copy = F.State;
		TestEqual(TEXT("En el límite del plazo, ClockExpired"), AsInt(Consult(F, Koschei)), AsInt(EIntentResult::ClockExpired));
		TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));
	}
	F.State.Oracle = FOracleState();
	Copy = F.State;
	TestEqual(TEXT("Sin consulta abierta, WrongPhase"), AsInt(Consult(F, Koschei)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleIgnoredTest, "Soulbound.Core.Oracle.PlazoAgotadoEsInsolencia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleIgnoredTest::RunTest(const FString& Parameters)
{
	// CA-ORA-1 (R-ORA-3, R-INS-1): sin intención, al agotarse el plazo la sentencia se revela y se aplica igual, y la insolencia
	// del consultante sube uno con su castigo (S-016), después de la sentencia. Mientras espera, el turno siguiente no empieza.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	OnlySentence(F, TEXT("prisa"));
	const int32 Before = F.State.EventLog.Num();
	if (!TestTrue(TEXT("Se abre la consulta"), OpenForced(F, Koschei)))
	{
		return false;
	}
	F.Now += 3.0;
	F.Advance();
	TestEqual(TEXT("Dentro del plazo, el turno siguiente no empieza"), F.Since(Before, EMatchEventType::TurnStarted).Num(), 0);
	TestTrue(TEXT("Y la consulta sigue abierta"), F.State.Oracle.IsWaiting());

	F.Now = F.State.Oracle.DeadlineSeconds;
	F.Advance();
	TestEqual(TEXT("OracleIgnored de Koschei"), F.SeatsSince(Before, EMatchEventType::OracleIgnored), TArray<int32>{Koschei});
	TestEqual(TEXT("La sentencia se revela igual"), F.Since(Before, EMatchEventType::OracleSentence).Num(), 1);
	TestEqual(TEXT("Y se aplica igual"), F.State.Seats[Koschei].ExtraTurns, 1);
	TestEqual(TEXT("La insolencia sube uno"), F.State.Seats[Koschei].Insolence, 1);
	int32 SentenceAt = INDEX_NONE;
	int32 PunishedAt = INDEX_NONE;
	for (int32 Index = Before; Index < F.State.EventLog.Num(); ++Index)
	{
		const EMatchEventType Type = F.State.EventLog[Index].Type;
		SentenceAt = Type == EMatchEventType::OracleSentence && SentenceAt == INDEX_NONE ? Index : SentenceAt;
		PunishedAt = Type == EMatchEventType::InsolencePunished && PunishedAt == INDEX_NONE ? Index : PunishedAt;
	}
	TestTrue(TEXT("Con su castigo, después de la sentencia"), SentenceAt != INDEX_NONE && PunishedAt > SentenceAt);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleBiasTest, "Soulbound.Core.Oracle.SesgoDeLaSentencia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleBiasTest::RunTest(const FString& Parameters)
{
	// CA-ORA-2 (R-ORA-4): tres decretos con pesos 1, 2 y 7 en el Acto II. Con sesgo 0, uniforme; con sesgo 1, los pesos del
	// cerebro, con los disparadores (Líder claro multiplica por 3 el primero); con 0,5, la media. Las probabilidades, exactas; los
	// sorteos, 6000 por caso con semilla fija y tolerancia de 0,025. Sin candidatos, "nada": OracleEmpty, sin uso ni insolencia.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	const double Weights[] = {1.0, 2.0, 7.0};
	TArray<int32> Events;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 Event = F.AddBagEvent(FString::Printf(TEXT("sentencia-%d"), Index), EAct::ActTwo, EWardenChannel::Strike,
			{{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 1)}}, true);
		F.State.WardenEvents.Events[Event].Delivery = EEventDelivery::Any;
		F.State.WardenEvents.Events[Event].Weights[AsInt(EAct::ActTwo)] = Weights[Index];
		Events.Add(Event);
	}
	F.KeepOnly({TEXT("sentencia-0"), TEXT("sentencia-1"), TEXT("sentencia-2")});
	F.Now = 400.0;

	auto Check = [&](double Bias, const TArray<double>& Expected, const TArray<FActiveTrigger>& Triggers, const TCHAR* Context)
	{
		F.State.WardenParams.OracleBias = Bias;
		const TArray<TPair<int32, double>> Probabilities = Soulbound::Rules::OracleSentenceProbabilities(F.State, Koschei, F.Now, Triggers);
		if (!TestEqual(FString::Printf(TEXT("%s: tres candidatos"), Context), Probabilities.Num(), 3))
		{
			return;
		}
		int32 Counts[3] = {};
		FRandomStream Random(31);
		constexpr int32 Draws = 6000;
		for (int32 Draw = 0; Draw < Draws; ++Draw)
		{
			const int32 Index = Events.Find(Soulbound::Rules::DrawOracleSentence(F.State, Koschei, F.Now, Triggers, Random));
			if (Index != INDEX_NONE)
			{
				++Counts[Index];
			}
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TestEqual(FString::Printf(TEXT("%s: el candidato %d en su orden"), Context, Index), Probabilities[Index].Key, Events[Index]);
			TestTrue(FString::Printf(TEXT("%s: probabilidad del %d, %.4f (esperada %.4f)"), Context, Index, Probabilities[Index].Value, Expected[Index]),
				FMath::IsNearlyEqual(Probabilities[Index].Value, Expected[Index], 1e-9));
			const double Frequency = Counts[Index] / static_cast<double>(Draws);
			TestTrue(FString::Printf(TEXT("%s: frecuencia del %d, %.3f (esperada %.3f)"), Context, Index, Frequency, Expected[Index]),
				FMath::Abs(Frequency - Expected[Index]) < 0.025);
		}
	};
	Check(0.0, {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}, {}, TEXT("Sesgo 0"));
	Check(1.0, {0.1, 0.2, 0.7}, {}, TEXT("Sesgo 1"));
	Check(0.5, {0.5 / 3.0 + 0.05, 0.5 / 3.0 + 0.1, 0.5 / 3.0 + 0.35}, {}, TEXT("Sesgo 0,5"));

	// Con disparadores: el primero, en la tabla de Líder claro por 3; Fenrir con dos tipos de runa y los demás sin ninguna.
	FTriggerTable* Leader = F.State.WardenParams.Triggers.Tables.FindByPredicate([](const FTriggerTable& Table) { return Table.Trigger == EWardenTrigger::ClearLeader; });
	if (!TestNotNull(TEXT("warden.json tiene la tabla de Líder claro"), Leader))
	{
		return false;
	}
	FTriggerEventEntry& Entry = Leader->Events.AddDefaulted_GetRef();
	Entry.EventId = TEXT("sentencia-0");
	Entry.EventIndex = Events[0];
	Entry.Multiplier = 3.0;
	F.State.Seats[Fenrir].Runes[0] = 1;
	F.State.Seats[Fenrir].Runes[1] = 1;
	const TArray<FActiveTrigger> Triggers = Soulbound::Rules::ActiveTriggers(F.State, F.Now);
	TestTrue(TEXT("Líder claro activo, por Fenrir"), Triggers.Contains(FActiveTrigger{EWardenTrigger::ClearLeader, Fenrir}));
	Check(1.0, {0.25, 2.0 / 12.0, 7.0 / 12.0}, Triggers, TEXT("Sesgo 1 con Líder claro"));

	// Sin candidatos: "nada".
	F.KeepOnly({});
	const TArray<int32> UsesBefore = F.State.Warden.UsesLeft;
	const int32 Before = F.State.EventLog.Num();
	TestFalse(TEXT("Sin candidatos no queda consulta abierta"), OpenForced(F, Koschei));
	TestEqual(TEXT("OracleOpened"), F.Since(Before, EMatchEventType::OracleOpened).Num(), 1);
	TestEqual(TEXT("OracleEmpty de Koschei"), F.SeatsSince(Before, EMatchEventType::OracleEmpty), TArray<int32>{Koschei});
	TestFalse(TEXT("Nada que esperar"), F.State.Oracle.IsActive());
	TestTrue(TEXT("Sin uso"), F.State.Warden.UsesLeft == UsesBefore);
	TestEqual(TEXT("Sin insolencia"), F.State.Seats[Koschei].Insolence, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleDeliveryTest, "Soulbound.Core.Oracle.ElCerebroEligeLaEntrega",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleDeliveryTest::RunTest(const FString& Parameters)
{
	// CA-ORA-3 (R-ORA-1): un golpe con "any" puede entregarse por el Oráculo; el cerebro elige entre directo y Oráculo con su sorteo;
	// con "oracle", siempre por el Oráculo; con "direct", nunca. 60 evaluaciones con semillas distintas.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes.Init(1, Soulbound::Board::DomainCount);
	F.KeepOnly({TEXT("ojos-en-ti")});
	F.AlwaysIntervene(EWardenChannel::Strike);
	const int32 Eyes = F.IndexOf(TEXT("ojos-en-ti"));
	F.Director(Koschei);
	auto Count = [&](EEventDelivery Delivery, int32& OutOracle, int32& OutDirect)
	{
		F.State.WardenEvents.Events[Eyes].Delivery = Delivery;
		OutOracle = 0;
		OutDirect = 0;
		for (int32 Seed = 1; Seed <= 60; ++Seed)
		{
			F.State.Warden.LastEffectSeconds = 390.0;
			const FWardenDecision Decision = F.Evaluate(400.0, Seed);
			if (Decision.Kind == EWardenDecisionKind::Strike && Decision.EventIndex == Eyes)
			{
				(Decision.bViaOracle ? OutOracle : OutDirect) += 1;
			}
		}
	};
	int32 Oracle = 0;
	int32 Direct = 0;
	Count(EEventDelivery::Any, Oracle, Direct);
	TestEqual(TEXT("\"any\": siempre el golpe"), Oracle + Direct, 60);
	TestTrue(FString::Printf(TEXT("\"any\": por el Oráculo y directo, a partes iguales (%d y %d)"), Oracle, Direct), Oracle >= 15 && Direct >= 15);
	Count(EEventDelivery::Oracle, Oracle, Direct);
	TestEqual(TEXT("\"oracle\": candidato, siempre por el Oráculo"), Oracle, 60);
	Count(EEventDelivery::Direct, Oracle, Direct);
	TestEqual(TEXT("\"direct\": nunca por el Oráculo"), Direct, 60);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleStrikeTest, "Soulbound.Core.Oracle.GolpePorOraculoParaElCortado",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleStrikeTest::RunTest(const FString& Parameters)
{
	// R-ORA-1, R-ORA-2 (decisión del autor del 23/10/2026): un golpe por el Oráculo corta el turno como cualquier golpe, y la
	// sentencia es el decreto que eligió el cerebro, para el cortado. Cuenta para la garantía y ocupa el hueco al decidirse; el
	// evento se aplica al consultar, dirigido por el cortado.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes.Init(1, Soulbound::Board::DomainCount);
	const int32 Eyes = F.IndexOf(TEXT("ojos-en-ti"));
	F.BeginTurn(Koschei);
	FWardenDecision Decision = F.MakeDecision(EWardenDecisionKind::Strike, TEXT("ojos-en-ti"));
	Decision.bViaOracle = true;
	const int32 Before = F.State.EventLog.Num();
	F.Apply(Decision);
	TestEqual(TEXT("El golpe corta el turno"), F.SeatsSince(Before, EMatchEventType::TurnInterrupted), TArray<int32>{Koschei});
	const TArray<FMatchEvent> Opened = F.Since(Before, EMatchEventType::OracleOpened);
	TestTrue(TEXT("Y abre la consulta para el cortado, por golpe"), Opened.Num() == 1 && Opened[0].Seat == Koschei && Opened[0].Value == AsInt(EOracleOrigin::Strike));
	TestEqual(TEXT("La sentencia es el decreto del cerebro"), F.State.Oracle.Sentence, Eyes);
	TestEqual(TEXT("Cuenta para la garantía"), F.State.Warden.LastEffectSeconds, F.Now);
	TestTrue(TEXT("Y ocupa el hueco"), F.State.Warden.bGapUsed);
	TestEqual(TEXT("El evento aún no empieza"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 0);

	F.Now += 1.0;
	TestEqual(TEXT("Koschei consulta"), AsInt(Consult(F, Koschei)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El evento lo dirige el cortado"), F.SeatsSince(Before, EMatchEventType::WardenEventStarted), TArray<int32>{Koschei});
	TestEqual(TEXT("Y su Brazal queda revelado"), F.SeatsSince(Before, EMatchEventType::BracerRevealed), TArray<int32>{Koschei});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleTyphonTest, "Soulbound.Core.Oracle.TifonRechazaUnaVez",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleTyphonTest::RunTest(const FString& Parameters)
{
	// CA-ORA-4 (R-ORA-5): Tifón rechaza dentro del plazo una sentencia llegada por Nodo, por Consulta forzada y por golpe; en los
	// tres casos se sortea otra, que se aplica en el acto, sin consulta y sin rechazo; rechazar no es insolencia. Otro Elegido no
	// puede rechazar: InvalidOption, sin tocar nada.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	MakeTifon(F, Koschei);
	OnlySentence(F, TEXT("prisa"));
	const int32 Haste = F.IndexOf(TEXT("prisa"));
	auto CheckRefusal = [&](int32 Before, const TCHAR* Context)
	{
		if (!TestTrue(FString::Printf(TEXT("%s: la consulta espera, rechazable"), Context), F.State.Oracle.IsWaiting() && F.State.Oracle.bRefusable))
		{
			return;
		}
		const int32 ExtraBefore = F.State.Seats[Koschei].ExtraTurns;
		F.Now += 1.0;
		TestEqual(FString::Printf(TEXT("%s: Tifón rechaza"), Context), AsInt(Refuse(F, Koschei)), AsInt(EIntentResult::Accepted));
		const TArray<FMatchEvent> Refused = F.Since(Before, EMatchEventType::OracleRefused);
		TestTrue(FString::Printf(TEXT("%s: OracleRefused de la primera"), Context), Refused.Num() == 1 && Refused[0].Seat == Koschei && Refused[0].Value == Haste);
		const TArray<FMatchEvent> Sentences = F.Since(Before, EMatchEventType::OracleSentence);
		TestTrue(FString::Printf(TEXT("%s: una sola sentencia revelada, la segunda"), Context), Sentences.Num() == 1 && Sentences[0].Detail == 1);
		TestEqual(FString::Printf(TEXT("%s: la segunda se aplica en el acto"), Context), F.State.Seats[Koschei].ExtraTurns, ExtraBefore + 1);
		TestFalse(FString::Printf(TEXT("%s: sin otra consulta"), Context), F.State.Oracle.IsWaiting());
		TestEqual(FString::Printf(TEXT("%s: la segunda no se rechaza"), Context), AsInt(Refuse(F, Koschei)), AsInt(EIntentResult::WrongPhase));
		TestEqual(FString::Printf(TEXT("%s: rechazar no es insolencia"), Context), F.State.Seats[Koschei].Insolence, 0);
		TestEqual(FString::Printf(TEXT("%s: ni desatención"), Context), F.Since(Before, EMatchEventType::OracleIgnored).Num(), 0);
	};

	// Por Consulta forzada.
	int32 Before = F.State.EventLog.Num();
	OpenForced(F, Koschei);
	CheckRefusal(Before, TEXT("Forzada"));

	// Por golpe, cortando su turno.
	F.BeginTurn(Koschei);
	FWardenDecision Decision = F.MakeDecision(EWardenDecisionKind::Strike, TEXT("prisa"));
	Decision.bViaOracle = true;
	Before = F.State.EventLog.Num();
	F.Apply(Decision);
	CheckRefusal(Before, TEXT("Golpe"));

	// Por Nodo, dentro de su turno: tras el rechazo, el turno sigue.
	const int32 Node = FirstOracleNode(F);
	if (TestTrue(TEXT("El tablero tiene un Nodo del Oráculo"), Node != INDEX_NONE))
	{
		F.Director(Koschei);
		Before = F.State.EventLog.Num();
		F.Land(Koschei, Node);
		CheckRefusal(Before, TEXT("Nodo"));
		TestNotEqual(TEXT("Nodo: el turno ya no espera en la consulta"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::OracleConsult));
	}

	// Otro Elegido no puede rechazar.
	F.Director(Koschei);
	if (TestTrue(TEXT("Se abre la consulta de Fenrir"), OpenForced(F, Fenrir)))
	{
		const FMatchState Copy = F.State;
		TestEqual(TEXT("Fenrir no rechaza"), AsInt(Refuse(F, Fenrir)), AsInt(EIntentResult::InvalidOption));
		TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));
		TestEqual(TEXT("Fenrir consulta"), AsInt(Consult(F, Fenrir)), AsInt(EIntentResult::Accepted));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleNodeTest, "Soulbound.Core.Oracle.NodoDentroDelTurno",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleNodeTest::RunTest(const FString& Parameters)
{
	// CA-ORA-5 (R-ORA-2, R-INT-1): acabar en un Nodo del Oráculo tras el Despertar abre la consulta en la fase OracleConsult,
	// después de la casilla y antes de la acción; resuelta sin salir del tablero, el turno sigue (la acción contra un jugador);
	// resuelta con Vuelta al hogar o con el Vacío, el turno termina.
	struct FCase
	{
		const TCHAR* Sentence;
		bool bLeavesBoard;
	};
	for (const FCase& Case : {FCase{TEXT("prisa"), false}, FCase{TEXT("al-altar"), true}, FCase{TEXT("destierro"), true}})
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		const int32 Node = FirstOracleNode(F);
		if (!TestTrue(TEXT("El tablero tiene un Nodo del Oráculo"), Node != INDEX_NONE))
		{
			return false;
		}
		OnlySentence(F, Case.Sentence);
		// Alguien con quien compartir casilla y el poder de expulsar: así hay una acción que ofrecer después.
		F.State.Seats[EchoA].TileId = Node;
		F.GivePower(Fenrir, EPower::Expel);
		const int32 Before = F.State.EventLog.Num();
		F.Land(Fenrir, Node);
		const TArray<FMatchEvent> Opened = F.Since(Before, EMatchEventType::OracleOpened);
		if (!TestTrue(FString::Printf(TEXT("%s: OracleOpened de Fenrir, por Nodo"), Case.Sentence),
			Opened.Num() == 1 && Opened[0].Seat == Fenrir && Opened[0].Value == AsInt(EOracleOrigin::Node)))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s: en la fase de consulta"), Case.Sentence), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::OracleConsult));
		TestFalse(FString::Printf(TEXT("%s: antes de la acción"), Case.Sentence), F.State.PendingChoice.IsOpen());
		TestTrue(FString::Printf(TEXT("%s: dentro del turno"), Case.Sentence), F.State.Oracle.bInTurn);
		F.Now += 1.0;
		TestEqual(FString::Printf(TEXT("%s: Fenrir consulta"), Case.Sentence), AsInt(Consult(F, Fenrir)), AsInt(EIntentResult::Accepted));
		F.Advance();
		if (Case.bLeavesBoard)
		{
			TestFalse(FString::Printf(TEXT("%s: sin acción"), Case.Sentence), F.State.PendingChoice.IsOpen());
			TestEqual(FString::Printf(TEXT("%s: el turno termina"), Case.Sentence), F.SeatsSince(Before, EMatchEventType::TurnEnded), TArray<int32>{Fenrir});
			TestNotEqual(FString::Printf(TEXT("%s: Fenrir dejó la casilla"), Case.Sentence), F.State.Seats[Fenrir].TileId, Node);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s: el turno sigue con la acción"), Case.Sentence), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
			TestEqual(FString::Printf(TEXT("%s: sin terminar"), Case.Sentence), F.Since(Before, EMatchEventType::TurnEnded).Num(), 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleNodeBeforeAwakeningTest, "Soulbound.Core.Oracle.NodoAntesDelDespertar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleNodeBeforeAwakeningTest::RunTest(const FString& Parameters)
{
	// CA-ORA-5 (R-ORA-2, R-TAB-8): antes del Despertar, el Nodo del Oráculo es una casilla normal: no abre consulta.
	FWardenFixture F;
	if (!F.StartInRace(*this))
	{
		return false;
	}
	const int32 Node = FirstOracleNode(F);
	if (!TestTrue(TEXT("El tablero tiene un Nodo del Oráculo"), Node != INDEX_NONE))
	{
		return false;
	}
	const int32 Before = F.State.EventLog.Num();
	TestTrue(TEXT("El turno termina"), F.Land(EchoA, Node));
	TestEqual(TEXT("Tablero Primordial"), AsInt(F.State.Board.Phase), AsInt(EBoardPhase::Primordial));
	TestEqual(TEXT("Sin consulta"), F.Since(Before, EMatchEventType::OracleOpened).Num(), 0);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));

	// Despierto el tablero con el primer vínculo, la misma llegada sí abre la consulta, aunque la carrera siga.
	OnlySentence(F, TEXT("prisa"));
	F.Bond(*this, Fenrir, FWardenFixture::FenrirDomain);
	const int32 After = F.State.EventLog.Num();
	F.Land(EchoA, Node);
	TestEqual(TEXT("Tras el Despertar, la consulta se abre"), F.SeatsSince(After, EMatchEventType::OracleOpened), TArray<int32>{EchoA});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleForcedRivalTest, "Soulbound.Core.Oracle.ConsultaForzadaSobreRival",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleForcedRivalTest::RunTest(const FString& Parameters)
{
	// CA-ORA-5 (R-ORA-2, R-EFE-3): Consulta forzada sobre un rival señalado (Voluntad ajena) → la consulta se abre entre turnos
	// para ese rival y el siguiente turno espera a que se resuelva.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	OnlySentence(F, TEXT("prisa"));
	const int32 Before = F.State.EventLog.Num();
	if (!TestEqual(TEXT("Voluntad ajena empieza"), AsInt(F.Launch(TEXT("voluntad-ajena"))), AsInt(EWardenEventStart::Started)))
	{
		return false;
	}
	TestEqual(TEXT("Fenrir señala"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	TestEqual(TEXT("Señala al Eco A"), AsInt(F.Answer(EchoA)), AsInt(EIntentResult::Accepted));
	F.Advance();
	TestEqual(TEXT("La consulta se abre para el Eco A"), F.SeatsSince(Before, EMatchEventType::OracleOpened), TArray<int32>{EchoA});
	TestTrue(TEXT("Espera"), F.State.Oracle.IsWaiting() && F.State.Oracle.Consultant == EchoA);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	F.Now += 1.0;
	F.Advance();
	TestEqual(TEXT("El siguiente turno espera"), F.Since(Before, EMatchEventType::TurnStarted).Num(), 0);
	TestEqual(TEXT("El Eco A consulta"), AsInt(Consult(F, EchoA)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("La sentencia es para él"), F.State.Seats[EchoA].ExtraTurns, 1);
	F.Advance();
	TestEqual(TEXT("Resuelta, el turno siguiente empieza"), F.Since(Before, EMatchEventType::TurnStarted).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOracleInsolenceTest, "Soulbound.Core.Oracle.CastigoLevePuedeSerConsultaForzada",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOracleInsolenceTest::RunTest(const FString& Parameters)
{
	// CA-INS-1b (R-INS-2): la Consulta forzada entra en los castigos leves; un castigo leve sobre quien no tiene runas (Revelar
	// Brazal no vale y el Alto nunca castiga) es la Consulta forzada, que abre la consulta para el insolente.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	OnlySentence(F, TEXT("prisa"));
	TestTrue(TEXT("La Consulta forzada es leve"), Soulbound::Rules::InsolencePunishmentEffects(EEffectSeverity::Light).Contains(EEffect::ForcedOracle));
	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::PunishInsolence(F.State, Koschei, FPendingPunishment{1, EInsolencePunishment::Light}, F.Now);
	const TArray<FMatchEvent> Applied = F.Since(Before, EMatchEventType::EffectApplied);
	TestTrue(TEXT("El castigo es la Consulta forzada sobre Koschei"), Applied.Num() == 1 && Applied[0].Seat == Koschei && Applied[0].Value == AsInt(EEffect::ForcedOracle));
	F.Advance();
	TestEqual(TEXT("Y abre la consulta para él"), F.SeatsSince(Before, EMatchEventType::OracleOpened), TArray<int32>{Koschei});
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
