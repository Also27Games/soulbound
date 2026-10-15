// Tests de los efectos encadenados (S-015, GDD 05 y 04): la Prueba del Warden (R-EFE-10), el Robo decretado (R-EFE-12), el
// Duelo forzado (R-EFE-13), el Duelo contra el Warden y "el Warden te roba" (R-DIR-4), y cómo un duelo o un trilero de un
// evento se juega entre turnos sin terminar ningún turno (R-INT-3). Cada test cita el criterio (CA-XXX-n) y la regla
// (R-XXX-n) que cubre. Filtro: "Soulbound.Core.ChainedEffects".
//
// Montaje: EventFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos).

#include "EventFixture.h"
#include "Match/DuelRules.h"
#include "Match/EffectCatalog.h"
#include "Match/EffectRules.h"
#include "Match/TurnRules.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FEventFixture;

	constexpr int32 Fenrir = FEventFixture::Fenrir;
	constexpr int32 Koschei = FEventFixture::Koschei;
	constexpr int32 EchoA = FEventFixture::EchoA;
	constexpr int32 EchoB = FEventFixture::EchoB;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** Suma de las reservas de todas las Fuentes: sube una por cada runa que vuelve a su Fuente. */
	int32 TotalReserve(const FMatchState& State)
	{
		int32 Total = 0;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			Total += Tile.Type == ETileType::PowerSource ? Tile.RuneReserve : 0;
		}
		return Total;
	}

	/** Juega el trilero en curso hasta el final: deja pasar la muestra y la mezcla y contesta cada elección con su primera opción. */
	void PlayShellGame(FEventFixture& F)
	{
		for (int32 Step = 0; Step < 24 && F.State.ShellGame.IsActive(); ++Step)
		{
			if (F.State.PendingChoice.IsOpen())
			{
				F.Choose(F.State.PendingChoice.Seat, F.State.PendingChoice.Kind, F.State.PendingChoice.Options[0]);
			}
			else if (F.State.ShellGame.Stage == EShellGameStage::Reveal || F.State.ShellGame.Stage == EShellGameStage::Shuffle)
			{
				F.Now = FMath::Max(F.Now, F.State.ShellGame.StageDeadlineSeconds);
			}
			F.Advance();
		}
	}

	/** Asiento con la afinidad DuelAdvantage (Shuten-dōji): su Dominio, o INDEX_NONE. */
	int32 DuelAdvantageDomain(const FMatchState& State)
	{
		for (int32 Domain = 0; Domain < State.Elects.Elects.Num(); ++Domain)
		{
			if (State.Elects.Elects[Domain].Affinity == EAffinity::DuelAdvantage)
			{
				return Domain;
			}
		}
		return INDEX_NONE;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Prueba del Warden (R-EFE-10)
// ---------------------------------------------------------------------------------------------------------------------

// CA-EFE-7, R-EFE-10: la Prueba queda pendiente en el asiento señalado; al empezar su turno la juega antes de tirar con la
// dificultad de datos; fallada, el turno termina sin tirada y el sorteo de la partida no se ha movido.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenTrialFailTest, "Soulbound.Core.ChainedEffects.WardenTrial.FallaYElTurnoTerminaSinTirar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenTrialFailTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Decree(Fenrir, App(EEffect::WardenTrial, EEffectTarget::ChosenRival));
	TestEqual(TEXT("Señala quien dirige"), F.State.PendingChoice.Seat, Fenrir);
	TestEqual(TEXT("Contesta"), AsInt(F.Answer(EchoA)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("La Prueba queda pendiente en el señalado"), F.State.Seats[EchoA].bWardenTrialPending);
	TestEqual(TEXT("Y queda en el registro"), F.Since(0, EMatchEventType::WardenTrialPending).Num(), 1);
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());

	const int32 SeedBefore = F.Random.GetCurrentSeed();
	F.BeginTurn(EchoA);
	TestEqual(TEXT("Antes de tirar, la prueba"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
	TestEqual(TEXT("Es la Prueba del Warden"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::WardenTrial));
	TestEqual(TEXT("Con la dificultad de datos"), AsInt(F.State.PendingDifficulty), AsInt(F.State.Params.WardenTrialDifficulty));
	TestEqual(TEXT("Que es normal"), AsInt(F.State.PendingDifficulty), AsInt(ESkillDifficulty::Normal));

	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("El asiento la contesta"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, EchoA, false, F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Registra que falla"), F.Since(From, EMatchEventType::WardenTrialFailed).Num(), 1);
	TestFalse(TEXT("Ya no está pendiente"), F.State.Seats[EchoA].bWardenTrialPending);
	TestEqual(TEXT("Sin prueba en curso"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));
	F.Advance();
	TestEqual(TEXT("Sin tirada"), F.Since(From, EMatchEventType::Rolled).Num(), 0);
	TestEqual(TEXT("El sorteo de la partida no se ha movido"), F.Random.GetCurrentSeed(), SeedBefore);
	return true;
}

// CA-EFE-7, R-EFE-10: superada, el turno sigue con normalidad (tira); y la Prueba es una sola vez.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenTrialPassTest, "Soulbound.Core.ChainedEffects.WardenTrial.SuperaYTiraConNormalidad",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenTrialPassTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Decree(Fenrir, App(EEffect::WardenTrial, EEffectTarget::ChosenRival));
	F.Answer(EchoB);
	F.BeginTurn(EchoB);
	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("La supera"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, EchoB, true, F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Registra que la supera"), F.Since(From, EMatchEventType::WardenTrialPassed).Num(), 1);
	TestFalse(TEXT("Ya no está pendiente"), F.State.Seats[EchoB].bWardenTrialPending);
	TestEqual(TEXT("El turno sigue: decisiones previas a la tirada"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::PreRoll));
	F.Advance();
	TestEqual(TEXT("Tira"), F.Since(From, EMatchEventType::Rolled).Num(), 1);
	TestEqual(TEXT("Y espera destino"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose));

	// Una sola vez: el siguiente turno de ese asiento se juega sin Prueba.
	F.FinishTurn();
	F.BeginTurn(EchoB);
	TestEqual(TEXT("El turno siguiente, sin Prueba"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::PreRoll));
	return true;
}

// CA-EFE-7, R-EFE-10: desterrado antes de su turno, la Prueba se anula; salir del Vacío no la devuelve.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenTrialVoidTest, "Soulbound.Core.ChainedEffects.WardenTrial.EntrarEnElVacioLaAnula",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenTrialVoidTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Decree(Fenrir, App(EEffect::WardenTrial, EEffectTarget::ChosenRival));
	F.Answer(EchoA);
	TestTrue(TEXT("Pendiente"), F.State.Seats[EchoA].bWardenTrialPending);
	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("Es desterrado"), AsInt(Soulbound::Rules::Banish(F.State, EchoA, false, F.Now)), AsInt(EEffectResult::Applied));
	TestFalse(TEXT("Ya no está pendiente"), F.State.Seats[EchoA].bWardenTrialPending);
	TestEqual(TEXT("Queda anulada en el registro"), F.Since(From, EMatchEventType::WardenTrialCancelled).Num(), 1);

	F.BeginTurn(EchoA);
	TestEqual(TEXT("Un Eco sale del Vacío sin jugar, sin Prueba"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Ninguna prueba"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));
	TestEqual(TEXT("Ni superada ni fallada"), F.Since(From, EMatchEventType::WardenTrialPassed).Num() + F.Since(From, EMatchEventType::WardenTrialFailed).Num(), 0);
	return true;
}

// CA-EFE-7, R-EFE-10: con un turno perdido, el turno se salta y la Prueba sigue pendiente para el siguiente turno jugado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenTrialLostTurnTest, "Soulbound.Core.ChainedEffects.WardenTrial.UnTurnoPerdidoLaDejaPendiente",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenTrialLostTurnTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Decree(Fenrir, App(EEffect::WardenTrial, EEffectTarget::ChosenRival));
	F.Answer(EchoA);
	Soulbound::Rules::GrantLostTurns(F.State, EchoA, 1);

	const int32 From = F.State.EventLog.Num();
	F.State.ActiveSeat = EchoA;
	F.State.TurnPhase = ETurnPhase::BetweenTurns;
	F.Advance();
	TestEqual(TEXT("Se salta su turno"), F.Since(From, EMatchEventType::TurnSkipped).Num(), 1);
	TestTrue(TEXT("La Prueba sigue pendiente"), F.State.Seats[EchoA].bWardenTrialPending);
	TestEqual(TEXT("Y no se ha jugado"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));

	F.BeginTurn(EchoA);
	TestEqual(TEXT("En su siguiente turno jugado, la Prueba"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::WardenTrial));
	return true;
}

// CA-EFE-7, R-EFE-10: como mucho una; una segunda sobre el mismo asiento no es válida y es burla; en el Vacío no es objetivo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenTrialSecondTest, "Soulbound.Core.ChainedEffects.WardenTrial.UnaSegundaNoEsValida",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenTrialSecondTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const FEffectApplication Trial = App(EEffect::WardenTrial, EEffectTarget::ActivePlayer);
	F.Decree(EchoA, Trial);
	TestTrue(TEXT("La primera queda pendiente"), F.State.Seats[EchoA].bWardenTrialPending);
	TestEqual(TEXT("Una segunda no es válida"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::WardenTrial, EchoA, EchoA)), AsInt(EEffectResult::AlreadyPending));

	const int32 From = F.State.EventLog.Num();
	F.Decree(EchoA, Trial);
	TestEqual(TEXT("Burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin una segunda Prueba"), F.Since(From, EMatchEventType::WardenTrialPending).Num(), 0);
	TestTrue(TEXT("Sigue pendiente la primera"), F.State.Seats[EchoA].bWardenTrialPending);

	Soulbound::Rules::Banish(F.State, EchoB, false, F.Now);
	TestEqual(TEXT("En el Vacío no es objetivo"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::WardenTrial, EchoB, Fenrir)), AsInt(EEffectResult::InVoid));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Robo decretado (R-EFE-12)
