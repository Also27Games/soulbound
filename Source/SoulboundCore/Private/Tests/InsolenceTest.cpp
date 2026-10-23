// Tests de la insolencia con castigo y de la Furia (S-016, PR B; GDD 06): la tabla por gravedad (R-INS-2), las listas de
// efectos que salen del catálogo, cuándo se aplica el castigo (al terminar el turno, en el acto, a la salida del Vacío), la
// Furia a la quinta (R-INS-3) y el Vacío de la Furia con sus excepciones (Desvío, Liberación, escalera, cuenta reiniciada).
// Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro: "Soulbound.Core.Insolence".
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos,
// con los eventos y el Warden de los archivos). La tabla de insolencia se cambia a mano cuando el test necesita una gravedad.

#include "Match/EffectCatalog.h"
#include "Match/EffectRules.h"
#include "Match/InsolenceRules.h"
#include "Match/VoidRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::FWardenFixture;

	constexpr int32 Fenrir = FWardenFixture::Fenrir;
	constexpr int32 Koschei = FWardenFixture::Koschei;
	constexpr int32 FenrirDomain = FWardenFixture::FenrirDomain;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** Los castigos que puede dar la tabla, en ningún caso un regalo (R-INS-2). */
	const TArray<EEffect> AllPunishments = {EEffect::Halt, EEffect::RevealBracer, EEffect::ForcedOracle, EEffect::SendHome, EEffect::LoseTurn, EEffect::WardenTrial, EEffect::LoseRune, EEffect::Banish};

	/** Todos los niveles de la tabla dan la gravedad Severity: lo que el test necesita para mirar una gravedad. */
	void ForceSeverity(FWardenFixture& F, EInsolencePunishment Severity)
	{
		for (FInsolenceRow& Row : F.State.WardenParams.InsolenceTable)
		{
			if (!Row.bFury)
			{
				Row.SeverityPercents = {Severity == EInsolencePunishment::Light ? 100 : 0, Severity == EInsolencePunishment::Medium ? 100 : 0,
					Severity == EInsolencePunishment::Severe ? 100 : 0};
			}
		}
	}

	/** Una runa de cada tipo para Seat, fuera de su hogar: así casi todos los castigos valen sobre él. */
	void RichAndAway(FWardenFixture& F, int32 Seat)
	{
		F.State.Seats[Seat].Runes.Init(1, Soulbound::Board::DomainCount);
		F.MoveAway(Seat);
	}

	/** Posición en el registro del primer evento de tipo Type desde From, o INDEX_NONE. */
	int32 FindFrom(const FMatchState& State, int32 From, EMatchEventType Type)
	{
		for (int32 Index = From; Index < State.EventLog.Num(); ++Index)
		{
			if (State.EventLog[Index].Type == Type)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Lo que hizo el castigo que empieza en el InsolencePunished de la posición At: el efecto del EffectApplied que lo sigue, o
	 * INDEX_NONE si lo que sigue es una burla.
	 */
	int32 PunishmentEffect(const FMatchState& State, int32 At)
	{
		if (!State.EventLog.IsValidIndex(At + 1))
		{
			return INDEX_NONE;
		}
		const FMatchEvent& Next = State.EventLog[At + 1];
		return Next.Type == EMatchEventType::EffectApplied ? Next.Value : INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceListsTest, "Soulbound.Core.Insolence.ListasDelCatalogo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceListsTest::RunTest(const FString& Parameters)
{
	// R-INS-2 (decisión del autor del 19/10/2026): las listas de cada gravedad salen del catálogo (de esa gravedad, implementados,
	// con dirección activo) y coinciden con las del GDD 06: leve, Revelar Brazal y, desde S-017 (CA-INS-1b, R-INS-2), la Consulta
	// forzada (el Alto sale, pero nunca vale sin un turno que cortar); media, Vuelta al hogar, Turno perdido y Prueba del Warden;
	// grave, Perder una runa y el Vacío. Ningún regalo.
	TestEqual(TEXT("Leve: Alto, Revelar Brazal y Consulta forzada"), Soulbound::Rules::InsolencePunishmentEffects(EEffectSeverity::Light),
		TArray<EEffect>{EEffect::Halt, EEffect::RevealBracer, EEffect::ForcedOracle});
	TestEqual(TEXT("Media: Vuelta al hogar, Turno perdido y Prueba del Warden"), Soulbound::Rules::InsolencePunishmentEffects(EEffectSeverity::Medium),
		TArray<EEffect>{EEffect::SendHome, EEffect::LoseTurn, EEffect::WardenTrial});
	TestEqual(TEXT("Grave: Perder una runa y el Vacío"), Soulbound::Rules::InsolencePunishmentEffects(EEffectSeverity::Severe), TArray<EEffect>{EEffect::LoseRune, EEffect::Banish});
	TestTrue(TEXT("Ningún regalo"), Soulbound::Rules::InsolencePunishmentEffects(EEffectSeverity::Gift).IsEmpty());

	// El Alto nunca castiga: con la gravedad leve, sin runas (Revelar Brazal no vale) y con una Consulta forzada ya esperando (otra
	// no vale, S-017), burla. Aunque el castigo caiga dentro de un golpe que cortó el turno de ese mismo asiento.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	ForceSeverity(F, EInsolencePunishment::Light);
	F.State.Oracle.Queued.Add(Koschei);
	F.State.CurrentEvent.EventIndex = F.IndexOf(TEXT("tropiezo"));
	F.State.CurrentEvent.Director = Koschei;
	F.State.CurrentEvent.bInterruptedTurn = true;
	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::PunishInsolence(F.State, Koschei, FPendingPunishment{1, EInsolencePunishment::Light}, F.Now);
	TestEqual(TEXT("Sin efecto aplicado"), F.Since(Before, EMatchEventType::EffectApplied).Num(), 0);
	TestEqual(TEXT("Burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceTableTest, "Soulbound.Core.Insolence.GravedadPorLaTabla",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceTableTest::RunTest(const FString& Parameters)
{
	// CA-INS-1 (R-INS-2): la gravedad se sortea con la fila del nivel anterior a la subida: 0 y 1, 70/25/5; 2 y 3, 30/50/20;
	// 4, la Furia sin sorteo. 2000 sorteos por nivel, con margen.
	FWardenParams Params;
	if (!Soulbound::Tests::LoadTestWardenParams(*this, Params))
	{
		return false;
	}
	struct FExpected
	{
		int32 Level;
		double Light, Medium, Severe;
	};
	for (const FExpected& Row : {FExpected{0, 0.70, 0.25, 0.05}, FExpected{1, 0.70, 0.25, 0.05}, FExpected{2, 0.30, 0.50, 0.20}, FExpected{3, 0.30, 0.50, 0.20}})
	{
		FRandomStream Random(100 + Row.Level);
		int32 Counts[4] = {};
		constexpr int32 Draws = 2000;
		for (int32 Draw = 0; Draw < Draws; ++Draw)
		{
			++Counts[AsInt(Soulbound::Rules::DrawInsolencePunishment(Params, Row.Level, Random))];
		}
		TestEqual(FString::Printf(TEXT("Nivel %d: nunca la Furia"), Row.Level), Counts[AsInt(EInsolencePunishment::Fury)], 0);
		TestTrue(FString::Printf(TEXT("Nivel %d: leve %.2f"), Row.Level, Counts[0] / double(Draws)), FMath::Abs(Counts[0] / double(Draws) - Row.Light) < 0.04);
		TestTrue(FString::Printf(TEXT("Nivel %d: media %.2f"), Row.Level, Counts[1] / double(Draws)), FMath::Abs(Counts[1] / double(Draws) - Row.Medium) < 0.04);
		TestTrue(FString::Printf(TEXT("Nivel %d: grave %.2f"), Row.Level, Counts[2] / double(Draws)), FMath::Abs(Counts[2] / double(Draws) - Row.Severe) < 0.03);
	}
	FRandomStream Random(9);
	TestEqual(TEXT("Nivel 4: la Furia"), AsInt(Soulbound::Rules::DrawInsolencePunishment(Params, 4, Random)), AsInt(EInsolencePunishment::Fury));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceClockTest, "Soulbound.Core.Insolence.RelojAgotadoCastigaAlTerminarElTurno",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceClockTest::RunTest(const FString& Parameters)
{
	// CA-INS-1 (R-INS-1, R-INS-2): con insolencia 0 y el reloj de turno agotado, sube a 1 y trae un castigo de la tabla del nivel
	// 0 sobre él, aplicado al terminar el turno (entre turnos, por el camino de un efecto de evento) y nunca un regalo. Treinta
	// semillas del Warden.
	int32 Applied = 0;
	for (int32 Seed = 1; Seed <= 30; ++Seed)
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.State.Warden.Seed = Seed;
		RichAndAway(F, Koschei);
		F.BeginTurn(Koschei);
		const int32 Before = F.State.EventLog.Num();
		for (int32 Step = 0; Step < 4 && F.State.TurnPhase != ETurnPhase::End; ++Step)
		{
			F.Now = F.State.TurnDeadlineSeconds;
			F.Advance();
		}
		TestEqual(FString::Printf(TEXT("Semilla %d: el reloj se agota"), Seed), F.Since(Before, EMatchEventType::TurnClockExpired).Num(), 1);
		TestEqual(FString::Printf(TEXT("Semilla %d: insolencia 1"), Seed), F.State.Seats[Koschei].Insolence, 1);
		TestEqual(FString::Printf(TEXT("Semilla %d: con el turno aún sin terminar, sin castigo"), Seed), F.Since(Before, EMatchEventType::InsolencePunished).Num(), 0);
		F.Advance();
		const int32 Ended = FindFrom(F.State, Before, EMatchEventType::TurnEnded);
		const int32 Punished = FindFrom(F.State, Before, EMatchEventType::InsolencePunished);
		if (!TestTrue(FString::Printf(TEXT("Semilla %d: castigado al terminar el turno"), Seed), Ended != INDEX_NONE && Punished > Ended))
		{
			continue;
		}
		const FMatchEvent& Event = F.State.EventLog[Punished];
		TestEqual(FString::Printf(TEXT("Semilla %d: al insolente"), Seed), Event.Seat, Koschei);
		TestEqual(FString::Printf(TEXT("Semilla %d: con el nivel alcanzado"), Seed), Event.Value, 1);
		TestNotEqual(FString::Printf(TEXT("Semilla %d: del nivel 0, nunca la Furia"), Seed), Event.Detail, AsInt(EInsolencePunishment::Fury));
		const int32 Effect = PunishmentEffect(F.State, Punished);
		if (Effect != INDEX_NONE)
		{
			++Applied;
			TestTrue(FString::Printf(TEXT("Semilla %d: un castigo de la tabla, nunca un regalo"), Seed), AllPunishments.Contains(static_cast<EEffect>(Effect))
				&& !Soulbound::Rules::IsGift(static_cast<EEffect>(Effect)));
			TestEqual(FString::Printf(TEXT("Semilla %d: sobre él"), Seed), F.State.EventLog[Punished + 1].Seat, Koschei);
		}
	}
	TestTrue(TEXT("Los castigos se aplican"), Applied > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceInvalidTest, "Soulbound.Core.Insolence.CastigoNoValidoBuscaOtroDeLaMismaGravedad",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceInvalidTest::RunTest(const FString& Parameters)
{
	// CA-INS-1 (R-INS-2): un castigo que no vale sobre él (Vuelta al hogar estando en casa) se cambia por otro de la misma
	// gravedad; sin ninguno válido, burla. Aquí la gravedad es siempre la media.
	TSet<int32> Seen;
	for (int32 Seed = 1; Seed <= 30; ++Seed)
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		ForceSeverity(F, EInsolencePunishment::Medium);
		F.State.Warden.Seed = Seed;
		const int32 Before = F.State.EventLog.Num();
		Soulbound::Rules::RaiseInsolence(F.State, Koschei, F.Now);
		const int32 Punished = FindFrom(F.State, Before, EMatchEventType::InsolencePunished);
		if (!TestTrue(FString::Printf(TEXT("Semilla %d: entre turnos, castigado en el acto"), Seed), Punished != INDEX_NONE))
		{
			continue;
		}
		const int32 Effect = PunishmentEffect(F.State, Punished);
		TestNotEqual(FString::Printf(TEXT("Semilla %d: estando en casa, nunca Vuelta al hogar"), Seed), Effect, AsInt(EEffect::SendHome));
		TestTrue(FString::Printf(TEXT("Semilla %d: otro de la media"), Seed), Effect == AsInt(EEffect::LoseTurn) || Effect == AsInt(EEffect::WardenTrial));
		Seen.Add(Effect);
	}
	TestEqual(TEXT("Salen los dos que valen"), Seen.Num(), 2);

	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	ForceSeverity(F, EInsolencePunishment::Light);
	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::RaiseInsolence(F.State, Koschei, F.Now);
	TestEqual(TEXT("Leve sin runas (ni Revelar Brazal ni Alto valen): burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Y ningún efecto"), F.Since(Before, EMatchEventType::EffectApplied).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFirstActTest, "Soulbound.Core.Insolence.GraveEnElActoIEsVueltaAlHogar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFirstActTest::RunTest(const FString& Parameters)
{
	// CA-INS-1 (R-INS-2, R-CAR-7): en el Acto I, lo grave es Vuelta al hogar (en la carrera nadie va al Vacío salvo la Furia);
	// fuera de él, Perder una runa o el Vacío.
	FWardenFixture F;
	if (!F.StartInRace(*this))
	{
		return false;
	}
	ForceSeverity(F, EInsolencePunishment::Severe);
	const TArray<int32> Away = F.NormalTiles(FenrirDomain);
	if (!TestTrue(TEXT("Hay casillas fuera del Altar"), !Away.IsEmpty()))
	{
		return false;
	}
	F.State.Seats[Koschei].TileId = Away[0];
	F.Now = 60.0;
	int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::RaiseInsolence(F.State, Koschei, F.Now);
	int32 Punished = FindFrom(F.State, Before, EMatchEventType::InsolencePunished);
	TestEqual(TEXT("Acto I: Vuelta al hogar"), Punished == INDEX_NONE ? INDEX_NONE : PunishmentEffect(F.State, Punished), AsInt(EEffect::SendHome));
	TestEqual(TEXT("Al Altar, su hogar en la carrera"), F.State.Seats[Koschei].TileId, F.State.Board.FindAltar());

	FWardenFixture After;
	if (!After.StartWithWarden(*this))
	{
		return false;
	}
	ForceSeverity(After, EInsolencePunishment::Severe);
	RichAndAway(After, Koschei);
	Before = After.State.EventLog.Num();
	Soulbound::Rules::RaiseInsolence(After.State, Koschei, After.Now);
	Punished = FindFrom(After.State, Before, EMatchEventType::InsolencePunished);
	const int32 Effect = Punished == INDEX_NONE ? INDEX_NONE : PunishmentEffect(After.State, Punished);
	TestTrue(TEXT("Acto II: Perder una runa o el Vacío"), Effect == AsInt(EEffect::LoseRune) || Effect == AsInt(EEffect::Banish));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceVoidTest, "Soulbound.Core.Insolence.EnElVacioSeAplazaALaSalida",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceVoidTest::RunTest(const FString& Parameters)
{
	// CA-INS-1 (R-INS-2): a quien está en el Vacío el castigo se le aplaza a su salida, con la gravedad ya sorteada.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	ForceSeverity(F, EInsolencePunishment::Medium);
	F.MoveAway(Koschei);
	Soulbound::Rules::Banish(F.State, Koschei, false, F.Now);
	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::RaiseInsolence(F.State, Koschei, F.Now);
	TestEqual(TEXT("Insolencia 1"), F.State.Seats[Koschei].Insolence, 1);
	TestEqual(TEXT("Sin castigo dentro"), F.Since(Before, EMatchEventType::InsolencePunished).Num(), 0);
	if (TestEqual(TEXT("Aplazado a la salida"), F.State.Seats[Koschei].VoidPunishments.Num(), 1))
	{
		TestEqual(TEXT("Con su gravedad"), AsInt(F.State.Seats[Koschei].VoidPunishments[0].Kind), AsInt(EInsolencePunishment::Medium));
	}
	F.Now += 1.0;
	Soulbound::Rules::ReleaseVoid(F.State, F.Now);
	const int32 Left = FindFrom(F.State, Before, EMatchEventType::LeftVoid);
	const int32 Punished = FindFrom(F.State, Before, EMatchEventType::InsolencePunished);
	TestTrue(TEXT("Al salir, el castigo"), Left != INDEX_NONE && Punished > Left);
	TestTrue(TEXT("Ya no espera"), F.State.Seats[Koschei].VoidPunishments.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceIgnoredTest, "Soulbound.Core.Insolence.DesatencionCastigaEnElActo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceIgnoredTest::RunTest(const FString& Parameters)
{
	// R-INS-2 (decisión del autor del 19/10/2026): al desatender una elección de un evento, primero lo que el Warden hace en su
	// lugar (R-DIR-4) y en el mismo instante el castigo; si eso lo deja en juego (un Duelo contra el Warden), el castigo espera a
	// que acabe.
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.Director(Fenrir);
		const int32 Before = F.State.EventLog.Num();
		F.Launch(TEXT("dos-turnos-o-casa"));
		F.Expire();
		const int32 Chosen = FindFrom(F.State, Before, EMatchEventType::DilemmaChosen);
		const int32 Punished = FindFrom(F.State, Before, EMatchEventType::InsolencePunished);
		TestTrue(TEXT("Dilema: primero se resuelve en contra, después el castigo"), Chosen != INDEX_NONE && Punished > Chosen);
		if (Punished != INDEX_NONE)
		{
			TestEqual(TEXT("En el mismo instante"), F.State.EventLog[Punished].ClockSeconds, F.State.EventLog[Chosen].ClockSeconds);
			TestEqual(TEXT("Al que no contestó"), F.State.EventLog[Punished].Seat, Fenrir);
		}
	}
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.State.Seats[Fenrir].Runes[0] = 1;
		F.State.Seats[Koschei].Runes[0] = 1;
		F.Director(Fenrir);
		const int32 Before = F.State.EventLog.Num();
		F.Launch(TEXT("duelo-con-quien-senales"));
		F.Expire();
		TestEqual(TEXT("Duelo contra el Warden en curso"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
		TestEqual(TEXT("Mientras dura, sin castigo"), F.Since(Before, EMatchEventType::InsolencePunished).Num(), 0);
		F.Perform(Fenrir, 0.0);
		F.Advance();
		TestEqual(TEXT("Al acabar, el castigo"), F.Since(Before, EMatchEventType::InsolencePunished).Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFuryTest, "Soulbound.Core.Insolence.FuriaANivelCinco",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFuryTest::RunTest(const FString& Parameters)
{
	// CA-INS-2 (R-INS-3): a 4 y otra insolencia, la Furia sin sorteo: al Vacío de la Furia, insolencia 0. También durante la
	// carrera (la única excepción a R-CAR-7). Y por el reloj de turno, al terminar ese turno.
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.State.Seats[Koschei].Insolence = 4;
		const int32 Before = F.State.EventLog.Num();
		Soulbound::Rules::RaiseInsolence(F.State, Koschei, F.Now);
		const int32 Fury = FindFrom(F.State, Before, EMatchEventType::FuryUnleashed);
		const int32 Banished = FindFrom(F.State, Before, EMatchEventType::Banished);
		TestTrue(TEXT("La Furia y después el destierro"), Fury != INDEX_NONE && Banished > Fury);
		if (Banished != INDEX_NONE)
		{
			TestEqual(TEXT("Destierro de la Furia"), F.State.EventLog[Banished].Value, 1);
		}
		TestTrue(TEXT("En el Vacío de la Furia"), F.State.Seats[Koschei].IsInVoid() && F.State.Seats[Koschei].bFuryVoid);
		TestEqual(TEXT("Insolencia 0"), F.State.Seats[Koschei].Insolence, 0);
	}
	{
		FWardenFixture F;
		if (!F.StartInRace(*this))
		{
			return false;
		}
		F.State.Seats[Koschei].Insolence = 4;
		Soulbound::Rules::RaiseInsolence(F.State, Koschei, 60.0);
		TestTrue(TEXT("En la carrera también: Vacío de la Furia"), F.State.Seats[Koschei].IsInVoid() && F.State.Seats[Koschei].bFuryVoid);
	}
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.State.Seats[Koschei].Insolence = 4;
		F.BeginTurn(Koschei);
		F.Advance();
		F.Now = F.State.TurnDeadlineSeconds;
		F.Advance();
		TestFalse(TEXT("Reloj agotado: en su turno aún no"), F.State.Seats[Koschei].IsInVoid());
		F.Advance();
		TestTrue(TEXT("Al terminar el turno, la Furia"), F.State.Seats[Koschei].bFuryVoid && F.State.Seats[Koschei].IsInVoid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFuryDeflectionTest, "Soulbound.Core.Insolence.DesvioEnLaFuriaNoSaca",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFuryDeflectionTest::RunTest(const FString& Parameters)
{
	// CA-INS-2 (R-INS-3): en el Vacío de la Furia, Desvío se ofrece pero no funciona: la runa no se consume (FuryRefusal) y sigue
	// la escalera de escape.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.GivePower(Fenrir, EPower::Deflection);
	F.State.Seats[Fenrir].Insolence = 4;
	Soulbound::Rules::RaiseInsolence(F.State, Fenrir, F.Now);
	if (!TestTrue(TEXT("Fenrir en el Vacío de la Furia"), F.State.Seats[Fenrir].bFuryVoid))
	{
		return false;
	}
	F.BeginTurn(Fenrir);
	TestEqual(TEXT("Desvío se ofrece"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Deflection));
	const int32 Before = F.State.EventLog.Num();
	TestEqual(TEXT("Lo usa"), AsInt(F.Answer(1)), AsInt(EIntentResult::Accepted));
	const TArray<FMatchEvent> Refusals = F.Since(Before, EMatchEventType::FuryRefusal);
	if (TestEqual(TEXT("El Warden se niega"), Refusals.Num(), 1))
	{
		TestEqual(TEXT("Por Desvío"), Refusals[0].Value, 0);
	}
	TestEqual(TEXT("La runa no se consume"), F.RuneTotal(Fenrir), 1);
	TestTrue(TEXT("Sigue dentro"), F.State.Seats[Fenrir].IsInVoid());
	TestEqual(TEXT("Y juega el escape"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::VoidEscape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFuryReleaseTest, "Soulbound.Core.Insolence.LiberacionDejaDentroAlDeLaFuria",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFuryReleaseTest::RunTest(const FString& Parameters)
{
	// CA-INS-2 (R-INS-3): una Liberación saca a todos menos al de la Furia (FuryRefusal). Con solo él dentro, no es válida: burla
	// (decisión del autor, plan del PR A).
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.State.Seats[Fenrir].Insolence = 4;
	Soulbound::Rules::RaiseInsolence(F.State, Fenrir, F.Now);
	F.MoveAway(Koschei);
	Soulbound::Rules::Banish(F.State, Koschei, false, F.Now);
	const int32 Before = F.State.EventLog.Num();
	TestEqual(TEXT("Liberación"), AsInt(Soulbound::Rules::ReleaseVoid(F.State, F.Now)), AsInt(EEffectResult::Applied));
	TestFalse(TEXT("Koschei sale"), F.State.Seats[Koschei].IsInVoid());
	TestTrue(TEXT("Fenrir se queda en el Vacío de la Furia"), F.State.Seats[Fenrir].IsInVoid() && F.State.Seats[Fenrir].bFuryVoid);
	const TArray<FMatchEvent> Refusals = F.Since(Before, EMatchEventType::FuryRefusal);
	if (TestEqual(TEXT("El Warden se niega"), Refusals.Num(), 1))
	{
		TestEqual(TEXT("A Fenrir, por la Liberación"), Refusals[0].Seat, Fenrir);
		TestEqual(TEXT("Value 1"), Refusals[0].Value, 1);
	}
	TestEqual(TEXT("Solo él dentro: la Liberación no vale"), AsInt(Soulbound::Rules::CanReleaseVoid(F.State)), AsInt(EEffectResult::NobodyInVoid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFuryEscapeTest, "Soulbound.Core.Insolence.EscapeSacaDeLaFuria",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFuryEscapeTest::RunTest(const FString& Parameters)
{
	// CA-INS-2 (R-INS-3, R-VAC-2): del Vacío de la Furia se sale como siempre, superando el escape. Desde la decisión del autor
	// del 20/10/2026, con la escalera de la Furia (LaFuriaIgualaATodos); hasta entonces, con la de cada uno.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.State.Seats[Fenrir].Insolence = 4;
	Soulbound::Rules::RaiseInsolence(F.State, Fenrir, F.Now);
	F.BeginTurn(Fenrir);
	TestEqual(TEXT("Escape del Vacío"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::VoidEscape));
	TestEqual(TEXT("Con la escalera de la Furia"), AsInt(F.State.PendingDifficulty),
		F.State.Params.FuryEscapeLadder.IsValidIndex(0) ? AsInt(F.State.Params.FuryEscapeLadder[0]) : INDEX_NONE);
	TestEqual(TEXT("Lo supera"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, Fenrir, true, F.Now)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("Sale"), F.State.Seats[Fenrir].IsInVoid());
	TestFalse(TEXT("Y su Vacío deja de ser de la Furia"), F.State.Seats[Fenrir].bFuryVoid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFuryInsideTest, "Soulbound.Core.Insolence.FuriaSobreQuienYaEstaDentro",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFuryInsideTest::RunTest(const FString& Parameters)
{
	// CA-INS-2 (R-INS-3): la Furia sobre quien ya está en el Vacío: su Vacío pasa a ser de la Furia y su cuenta se reinicia.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.MoveAway(Koschei);
	Soulbound::Rules::Banish(F.State, Koschei, false, F.Now);
	F.State.Seats[Koschei].VoidTurns = 2;
	F.State.Seats[Koschei].Insolence = 4;
	const int32 Before = F.State.EventLog.Num();
	Soulbound::Rules::RaiseInsolence(F.State, Koschei, F.Now);
	TestEqual(TEXT("La Furia"), F.Since(Before, EMatchEventType::FuryUnleashed).Num(), 1);
	TestEqual(TEXT("Sin otro destierro"), F.Since(Before, EMatchEventType::Banished).Num(), 0);
	TestTrue(TEXT("Su Vacío es de la Furia"), F.State.Seats[Koschei].IsInVoid() && F.State.Seats[Koschei].bFuryVoid);
	TestEqual(TEXT("La cuenta vuelve a 0"), F.State.Seats[Koschei].VoidTurns, 0);
	TestEqual(TEXT("Insolencia 0"), F.State.Seats[Koschei].Insolence, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInsolenceFuryEqualsAllTest, "Soulbound.Core.Insolence.LaFuriaIgualaATodos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInsolenceFuryEqualsAllTest::RunTest(const FString& Parameters)
{
	// R-INS-3 (decisión del autor del 20/10/2026): la Furia castiga al jugador, no al personaje; el Vacío de la Furia es otro sitio
	// con sus reglas e iguala a todos. Un Eco no sale en su primer turno sin jugar (R-ECO-5) y Mictlantecuhtli no usa su escalera:
	// los dos juegan la de la Furia (furyEscapeLadder: difícil, difícil, normal) y salen al cuarto turno, como siempre. Fuera de
	// la Furia, el Eco sale sin jugar y Mictlantecuhtli juega fácil.
	const ESkillDifficulty Ladder[] = {ESkillDifficulty::Hard, ESkillDifficulty::Hard, ESkillDifficulty::Normal};
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		// Fenrir con la afinidad de Mictlantecuhtli: lo que el test necesita es su escalera.
		F.State.Elects.Elects[FenrirDomain].Affinity = EAffinity::VoidEscape;
		for (const int32 Seat : {FWardenFixture::EchoA, Fenrir})
		{
			const FString Who = Seat == Fenrir ? TEXT("Mictlantecuhtli") : TEXT("Eco");
			F.State.Seats[Seat].Insolence = 4;
			Soulbound::Rules::RaiseInsolence(F.State, Seat, F.Now);
			if (!TestTrue(FString::Printf(TEXT("%s: en el Vacío de la Furia"), *Who), F.State.Seats[Seat].IsInVoid() && F.State.Seats[Seat].bFuryVoid))
			{
				continue;
			}
			for (int32 Turn = 1; Turn <= 3; ++Turn)
			{
				F.BeginTurn(Seat);
				TestEqual(FString::Printf(TEXT("%s, turno %d dentro: juega el escape"), *Who, Turn), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::VoidEscape));
				TestEqual(FString::Printf(TEXT("%s, turno %d dentro: la dificultad de la escalera de la Furia"), *Who, Turn), AsInt(F.State.PendingDifficulty), AsInt(Ladder[Turn - 1]));
				Soulbound::Rules::SubmitSkillTrialResult(F.State, Seat, false, F.Now);
				TestTrue(FString::Printf(TEXT("%s, turno %d dentro: lo falla y sigue dentro"), *Who, Turn), F.State.Seats[Seat].IsInVoid());
			}
			const int32 Before = F.State.EventLog.Num();
			F.BeginTurn(Seat);
			const TArray<FMatchEvent> Left = F.Since(Before, EMatchEventType::LeftVoid);
			if (TestEqual(FString::Printf(TEXT("%s: al cuarto turno sale"), *Who), Left.Num(), 1))
			{
				TestEqual(FString::Printf(TEXT("%s: sin jugar, por el tope"), *Who), Left[0].Value, AsInt(EVoidExit::TurnLimit));
			}
		}
	}
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.State.Elects.Elects[FenrirDomain].Affinity = EAffinity::VoidEscape;
		F.MoveAway(FWardenFixture::EchoA);
		Soulbound::Rules::Banish(F.State, FWardenFixture::EchoA, false, F.Now);
		int32 Before = F.State.EventLog.Num();
		F.BeginTurn(FWardenFixture::EchoA);
		const TArray<FMatchEvent> Left = F.Since(Before, EMatchEventType::LeftVoid);
		TestTrue(TEXT("Fuera de la Furia, el Eco sale en su primer turno sin jugar"), Left.Num() == 1 && Left[0].Value == AsInt(EVoidExit::Echo));
		F.MoveAway(Fenrir);
		Soulbound::Rules::Banish(F.State, Fenrir, false, F.Now);
		F.BeginTurn(Fenrir);
		TestEqual(TEXT("Fuera de la Furia, Mictlantecuhtli juega fácil"), AsInt(F.State.PendingDifficulty), AsInt(ESkillDifficulty::Easy));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
