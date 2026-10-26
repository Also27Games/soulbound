// Tests de las votaciones (S-017, PR B; GDD 06): todos votan y el efecto cae sobre el más votado, con los votos públicos al
// resolverse (R-VOT-1 a R-VOT-3); el empate lo desempata el Warden; quien no vota recibe un voto al azar y sube su insolencia
// (R-VOT-4); los votos que no valen se rechazan; los del Vacío votan; y la regla mínima de los dummies (R-VOT-5). Cada test cita el
// criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro: "Soulbound.Core.Vote". Los datos (CA-VOT-2), en WardenDataTest.
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos,
// con los eventos y el Warden de los archivos). Las votaciones de los tests se añaden a la tabla a mano.

#include "Decider/Decider.h"
#include "Match/EffectRules.h"
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

	/** Añade una votación con el efecto Effect al más votado (escena del Acto II) y devuelve su identificador. */
	FString AddVote(FWardenFixture& F, const FString& Id, EEffect Effect)
	{
		const int32 Index = F.AddBagEvent(Id, EAct::ActTwo, EWardenChannel::Scene, {{App(Effect, EEffectTarget::MostVoted)}}, false);
		F.State.WardenEvents.Events[Index].bVote = true;
		return Id;
	}

	EIntentResult Vote(FWardenFixture& F, int32 Seat, int32 Voted) { return Soulbound::Rules::SubmitVote(F.State, Seat, Voted, F.Now); }

	/** Los VoteCast desde From, como pares (quien vota, votado). */
	TArray<TPair<int32, int32>> CastSince(const FWardenFixture& F, int32 From)
	{
		TArray<TPair<int32, int32>> Cast;
		for (const FMatchEvent& Event : F.Since(From, EMatchEventType::VoteCast))
		{
			Cast.Emplace(Event.Seat, Event.OtherSeat);
		}
		return Cast;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteAllTest, "Soulbound.Core.Vote.TodosVotan",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteAllTest::RunTest(const FString& Parameters)
{
	// CA-VOT-1 (R-VOT-1 a R-VOT-3): votación con cuatro asientos y todos votan → el más votado recibe el efecto. Los votos son
	// públicos al resolverse (decisión del autor del 26/10/2026): los cuatro VoteCast, en orden de asiento, y después el VoteResolved.
	// En una votación no dirige nadie: también se puede votar al último que jugó.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	const int32 Before = F.State.EventLog.Num();
	if (!TestEqual(TEXT("La votación empieza"), AsInt(F.Launch(AddVote(F, TEXT("voto-marca"), EEffect::Mark))), AsInt(EWardenEventStart::Started)))
	{
		return false;
	}
	const TArray<FMatchEvent> Started = F.Since(Before, EMatchEventType::VoteStarted);
	TestTrue(TEXT("VoteStarted con cuatro votables"), Started.Num() == 1 && Started[0].Detail == 4);
	TestTrue(TEXT("Abierta"), F.State.Vote.IsOpen());
	TestEqual(TEXT("Se puede votar a los cuatro, también al último que jugó"), F.State.Vote.Votables, TArray<int32>{Fenrir, Koschei, EchoA, EchoB});
	TestEqual(TEXT("Plazo de los datos: 8 s"), F.State.Vote.DeadlineSeconds, F.Now + 8.0);
	TestEqual(TEXT("Fenrir no puede votarse a sí mismo"), Soulbound::Rules::VoteOptions(F.State, Fenrir), TArray<int32>{Koschei, EchoA, EchoB});

	TestEqual(TEXT("Fenrir vota al Eco A"), AsInt(Vote(F, Fenrir, EchoA)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Koschei vota al Eco A"), AsInt(Vote(F, Koschei, EchoA)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El Eco A vota al Eco B"), AsInt(Vote(F, EchoA, EchoB)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Mientras se vota, los votos no son públicos"), F.Since(Before, EMatchEventType::VoteCast).Num(), 0);
	TestEqual(TEXT("El Eco B vota al Eco A"), AsInt(Vote(F, EchoB, EchoA)), AsInt(EIntentResult::Accepted));

	TestEqual(TEXT("Con el último voto se resuelve: los cuatro votos, en orden de asiento"), CastSince(F, Before),
		(TArray<TPair<int32, int32>>{{Fenrir, EchoA}, {Koschei, EchoA}, {EchoA, EchoB}, {EchoB, EchoA}}));
	TestTrue(TEXT("Ningún voto al azar"), !F.Since(Before, EMatchEventType::VoteCast).ContainsByPredicate([](const FMatchEvent& E) { return E.Detail == 1; }));
	const TArray<FMatchEvent> Resolved = F.Since(Before, EMatchEventType::VoteResolved);
	TestTrue(TEXT("VoteResolved: el Eco A, con tres votos, sin desempate"), Resolved.Num() == 1 && Resolved[0].Seat == EchoA && Resolved[0].Value == 3 && Resolved[0].Detail == 0);
	TestEqual(TEXT("El efecto cae sobre el más votado"), F.SeatsSince(Before, EMatchEventType::Marked), TArray<int32>{EchoA});
	TestFalse(TEXT("La votación se cierra"), F.State.Vote.IsOpen());
	TestFalse(TEXT("Y el evento termina"), F.State.CurrentEvent.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteTieTest, "Soulbound.Core.Vote.EmpateLoDecideElWarden",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteTieTest::RunTest(const FString& Parameters)
{
	// CA-VOT-1 (R-VOT-3): con empate, el Warden desempata entre los empatados con el sorteo del evento, y lo registra. 20 votaciones
	// con sorteos distintos: siempre uno de los dos empatados, y salen los dos.
	TSet<int32> Chosen;
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.WardenRandom = FRandomStream(Seed);
		const int32 Before = F.State.EventLog.Num();
		F.Launch(AddVote(F, TEXT("voto-marca"), EEffect::Mark));
		Vote(F, Fenrir, Koschei);
		Vote(F, Koschei, Fenrir);
		Vote(F, EchoA, Koschei);
		Vote(F, EchoB, Fenrir);
		const TArray<FMatchEvent> Resolved = F.Since(Before, EMatchEventType::VoteResolved);
		if (TestEqual(FString::Printf(TEXT("Semilla %d: se resuelve"), Seed), Resolved.Num(), 1))
		{
			TestTrue(FString::Printf(TEXT("Semilla %d: uno de los empatados, con desempate registrado"), Seed),
				(Resolved[0].Seat == Fenrir || Resolved[0].Seat == Koschei) && Resolved[0].Value == 2 && Resolved[0].Detail == 1);
			Chosen.Add(Resolved[0].Seat);
			TestEqual(FString::Printf(TEXT("Semilla %d: el marcado es el elegido"), Seed), F.SeatsSince(Before, EMatchEventType::Marked), TArray<int32>{Resolved[0].Seat});
		}
	}
	TestEqual(TEXT("El desempate no es siempre el mismo"), Chosen.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteIgnoredTest, "Soulbound.Core.Vote.QuienNoVotaEsInsolente",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteIgnoredTest::RunTest(const FString& Parameters)
{
	// CA-VOT-1 (R-VOT-4, R-INS-1): uno no vota → al agotarse el plazo recibe un voto al azar entre los suyos y su insolencia sube,
	// con su castigo después del efecto (decisión del autor del 26/10/2026).
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	const int32 Before = F.State.EventLog.Num();
	F.Launch(AddVote(F, TEXT("voto-marca"), EEffect::Mark));
	Vote(F, Fenrir, EchoA);
	Vote(F, Koschei, EchoA);
	Vote(F, EchoA, Fenrir);
	F.Now += 3.0;
	F.Advance();
	TestTrue(TEXT("Dentro del plazo, la votación espera"), F.State.Vote.IsOpen());
	TestEqual(TEXT("Y el turno siguiente no empieza"), F.Since(Before, EMatchEventType::TurnStarted).Num(), 0);
	F.Now = F.State.Vote.DeadlineSeconds;
	F.Advance();
	const TArray<FMatchEvent> Cast = F.Since(Before, EMatchEventType::VoteCast);
	const FMatchEvent* Random = Cast.FindByPredicate([](const FMatchEvent& E) { return E.Seat == EchoB; });
	if (TestNotNull(TEXT("El Eco B recibe un voto"), Random))
	{
		TestEqual(TEXT("Al azar"), Random->Detail, 1);
		TestTrue(TEXT("Entre los suyos"), Random->OtherSeat != EchoB && (Random->OtherSeat == Fenrir || Random->OtherSeat == Koschei || Random->OtherSeat == EchoA));
	}
	TestEqual(TEXT("Cuatro votos registrados"), Cast.Num(), 4);
	TestEqual(TEXT("Su insolencia sube"), F.State.Seats[EchoB].Insolence, 1);
	int32 MarkedAt = INDEX_NONE;
	int32 PunishedAt = INDEX_NONE;
	for (int32 Index = Before; Index < F.State.EventLog.Num(); ++Index)
	{
		const EMatchEventType Type = F.State.EventLog[Index].Type;
		MarkedAt = Type == EMatchEventType::Marked && MarkedAt == INDEX_NONE ? Index : MarkedAt;
		PunishedAt = Type == EMatchEventType::InsolencePunished && PunishedAt == INDEX_NONE ? Index : PunishedAt;
	}
	TestTrue(TEXT("Primero el efecto, después el castigo"), MarkedAt != INDEX_NONE && PunishedAt > MarkedAt);
	TestEqual(TEXT("Nadie más es insolente"), F.State.Seats[Fenrir].Insolence + F.State.Seats[Koschei].Insolence + F.State.Seats[EchoA].Insolence, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteInvalidTest, "Soulbound.Core.Vote.VotoInvalidoSeRechaza",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteInvalidTest::RunTest(const FString& Parameters)
{
	// CA-VOT-1 (R-VOT-2): votarse a sí mismo o votar a quien el efecto no admite (El chivo no manda al Vacío a quien ya está dentro)
	// se rechaza; un segundo voto, fuera de plazo o sin votación, también. Ninguno toca nada.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	FMatchState Copy = F.State;
	TestEqual(TEXT("Sin votación abierta, WrongPhase"), AsInt(Vote(F, Fenrir, Koschei)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));

	Soulbound::Rules::Banish(F.State, EchoA, false, F.Now);
	F.Launch(AddVote(F, TEXT("voto-vacio"), EEffect::Banish));
	TestFalse(TEXT("Quien ya está en el Vacío no se puede votar"), F.State.Vote.Votables.Contains(EchoA));
	Copy = F.State;
	TestEqual(TEXT("Votarse a sí mismo: InvalidOption"), AsInt(Vote(F, Fenrir, Fenrir)), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("Votar a quien el efecto no admite: InvalidOption"), AsInt(Vote(F, Fenrir, EchoA)), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("Un asiento que no existe: WrongSeat"), AsInt(Vote(F, 7, Koschei)), AsInt(EIntentResult::WrongSeat));
	TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));
	TestEqual(TEXT("Fenrir vota a Koschei"), AsInt(Vote(F, Fenrir, Koschei)), AsInt(EIntentResult::Accepted));
	Copy = F.State;
	TestEqual(TEXT("Un segundo voto: WrongPhase"), AsInt(Vote(F, Fenrir, EchoB)), AsInt(EIntentResult::WrongPhase));
	F.Now = F.State.Vote.DeadlineSeconds;
	TestEqual(TEXT("En el plazo o después: ClockExpired"), AsInt(Vote(F, Koschei, EchoB)), AsInt(EIntentResult::ClockExpired));
	TestTrue(TEXT("Sin tocar nada"), Soulbound::Tests::SameState(Copy, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteVoidTest, "Soulbound.Core.Vote.ElDelVacioVota",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteVoidTest::RunTest(const FString& Parameters)
{
	// CA-VOT-1 (R-VOT-2): un asiento en el Vacío vota. Y quien no tiene a nadie a quien votar (solo se le puede votar a él) no vota,
	// sin insolencia: la votación se resuelve con los votos de los demás (decisión del autor del 26/10/2026). La Vuelta al hogar o
	// el Vacío del más votado no cobran su marca: en una votación no señala nadie.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	Soulbound::Rules::Banish(F.State, Fenrir, false, F.Now);
	Soulbound::Rules::Banish(F.State, Koschei, false, F.Now);
	Soulbound::Rules::Banish(F.State, EchoA, false, F.Now);
	F.State.Seats[EchoB].MarkedUntilRound = F.State.Round + 3;
	const int32 Before = F.State.EventLog.Num();
	F.Launch(AddVote(F, TEXT("voto-vacio"), EEffect::Banish));
	TestEqual(TEXT("Solo se puede votar al Eco B"), F.State.Vote.Votables, TArray<int32>{EchoB});
	TestTrue(TEXT("El Eco B no tiene a quién votar"), Soulbound::Rules::VoteOptions(F.State, EchoB).IsEmpty());
	TestEqual(TEXT("Fenrir, en el Vacío, vota"), AsInt(Vote(F, Fenrir, EchoB)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Koschei, en el Vacío, vota"), AsInt(Vote(F, Koschei, EchoB)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El Eco A, en el Vacío, vota"), AsInt(Vote(F, EchoA, EchoB)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Con los tres votos posibles, se resuelve"), F.Since(Before, EMatchEventType::VoteResolved).Num(), 1);
	TestEqual(TEXT("Tres votos registrados: el Eco B no vota"), F.Since(Before, EMatchEventType::VoteCast).Num(), 3);
	TestEqual(TEXT("Sin insolencia para él"), F.State.Seats[EchoB].Insolence, 0);
	TestTrue(TEXT("Al Vacío"), F.State.Seats[EchoB].IsInVoid());
	TestEqual(TEXT("Sin cobro de marca"), F.Since(Before, EMatchEventType::MarkCollected).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteEdgeTest, "Soulbound.Core.Vote.SinVotablesYRegalo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteEdgeTest::RunTest(const FString& Parameters)
{
	// R-VOT-1, R-DIR-3: sin nadie a quien votar, burla y no hay votación. Un regalo (Favor, Runa regalada): el más votado elige la
	// runa, con el reloj corto de la Runa regalada (decisión del autor del 26/10/2026).
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		for (FTileState& Tile : F.State.Board.Tiles)
		{
			Tile.RuneReserve = 0;
		}
		const int32 Before = F.State.EventLog.Num();
		F.Launch(AddVote(F, TEXT("voto-regalo"), EEffect::GiftRune));
		TestEqual(TEXT("Sin reservas, nadie se puede votar: sin votación"), F.Since(Before, EMatchEventType::VoteStarted).Num(), 0);
		TestEqual(TEXT("Burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
		TestFalse(TEXT("Nada abierto"), F.State.Vote.IsOpen());
	}
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.Launch(AddVote(F, TEXT("voto-regalo"), EEffect::GiftRune));
		Vote(F, Fenrir, Koschei);
		Vote(F, Koschei, EchoB);
		Vote(F, EchoA, Koschei);
		Vote(F, EchoB, Koschei);
		TestEqual(TEXT("Koschei, el más votado, elige la runa"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::GiftRune));
		TestEqual(TEXT("Se le pide a él"), F.State.PendingChoice.Seat, Koschei);
		TestEqual(TEXT("Con el reloj corto"), F.State.PendingChoice.DeadlineSeconds, F.Now + F.State.Params.ShortChoiceClockSeconds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoteDummyTest, "Soulbound.Core.Vote.VotoDelDummy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoteDummyTest::RunTest(const FString& Parameters)
{
	// R-VOT-5 (decisión del autor del 22/10/2026): el dummy vota, en un castigo, al que más tipos de runa distintos tiene; en un
	// regalo, al que menos; con empate, al azar con su sorteo.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	F.State.Seats[Koschei].Runes[1] = 1;
	F.State.Seats[Koschei].Runes[2] = 1;
	F.State.Seats[EchoA].Runes[0] = 1;
	F.State.Seats[EchoA].Runes[1] = 1;
	F.State.Seats[EchoB].Runes[0] = 1;
	F.State.Seats[EchoB].Runes[1] = 1;
	FRandomStream Rng(9);
	FDummyDecider Dummy;

	F.Launch(AddVote(F, TEXT("voto-marca"), EEffect::Mark));
	TestEqual(TEXT("Castigo: al que más tipos tiene"), Dummy.ChooseVote(F.State, Fenrir, Soulbound::Rules::VoteOptions(F.State, Fenrir), Rng), Koschei);
	TSet<int32> Tied;
	for (int32 Draw = 0; Draw < 30; ++Draw)
	{
		Tied.Add(Dummy.ChooseVote(F.State, Koschei, Soulbound::Rules::VoteOptions(F.State, Koschei), Rng));
	}
	TestTrue(TEXT("Castigo con empate (los dos Ecos, dos tipos): al azar entre ellos"), Tied.Num() == 2 && Tied.Contains(EchoA) && Tied.Contains(EchoB));

	FWardenFixture G;
	if (!G.StartWithWarden(*this))
	{
		return false;
	}
	G.State.Seats[Koschei].Runes[0] = 1;
	G.State.Seats[EchoA].Runes.Init(1, Soulbound::Board::DomainCount);
	G.State.Seats[EchoB].Runes[0] = 1;
	G.State.Seats[EchoB].Runes[1] = 1;
	G.Launch(AddVote(G, TEXT("voto-regalo"), EEffect::GiftRune));
	TestEqual(TEXT("Regalo: al que menos tipos tiene (Fenrir, ninguno)"), Dummy.ChooseVote(G.State, EchoA, Soulbound::Rules::VoteOptions(G.State, EchoA), Rng), Fenrir);
	TestEqual(TEXT("Regalo, sin Fenrir entre sus opciones: Koschei (uno)"), Dummy.ChooseVote(G.State, Fenrir, Soulbound::Rules::VoteOptions(G.State, Fenrir), Rng), Koschei);
	TestEqual(TEXT("Sin opciones, INDEX_NONE"), Dummy.ChooseVote(G.State, Fenrir, {}, Rng), static_cast<int32>(INDEX_NONE));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