// ---------------------------------------------------------------------------------------------------------------------

// CA-EFE-9, R-EFE-12, R-INT-3: "a todos" con tres rivales y uno sin runas: dos trileros, uno tras otro y en orden de asiento;
// el ladrón tiene dos runas más. Los trileros son entre turnos: no cambian TurnCount ni el último que jugó.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedDecreedTheftAllTest, "Soulbound.Core.ChainedEffects.DecreedTheft.ATodosUnTileroPorRivalConRunas",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedDecreedTheftAllTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	// Roba quien juega justo antes de Koschei: así a Koschei, sin runas, le toca el siguiente turno sin ser parte del evento.
	const int32 Thief = F.PreviousOf(Koschei);
	TArray<int32> Victims;
	for (int32 Seat = 0; Seat < F.State.Seats.Num(); ++Seat)
	{
		if (Seat != Thief && Seat != Koschei)
		{
			Victims.Add(Seat);
		}
	}
	F.State.Seats[Victims[0]].Runes[0] = 1;
	F.State.Seats[Victims[1]].Runes[2] = 1;
	const int32 TurnsBefore = F.State.TurnCount;
	const int32 SeedBefore = F.Random.GetCurrentSeed();
	const int32 From = F.State.EventLog.Num();
	F.Decree(Thief, App(EEffect::DecreedTheft, EEffectTarget::Everyone));

	TestEqual(TEXT("A quien le toca es Koschei"), F.State.ActiveSeat, Koschei);
	TestEqual(TEXT("El primero, en orden de asiento"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
	TestEqual(TEXT("Roba quien dirige"), F.State.ShellGame.Thief, Thief);
	TestEqual(TEXT("Al primero con runas"), F.State.ShellGame.Victim, Victims[0]);
	TestTrue(TEXT("El ladrón está en juego"), Soulbound::Rules::IsSeatInPlay(F.State, Thief));
	TestTrue(TEXT("La víctima está en juego"), Soulbound::Rules::IsSeatInPlay(F.State, Victims[0]));
	TestFalse(TEXT("A quien le toca el turno, que no participa, no lo está"), Soulbound::Rules::IsSeatInPlay(F.State, Koschei));
	TestFalse(TEXT("Ni la víctima del segundo"), Soulbound::Rules::IsSeatInPlay(F.State, Victims[1]));

	PlayShellGame(F);
	TestEqual(TEXT("Entre turnos otra vez"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestTrue(TEXT("El evento sigue"), F.State.CurrentEvent.IsActive());
	TestEqual(TEXT("Una runa"), F.RuneTotal(Thief), 1);
	TestFalse(TEXT("Nadie en juego"), Soulbound::Rules::IsSeatInPlay(F.State, Victims[0]));

	F.Advance();
	TestEqual(TEXT("El segundo trilero"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
	TestEqual(TEXT("Al segundo"), F.State.ShellGame.Victim, Victims[1]);
	PlayShellGame(F);
	TestEqual(TEXT("Dos runas más"), F.RuneTotal(Thief), 2);
	F.Advance();
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());

	const TArray<FMatchEvent> Started = F.Since(From, EMatchEventType::ShellGameStarted);
	if (TestEqual(TEXT("Dos trileros, ninguno por quien no tiene runas"), Started.Num(), 2))
	{
		TestEqual(TEXT("Uno tras otro, en orden de asiento"), Started[0].OtherSeat, Victims[0]);
		TestEqual(TEXT("El segundo"), Started[1].OtherSeat, Victims[1]);
	}
	TestEqual(TEXT("TurnCount no cambia"), F.State.TurnCount, TurnsBefore);
	TestEqual(TEXT("El último que jugó, tampoco"), F.State.LastTurnSeat, Thief);
	TestEqual(TEXT("Ni el sorteo de la partida"), F.Random.GetCurrentSeed(), SeedBefore);

	// El siguiente turno empieza después de que el evento termine.
	F.Advance();
	TestEqual(TEXT("Empieza el turno siguiente"), F.Since(From, EMatchEventType::TurnStarted).Num(), 1);
	TestEqual(TEXT("De quien le tocaba"), F.State.ActiveSeat, Koschei);
	return true;
}

// CA-EFE-9, R-EFE-12, R-ROB-5: con Koschei entre las víctimas, aparta una runa antes del trilero (y la conserva).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedDecreedTheftKoscheiTest, "Soulbound.Core.ChainedEffects.DecreedTheft.KoscheiApartaUna",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedDecreedTheftKoscheiTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	F.State.Seats[Koschei].Runes[3] = 1;
	F.State.Seats[EchoA].Runes[2] = 1;
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::DecreedTheft, EEffectTarget::Everyone));

	TestEqual(TEXT("Koschei, el primero por asiento, aparta"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::SafeRune));
	TestEqual(TEXT("La elección es suya"), F.State.PendingChoice.Seat, Koschei);
	TestEqual(TEXT("Con el trilero en curso entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
	PlayShellGame(F);
	TestEqual(TEXT("Koschei conserva la apartada"), F.RuneTotal(Koschei), 1);
	TestEqual(TEXT("El ladrón se lleva una"), F.RuneTotal(Fenrir), 1);
	TestEqual(TEXT("Una sola apartada"), F.Since(From, EMatchEventType::SafeRuneSetAside).Num(), 1);

	F.Advance();
	PlayShellGame(F);
	TestEqual(TEXT("Y la de EchoA"), F.RuneTotal(Fenrir), 2);
	return true;
}

// CA-EFE-9, R-EFE-12, R-ROB-4: "rival señalado" ofrece todos los rivales del tablero, con o sin runas, nunca a quien está en el
// Vacío; señalar a uno sin runas es burla para el ladrón, sin trilero, y el evento sigue.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedDecreedTheftChosenTest, "Soulbound.Core.ChainedEffects.DecreedTheft.RivalSenaladoSinRunasEsBurla",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedDecreedTheftChosenTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	Soulbound::Rules::Banish(F.State, EchoA, false, F.Now);
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::DecreedTheft, EEffectTarget::ChosenRival));

	TestEqual(TEXT("Elige quien dirige"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	TestEqual(TEXT("Todos los rivales del tablero, con o sin runas, y no el del Vacío"), F.State.PendingChoice.Options, TArray<int32>({Koschei, EchoB}));
	TestEqual(TEXT("Señala a quien no tiene runas"), AsInt(F.Answer(EchoB)), AsInt(EIntentResult::Accepted));
	const TArray<FMatchEvent> NoRunes = F.Since(From, EMatchEventType::StealNoRunes);
	if (TestEqual(TEXT("Burla al ladrón (R-ROB-4)"), NoRunes.Num(), 1))
	{
		TestEqual(TEXT("El ladrón"), NoRunes[0].Seat, Fenrir);
		TestEqual(TEXT("La víctima"), NoRunes[0].OtherSeat, EchoB);
	}
	TestEqual(TEXT("Sin trilero"), F.Since(From, EMatchEventType::ShellGameStarted).Num(), 0);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestFalse(TEXT("El evento sigue por su siguiente aplicación y, sin más, termina"), F.State.CurrentEvent.IsActive());
	return true;
}

// CA-EFE-9, R-EFE-12: "al azar" elige uno entre los del tablero con runas, con el sorteo del evento, y roba con trilero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedDecreedTheftRandomTest, "Soulbound.Core.ChainedEffects.DecreedTheft.AlAzarEntreLosQueTienenRunas",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedDecreedTheftRandomTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[EchoB].Runes[2] = 1;
	const int32 SeedBefore = F.Random.GetCurrentSeed();
	F.Decree(Fenrir, App(EEffect::DecreedTheft, EEffectTarget::RandomPlayer));
	TestEqual(TEXT("El único con runas"), F.State.ShellGame.Victim, EchoB);
	PlayShellGame(F);
	TestEqual(TEXT("Roba su runa"), F.RuneTotal(Fenrir), 1);
	TestEqual(TEXT("Con el sorteo del evento, no el de la partida"), F.Random.GetCurrentSeed(), SeedBefore);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Duelo forzado (R-EFE-13)
// ---------------------------------------------------------------------------------------------------------------------

// CA-EFE-10, R-EFE-13: con AllRunes, todas las runas del perdedor pasan al ganador sin trilero, con un registro por runa. Solo
// se ofrecen rivales con runas (decisión del autor del 15/10/2026). El duelo es entre turnos y los dos están en juego.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedForcedDuelAllRunesTest, "Soulbound.Core.ChainedEffects.ForcedDuel.TodasLasRunasSinTrilero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedForcedDuelAllRunesTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[EchoA].Runes[0] = 2;
	F.State.Seats[EchoA].Runes[2] = 1;
	F.State.Seats[Fenrir].Runes[4] = 1;
	const int32 TurnsBefore = F.State.TurnCount;
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::AllRunes));

	TestEqual(TEXT("Solo se ofrece a quien tiene runas"), F.State.PendingChoice.Options, TArray<int32>({EchoA}));
	TestEqual(TEXT("Señala"), AsInt(F.Answer(EchoA)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("El duelo, entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
	TestEqual(TEXT("Quien dirige reta"), F.State.Duel.Challenger, Fenrir);
	TestEqual(TEXT("Al señalado"), F.State.Duel.Challenged, EchoA);
	TestTrue(TEXT("Todas las runas en juego"), F.State.Duel.bAllRunes);
	TestTrue(TEXT("El retador está en juego"), Soulbound::Rules::IsSeatInPlay(F.State, Fenrir));
	TestTrue(TEXT("El retado, también"), Soulbound::Rules::IsSeatInPlay(F.State, EchoA));
	const TArray<FMatchEvent> Challenged = F.Since(From, EMatchEventType::DuelChallenged);
	if (TestEqual(TEXT("Registra el reto"), Challenged.Num(), 1))
	{
		TestEqual(TEXT("Es un Duelo forzado"), Challenged[0].Value, 2);
	}

	TestEqual(TEXT("Rinde el retador"), AsInt(F.Perform(Fenrir, 0.9)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Rinde el retado"), AsInt(F.Perform(EchoA, 0.1)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Entre turnos, sin trilero"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestEqual(TEXT("El perdedor se queda sin runas"), F.RuneTotal(EchoA), 0);
	TestEqual(TEXT("El ganador las tiene todas"), F.RuneTotal(Fenrir), 4);
	const TArray<FMatchEvent> Seized = F.Since(From, EMatchEventType::RunesSeized);
	TestEqual(TEXT("Un registro por runa"), Seized.Num(), 3);
	for (const FMatchEvent& Event : Seized)
	{
		TestEqual(TEXT("Al ganador"), Event.Seat, Fenrir);
		TestEqual(TEXT("Desde el perdedor"), Event.OtherSeat, EchoA);
	}
	TestEqual(TEXT("Sin trilero"), F.Since(From, EMatchEventType::ShellGameStarted).Num(), 0);
	TestEqual(TEXT("No termina ningún turno"), F.State.TurnCount, TurnsBefore);
	F.Advance();
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());
	return true;
}

// CA-EFE-10, R-EFE-13, R-DUE-5: con OneRune, el ganador roba una con el trilero, sea retador o retado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedForcedDuelOneRuneTest, "Soulbound.Core.ChainedEffects.ForcedDuel.UnaRunaConTrilero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedForcedDuelOneRuneTest::RunTest(const FString& Parameters)
{
	for (const bool bChallengerWins : {true, false})
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		F.State.Seats[EchoA].Runes[0] = 2;
		F.State.Seats[Fenrir].Runes[4] = 2;
		F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::OneRune));
		F.Answer(EchoA);
		F.Perform(Fenrir, bChallengerWins ? 0.9 : 0.1);
		F.Perform(EchoA, bChallengerWins ? 0.1 : 0.9);
		const int32 Winner = bChallengerWins ? Fenrir : EchoA;
		const int32 Loser = bChallengerWins ? EchoA : Fenrir;
		TestEqual(TEXT("El botín es un trilero"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
		TestEqual(TEXT("Roba el ganador"), F.State.ShellGame.Thief, Winner);
		TestEqual(TEXT("Al perdedor"), F.State.ShellGame.Victim, Loser);
		PlayShellGame(F);
		TestEqual(TEXT("Entre turnos otra vez"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
		TestEqual(TEXT("El perdedor pierde una"), F.RuneTotal(Loser), 1);
		TestEqual(TEXT("El ganador gana una"), F.RuneTotal(Winner), 3);
		F.Advance();
		TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());
	}
	return true;
}

// CA-EFE-10, R-EFE-13, R-DUE-6: un retador sin runas que pierde vuelve a su hogar, sin botín.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedForcedDuelHomeTest, "Soulbound.Core.ChainedEffects.ForcedDuel.RetadorSinRunasQuePierdeVaASuHogar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedForcedDuelHomeTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.MoveAway(Fenrir);
	F.State.Seats[EchoA].Runes[0] = 1;
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::OneRune));
	F.Answer(EchoA);
	F.Perform(Fenrir, 0.0);
	F.Perform(EchoA, 1.0);
	TestEqual(TEXT("A su hogar"), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
	TestEqual(TEXT("Registrado"), F.Since(From, EMatchEventType::DuelLoserSentHome).Num(), 1);
	TestEqual(TEXT("Sin trilero"), F.Since(From, EMatchEventType::ShellGameStarted).Num(), 0);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestEqual(TEXT("El otro conserva su runa"), F.RuneTotal(EchoA), 1);
	return true;
}

// CA-EFE-10, R-EFE-13, R-DIR-1: "dos rivales señalados": dos elecciones seguidas, el primero señalado reta al segundo, y el
// segundo no se ofrece como opción de sí mismo ni al primero; quien dirige no se bate. Se filtra a quien no tiene runas.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedForcedDuelTwoChosenTest, "Soulbound.Core.ChainedEffects.ForcedDuel.DosRivalesSenalados",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedForcedDuelTwoChosenTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	F.State.Seats[EchoA].Runes[2] = 1;
	F.State.Seats[EchoB].Runes[3] = 1;
	F.State.Seats[Fenrir].Runes[4] = 1;
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::TwoChosenRivals, 1, EEffectVariant::AllRunes));

	TestEqual(TEXT("Primera señal"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	TestEqual(TEXT("La da quien dirige"), F.State.PendingChoice.Seat, Fenrir);
	TestEqual(TEXT("Rivales con runas, sin quien dirige"), F.State.PendingChoice.Options, TArray<int32>({Koschei, EchoA, EchoB}));
	TestEqual(TEXT("Señala al primero"), AsInt(F.Answer(EchoB)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Segunda señal"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	TestEqual(TEXT("La da también quien dirige"), F.State.PendingChoice.Seat, Fenrir);
	TestEqual(TEXT("Sin el primero"), F.State.PendingChoice.Options, TArray<int32>({Koschei, EchoA}));
	TestEqual(TEXT("Señala al segundo"), AsInt(F.Answer(Koschei)), AsInt(EIntentResult::Accepted));

	TestEqual(TEXT("El duelo"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
	TestEqual(TEXT("El primero reta"), F.State.Duel.Challenger, EchoB);
	TestEqual(TEXT("Al segundo"), F.State.Duel.Challenged, Koschei);
	TestFalse(TEXT("Quien dirige no se bate"), Soulbound::Rules::IsSeatInPlay(F.State, Fenrir));
	const TArray<FMatchEvent> Challenged = F.Since(From, EMatchEventType::DuelChallenged);
	if (TestEqual(TEXT("Registra el reto"), Challenged.Num(), 1))
	{
		TestEqual(TEXT("Retador"), Challenged[0].Seat, EchoB);
		TestEqual(TEXT("Retado"), Challenged[0].OtherSeat, Koschei);
	}
	return true;
}

// CA-EFE-10, R-EFE-13, R-DIR-3: a quien no tiene runas no se le señala; con menos de dos válidos es burla, sin elección.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedForcedDuelTooFewTest, "Soulbound.Core.ChainedEffects.ForcedDuel.MenosDeDosValidosEsBurla",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedForcedDuelTooFewTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::TwoChosenRivals, 1, EEffectVariant::AllRunes));
	TestEqual(TEXT("Burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestFalse(TEXT("Sin elección"), F.State.PendingChoice.IsOpen());
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());
	TestEqual(TEXT("Sin duelo"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));

	// Lo mismo con "dos al azar".
	FEventFixture G;
	if (!G.StartAfterRace(*this))
	{
		return false;
	}
	G.State.Seats[Koschei].Runes[0] = 1;
	const int32 FromG = G.State.EventLog.Num();
	G.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::RandomPlayer, 1, EEffectVariant::OneRune));
	TestEqual(TEXT("Dos al azar con uno válido: burla"), G.Since(FromG, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin duelo"), AsInt(G.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	return true;
}

// CA-EFE-10, R-EFE-13: "dos al azar": dos válidos con el sorteo del evento, distintos y rivales de quien dirige; el retador se
// sortea; misma semilla, mismo duelo; nada toca el sorteo de la partida.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedForcedDuelRandomTest, "Soulbound.Core.ChainedEffects.ForcedDuel.DosAlAzar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedForcedDuelRandomTest::RunTest(const FString& Parameters)
{
	TArray<TPair<int32, int32>> Pairs;
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		F.State.Seats[Koschei].Runes[0] = 1;
		F.State.Seats[EchoA].Runes[2] = 1;
		F.State.Seats[EchoB].Runes[3] = 1;
		F.WardenRandom.Initialize(11);
		const int32 SeedBefore = F.Random.GetCurrentSeed();
		F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::RandomPlayer, 1, EEffectVariant::OneRune));
		TestEqual(TEXT("Empieza el duelo"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
		TestTrue(TEXT("Dos distintos"), F.State.Duel.Challenger != F.State.Duel.Challenged);
		TestTrue(TEXT("Ninguno es quien dirige"), F.State.Duel.Challenger != Fenrir && F.State.Duel.Challenged != Fenrir);
		TestEqual(TEXT("Con el sorteo del evento"), F.Random.GetCurrentSeed(), SeedBefore);
		Pairs.Add(TPair<int32, int32>(F.State.Duel.Challenger, F.State.Duel.Challenged));
	}
	TestTrue(TEXT("La misma semilla da el mismo duelo"), Pairs[0] == Pairs[1]);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Duelo contra el Warden y "el Warden te roba" (R-DIR-4)
// ---------------------------------------------------------------------------------------------------------------------

// R-DIR-4, Duelo contra el Warden: si quien dirige no señala a tiempo, se bate contra el Warden como retado, que reta con el
// rendimiento de datos; si gana el Warden, lo que estaba en juego vuelve a su Fuente; si gana el jugador, no pierde nada;
// en todos los casos, insolencia. El empate es del retado (decisión de S-012).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenDuelClockTest, "Soulbound.Core.ChainedEffects.WardenDuel.RelojAgotado",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenDuelClockTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Name;
		EEffectTarget Target;
		EEffectVariant Variant;
		bool bSecondSignal;
		double Performance;
		int32 RunesLeft;
		int32 Returned;
		bool bWardenWins;
	};
	const FCase Cases[] = {
		{TEXT("una señal, todas, gana el Warden"), EEffectTarget::ChosenRival, EEffectVariant::AllRunes, false, 0.5, 0, 3, true},
		{TEXT("una señal, una, gana el Warden"), EEffectTarget::ChosenRival, EEffectVariant::OneRune, false, 0.5, 2, 1, true},
		{TEXT("segunda señal, todas, gana el Warden"), EEffectTarget::TwoChosenRivals, EEffectVariant::AllRunes, true, 0.5, 0, 3, true},
		{TEXT("gana el jugador"), EEffectTarget::ChosenRival, EEffectVariant::AllRunes, false, 0.9, 3, 0, false},
		{TEXT("empate: es del retado"), EEffectTarget::ChosenRival, EEffectVariant::AllRunes, false, 0.75, 3, 0, false},
	};
	for (const FCase& Case : Cases)
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		F.State.Seats[Fenrir].Runes[0] = 2;
		F.State.Seats[Fenrir].Runes[3] = 1;
		F.State.Seats[EchoA].Runes[2] = 1;
		F.State.Seats[EchoB].Runes[4] = 1;
		const int32 SeedBefore = F.Random.GetCurrentSeed();
		const int32 ReserveBefore = TotalReserve(F.State);
		const int32 From = F.State.EventLog.Num();
		const FString Context = Case.Name;
		F.Decree(Fenrir, App(EEffect::ForcedDuel, Case.Target, 1, Case.Variant));
		if (Case.bSecondSignal)
		{
			F.Answer(EchoA);
		}
		F.Expire();

		const TArray<FMatchEvent> Ignored = F.Since(From, EMatchEventType::WardenIgnored);
		if (TestEqual(*FString::Printf(TEXT("%s: desatiende"), *Context), Ignored.Num(), 1))
		{
			TestEqual(*FString::Printf(TEXT("%s: él"), *Context), Ignored[0].Seat, Fenrir);
		}
		TestEqual(*FString::Printf(TEXT("%s: insolencia"), *Context), F.State.Seats[Fenrir].Insolence, 1);
		TestEqual(*FString::Printf(TEXT("%s: el duelo"), *Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
		TestTrue(*FString::Printf(TEXT("%s: contra el Warden"), *Context), F.State.Duel.bAgainstWarden);
		TestEqual(*FString::Printf(TEXT("%s: el jugador es el retado"), *Context), F.State.Duel.Challenged, Fenrir);
		TestEqual(*FString::Printf(TEXT("%s: con el rendimiento de datos"), *Context), F.State.Duel.ChallengerPerformance, F.State.Params.WardenDuelPerformance);
		TestEqual(*FString::Printf(TEXT("%s: que es 0,75"), *Context), F.State.Duel.ChallengerPerformance, 0.75);
		TestTrue(*FString::Printf(TEXT("%s: está en juego"), *Context), Soulbound::Rules::IsSeatInPlay(F.State, Fenrir));
		const TArray<FMatchEvent> Started = F.Since(From, EMatchEventType::WardenDuelStarted);
		if (TestEqual(*FString::Printf(TEXT("%s: registra el reto"), *Context), Started.Num(), 1))
		{
			TestEqual(*FString::Printf(TEXT("%s: de quien se bate"), *Context), Started[0].Seat, Fenrir);
			TestEqual(*FString::Printf(TEXT("%s: rendimiento del Warden"), *Context), Started[0].Value, 75);
		}

		TestEqual(*FString::Printf(TEXT("%s: rinde"), *Context), AsInt(F.Perform(Fenrir, Case.Performance)), AsInt(EIntentResult::Accepted));
		TestEqual(*FString::Printf(TEXT("%s: entre turnos"), *Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
		TestEqual(*FString::Printf(TEXT("%s: runas"), *Context), F.RuneTotal(Fenrir), Case.RunesLeft);
		TestEqual(*FString::Printf(TEXT("%s: vuelven a la Fuente"), *Context), F.Since(From, EMatchEventType::RuneReturned).Num(), Case.Returned);
		TestEqual(*FString::Printf(TEXT("%s: la reserva sube"), *Context), TotalReserve(F.State), ReserveBefore + Case.Returned);
		TestEqual(*FString::Printf(TEXT("%s: gana el Warden"), *Context), F.Since(From, EMatchEventType::WardenDuelWon).Num(), Case.bWardenWins ? 1 : 0);
		TestEqual(*FString::Printf(TEXT("%s: o pierde"), *Context), F.Since(From, EMatchEventType::WardenDuelLost).Num(), Case.bWardenWins ? 0 : 1);
		TestEqual(*FString::Printf(TEXT("%s: insolencia sigue en 1"), *Context), F.State.Seats[Fenrir].Insolence, 1);
		TestEqual(*FString::Printf(TEXT("%s: sin trilero: el Warden no tiene Brazal"), *Context), F.Since(From, EMatchEventType::ShellGameStarted).Num(), 0);
		TestEqual(*FString::Printf(TEXT("%s: sorteo de la partida intacto"), *Context), F.Random.GetCurrentSeed(), SeedBefore);
		F.Advance();
		TestFalse(*FString::Printf(TEXT("%s: el evento termina"), *Context), F.State.CurrentEvent.IsActive());
	}
	return true;
}

// R-DIR-4, R-COM-2: un dilema resuelto en contra que contiene un Duelo forzado: el jugador se bate contra el Warden.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenDuelDilemmaTest, "Soulbound.Core.ChainedEffects.WardenDuel.DilemaEnContra",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenDuelDilemmaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Fenrir].Runes[0] = 1;
	F.State.Seats[EchoA].Runes[2] = 1;
	const FString Id = F.AddEvent(TEXT("dilema-de-duelos"), {
		{App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::OneRune)},
		{App(EEffect::ForcedDuel, EEffectTarget::TwoChosenRivals, 1, EEffectVariant::AllRunes)},
	});
	const int32 From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(Id);
	F.Expire();
	TestEqual(TEXT("Insolencia, una sola vez"), F.State.Seats[Fenrir].Insolence, 1);
	TestEqual(TEXT("Se bate contra el Warden"), F.Since(From, EMatchEventType::WardenDuelStarted).Num(), 1);
	TestTrue(TEXT("Contra el Warden"), F.State.Duel.bAgainstWarden);
	TestEqual(TEXT("El retado es quien dirige"), F.State.Duel.Challenged, Fenrir);
	TestFalse(TEXT("Sin elecciones dentro de la resolución en contra"), F.State.PendingChoice.IsOpen());
	return true;
}

// R-DIR-4, R-ELE-2: Shuten-dōji suma su margen al rendimiento contra el Warden, como en cualquier duelo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenDuelMarginTest, "Soulbound.Core.ChainedEffects.WardenDuel.ShutenDojiSumaSuMargen",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenDuelMarginTest::RunTest(const FString& Parameters)
{
	// 0,65 + 0,15 supera el 0,75 del Warden; 0,55 + 0,15 no.
	for (const TPair<double, bool>& Case : {TPair<double, bool>(0.65, false), TPair<double, bool>(0.55, true)})
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		const int32 Domain = DuelAdvantageDomain(F.State);
		if (!TestTrue(TEXT("Hay un Elegido con la afinidad"), Domain != INDEX_NONE))
		{
			return false;
		}
		F.State.Seats[EchoA].Role = ESeatRole::Elect;
		F.State.Seats[EchoA].DomainId = Domain;
		F.State.Seats[EchoA].Runes[0] = 1;
		F.State.Seats[Fenrir].Runes[2] = 1;
		F.Decree(EchoA, App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::AllRunes));
		F.Expire();
		TestTrue(TEXT("Se bate contra el Warden"), F.State.Duel.bAgainstWarden);
		const int32 From = F.State.EventLog.Num();
		F.Perform(EchoA, Case.Key);
		TestEqual(FString::Printf(TEXT("Con %.2f gana el Warden: %d"), Case.Key, Case.Value ? 1 : 0), F.Since(From, EMatchEventType::WardenDuelWon).Num(), Case.Value ? 1 : 0);
		TestEqual(TEXT("Y lo que gana el jugador es no perder nada"), F.RuneTotal(EchoA), Case.Value ? 0 : 1);
	}
	return true;
}

// R-DIR-4, R-DIR-3: sin runas en juego no hay duelo contra el Warden: burla, con insolencia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenDuelNoRunesTest, "Soulbound.Core.ChainedEffects.WardenDuel.SinRunasEsBurla",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenDuelNoRunesTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[EchoA].Runes[0] = 1;
	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::OneRune));
	F.Expire();
	TestEqual(TEXT("Insolencia"), F.State.Seats[Fenrir].Insolence, 1);
	TestEqual(TEXT("Burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin duelo"), F.Since(From, EMatchEventType::WardenDuelStarted).Num(), 0);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	return true;
}

