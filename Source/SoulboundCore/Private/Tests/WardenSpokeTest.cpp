// Tests del registro de la voz del Warden (S-021, GDD 06): cada frase deja WardenSpoke justo después del evento que la causa
// (CA-VOZ-3, R-WAR-10) y los avisos del final (CA-PRO-5, R-PRO-5). Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre.
// Filtro: "Soulbound.Core.Voice". Montaje: VoiceFixture.h (la partida del Warden de WardenFixture.h con las frases de frases.json).

#include "Match/BondRaceRules.h"
#include "Match/DebugEdits.h"
#include "Match/InsolenceRules.h"
#include "Match/MatchStart.h"
#include "Match/OracleRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenVoice.h"
#include "VoiceFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::FVoiceFixture;
	using Soulbound::Tests::IndexOfEvent;
	using Soulbound::Tests::IsFollowedBySpoke;
	using Soulbound::Tests::SpokenKey;

	constexpr int32 Fenrir = FVoiceFixture::Fenrir;
	constexpr int32 Koschei = FVoiceFixture::Koschei;
	constexpr int32 EchoA = FVoiceFixture::EchoA;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** Cuántos WardenSpoke hay desde From. */
	int32 SpokenSince(const FMatchState& State, int32 From)
	{
		int32 Count = 0;
		for (int32 Index = From; Index < State.EventLog.Num(); ++Index)
		{
			Count += State.EventLog[Index].Type == EMatchEventType::WardenSpoke ? 1 : 0;
		}
		return Count;
	}

	/** La variante que dijo el WardenSpoke es una de las de su entrada. */
	bool HasValidVariant(const FMatchState& State, const FMatchEvent& Event)
	{
		return State.WardenLines.Entries.IsValidIndex(Event.Value) && State.WardenLines.Entries[Event.Value].Lines.IsValidIndex(Event.Detail);
	}

	/** Seat es aspirante: en el Altar con los seis tipos de runa. */
	void MakeAspirant(FVoiceFixture& F, int32 Seat)
	{
		F.State.Seats[Seat].TileId = F.State.Board.FindAltar();
		F.State.Seats[Seat].Runes = {1, 1, 1, 1, 1, 1};
	}

	/** Una gravedad de la tabla como única posible, para que el castigo sea el que el test mira. */
	FString InsolenceKeyOf(EInsolencePunishment Severity)
	{
		return Soulbound::Rules::InsolenceLineKey(Severity);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceStartTest, "Soulbound.Core.Voice.RegistroAlEmpezar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceStartTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-3 (R-PRO-1, R-WAR-10): StartMatch deja WardenSpoke con programado:inicio, una programado:corrupcion por Dominio corrompido
	// (6 menos 4 = 2) y una programado:orden-n por asiento, en el orden de turno, cada una para su asiento. Sin frases, no deja ninguna.
	FBoardState Board;
	FMatchParams Params;
	FElectsTable Elects;
	FWardenEventTable Events;
	FWardenParams WardenParams;
	FWardenLineTable Lines;
	if (!Soulbound::Tests::LoadBoardFile(*this, Soulbound::Tests::RaceBoardFileName(), Board) || !Soulbound::Tests::LoadTestMatchParams(*this, Params)
		|| !Soulbound::Tests::LoadTestElects(*this, Elects) || !Soulbound::Tests::LoadTestWardenEvents(*this, Events)
		|| !Soulbound::Tests::LoadTestWardenParams(*this, WardenParams) || !Soulbound::Tests::LoadTestWardenLines(*this, Lines))
	{
		return false;
	}
	FRandomStream Random(42);
	const FMatchStartResult Start = Soulbound::Rules::StartMatch(Board, Params, Elects, Soulbound::Match::MaxPlayers, Random, Events, WardenParams, Lines);
	if (!TestTrue(TEXT("La partida empieza"), Start.IsSuccess()))
	{
		return false;
	}
	const FMatchState& State = Start.State;
	TestEqual(TEXT("La voz tiene la semilla de la partida más 5"), State.Warden.VoiceSeed, 42 + 5);
	TArray<FMatchEvent> Spoken = State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::WardenSpoke; });
	if (!TestEqual(TEXT("Una de inicio, dos de corrupción y cuatro del orden"), Spoken.Num(), 1 + 2 + 4))
	{
		return false;
	}
	TestEqual(TEXT("Primero, el inicio, a la mesa"), SpokenKey(State, Spoken[0]), Soulbound::Warden::LineStart);
	TestEqual(TEXT("A la mesa"), Spoken[0].Seat, INDEX_NONE);
	TestEqual(TEXT("Corrupción 1"), SpokenKey(State, Spoken[1]), Soulbound::Warden::LineCorruption);
	TestEqual(TEXT("Corrupción 2"), SpokenKey(State, Spoken[2]), Soulbound::Warden::LineCorruption);
	for (int32 Position = 0; Position < Soulbound::Warden::OrderLineCount; ++Position)
	{
		const FMatchEvent& Order = Spoken[3 + Position];
		TestEqual(FString::Printf(TEXT("Posición %d del orden"), Position + 1), SpokenKey(State, Order), Soulbound::Rules::ScheduledOrderKey(Position + 1));
		TestEqual(FString::Printf(TEXT("La posición %d habla al asiento que la ocupa"), Position + 1), Order.Seat, State.TurnOrder[Position]);
	}
	for (const FMatchEvent& Event : Spoken)
	{
		TestTrue(TEXT("Con una variante que existe"), HasValidVariant(State, Event));
		TestEqual(TEXT("En el instante 0"), Event.ClockSeconds, 0.0);
	}

	FRandomStream Same(42);
	const FMatchStartResult Silent = Soulbound::Rules::StartMatch(Board, Params, Elects, Soulbound::Match::MaxPlayers, Same, Events, WardenParams);
	TestEqual(TEXT("Sin frases, ningún WardenSpoke"), SpokenSince(Silent.State, 0), 0);
	TestEqual(TEXT("Y la misma partida: mismo orden de turno"), Silent.State.TurnOrder, State.TurnOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceStrikeTest, "Soulbound.Core.Voice.RegistroDelGolpe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceStrikeTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-3 (R-WAR-10, R-SAC-3): un evento lanzado por el Warden directo deja WardenSpoke con la clave de su evento justo después de
	// WardenEventStarted y antes de aplicarse su efecto, a la mesa; el mismo evento por el Oráculo, tras OracleSentence y sus
	// disparadores, una sola vez (no otra al lanzarse el evento).
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		TestEqual(TEXT("Se lanza"), AsInt(F.Launch(TEXT("prisa"))), AsInt(EWardenEventStart::Started));
		const int32 Started = IndexOfEvent(F.State, EMatchEventType::WardenEventStarted, From);
		TestTrue(TEXT("Con WardenEventStarted"), Started != INDEX_NONE);
		TestTrue(TEXT("Justo después, la frase del evento"), IsFollowedBySpoke(F.State, Started, TEXT("evento:prisa")));
		TestEqual(TEXT("Una sola"), SpokenSince(F.State, From), 1);
		TestEqual(TEXT("A la mesa"), F.State.EventLog[Started + 1].Seat, INDEX_NONE);
		TestTrue(TEXT("Con una variante que existe"), HasValidVariant(F.State, F.State.EventLog[Started + 1]));
		TestTrue(TEXT("Antes de aplicarse el efecto"), IndexOfEvent(F.State, EMatchEventType::EffectApplied, From) > Started + 1);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.KeepOnly({TEXT("prisa")});
		F.State.WardenEvents.Events[F.IndexOf(TEXT("prisa"))].Delivery = EEventDelivery::Any;
		const int32 From = F.State.EventLog.Num();
		F.Now += 1.0;
		if (!TestTrue(TEXT("Se abre la consulta"), Soulbound::Rules::OpenOracle(F.State, Koschei, EOracleOrigin::Forced, INDEX_NONE, false, F.Now)))
		{
			return false;
		}
		TestEqual(TEXT("Abrirla no habla"), SpokenSince(F.State, From), 0);
		F.Now += 1.0;
		TestEqual(TEXT("Consulta"), AsInt(Soulbound::Rules::SubmitOracleConsult(F.State, Koschei, F.Now)), AsInt(EIntentResult::Accepted));
		const int32 Sentence = IndexOfEvent(F.State, EMatchEventType::OracleSentence, From);
		const int32 Started = IndexOfEvent(F.State, EMatchEventType::WardenEventStarted, From);
		TestTrue(TEXT("Con la sentencia"), Sentence != INDEX_NONE && Started != INDEX_NONE);
		int32 Spoke = Sentence + 1;
		while (F.State.EventLog.IsValidIndex(Spoke) && F.State.EventLog[Spoke].Type == EMatchEventType::WardenTriggered)
		{
			++Spoke;
		}
		TestTrue(TEXT("Tras la sentencia (y sus disparadores), la frase del evento"), F.State.EventLog.IsValidIndex(Spoke) && SpokenKey(F.State, F.State.EventLog[Spoke]) == TEXT("evento:prisa"));
		TestTrue(TEXT("Antes de que el evento se lance"), Spoke < Started);
		TestEqual(TEXT("Una sola"), SpokenSince(F.State, From), 1);
		TestEqual(TEXT("A la mesa"), F.State.EventLog[Spoke].Seat, INDEX_NONE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceInsolenceTest, "Soulbound.Core.Voice.RegistroDeLaInsolencia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceInsolenceTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-3 (R-INS-2, R-INS-3, R-WAR-10): un castigo de insolencia deja WardenSpoke con su gravedad justo después de InsolencePunished,
	// para el castigado; la Furia, con "furia" justo después de FuryUnleashed.
	const TArray<EInsolencePunishment> Severities = {EInsolencePunishment::Light, EInsolencePunishment::Medium, EInsolencePunishment::Severe};
	const TArray<FString> Expected = {TEXT("insolencia:leve"), TEXT("insolencia:media"), TEXT("insolencia:grave")};
	for (int32 Index = 0; Index < Severities.Num(); ++Index)
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("La clave de la gravedad %d"), Index), InsolenceKeyOf(Severities[Index]), Expected[Index]);
		const int32 From = F.State.EventLog.Num();
		Soulbound::Rules::PunishInsolence(F.State, Koschei, FPendingPunishment{1, Severities[Index]}, F.Now);
		const int32 Punished = IndexOfEvent(F.State, EMatchEventType::InsolencePunished, From);
		TestTrue(FString::Printf(TEXT("Gravedad %d: justo después del castigo, su frase"), Index), Punished != INDEX_NONE && IsFollowedBySpoke(F.State, Punished, Expected[Index]));
		TestEqual(TEXT("Para el castigado"), F.State.EventLog[Punished + 1].Seat, Koschei);
		TestEqual(TEXT("Una sola"), SpokenSince(F.State, From), 1);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		TestEqual(TEXT("La clave de la Furia"), InsolenceKeyOf(EInsolencePunishment::Fury), Soulbound::Warden::LineFury);
		const int32 From = F.State.EventLog.Num();
		Soulbound::Rules::PunishInsolence(F.State, Koschei, FPendingPunishment{5, EInsolencePunishment::Fury}, F.Now);
		const int32 Fury = IndexOfEvent(F.State, EMatchEventType::FuryUnleashed, From);
		TestTrue(TEXT("Justo después de la Furia, la frase furia"), Fury != INDEX_NONE && IsFollowedBySpoke(F.State, Fury, Soulbound::Warden::LineFury));
		TestEqual(TEXT("Para el castigado"), F.State.EventLog[Fury + 1].Seat, Koschei);
		TestEqual(TEXT("Y es la única frase"), SpokenSince(F.State, From), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceRaceTest, "Soulbound.Core.Voice.RegistroDeLaCarrera",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceRaceTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-3 (R-PRO-2, R-PRO-3, R-WAR-10): un vínculo deja la presentación de ese Elegido justo después de Bonded, para el vinculado;
	// el fin de la carrera deja programado:fin-carrera justo después de BondRaceEnded, a la mesa; si nadie se vinculó, la victoria del
	// Warden deja programado:victoria justo después de WardenVictory.
	{
		FVoiceFixture F;
		if (!F.StartInRace(*this) || !Soulbound::Tests::LoadTestWardenLines(*this, F.Lines))
		{
			return false;
		}
		F.State.WardenLines = F.Lines;
		F.State.Warden.VoiceSeed = 5;
		const int32 From = F.State.EventLog.Num();
		if (!F.Bond(*this, Fenrir, FVoiceFixture::FenrirDomain))
		{
			return false;
		}
		const int32 Bonded = IndexOfEvent(F.State, EMatchEventType::Bonded, From);
		TestTrue(TEXT("Justo después del vínculo, la presentación de su Elegido"),
			Bonded != INDEX_NONE && IsFollowedBySpoke(F.State, Bonded, Soulbound::Rules::IntroductionKey(FVoiceFixture::FenrirDomain)));
		TestEqual(TEXT("Para el vinculado"), F.State.EventLog[Bonded + 1].Seat, Fenrir);
		TestEqual(TEXT("Una sola"), SpokenSince(F.State, From), 1);

		const int32 BeforeEnd = F.State.EventLog.Num();
		F.Now += 1.0;
		TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now));
		const int32 Ended = IndexOfEvent(F.State, EMatchEventType::BondRaceEnded, BeforeEnd);
		TestTrue(TEXT("Justo después, programado:fin-carrera"), Ended != INDEX_NONE && IsFollowedBySpoke(F.State, Ended, Soulbound::Warden::LineRaceEnd));
		TestEqual(TEXT("A la mesa"), F.State.EventLog[Ended + 1].Seat, INDEX_NONE);
		TestEqual(TEXT("Y es la única"), SpokenSince(F.State, BeforeEnd), 1);
	}
	{
		FVoiceFixture F;
		if (!F.StartInRace(*this) || !Soulbound::Tests::LoadTestWardenLines(*this, F.Lines))
		{
			return false;
		}
		F.State.WardenLines = F.Lines;
		F.State.Warden.VoiceSeed = 5;
		const int32 From = F.State.EventLog.Num();
		F.Now += 1.0;
		TestTrue(TEXT("La carrera termina sin vínculos"), Soulbound::Rules::EndBondRace(F.State, F.Now));
		TestEqual(TEXT("Gana el Warden"), AsInt(F.State.Outcome), AsInt(EMatchOutcome::WardenVictory));
		const int32 Victory = IndexOfEvent(F.State, EMatchEventType::WardenVictory, From);
		TestTrue(TEXT("Justo después de la victoria, programado:victoria"), Victory != INDEX_NONE && IsFollowedBySpoke(F.State, Victory, Soulbound::Warden::LineVictory));
		const int32 Ended = IndexOfEvent(F.State, EMatchEventType::BondRaceEnded, From);
		TestTrue(TEXT("Y tras el fin de la carrera, programado:fin-carrera"), Ended != INDEX_NONE && IsFollowedBySpoke(F.State, Ended, Soulbound::Warden::LineRaceEnd));
		TestEqual(TEXT("Dos frases"), SpokenSince(F.State, From), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceEndingTest, "Soulbound.Core.Voice.RegistroDelFinal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceEndingTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-3 (R-PRO-4, R-PRO-5, R-WAR-10): el aspirante deja programado:aspirante justo después de AspirantAnnounced, para él; el fin por
	// tiempo, programado:fin-tiempo justo después de WardenVictory; la victoria de un jugador, programado:victoria justo después de MatchWon.
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		MakeAspirant(F, Fenrir);
		const int32 From = F.State.EventLog.Num();
		F.Now += 1.0;
		F.Advance();
		const int32 Announced = IndexOfEvent(F.State, EMatchEventType::AspirantAnnounced, From);
		TestTrue(TEXT("Justo después del anuncio, programado:aspirante"), Announced != INDEX_NONE && IsFollowedBySpoke(F.State, Announced, Soulbound::Warden::LineAspirant));
		TestEqual(TEXT("Para el aspirante"), F.State.EventLog[Announced + 1].Seat, Fenrir);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.Now = F.State.WardenParams.MatchEndSeconds;
		const int32 From = F.State.EventLog.Num();
		F.Advance();
		const int32 Victory = IndexOfEvent(F.State, EMatchEventType::WardenVictory, From);
		TestTrue(TEXT("Justo después de la victoria por tiempo, programado:fin-tiempo"), Victory != INDEX_NONE && IsFollowedBySpoke(F.State, Victory, Soulbound::Warden::LineTimeOut));
		TestEqual(TEXT("A la mesa"), F.State.EventLog[Victory + 1].Seat, INDEX_NONE);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		MakeAspirant(F, Fenrir);
		F.BeginTurn(Fenrir);
		const int32 From = F.State.EventLog.Num();
		F.Now += 2.0;
		TestEqual(TEXT("El resultado se acepta"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, Fenrir, true, F.Now)), AsInt(EIntentResult::Accepted));
		const int32 Won = IndexOfEvent(F.State, EMatchEventType::MatchWon, From);
		TestTrue(TEXT("Justo después de la victoria del jugador, programado:victoria"), Won != INDEX_NONE && IsFollowedBySpoke(F.State, Won, Soulbound::Warden::LineVictory));
		TestEqual(TEXT("Y la frase es lo último de la partida"), F.State.EventLog.Last().Type, EMatchEventType::WardenSpoke);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceSayTest, "Soulbound.Core.Voice.EdicionSbSay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceSayTest::RunTest(const FString& Parameters)
{
	// R-VOZ-5 (S-021, criterio manual de SbSay): SayLine dice una frase de la tabla sin esperar al suceso: deja el evento de la edición y,
	// justo después, el WardenSpoke de esa clave a la mesa. Una clave que no está en la tabla se rechaza sin cambiar nada, como la tabla vacía.
	FVoiceFixture F;
	if (!F.StartWithVoice(*this))
	{
		return false;
	}
	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("Se dice"), AsInt(Soulbound::Debug::SayLine(F.State, TEXT("evento:tropiezo"), F.Now)), AsInt(EDebugEditResult::Applied));
	if (TestEqual(TEXT("Dos eventos: la edición y la frase"), F.State.EventLog.Num() - From, 2))
	{
		TestEqual(TEXT("Primero, la edición"), AsInt(F.State.EventLog[From].Type), AsInt(EMatchEventType::DebugEdit));
		TestEqual(TEXT("De este tipo"), F.State.EventLog[From].Value, AsInt(EDebugEditKind::Say));
		TestEqual(TEXT("Justo después, la frase pedida"), SpokenKey(F.State, F.State.EventLog[From + 1]), FString(TEXT("evento:tropiezo")));
		TestEqual(TEXT("A la mesa"), F.State.EventLog[From + 1].Seat, INDEX_NONE);
	}
	const FMatchState Before = F.State;
	TestEqual(TEXT("Una clave que no existe"), AsInt(Soulbound::Debug::SayLine(F.State, TEXT("evento:no-existe"), F.Now)), AsInt(EDebugEditResult::UnknownLine));
	TestTrue(TEXT("Sin cambiar nada"), Soulbound::Tests::SameState(Before, F.State));
	F.State.WardenLines = FWardenLineTable();
	TestEqual(TEXT("Sin tabla"), AsInt(Soulbound::Debug::SayLine(F.State, TEXT("evento:tropiezo"), F.Now)), AsInt(EDebugEditResult::UnknownLine));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceWarningsTest, "Soulbound.Core.Voice.AvisosDelFinal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceWarningsTest::RunTest(const FString& Parameters)
{
	// CA-PRO-5 (R-PRO-5): con matchEndSeconds 2400, a los 2340 s MatchEndWarning con 60 y su frase; a los 2390, con 10; una sola vez cada
	// uno, aunque AdvanceTurn se llame varias veces; no se dispara antes; con el fin de partida antes (victoria de un jugador), ninguno;
	// y nunca después del fin. Dependen del estado, no de que AdvanceTurn pase justo en ese segundo.
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		TestEqual(TEXT("El fin de la partida está en los datos: 2400 s"), F.State.WardenParams.MatchEndSeconds, 2400.0);
		const int32 From = F.State.EventLog.Num();
		F.Now = 2339.0;
		F.Advance();
		TestEqual(TEXT("A los 2339 s, ninguno"), F.Since(From, EMatchEventType::MatchEndWarning).Num(), 0);
		F.Now = 2340.0;
		F.Advance();
		TArray<FMatchEvent> Warnings = F.Since(From, EMatchEventType::MatchEndWarning);
		if (!TestEqual(TEXT("A los 2340 s, el de 60"), Warnings.Num(), 1))
		{
			return false;
		}
		TestEqual(TEXT("Con los segundos que quedan"), Warnings[0].Value, 60);
		TestEqual(TEXT("En su instante"), Warnings[0].ClockSeconds, 2340.0);
		const int32 First = IndexOfEvent(F.State, EMatchEventType::MatchEndWarning, From);
		TestTrue(TEXT("Con su frase justo después"), IsFollowedBySpoke(F.State, First, Soulbound::Warden::LineLastMinute));
		TestEqual(TEXT("A la mesa"), F.State.EventLog[First + 1].Seat, INDEX_NONE);
		TestFalse(TEXT("No corta la partida"), F.State.IsMatchOver());
		for (const double Later : {2341.0, 2350.0, 2389.0})
		{
			F.Now = Later;
			F.Advance();
		}
		TestEqual(TEXT("Una sola vez"), F.Since(From, EMatchEventType::MatchEndWarning).Num(), 1);
		F.Now = 2390.0;
		F.Advance();
		Warnings = F.Since(From, EMatchEventType::MatchEndWarning);
		if (!TestEqual(TEXT("A los 2390 s, el de 10"), Warnings.Num(), 2))
		{
			return false;
		}
		TestEqual(TEXT("Con 10"), Warnings[1].Value, 10);
		const int32 Second = IndexOfEvent(F.State, EMatchEventType::MatchEndWarning, First + 1);
		TestTrue(TEXT("Con su frase justo después"), IsFollowedBySpoke(F.State, Second, Soulbound::Warden::LineTenSeconds));
		F.Now = 2395.0;
		F.Advance();
		TestEqual(TEXT("Cada uno una sola vez"), F.Since(From, EMatchEventType::MatchEndWarning).Num(), 2);
		TestFalse(TEXT("Y la partida sigue"), F.State.IsMatchOver());
		F.Now = 2400.0;
		F.Advance();
		TestTrue(TEXT("A los 2400 s termina por tiempo"), F.State.IsMatchOver());
		TestEqual(TEXT("Sin un tercer aviso"), F.Since(From, EMatchEventType::MatchEndWarning).Num(), 2);
	}
	{
		// Un salto del reloj: el primer AdvanceTurn llega ya pasados los 2340 s, y avisa de lo que no avisó (en orden), pero no tras el fin.
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		F.Now = 2395.0;
		F.Advance();
		const TArray<FMatchEvent> Warnings = F.Since(From, EMatchEventType::MatchEndWarning);
		TestTrue(TEXT("Llegando tarde, avisa de 60 y de 10, en ese orden"), Warnings.Num() == 2 && Warnings[0].Value == 60 && Warnings[1].Value == 10);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		F.Now = 2450.0;
		F.Advance();
		TestTrue(TEXT("Si el primer AdvanceTurn llega después del fin, termina sin avisos"), F.State.IsMatchOver() && F.Since(From, EMatchEventType::MatchEndWarning).Num() == 0);
	}
	{
		// Con la partida acabada antes por la victoria de un jugador, no se dispara ninguno.
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		MakeAspirant(F, Fenrir);
		F.BeginTurn(Fenrir);
		F.Now += 2.0;
		Soulbound::Rules::SubmitSkillTrialResult(F.State, Fenrir, true, F.Now);
		TestTrue(TEXT("Gana Fenrir"), F.State.IsMatchOver() && F.State.Outcome == EMatchOutcome::PlayerVictory);
		const int32 From = F.State.EventLog.Num();
		F.Now = 2395.0;
		F.Advance();
		TestEqual(TEXT("Con el fin antes, ningún aviso"), F.Since(From, EMatchEventType::MatchEndWarning).Num(), 0);
	}
	{
		// Sin Warden en la partida no hay fin por tiempo ni, por tanto, avisos.
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.State.WardenParams = FWardenParams();
		const int32 From = F.State.EventLog.Num();
		F.Now = 2395.0;
		F.Advance();
		TestEqual(TEXT("Sin Warden, ningún aviso"), F.Since(From, EMatchEventType::MatchEndWarning).Num(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
