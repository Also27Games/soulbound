// Tests de los disparadores contextuales (S-017, GDD 06): las cinco condiciones con sus umbrales de datos (R-DIS-1), lo que sus
// tablas hacen con los pesos y las direcciones (R-DIS-2), el peso "al azar" por el tiempo sin ser objetivo (R-DIS-3) y el rival
// sugerido que siguen los dummies (decisión del autor del 23/10/2026). Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n)
// que cubre. Filtro: "Soulbound.Core.Warden.Triggers". La tabla de warden.json, en Soulbound.Core.Warden.Data.TablaDeDisparadores.
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos,
// con los eventos y el Warden de los archivos). Las runas se fijan a mano: es lo que dispara, no lo que se comprueba.

#include "Decider/Decider.h"
#include "Match/EffectRules.h"
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
	constexpr int32 EchoB = FWardenFixture::EchoB;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** Da a Seat Types tipos de runa distintos, una de cada, desde el tipo 0. */
	void GiveTypes(FWardenFixture& F, int32 Seat, int32 Types)
	{
		F.State.Seats[Seat].Runes.Init(0, Soulbound::Board::DomainCount);
		for (int32 Type = 0; Type < Types; ++Type)
		{
			F.State.Seats[Seat].Runes[Type] = 1;
		}
	}

	/** Todos fueron objetivo hace un instante: así Jugador ignorado no salta por el reloj de la partida. */
	void EveryoneJustTargeted(FWardenFixture& F)
	{
		for (FSeatState& Seat : F.State.Seats)
		{
			Seat.LastTargetedSeconds = F.Now;
		}
		F.State.LastProgressSeconds = F.Now;
	}

	bool Has(const TArray<FActiveTrigger>& Triggers, EWardenTrigger Trigger, int32 Seat = INDEX_NONE)
	{
		return Triggers.ContainsByPredicate([&](const FActiveTrigger& Active) { return Active.Trigger == Trigger && (Seat == INDEX_NONE || Active.Seat == Seat); });
	}

	/** Peso del evento Id en el acto Act con los disparadores activos ahora, si lo dirigiera Director. */
	double Weight(const FWardenFixture& F, const TCHAR* Id, EAct Act, int32 Director)
	{
		const int32 Index = F.IndexOf(Id);
		return Soulbound::Rules::TriggeredEventWeight(F.State, Index, Act, Director, Soulbound::Rules::ActiveTriggers(F.State, F.Now));
	}

	double BaseWeight(const FWardenFixture& F, const TCHAR* Id, EAct Act)
	{
		return F.State.WardenEvents.Events[F.IndexOf(Id)].WeightIn(Act);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggersClearLeaderTest, "Soulbound.Core.Warden.Triggers.LiderClaro",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTriggersClearLeaderTest::RunTest(const FString& Parameters)
{
	// CA-DIS-1 (R-DIS-1, R-DIS-2): con dos tipos de runa más que el segundo, Líder claro está activo, causado por él, y el peso de
	// los eventos de su tabla es el base por su multiplicador; con uno solo de ventaja, o cuando deja de cumplirse, vuelve al
	// base. Enséñame pasa a dirigirse al líder (GDD 06), sin elección. Los golpes contra el activo, por 2 solo si el activo es él.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Now = 400.0;
	EveryoneJustTargeted(F);
	GiveTypes(F, Fenrir, 3);
	GiveTypes(F, Koschei, 2);
	TestFalse(TEXT("Con un tipo de ventaja, sin Líder claro"), Has(Soulbound::Rules::ActiveTriggers(F.State, F.Now), EWardenTrigger::ClearLeader));
	TestEqual(TEXT("Sin él, El precio pesa lo de su saco"), Weight(F, TEXT("el-precio"), EAct::ActTwo, Koschei), BaseWeight(F, TEXT("el-precio"), EAct::ActTwo));

	GiveTypes(F, Koschei, 1);
	const TArray<FActiveTrigger> Triggers = Soulbound::Rules::ActiveTriggers(F.State, F.Now);
	TestTrue(TEXT("Con dos de ventaja, Líder claro por Fenrir"), Has(Triggers, EWardenTrigger::ClearLeader, Fenrir));
	TestEqual(TEXT("Un solo Líder claro"), Triggers.FilterByPredicate([](const FActiveTrigger& A) { return A.Trigger == EWardenTrigger::ClearLeader; }).Num(), 1);
	TestEqual(TEXT("El perseguido: base por 3"), Weight(F, TEXT("el-perseguido"), EAct::ActThree, Koschei), 3.0 * BaseWeight(F, TEXT("el-perseguido"), EAct::ActThree));
	TestEqual(TEXT("Destierro del líder: base por 2"), Weight(F, TEXT("destierro-del-lider"), EAct::ActThree, Koschei), 2.0 * BaseWeight(F, TEXT("destierro-del-lider"), EAct::ActThree));
	TestEqual(TEXT("El precio: base por 2"), Weight(F, TEXT("el-precio"), EAct::ActTwo, Koschei), 2.0 * BaseWeight(F, TEXT("el-precio"), EAct::ActTwo));
	TestEqual(TEXT("Un evento fuera de la tabla, su base"), Weight(F, TEXT("arquitecto"), EAct::ActTwo, Koschei), BaseWeight(F, TEXT("arquitecto"), EAct::ActTwo));
	TestTrue(TEXT("Impuesto es un golpe contra el activo"), Soulbound::Rules::IsStrikeAgainstActive(F.State, F.IndexOf(TEXT("impuesto"))));
	TestFalse(TEXT("Prisa, un regalo, no"), Soulbound::Rules::IsStrikeAgainstActive(F.State, F.IndexOf(TEXT("prisa"))));
	TestEqual(TEXT("Golpe contra el activo, dirigido por otro: su base"), Weight(F, TEXT("tropiezo"), EAct::ActTwo, Koschei), BaseWeight(F, TEXT("tropiezo"), EAct::ActTwo));
	TestEqual(TEXT("Golpe contra el activo, dirigido por el líder: por 2"), Weight(F, TEXT("tropiezo"), EAct::ActTwo, Fenrir), 2.0 * BaseWeight(F, TEXT("tropiezo"), EAct::ActTwo));

	// Enséñame (Revelar Brazal a rival señalado): con Líder claro va al líder, sin que Koschei señale.
	F.Director(Koschei);
	const int32 Before = F.State.EventLog.Num();
	F.Launch(TEXT("ensename"));
	TestFalse(TEXT("Enséñame con Líder claro: sin elección"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Y el Brazal revelado es el del líder"), F.SeatsSince(Before, EMatchEventType::BracerRevealed), TArray<int32>{Fenrir});

	// Deja de cumplirse: vuelve al base.
	GiveTypes(F, Koschei, 2);
	TestFalse(TEXT("Sin la ventaja, sin Líder claro"), Has(Soulbound::Rules::ActiveTriggers(F.State, F.Now), EWardenTrigger::ClearLeader));
	TestEqual(TEXT("Y El perseguido vuelve a su base"), Weight(F, TEXT("el-perseguido"), EAct::ActThree, Koschei), BaseWeight(F, TEXT("el-perseguido"), EAct::ActThree));
	F.Director(Koschei);
	F.Launch(TEXT("ensename"));
	TestEqual(TEXT("Y Enséñame vuelve a pedir que se señale"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggersImminentVictoryTest, "Soulbound.Core.Warden.Triggers.VictoriaInminente",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTriggersImminentVictoryTest::RunTest(const FString& Parameters)
{
	// CA-DIS-2 (R-DIS-1, R-DIS-2): con cinco tipos, Victoria inminente; la probabilidad de intervenir, por 1,5; los eventos contra
	// él dominan los candidatos de su canal; Primer muro sella en su camino al Altar. Con seis en el Altar, también.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Now = 1700.0;
	EveryoneJustTargeted(F);
	GiveTypes(F, Fenrir, 5);
	GiveTypes(F, Koschei, 4);
	const TArray<FActiveTrigger> Triggers = Soulbound::Rules::ActiveTriggers(F.State, F.Now);
	TestTrue(TEXT("Victoria inminente por Fenrir"), Has(Triggers, EWardenTrigger::ImminentVictory, Fenrir));
	TestFalse(TEXT("Koschei, con cuatro, no"), Has(Triggers, EWardenTrigger::ImminentVictory, Koschei));
	TestTrue(TEXT("Probabilidad de intervenir del Acto III por 1,5"),
		FMath::IsNearlyEqual(Soulbound::Rules::TriggeredInterveneProbability(F.State, EAct::ActThree, Triggers), FMath::Min(1.0, 1.5 * F.State.WardenParams.Acts[2].InterveneProbability)));
	TestEqual(TEXT("Destierro del líder: base por 4"), Weight(F, TEXT("destierro-del-lider"), EAct::ActThree, Koschei), 4.0 * BaseWeight(F, TEXT("destierro-del-lider"), EAct::ActThree));
	TestEqual(TEXT("Golpe contra el activo dirigido por él: por 3"), Weight(F, TEXT("tropiezo"), EAct::ActThree, Fenrir), 3.0 * BaseWeight(F, TEXT("tropiezo"), EAct::ActThree));

	// Dominan: en cada canal, los eventos contra él son los que más pesan entre los candidatos.
	for (int32 Seat = 0; Seat < F.State.Seats.Num(); ++Seat)
	{
		F.MoveAway(Seat);
	}
	F.Director(Koschei);
	for (const TPair<EWardenChannel, const TCHAR*> Channel : {TPair<EWardenChannel, const TCHAR*>(EWardenChannel::Strike, TEXT("cosecha-del-lider")),
		TPair<EWardenChannel, const TCHAR*>(EWardenChannel::Scene, TEXT("el-perseguido"))})
	{
		const TArray<int32> Candidates = Soulbound::Rules::EventCandidates(F.State, Channel.Key, F.Now);
		const double Against = Weight(F, Channel.Value, EAct::ActThree, Koschei);
		for (const int32 Index : Candidates)
		{
			const FString& Id = F.State.WardenEvents.Events[Index].Id;
			if (Id != Channel.Value && Id != TEXT("destierro-del-lider"))
			{
				const double Other = Soulbound::Rules::TriggeredEventWeight(F.State, Index, EAct::ActThree, Koschei, Triggers);
				TestTrue(FString::Printf(TEXT("%s pesa más que %s (%.2f y %.2f)"), Channel.Value, *Id, Against, Other), Against > Other);
			}
		}
	}

	// Primer muro: el Sello cae en el camino de Fenrir al Altar.
	const int32 Expected = Soulbound::Rules::SealTowardsAltarTile(F.State, Fenrir);
	TestNotEqual(TEXT("Hay casilla en su camino al Altar"), Expected, static_cast<int32>(INDEX_NONE));
	const int32 Before = F.State.EventLog.Num();
	F.Launch(TEXT("primer-muro"));
	const TArray<FMatchEvent> Sealed = F.Since(Before, EMatchEventType::SealPlaced);
	TestTrue(TEXT("Primer muro sella la casilla de su camino al Altar"), Sealed.Num() == 1 && Sealed[0].Value == Expected);

	// Seis en el Altar.
	GiveTypes(F, Fenrir, 0);
	GiveTypes(F, Koschei, 6);
	F.State.Seats[Koschei].TileId = F.State.Board.FindAltar();
	F.State.WardenParams.Triggers.ImminentVictoryTypes = 7;
	TestTrue(TEXT("En el Altar con seis, aunque el umbral fuera mayor"), Has(Soulbound::Rules::ActiveTriggers(F.State, F.Now), EWardenTrigger::ImminentVictory, Koschei));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggersIgnoredTest, "Soulbound.Core.Warden.Triggers.JugadorIgnorado",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTriggersIgnoredTest::RunTest(const FString& Parameters)
{
	// CA-DIS-3 (R-DIS-1, R-DIS-3): quien pasa el umbral sin ser objetivo es Jugador ignorado y su peso como "al azar" es mayor
	// (1 + segundos / (4 × 60), por 3); en el Vacío no cuenta. Ser objetivo de un efecto lo vuelve a poner a cero. Una dirección
	// al azar cae más sobre él.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Now = 400.0;
	EveryoneJustTargeted(F);
	F.State.Seats[EchoA].LastTargetedSeconds = 100.0;
	F.State.Seats[Koschei].LastTargetedSeconds = 300.0;
	const TArray<FActiveTrigger> Triggers = Soulbound::Rules::ActiveTriggers(F.State, F.Now);
	TestTrue(TEXT("300 s sin ser objetivo: Jugador ignorado"), Has(Triggers, EWardenTrigger::IgnoredPlayer, EchoA));
	TestFalse(TEXT("100 s: no"), Has(Triggers, EWardenTrigger::IgnoredPlayer, Koschei));
	TestTrue(TEXT("Peso al azar del ignorado: (1 + 300/240) × 3"),
		FMath::IsNearlyEqual(Soulbound::Rules::RandomTargetWeight(F.State, EchoA, F.Now, Triggers), (1.0 + 300.0 / 240.0) * 3.0));
	TestTrue(TEXT("Peso al azar de Koschei: 1 + 100/240, sin disparador"),
		FMath::IsNearlyEqual(Soulbound::Rules::RandomTargetWeight(F.State, Koschei, F.Now, Triggers), 1.0 + 100.0 / 240.0));
	TestTrue(TEXT("Peso al azar de quien acaba de ser objetivo: 1"), FMath::IsNearlyEqual(Soulbound::Rules::RandomTargetWeight(F.State, Fenrir, F.Now, Triggers), 1.0));

	// Una dirección al azar (Marca ciega, Marcado al azar) cae más sobre el ignorado. 200 lanzamientos, cada uno sobre el estado
	// de partida, con el sorteo del Warden avanzando.
	const FMatchState Saved = F.State;
	TMap<int32, int32> Marked;
	for (int32 Launch = 0; Launch < 200; ++Launch)
	{
		F.State = Saved;
		F.WardenRandom = FRandomStream(1000 + Launch);
		// Siempre en el mismo instante: Launch lo adelanta un segundo, a 400.
		F.Now = 399.0;
		const int32 Before = F.State.EventLog.Num();
		F.Director(Fenrir);
		F.Launch(TEXT("marca-ciega"));
		for (const int32 Seat : F.SeatsSince(Before, EMatchEventType::Marked))
		{
			++Marked.FindOrAdd(Seat);
		}
	}
	TestTrue(FString::Printf(TEXT("Marca ciega cae más sobre el ignorado (%d) que sobre Koschei (%d)"), Marked.FindRef(EchoA), Marked.FindRef(Koschei)),
		Marked.FindRef(EchoA) > 2 * Marked.FindRef(Koschei));
	F.State = Saved;

	// Ser objetivo lo pone a cero.
	F.Director(Fenrir);
	F.Now += 1.0;
	FRandomStream Draw(3);
	Soulbound::Rules::ApplyEffect(F.State, App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 1), EchoA, EchoA, INDEX_NONE, INDEX_NONE, F.Now, Draw);
	TestEqual(TEXT("Objetivo de un efecto: su instante se actualiza"), F.State.Seats[EchoA].LastTargetedSeconds, F.Now);
	TestFalse(TEXT("Y deja de ser ignorado"), Has(Soulbound::Rules::ActiveTriggers(F.State, F.Now), EWardenTrigger::IgnoredPlayer, EchoA));

	// En el Vacío no cuenta.
	F.State.Seats[EchoB].LastTargetedSeconds = 0.0;
	Soulbound::Rules::Banish(F.State, EchoB, false, F.Now);
	F.State.Seats[EchoB].LastTargetedSeconds = 0.0;
	TestFalse(TEXT("En el Vacío no es Jugador ignorado"), Has(Soulbound::Rules::ActiveTriggers(F.State, F.Now), EWardenTrigger::IgnoredPlayer, EchoB));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggersMultiplyTest, "Soulbound.Core.Warden.Triggers.DosDisparadoresSeMultiplican",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTriggersMultiplyTest::RunTest(const FString& Parameters)
{
	// CA-DIS-4 (R-DIS-1): dos disparadores sobre el mismo evento → base por los dos. Fenrir con tres runas de un tipo y una de
	// otro: Líder claro (dos tipos frente a ninguno) y Acaparador; El precio está en las dos tablas, por 2 en cada una.
	// Impuesto, por 3 con Acaparador solo si el activo es el acaparador.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Now = 400.0;
	EveryoneJustTargeted(F);
	F.State.Seats[Fenrir].Runes[0] = 3;
	F.State.Seats[Fenrir].Runes[1] = 1;
	const TArray<FActiveTrigger> Triggers = Soulbound::Rules::ActiveTriggers(F.State, F.Now);
	TestTrue(TEXT("Líder claro y Acaparador, los dos por Fenrir"), Has(Triggers, EWardenTrigger::ClearLeader, Fenrir) && Has(Triggers, EWardenTrigger::Hoarder, Fenrir));
	TestEqual(TEXT("El precio: base por 2 y por 2"), Weight(F, TEXT("el-precio"), EAct::ActTwo, Koschei), 4.0 * BaseWeight(F, TEXT("el-precio"), EAct::ActTwo));
	TestEqual(TEXT("Impuesto dirigido por Koschei: su base"), Weight(F, TEXT("impuesto"), EAct::ActTwo, Koschei), BaseWeight(F, TEXT("impuesto"), EAct::ActTwo));
	// Dirigido por Fenrir: por 3 de Acaparador y por 2 de golpe contra el activo de Líder claro.
	TestEqual(TEXT("Impuesto dirigido por el acaparador: por 3 y por 2"), Weight(F, TEXT("impuesto"), EAct::ActTwo, Fenrir), 6.0 * BaseWeight(F, TEXT("impuesto"), EAct::ActTwo));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggersStalledTest, "Soulbound.Core.Warden.Triggers.PartidaEstancada",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTriggersStalledTest::RunTest(const FString& Parameters)
{
	// R-DIS-1 (decisión del autor del 23/10/2026): cuatro minutos sin runas obtenidas ni duelos → Partida estancada, sin causante;
	// sus eventos, por 3. Una runa obtenida lo apaga. Durante la carrera no cuenta, y el fin de la carrera arranca su reloj.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Now = 600.0;
	EveryoneJustTargeted(F);
	F.State.LastProgressSeconds = 350.0;
	TArray<FActiveTrigger> Triggers = Soulbound::Rules::ActiveTriggers(F.State, F.Now);
	TestTrue(TEXT("250 s sin progreso: Partida estancada, sin causante"), Has(Triggers, EWardenTrigger::StalledMatch));
	TestTrue(TEXT("Sin causante"), Triggers.Contains(FActiveTrigger{EWardenTrigger::StalledMatch, INDEX_NONE}));
	TestEqual(TEXT("Piedad, por 3"), Weight(F, TEXT("piedad"), EAct::ActTwo, Koschei), 3.0 * BaseWeight(F, TEXT("piedad"), EAct::ActTwo));

	// Una runa obtenida (regalada por el Warden) cuenta como progreso.
	FRandomStream Draw(3);
	const TArray<int32> Gifts = Soulbound::Rules::GiftRuneOptions(F.State);
	if (TestFalse(TEXT("Hay runas que regalar"), Gifts.IsEmpty()))
	{
		Soulbound::Rules::GiftRune(F.State, Koschei, Gifts[0], F.Now);
		TestEqual(TEXT("Runa obtenida: el reloj del estancamiento vuelve a empezar"), F.State.LastProgressSeconds, F.Now);
		TestFalse(TEXT("Y se apaga"), Has(Soulbound::Rules::ActiveTriggers(F.State, F.Now), EWardenTrigger::StalledMatch));
	}

	// En la carrera, no.
	FWardenFixture R;
	if (!R.StartInRace(*this))
	{
		return false;
	}
	R.Now = 290.0;
	TestFalse(TEXT("Durante la carrera, nunca"), Has(Soulbound::Rules::ActiveTriggers(R.State, R.Now), EWardenTrigger::StalledMatch));
	R.Now = 295.0;
	Soulbound::Rules::EndBondRace(R.State, R.Now);
	TestEqual(TEXT("El fin de la carrera arranca su reloj"), R.State.LastProgressSeconds, R.Now);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggersSuggestionTest, "Soulbound.Core.Warden.Triggers.RivalSugerido",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTriggersSuggestionTest::RunTest(const FString& Parameters)
{
	// R-DIS-2 (decisión del autor del 23/10/2026): en un evento con rival señalado, el disparador no quita la elección al activo;
	// la preferencia queda como rival sugerido en el evento en curso, y el dummy la sigue. Sin disparador, no hay sugerencia.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Now = 400.0;
	EveryoneJustTargeted(F);
	GiveTypes(F, Fenrir, 2);
	GiveTypes(F, EchoA, 0);
	F.Director(Koschei);
	F.Launch(TEXT("el-precio"));
	TestEqual(TEXT("El precio es un dilema"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Dilemma));
	F.Answer(0);
	if (!TestEqual(TEXT("La opción del robo: Koschei señala"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival)))
	{
		return false;
	}
	TestTrue(TEXT("Todos los rivales siguen entre las opciones"), F.State.PendingChoice.Options.Contains(EchoA) && F.State.PendingChoice.Options.Contains(Fenrir));
	TestEqual(TEXT("Rival sugerido: el líder"), F.State.CurrentEvent.SuggestedRival, Fenrir);
	FRandomStream Rng(5);
	for (int32 Draw = 0; Draw < 20; ++Draw)
	{
		TestEqual(TEXT("El dummy sigue la sugerencia"), FDummyDecider().ChooseOption(F.State, Koschei, F.State.PendingChoice, Rng), Fenrir);
	}

	// Sin disparador.
	FWardenFixture G;
	if (!G.StartWithWarden(*this))
	{
		return false;
	}
	G.Now = 400.0;
	EveryoneJustTargeted(G);
	G.Director(Koschei);
	G.Launch(TEXT("el-precio"));
	G.Answer(0);
	TestEqual(TEXT("Sin disparador, sin sugerencia"), G.State.CurrentEvent.SuggestedRival, INDEX_NONE);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