// R-DIR-4, R-EFE-12: si quien dirige un Robo decretado no señala a tiempo, el Warden le roba a él: una runa suya al azar vuelve
// a su Fuente, con insolencia; sin runas, burla. Con el sorteo del evento.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenStealsTest, "Soulbound.Core.ChainedEffects.WardenSteals.RelojAgotado",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenStealsTest::RunTest(const FString& Parameters)
{
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		F.State.Seats[Fenrir].Runes[0] = 1;
		F.State.Seats[Fenrir].Runes[2] = 1;
		F.State.Seats[EchoA].Runes[3] = 1;
		const int32 SeedBefore = F.Random.GetCurrentSeed();
		const int32 ReserveBefore = TotalReserve(F.State);
		const int32 From = F.State.EventLog.Num();
		F.Decree(Fenrir, App(EEffect::DecreedTheft, EEffectTarget::ChosenRival));
		F.Expire();
		TestEqual(TEXT("Insolencia"), F.State.Seats[Fenrir].Insolence, 1);
		TestEqual(TEXT("El Warden le roba una"), F.RuneTotal(Fenrir), 1);
		TestEqual(TEXT("Registrado"), F.Since(From, EMatchEventType::RuneTakenByWarden).Num(), 1);
		TestEqual(TEXT("Vuelve a su Fuente"), F.Since(From, EMatchEventType::RuneReturned).Num(), 1);
		TestEqual(TEXT("Y la reserva sube"), TotalReserve(F.State), ReserveBefore + 1);
		TestEqual(TEXT("Sin trilero"), F.Since(From, EMatchEventType::ShellGameStarted).Num(), 0);
		TestEqual(TEXT("Con el sorteo del evento"), F.Random.GetCurrentSeed(), SeedBefore);
		TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());
	}
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		F.State.Seats[EchoA].Runes[3] = 1;
		const int32 From = F.State.EventLog.Num();
		F.Decree(Fenrir, App(EEffect::DecreedTheft, EEffectTarget::ChosenRival));
		F.Expire();
		TestEqual(TEXT("Insolencia aunque no tenga runas"), F.State.Seats[Fenrir].Insolence, 1);
		TestEqual(TEXT("Burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
		TestEqual(TEXT("Sin runa tomada"), F.Since(From, EMatchEventType::RuneTakenByWarden).Num(), 0);
	}
	return true;
}

