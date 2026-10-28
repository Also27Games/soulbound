// Tests de los comentarios del Warden (S-021, GDD 06: R-COM-1, R-COM-2), corte A2: los cinco tipos que elige la evaluación, los cinco que
// dispara el suceso y los dos parámetros nuevos de warden.json. Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre.
// Filtro: "Soulbound.Core.Voice". Montaje: VoiceFixture.h (la partida del Warden con las frases de frases.json).

#include "HAL/PlatformFileManager.h"
#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
#include "Match/InsolenceRules.h"
#include "Match/ShellGameRules.h"
#include "Match/TurnRules.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Warden/WardenRemarks.h"
#include "Warden/WardenTriggers.h"
#include "Warden/WardenVoice.h"
#include "VoiceFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::FVoiceFixture;
	using Soulbound::Tests::IndexOfEvent;
	using Soulbound::Tests::SpokenKey;

	constexpr int32 Fenrir = FVoiceFixture::Fenrir;
	constexpr int32 Koschei = FVoiceFixture::Koschei;
	constexpr int32 EchoA = FVoiceFixture::EchoA;
	constexpr int32 EchoB = FVoiceFixture::EchoB;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** El candidato del tipo Kind, o nullptr si no es elegible. */
	const FRemarkCandidate* Find(const TArray<FRemarkCandidate>& Candidates, EWardenRemarkKind Kind)
	{
		return Candidates.FindByPredicate([Kind](const FRemarkCandidate& Candidate) { return Candidate.Kind == Kind; });
	}

	/** Si Kind es elegible para Seat. */
	bool Eligible(const FMatchState& State, double Now, EWardenRemarkKind Kind, int32 Seat)
	{
		const FRemarkCandidate* Candidate = Find(Soulbound::Rules::ApplicableRemarks(State, Now), Kind);
		return Candidate && Candidate->Seat == Seat;
	}

	/** Los tipos de los WardenRemark desde From. */
	TArray<FMatchEvent> RemarksSince(const FMatchState& State, int32 From)
	{
		TArray<FMatchEvent> Remarks;
		for (int32 Index = From; Index < State.EventLog.Num(); ++Index)
		{
			if (State.EventLog[Index].Type == EMatchEventType::WardenRemark)
			{
				Remarks.Add(State.EventLog[Index]);
			}
		}
		return Remarks;
	}

	/**
	 * Un comentario del suceso: desde From hay exactamente un WardenRemark, del tipo y del asiento dados, y justo después su WardenSpoke con la
	 * clave del tipo; no corta el turno ni gasta la garantía.
	 */
	void ExpectRemark(FAutomationTestBase& Test, const FMatchState& State, int32 From, EWardenRemarkKind Kind, int32 Seat, const TCHAR* Context)
	{
		const TArray<FMatchEvent> Remarks = RemarksSince(State, From);
		if (!Test.TestEqual(FString::Printf(TEXT("%s: un solo comentario"), Context), Remarks.Num(), 1))
		{
			return;
		}
		Test.TestEqual(FString::Printf(TEXT("%s: del tipo"), Context), Remarks[0].Value, AsInt(Kind));
		Test.TestEqual(FString::Printf(TEXT("%s: del asiento"), Context), Remarks[0].Seat, Seat);
		const int32 Index = IndexOfEvent(State, EMatchEventType::WardenRemark, From);
		Test.TestEqual(FString::Printf(TEXT("%s: justo después, su frase"), Context), State.EventLog.IsValidIndex(Index + 1) ? SpokenKey(State, State.EventLog[Index + 1]) : FString(),
			Soulbound::Rules::WardenRemarkKey(Kind));
		Test.TestEqual(FString::Printf(TEXT("%s: la frase, para el mismo asiento"), Context), State.EventLog.IsValidIndex(Index + 1) ? State.EventLog[Index + 1].Seat : -2, Seat);
		Test.TestEqual(FString::Printf(TEXT("%s: no corta nada"), Context), IndexOfEvent(State, EMatchEventType::TurnInterrupted, From), INDEX_NONE);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceRemarkParamsTest, "Soulbound.Core.Voice.ParametrosDeLosComentarios",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceRemarkParamsTest::RunTest(const FString& Parameters)
{
	// R-COM-1 (S-021, decisión del autor del 28/10/2026): hurryFraction (valor inicial 0,5) y cutLaughWindowSeconds (valor inicial 20) están en
	// warden.json, no fijos en el código; el cargador exige los dos y rechaza una fracción fuera de (0, 1] y una ventana que no es positiva.
	FWardenParams Params;
	if (!Soulbound::Tests::LoadTestWardenParams(*this, Params))
	{
		return false;
	}
	TestEqual(TEXT("hurryFraction, 0,5"), Params.HurryFraction, 0.5);
	TestEqual(TEXT("cutLaughWindowSeconds, 20"), Params.CutLaughWindowSeconds, 20.0);

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Soulbound::Tests::WardenParamsPath()))
	{
		return false;
	}
	const auto With = [&Json](const TCHAR* Old, const TCHAR* New)
	{
		return Json.Replace(Old, New);
	};
	TestTrue(TEXT("El archivo tiene los dos campos con ese formato"), Json.Contains(TEXT("\"hurryFraction\": 0.5")) && Json.Contains(TEXT("\"cutLaughWindowSeconds\": 20")));
	TestEqual(TEXT("Una fracción de 0"), static_cast<int32>(Soulbound::Rules::LoadWardenParamsFromJsonString(With(TEXT("\"hurryFraction\": 0.5"), TEXT("\"hurryFraction\": 0"))).Error),
		static_cast<int32>(EWardenParamsError::ProbabilityOutOfRange));
	TestEqual(TEXT("Una fracción de 1,5"), static_cast<int32>(Soulbound::Rules::LoadWardenParamsFromJsonString(With(TEXT("\"hurryFraction\": 0.5"), TEXT("\"hurryFraction\": 1.5"))).Error),
		static_cast<int32>(EWardenParamsError::ProbabilityOutOfRange));
	TestTrue(TEXT("Una fracción de 1 vale"), Soulbound::Rules::LoadWardenParamsFromJsonString(With(TEXT("\"hurryFraction\": 0.5"), TEXT("\"hurryFraction\": 1"))).IsSuccess());
	TestEqual(TEXT("Una ventana de 0"), static_cast<int32>(Soulbound::Rules::LoadWardenParamsFromJsonString(With(TEXT("\"cutLaughWindowSeconds\": 20"), TEXT("\"cutLaughWindowSeconds\": 0"))).Error),
		static_cast<int32>(EWardenParamsError::NonPositiveDuration));
	TestEqual(TEXT("Sin la fracción"), static_cast<int32>(Soulbound::Rules::LoadWardenParamsFromJsonString(With(TEXT("\"hurryFraction\": 0.5,"), TEXT(""))).Error),
		static_cast<int32>(EWardenParamsError::MalformedData));
	TestEqual(TEXT("Sin la ventana"), static_cast<int32>(Soulbound::Rules::LoadWardenParamsFromJsonString(With(TEXT("\"cutLaughWindowSeconds\": 20,"), TEXT(""))).Error),
		static_cast<int32>(EWardenParamsError::MalformedData));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceEvaluatedRemarksTest, "Soulbound.Core.Voice.ComentariosPorEvaluacion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceEvaluatedRemarksTest::RunTest(const FString& Parameters)
{
	// CA-COM-1 (R-COM-1), por evaluación: con el disparador Acaparador activo, Hoarder es elegible; con un asiento con cinco tipos,
	// FiveRunes; con el activo más allá de la fracción de su reloj sin intención, Hurry; si hubo un corte en la ventana, CutLaugh; sin nada de
	// eso, MockLast al asiento con menos runas (los duplicados cuentan) y, si empatan, al último del orden de turno de los empatados.
	FVoiceFixture F;
	if (!F.StartWithVoice(*this))
	{
		return false;
	}
	F.Now = 100.0;
	const TArray<int32>& Order = F.State.TurnOrder;
	const auto LastInOrder = [&Order](const TArray<int32>& Seats)
	{
		int32 Last = INDEX_NONE;
		for (const int32 Seat : Order)
		{
			Last = Seats.Contains(Seat) ? Seat : Last;
		}
		return Last;
	};

	// Sin nada: solo MockLast, y con los cuatro empatados a cero, al último del orden de turno.
	TArray<FRemarkCandidate> Candidates = Soulbound::Rules::ApplicableRemarks(F.State, F.Now);
	TestTrue(TEXT("Sin nada, solo MockLast"), Candidates.Num() == 1 && Candidates[0].Kind == EWardenRemarkKind::MockLast);
	TestEqual(TEXT("Empatados a cero, el último del orden"), Candidates.Num() == 1 ? Candidates[0].Seat : INDEX_NONE, Order.Last());

	// El de menos runas, contando duplicados; el empate se resuelve entre los empatados.
	F.State.Seats[Fenrir].Runes = {3, 0, 0, 0, 0, 0};
	F.State.Seats[Koschei].Runes = {0, 1, 0, 0, 0, 0};
	F.State.Seats[EchoA].Runes = {1, 1, 0, 0, 0, 0};
	TestTrue(TEXT("El de menos runas es el 3"), Eligible(F.State, F.Now, EWardenRemarkKind::MockLast, EchoB));
	F.State.Seats[EchoB].Runes = {2, 0, 0, 0, 0, 0};
	TestTrue(TEXT("Con duplicados: Koschei tiene 1, el menos"), Eligible(F.State, F.Now, EWardenRemarkKind::MockLast, Koschei));
	F.State.Seats[EchoA].Runes = {0, 1, 0, 0, 0, 0};
	TestTrue(TEXT("Empatan Koschei y el Eco A a 1: el último del orden de los dos"), Eligible(F.State, F.Now, EWardenRemarkKind::MockLast, LastInOrder({Koschei, EchoA})));
	for (FSeatState& Seat : F.State.Seats)
	{
		Seat.Runes.Init(0, Soulbound::Board::DomainCount);
	}

	// Cinco tipos.
	F.State.Seats[EchoA].Runes = {1, 1, 1, 1, 1, 0};
	TestTrue(TEXT("Cinco tipos: FiveRunes del asiento"), Eligible(F.State, F.Now, EWardenRemarkKind::FiveRunes, EchoA));
	TestTrue(TEXT("Y MockLast sigue siendo elegible"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::MockLast) != nullptr);
	F.State.Seats[EchoA].Runes = {2, 2, 2, 2, 0, 0};
	TestTrue(TEXT("Cuatro tipos con duplicados no son cinco"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::FiveRunes) == nullptr);
	F.State.Seats[EchoA].Runes.Init(0, Soulbound::Board::DomainCount);

	// Acaparador.
	F.State.Seats[Koschei].Runes[0] = F.State.WardenParams.Triggers.HoarderCount;
	bool bHoarderTrigger = false;
	for (const FActiveTrigger& Trigger : Soulbound::Rules::ActiveTriggers(F.State, F.Now))
	{
		bHoarderTrigger |= Trigger.Trigger == EWardenTrigger::Hoarder && Trigger.Seat == Koschei;
	}
	TestTrue(TEXT("El disparador Acaparador está activo"), bHoarderTrigger);
	TestTrue(TEXT("Hoarder es elegible, del acaparador"), Eligible(F.State, F.Now, EWardenRemarkKind::Hoarder, Koschei));
	F.State.Seats[Koschei].Runes.Init(0, Soulbound::Board::DomainCount);
	TestTrue(TEXT("Sin él, no"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::Hoarder) == nullptr);

	// Prisa: el activo, en RollAndChoose o con una elección suya abierta, más allá de la fracción del reloj de turno.
	const double Clock = F.State.Params.TurnClockSeconds;
	const double Fraction = F.State.WardenParams.HurryFraction;
	F.State.ActiveSeat = Fenrir;
	F.State.TurnPhase = ETurnPhase::RollAndChoose;
	F.State.TurnDeadlineSeconds = F.Now + Clock * (1.0 - (Fraction + 0.1));
	TestTrue(TEXT("Pasada la fracción del reloj en RollAndChoose: Hurry del activo"), Eligible(F.State, F.Now, EWardenRemarkKind::Hurry, Fenrir));
	F.State.TurnDeadlineSeconds = F.Now + Clock * (1.0 - (Fraction - 0.1));
	TestTrue(TEXT("Sin llegar a la fracción: no"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::Hurry) == nullptr);
	F.State.TurnDeadlineSeconds = F.Now + Clock * (1.0 - (Fraction + 0.1));
	F.State.TurnPhase = ETurnPhase::BetweenTurns;
	TestTrue(TEXT("Entre turnos, no"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::Hurry) == nullptr);
	F.State.TurnPhase = ETurnPhase::MoveAndResolve;
	TestTrue(TEXT("En otra fase sin elección, no"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::Hurry) == nullptr);
	F.State.PendingChoice.Kind = EChoiceKind::PlanarJump;
	F.State.PendingChoice.Seat = Fenrir;
	F.State.PendingChoice.Options = {1};
	TestTrue(TEXT("Con una elección suya abierta: Hurry"), Eligible(F.State, F.Now, EWardenRemarkKind::Hurry, Fenrir));
	F.State.PendingChoice.Seat = Koschei;
	TestTrue(TEXT("Con la elección de otro, no"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::Hurry) == nullptr);
	F.State.PendingChoice = FPendingChoice();
	F.State.TurnPhase = ETurnPhase::BetweenTurns;

	// Risa al cortar: un TurnInterrupted dentro de la ventana; el más reciente.
	const double Window = F.State.WardenParams.CutLaughWindowSeconds;
	FMatchEvent Cut;
	Cut.Type = EMatchEventType::TurnInterrupted;
	Cut.Seat = Koschei;
	Cut.ClockSeconds = F.Now - (Window + 10.0);
	F.State.EventLog.Add(Cut);
	TestTrue(TEXT("Un corte fuera de la ventana, no"), Find(Soulbound::Rules::ApplicableRemarks(F.State, F.Now), EWardenRemarkKind::CutLaugh) == nullptr);
	F.State.EventLog.Last().ClockSeconds = F.Now - (Window - 5.0);
	TestTrue(TEXT("Un corte dentro de la ventana: CutLaugh del cortado"), Eligible(F.State, F.Now, EWardenRemarkKind::CutLaugh, Koschei));
	Cut.Seat = EchoA;
	Cut.ClockSeconds = F.Now - 1.0;
	F.State.EventLog.Add(Cut);
	TestTrue(TEXT("Con dos, el más reciente"), Eligible(F.State, F.Now, EWardenRemarkKind::CutLaugh, EchoA));

	// Sin Warden en la partida, ninguno.
	F.State.WardenParams = FWardenParams();
	TestTrue(TEXT("Sin Warden, ningún comentario"), Soulbound::Rules::ApplicableRemarks(F.State, F.Now).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceRemarkDecisionTest, "Soulbound.Core.Voice.ComentarioDeLaDecision",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceRemarkDecisionTest::RunTest(const FString& Parameters)
{
	// CA-COM-1 (R-COM-1) y CA-VOZ-4: cuando el cerebro decide comentar, ApplyWardenDecision deja WardenRemark con el tipo (Value) y el
	// destinatario (Seat) y justo después su WardenSpoke; no corta el turno, no gasta la garantía, no toca el sorteo del Warden. El tipo sale
	// del sorteo de la voz entre los elegibles, con la misma probabilidad: con tres elegibles salen los tres; misma semilla, mismo tipo.
	FVoiceFixture F;
	if (!F.StartWithVoice(*this))
	{
		return false;
	}
	F.Now = 100.0;
	F.State.Seats[Koschei].Runes = {1, 1, 1, 1, 1, 0};
	F.State.ActiveSeat = EchoA;
	F.State.TurnPhase = ETurnPhase::RollAndChoose;
	F.State.TurnDeadlineSeconds = F.Now + F.State.Params.TurnClockSeconds * (1.0 - (F.State.WardenParams.HurryFraction + 0.2));
	const TArray<FRemarkCandidate> Candidates = Soulbound::Rules::ApplicableRemarks(F.State, F.Now);
	if (!TestEqual(TEXT("Tres elegibles: MockLast, FiveRunes y Hurry"), Candidates.Num(), 3))
	{
		return false;
	}

	TMap<int32, int32> Seen;
	for (int32 Seed = 1; Seed <= 90; ++Seed)
	{
		FVoiceFixture Copy = F;
		Copy.State.Warden.VoiceSeed = Seed;
		const FMatchState Before = Copy.State;
		const int32 From = Copy.State.EventLog.Num();
		FWardenDecision Decision;
		Decision.Kind = EWardenDecisionKind::Remark;
		Soulbound::Rules::ApplyWardenDecision(Copy.State, Decision, Copy.Now, Copy.Random);
		const TArray<FMatchEvent> Remarks = RemarksSince(Copy.State, From);
		if (!TestEqual(FString::Printf(TEXT("Semilla %d: un comentario"), Seed), Remarks.Num(), 1))
		{
			continue;
		}
		const FRemarkCandidate* Chosen = Find(Candidates, static_cast<EWardenRemarkKind>(Remarks[0].Value));
		TestTrue(FString::Printf(TEXT("Semilla %d: uno de los elegibles, para su asiento"), Seed), Chosen && Chosen->Seat == Remarks[0].Seat);
		Seen.FindOrAdd(Remarks[0].Value)++;
		const int32 Index = IndexOfEvent(Copy.State, EMatchEventType::WardenRemark, From);
		TestTrue(FString::Printf(TEXT("Semilla %d: justo después, su frase"), Seed),
			Copy.State.EventLog.IsValidIndex(Index + 1) && SpokenKey(Copy.State, Copy.State.EventLog[Index + 1]) == Soulbound::Rules::WardenRemarkKey(static_cast<EWardenRemarkKind>(Remarks[0].Value)));
		TestEqual(FString::Printf(TEXT("Semilla %d: ni corta"), Seed), IndexOfEvent(Copy.State, EMatchEventType::TurnInterrupted, From), INDEX_NONE);
		TestEqual(FString::Printf(TEXT("Semilla %d: ni toca la garantía"), Seed), Copy.State.Warden.LastEffectSeconds, Before.Warden.LastEffectSeconds);
		TestEqual(FString::Printf(TEXT("Semilla %d: ni el hueco"), Seed), Copy.State.Warden.bGapUsed, Before.Warden.bGapUsed);
		TestEqual(FString::Printf(TEXT("Semilla %d: ni el turno"), Seed), AsInt(Copy.State.TurnPhase), AsInt(Before.TurnPhase));
		TestEqual(FString::Printf(TEXT("Semilla %d: ni el sorteo del Warden"), Seed), Copy.State.Warden.Draws, Before.Warden.Draws);

		// Misma semilla, mismo comentario.
		FVoiceFixture Again = F;
		Again.State.Warden.VoiceSeed = Seed;
		Soulbound::Rules::ApplyWardenDecision(Again.State, Decision, Again.Now, Again.Random);
		const TArray<FMatchEvent> Repeated = RemarksSince(Again.State, From);
		TestTrue(FString::Printf(TEXT("Semilla %d: determinista"), Seed), Repeated.Num() == 1 && Repeated[0].Value == Remarks[0].Value && Repeated[0].Seat == Remarks[0].Seat);
	}
	TestEqual(TEXT("Salen los tres tipos"), Seen.Num(), 3);
	for (const TPair<int32, int32>& Count : Seen)
	{
		TestTrue(FString::Printf(TEXT("El tipo %d sale en una proporción razonable (%d de 90)"), Count.Key, Count.Value), Count.Value >= 15 && Count.Value <= 45);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceEventRemarksTest, "Soulbound.Core.Voice.ComentariosPorSuceso",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceEventRemarksTest::RunTest(const FString& Parameters)
{
	// CA-COM-1 (R-COM-2), por suceso: retar a quien no tiene runas → Beggar (al retador); retador sin runas que pierde → Fool (al perdedor);
	// trilero agotado sin elegir → Slow (a quien elegía); regalo perdido por no elegir (R-DIR-4) → Ungrateful (a quien lo ignoró); FuryRefusal →
	// NotThisTime (al de la Furia). Cada uno deja su WardenRemark con el tipo y su WardenSpoke, sin cortar el turno ni gastar la garantía; se
	// registran con o sin frases (decisión del autor del 28/10/2026).
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		int32 From = F.State.EventLog.Num();
		TestEqual(TEXT("Retar a quien no tiene runas"), AsInt(Soulbound::Rules::BeginDuel(F.State, Fenrir, Koschei, F.Now)), AsInt(EDuelStart::NoRunes));
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::Beggar, Fenrir, TEXT("Reto"));
		TestTrue(TEXT("Reto: tras DuelNoRunes"), IndexOfEvent(F.State, EMatchEventType::DuelNoRunes, From) < IndexOfEvent(F.State, EMatchEventType::WardenRemark, From));
		From = F.State.EventLog.Num();
		TestEqual(TEXT("Un Duelo forzado contra quien no tiene runas"), AsInt(Soulbound::Rules::BeginForcedDuel(F.State, EchoA, Koschei, false, F.Now)), AsInt(EDuelStart::NoRunes));
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::Beggar, EchoA, TEXT("Duelo forzado"));
	}
	{
		// El mismo reto sin frases: el comentario se registra igual, sin WardenSpoke.
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.State.WardenLines = FWardenLineTable();
		const int32 From = F.State.EventLog.Num();
		Soulbound::Rules::BeginDuel(F.State, Fenrir, Koschei, F.Now);
		const TArray<FMatchEvent> Remarks = RemarksSince(F.State, From);
		TestTrue(TEXT("Sin frases: el WardenRemark se registra"), Remarks.Num() == 1 && Remarks[0].Value == AsInt(EWardenRemarkKind::Beggar) && Remarks[0].Seat == Fenrir);
		TestEqual(TEXT("Sin frases: ningún WardenSpoke"), IndexOfEvent(F.State, EMatchEventType::WardenSpoke, From), INDEX_NONE);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		// Fool: el Eco A, sin runas, reta desde un Nodo de Duelo a Fenrir (que tiene) y pierde.
		F.State.Seats[Fenrir].Runes = {2, 0, 0, 0, 0, 0};
		F.State.Duel.Challenger = EchoA;
		F.State.Duel.Challenged = Fenrir;
		F.State.Duel.bFromDuelNode = true;
		Soulbound::Rules::RecordDuelPerformance(F.State, EchoA, 0.1);
		Soulbound::Rules::RecordDuelPerformance(F.State, Fenrir, 0.9);
		int32 From = F.State.EventLog.Num();
		const FDuelResult Result = Soulbound::Rules::ResolveDuel(F.State, F.Now);
		TestTrue(TEXT("El retador sin runas pierde y va a su hogar"), Result.Loser == EchoA && Result.bLoserSentHome);
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::Fool, EchoA, TEXT("Pardillo"));
		TestTrue(TEXT("Pardillo: tras DuelLoserSentHome"), IndexOfEvent(F.State, EMatchEventType::DuelLoserSentHome, From) < IndexOfEvent(F.State, EMatchEventType::WardenRemark, From));

		// Si el retador pierde con runas, no es un pardillo.
		FVoiceFixture WithRunes;
		if (!WithRunes.StartWithVoice(*this))
		{
			return false;
		}
		WithRunes.State.Seats[Fenrir].Runes = {2, 0, 0, 0, 0, 0};
		WithRunes.State.Seats[EchoA].Runes = {0, 1, 0, 0, 0, 0};
		WithRunes.State.Duel.Challenger = EchoA;
		WithRunes.State.Duel.Challenged = Fenrir;
		Soulbound::Rules::RecordDuelPerformance(WithRunes.State, EchoA, 0.1);
		Soulbound::Rules::RecordDuelPerformance(WithRunes.State, Fenrir, 0.9);
		From = WithRunes.State.EventLog.Num();
		Soulbound::Rules::ResolveDuel(WithRunes.State, WithRunes.Now);
		TestEqual(TEXT("Perder con runas no comenta"), RemarksSince(WithRunes.State, From).Num(), 0);
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.State.ShellGame.Thief = Fenrir;
		F.State.ShellGame.Victim = Koschei;
		const int32 From = F.State.EventLog.Num();
		Soulbound::Rules::ExpireShellPick(F.State, F.Now);
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::Slow, Fenrir, TEXT("Lento"));
		TestTrue(TEXT("Lento: tras ShellGameExpired"), IndexOfEvent(F.State, EMatchEventType::ShellGameExpired, From) < IndexOfEvent(F.State, EMatchEventType::WardenRemark, From));
	}
	{
		// Desagradecido: solo el regalo perdido (R-DIR-4). La variante de un castigo propio que se desatiende no comenta.
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		int32 From = F.State.EventLog.Num();
		F.Decree(EchoA, Soulbound::Tests::App(EEffect::GiftRune, EEffectTarget::ActivePlayer));
		F.Expire();
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::Ungrateful, EchoA, TEXT("Regalo ignorado"));
		TestTrue(TEXT("Desagradecido: tras la burla"), IndexOfEvent(F.State, EMatchEventType::WardenTaunt, From) < IndexOfEvent(F.State, EMatchEventType::WardenRemark, From));

		F.State.Seats[EchoA].Runes[1] = 1;
		F.State.Seats[EchoA].Runes[4] = 1;
		From = F.State.EventLog.Num();
		F.Decree(EchoA, Soulbound::Tests::App(EEffect::LoseRune, EEffectTarget::ActivePlayer, 1, EEffectVariant::Chosen));
		F.Expire();
		TestEqual(TEXT("Desatender el castigo propio no comenta"), RemarksSince(F.State, From).Num(), 0);
	}
	{
		// Esta vez no: la Liberación y el Desvío no valen en el Vacío de la Furia.
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.State.Seats[Fenrir].Insolence = 4;
		Soulbound::Rules::RaiseInsolence(F.State, Fenrir, F.Now);
		F.MoveAway(Koschei);
		Soulbound::Rules::Banish(F.State, Koschei, false, F.Now);
		const int32 From = F.State.EventLog.Num();
		Soulbound::Rules::ReleaseVoid(F.State, F.Now);
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::NotThisTime, Fenrir, TEXT("Liberación"));
		TestTrue(TEXT("Esta vez no: tras FuryRefusal"), IndexOfEvent(F.State, EMatchEventType::FuryRefusal, From) < IndexOfEvent(F.State, EMatchEventType::WardenRemark, From));
	}
	{
		FVoiceFixture F;
		if (!F.StartWithVoice(*this))
		{
			return false;
		}
		F.GivePower(Fenrir, EPower::Deflection);
		F.State.Seats[Fenrir].Insolence = 4;
		Soulbound::Rules::RaiseInsolence(F.State, Fenrir, F.Now);
		F.BeginTurn(Fenrir);
		const int32 From = F.State.EventLog.Num();
		F.Answer(1);
		ExpectRemark(*this, F.State, From, EWardenRemarkKind::NotThisTime, Fenrir, TEXT("Desvío"));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