// R-DIR-4, R-COM-2: un dilema resuelto en contra que contiene un Robo decretado: el Warden le roba a quien dirige.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedWardenStealsDilemmaTest, "Soulbound.Core.ChainedEffects.WardenSteals.DilemaEnContra",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedWardenStealsDilemmaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Fenrir].Runes[0] = 1;
	F.State.Seats[Fenrir].Runes[2] = 1;
	F.State.Seats[EchoA].Runes[3] = 1;
	const FString Id = F.AddEvent(TEXT("dilema-de-robos"), {
		{App(EEffect::DecreedTheft, EEffectTarget::ChosenRival)},
		{App(EEffect::DecreedTheft, EEffectTarget::Everyone)},
	});
	const int32 From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(Id);
	F.Expire();
	TestEqual(TEXT("Insolencia, una sola vez"), F.State.Seats[Fenrir].Insolence, 1);
	TestEqual(TEXT("El Warden le roba una"), F.RuneTotal(Fenrir), 1);
	TestEqual(TEXT("Registrado"), F.Since(From, EMatchEventType::RuneTakenByWarden).Num(), 1);
	TestEqual(TEXT("Sin trilero"), F.Since(From, EMatchEventType::ShellGameStarted).Num(), 0);
	TestEqual(TEXT("Nada le roba a EchoA"), F.RuneTotal(EchoA), 1);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Entre turnos (R-INT-3)
// ---------------------------------------------------------------------------------------------------------------------

// R-INT-3, S-015: quien está en el Vacío no es objetivo, ni ladrón, ni retador: con quien dirige dentro, el efecto es burla.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChainedVoidTest, "Soulbound.Core.ChainedEffects.EntreTurnos.ElVacioNoEsObjetivoNiLadronNiRetador",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FChainedVoidTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 1;
	F.State.Seats[EchoA].Runes[2] = 1;
	F.State.Seats[Fenrir].Runes[3] = 1;
	Soulbound::Rules::Banish(F.State, Fenrir, false, F.Now);
	TestEqual(TEXT("Ladrón en el Vacío: no vale"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::DecreedTheft, EchoA, Fenrir)), AsInt(EEffectResult::InVoid));
	TestEqual(TEXT("Retador en el Vacío: no vale"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::ForcedDuel, EchoA, Fenrir)), AsInt(EEffectResult::InVoid));
	TestEqual(TEXT("Víctima en el Vacío: no vale"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::DecreedTheft, Fenrir, EchoA)), AsInt(EEffectResult::InVoid));
	TestEqual(TEXT("Retado en el Vacío: no vale"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::ForcedDuel, Fenrir, EchoA)), AsInt(EEffectResult::InVoid));

	const int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::DecreedTheft, EEffectTarget::ChosenRival));
	TestEqual(TEXT("Robo decretado: burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestFalse(TEXT("Sin elección"), F.State.PendingChoice.IsOpen());
	F.Decree(Fenrir, App(EEffect::ForcedDuel, EEffectTarget::ChosenRival, 1, EEffectVariant::OneRune));
	TestEqual(TEXT("Duelo forzado: burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 2);
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
