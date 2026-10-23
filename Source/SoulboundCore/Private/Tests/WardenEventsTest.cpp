// Tests de los eventos del Warden (S-014, GDD 05): el archivo de eventos y su validación (R-EFE-0, R-COM-1), las direcciones
// y el Líder (R-DIR-1), el filtro de objetivos válidos y la burla (R-DIR-2, R-DIR-3), las desatenciones (R-DIR-4), decretos y
// dilemas con su reloj y su resolución en contra (R-COM-1 a R-COM-3), los ejemplos de R-COM-3 desde eventos.json, el evento
// entre turnos, el registro, el sorteo del Warden y las rondas. Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n)
// que cubre. Filtros: "Soulbound.Core.WardenEvents", "Soulbound.Core.Directions" y "Soulbound.Core.Turns.Ronda".
//
// Montaje: EventFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos).

#include "EventFixture.h"
#include "Match/BondRaceRules.h"
#include "Match/EffectCatalog.h"
#include "Match/EffectRules.h"
#include "Match/TurnRules.h"
#include "Match/WardenEventRules.h"
#include "Match/WardenEvents.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FEventFixture;
	using Soulbound::Tests::SameState;

	constexpr int32 Fenrir = FEventFixture::Fenrir;
	constexpr int32 Koschei = FEventFixture::Koschei;
	constexpr int32 EchoA = FEventFixture::EchoA;
	constexpr int32 EchoB = FEventFixture::EchoB;

	int32 AsInt(EEffectResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EWardenEventStart Result) { return static_cast<int32>(Result); }
	int32 AsInt(EWardenEventsError Error) { return static_cast<int32>(Error); }
	int32 AsInt(EIntentResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EChoiceKind Kind) { return static_cast<int32>(Kind); }
	int32 AsInt(EEffect Effect) { return static_cast<int32>(Effect); }
	int32 AsInt(EEffectTarget Target) { return static_cast<int32>(Target); }
	int32 AsInt(ETurnPhase Phase) { return static_cast<int32>(Phase); }

	/** Un archivo de eventos con un solo evento "a" de una opción, con Applications (texto JSON de la lista de aplicaciones). */
	FString OneEvent(const TCHAR* Applications)
	{
		return FString::Printf(TEXT("{\"events\": [{\"id\": \"a\", \"text\": \"t\", \"delivery\": \"direct\", \"options\": [[%s]]}]}"), Applications);
	}

	/** Un archivo de eventos con un solo evento "a" con Options (texto JSON de la lista de opciones). */
	FString WithOptions(const TCHAR* Options)
	{
		return FString::Printf(TEXT("{\"events\": [{\"id\": \"a\", \"text\": \"t\", \"delivery\": \"direct\", \"options\": %s}]}"), Options);
	}
}

// CA-EFE-1, R-EFE-0, R-COM-1: un archivo de eventos con un efecto, dirección, cantidad o variante fuera de lo permitido, con un
// número de opciones fuera de 1 a 4, un identificador repetido o JSON roto se rechaza al cargar, con su motivo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsRechazaTest, "Soulbound.Core.WardenEvents.RechazaFueraDeLoPermitido",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsRechazaTest::RunTest(const FString& Parameters)
{
	using E = EWardenEventsError;
	const TArray<TTuple<FString, FString, E>> Cases = {
		{TEXT("efecto desconocido"), OneEvent(TEXT("{\"effect\": \"Teleport\", \"target\": \"ActivePlayer\"}")), E::UnknownEffect},
		{TEXT("dirección no permitida"), OneEvent(TEXT("{\"effect\": \"SendHome\", \"target\": \"Echoes\"}")), E::TargetNotAllowed},
		{TEXT("dirección que no existe"), OneEvent(TEXT("{\"effect\": \"SendHome\", \"target\": \"Sideways\"}")), E::TargetNotAllowed},
		{TEXT("falta la dirección"), OneEvent(TEXT("{\"effect\": \"SendHome\"}")), E::TargetNotAllowed},
		{TEXT("el Sello con dirección"), OneEvent(TEXT("{\"effect\": \"PlaceSeal\", \"target\": \"ActivePlayer\"}")), E::TargetNotAllowed},
		{TEXT("cantidad 3"), OneEvent(TEXT("{\"effect\": \"LoseTurn\", \"target\": \"ActivePlayer\", \"amount\": 3}")), E::InvalidAmount},
		{TEXT("cantidad 0"), OneEvent(TEXT("{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\", \"amount\": 0}")), E::InvalidAmount},
		{TEXT("cantidad con decimales"), OneEvent(TEXT("{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\", \"amount\": 1.5}")), E::InvalidAmount},
		{TEXT("cantidad donde no va"), OneEvent(TEXT("{\"effect\": \"SendHome\", \"target\": \"ActivePlayer\", \"amount\": 1}")), E::InvalidAmount},
		{TEXT("variante de otro efecto"), OneEvent(TEXT("{\"effect\": \"LoseRune\", \"target\": \"ActivePlayer\", \"variant\": \"all\"}")), E::VariantNotAllowed},
		{TEXT("variante donde no va"), OneEvent(TEXT("{\"effect\": \"SendHome\", \"target\": \"ActivePlayer\", \"variant\": \"random\"}")), E::VariantNotAllowed},
		{TEXT("variante que no existe"), OneEvent(TEXT("{\"effect\": \"LoseRune\", \"target\": \"ActivePlayer\", \"variant\": \"maybe\"}")), E::VariantNotAllowed},
		{TEXT("sin opciones"), WithOptions(TEXT("[]")), E::InvalidOptionCount},
		{TEXT("cinco opciones"), WithOptions(TEXT("[[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}], [{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}], ")
			TEXT("[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}], [{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}], [{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}]]")),
			E::InvalidOptionCount},
		{TEXT("opción sin aplicaciones"), WithOptions(TEXT("[[]]")), E::InvalidOptionCount},
		{TEXT("identificador repetido"), TEXT("{\"events\": [{\"id\": \"a\", \"text\": \"t\", \"delivery\": \"direct\", \"options\": [[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}]]}, ")
			TEXT("{\"id\": \"a\", \"text\": \"u\", \"delivery\": \"direct\", \"options\": [[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}]]}]}"), E::DuplicateId},
		{TEXT("JSON roto"), TEXT("{\"events\": ["), E::MalformedData},
		{TEXT("falta el texto"), TEXT("{\"events\": [{\"id\": \"a\", \"delivery\": \"direct\", \"options\": [[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}]]}]}"), E::MalformedData},
		{TEXT("entrega desconocida"), TEXT("{\"events\": [{\"id\": \"a\", \"text\": \"t\", \"delivery\": \"paloma\", \"options\": [[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}]]}]}"), E::MalformedData},
	};
	for (const TTuple<FString, FString, E>& Case : Cases)
	{
		const FWardenEventsLoadResult Result = Soulbound::Rules::LoadWardenEventsFromJsonString(Case.Get<1>());
		TestEqual(FString::Printf(TEXT("%s: se rechaza con su motivo"), *Case.Get<0>()), AsInt(Result.Error), AsInt(Case.Get<2>()));
		TestFalse(FString::Printf(TEXT("%s: con un mensaje legible"), *Case.Get<0>()), Result.Message.IsEmpty());
	}

	// Controles: lo permitido carga; así cada rechazo es por lo que cambia.
	const TCHAR* const Valid[] = {
		TEXT("{\"effect\": \"SendHome\", \"target\": \"ChosenRival\"}"),
		TEXT("{\"effect\": \"PlaceSeal\"}"),
		TEXT("{\"effect\": \"LoseTurn\", \"target\": \"ActivePlayer\", \"amount\": 2}"),
		TEXT("{\"effect\": \"LoseRune\", \"target\": \"Everyone\", \"variant\": \"chosen\"}"),
		TEXT("{\"effect\": \"ForcedDuel\", \"target\": \"TwoChosenRivals\", \"variant\": \"one\"}"),
	};
	for (const TCHAR* Application : Valid)
	{
		const FWardenEventsLoadResult Result = Soulbound::Rules::LoadWardenEventsFromJsonString(OneEvent(Application));
		TestTrue(FString::Printf(TEXT("%s carga (%s)"), Application, *Result.Message), Result.IsSuccess());
	}
	// CA-ORA-3 (R-ORA-1, S-017): un dilema nunca llega por el Oráculo; ni con "oracle" ni con "any" carga. Un decreto, sí.
	for (const TCHAR* Delivery : {TEXT("oracle"), TEXT("any")})
	{
		const FWardenEventsLoadResult Oracle = Soulbound::Rules::LoadWardenEventsFromJsonString(FString::Printf(
			TEXT("{\"events\": [{\"id\": \"a\", \"text\": \"t\", \"delivery\": \"%s\", \"options\": [[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}], [{\"effect\": \"PlaceSeal\"}]]}]}"), Delivery));
		TestEqual(FString::Printf(TEXT("Un dilema con entrega \"%s\" no carga"), Delivery), AsInt(Oracle.Error), AsInt(E::OracleDilemma));
		TestFalse(TEXT("Con motivo"), Oracle.Message.IsEmpty());
	}
	const FWardenEventsLoadResult OracleDecree = Soulbound::Rules::LoadWardenEventsFromJsonString(
		TEXT("{\"events\": [{\"id\": \"a\", \"text\": \"t\", \"delivery\": \"oracle\", \"options\": [[{\"effect\": \"ExtraTurn\", \"target\": \"ActivePlayer\"}]]}]}"));
	TestTrue(FString::Printf(TEXT("Un decreto por Oráculo carga (%s)"), *OracleDecree.Message), OracleDecree.IsSuccess());

	// La validación también sobre la tabla ya construida.
	FWardenEventTable Table;
	FWardenEventDef& Event = Table.Events.AddDefaulted_GetRef();
	Event.Id = TEXT("b");
	Event.Text = TEXT("t");
	Event.Options.AddDefaulted_GetRef().Applications.Add(App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 3));
	TestEqual(TEXT("ValidateWardenEvents: cantidad 3"), AsInt(Soulbound::Rules::ValidateWardenEvents(Table).Error), AsInt(E::InvalidAmount));

	const FWardenEventsLoadResult Missing = Soulbound::Rules::LoadWardenEventsFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Events/no_existe.json")));
	TestEqual(TEXT("Archivo inexistente: FileNotFound"), AsInt(Missing.Error), AsInt(E::FileNotFound));
	return true;
}

// CA-COM-1, R-COM-1: eventos.json carga con los seis ejemplos de R-COM-3, los dos de las pruebas manuales y los tres de S-015
// (Prueba del Warden, Robo a todos y Duelo con quien señales), y viaja en el estado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsArchivoTest, "Soulbound.Core.WardenEvents.ArchivoCarga",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsArchivoTest::RunTest(const FString& Parameters)
{
	FWardenEventTable Table;
	if (!Soulbound::Tests::LoadTestWardenEvents(*this, Table))
	{
		return false;
	}
	const TCHAR* const Ids[] = {TEXT("pierdes-un-turno"), TEXT("dos-turnos-o-casa"), TEXT("duelo-por-todo"), TEXT("noche"), TEXT("liberacion-y-a-casa"),
		TEXT("lider-ensena-y-pierde"), TEXT("marca-al-lider"), TEXT("ley-de-sombras"), TEXT("prueba-del-warden"), TEXT("robo-a-todos"),
		TEXT("duelo-con-quien-senales")};
	for (const TCHAR* Id : Ids)
	{
		TestTrue(FString::Printf(TEXT("Está %s"), Id), Table.IndexOf(Id) != INDEX_NONE);
	}
	FEventFixture F;
	if (F.StartAfterRace(*this))
	{
		TestEqual(TEXT("StartMatch los deja en el estado"), F.State.WardenEvents.Events.Num(), Table.Events.Num());
	}
	return true;
}

// CA-DIR-1, R-DIR-1: el Líder es quien tiene más tipos distintos; con empate, más runas; si persiste, todos los empatados, que
// reciben el efecto. Sin runas empatan todos (decisión del autor del 12/10/2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionsLiderTest, "Soulbound.Core.Directions.LiderYDesempates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionsLiderTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	TestEqual(TEXT("Sin runas, empatan todos"), Soulbound::Rules::LeaderSeats(F.State), TArray<int32>{0, 1, 2, 3});

	F.State.Seats[EchoA].Runes[3] = 5;
	F.State.Seats[Fenrir].Runes[0] = 1;
	F.State.Seats[Fenrir].Runes[1] = 1;
	F.State.Seats[Koschei].Runes[0] = 2;
	F.State.Seats[Koschei].Runes[1] = 1;
	TestEqual(TEXT("Más tipos distintos; con empate, más runas"), Soulbound::Rules::LeaderSeats(F.State), TArray<int32>{Koschei});

	F.State.Seats[Fenrir].Runes[1] = 2;
	TestEqual(TEXT("Empate total: los dos"), Soulbound::Rules::LeaderSeats(F.State), TArray<int32>{Fenrir, Koschei});

	// Los empatados reciben el efecto todos.
	F.MoveAway(Fenrir);
	F.MoveAway(Koschei);
	const int32 From = F.State.EventLog.Num();
	F.Decree(EchoB, App(EEffect::SendHome, EEffectTarget::Leader));
	const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
	TestEqual(TEXT("Los dos líderes reciben el efecto"), Applied.Num(), 2);
	TestEqual(TEXT("Los dos vuelven a casa"), F.Since(From, EMatchEventType::SentHome).Num(), 2);

	F.State.Seats[EchoB].Runes[0] = 1;
	F.State.Seats[EchoB].Runes[2] = 1;
	F.State.Seats[EchoB].Runes[4] = 1;
	TestEqual(TEXT("Más tipos gana aunque tenga menos runas"), Soulbound::Rules::LeaderSeats(F.State), TArray<int32>{EchoB});
	return true;
}

// CA-DIR-2, R-DIR-2, R-DIR-3: una dirección sin objetivos válidos no se aplica y deja la burla; quien está en el Vacío no es
// objetivo de nada salvo Liberación.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionsSinObjetivosTest, "Soulbound.Core.Directions.SinObjetivosEsBurla",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionsSinObjetivosTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	int32 From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::RevealBracer, EEffectTarget::Everyone));
	TestEqual(TEXT("Nadie tiene runas: burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Y no se aplica"), F.Since(From, EMatchEventType::EffectApplied).Num(), 0);

	// El Eco 2, en el Vacío, con runas, fuera de su hogar y líder.
	F.State.Seats[EchoA].Runes[0] = 3;
	Soulbound::Rules::Banish(F.State, EchoA, false, F.Now);
	TestEqual(TEXT("El líder es el desterrado"), Soulbound::Rules::LeaderSeats(F.State), TArray<int32>{EchoA});
	const int32 Previous = F.PreviousOf(EchoA);
	const TPair<EEffect, EEffectTarget> Effects[] = {
		{EEffect::SendHome, EEffectTarget::NextPlayer},
		{EEffect::LoseTurn, EEffectTarget::NextPlayer},
		{EEffect::ExtraTurn, EEffectTarget::NextPlayer},
		{EEffect::GiftRune, EEffectTarget::NextPlayer},
		{EEffect::RevealBracer, EEffectTarget::Leader},
		{EEffect::SealAgainstRival, EEffectTarget::Leader},
		{EEffect::Mark, EEffectTarget::Leader},
		{EEffect::LoseRune, EEffectTarget::Leader},
		{EEffect::Banish, EEffectTarget::Leader},
	};
	for (const TPair<EEffect, EEffectTarget>& Effect : Effects)
	{
		const FString Name = Soulbound::Rules::EffectCode(Effect.Key);
		TestEqual(FString::Printf(TEXT("%s: en el Vacío no es objetivo"), *Name), AsInt(Soulbound::Rules::CanApplyEffect(F.State, Effect.Key, EchoA, Previous)),
			AsInt(EEffectResult::InVoid));
		From = F.State.EventLog.Num();
		F.Decree(Previous, App(Effect.Key, Effect.Value));
		TestEqual(FString::Printf(TEXT("%s: burla"), *Name), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
		TestEqual(FString::Printf(TEXT("%s: no se aplica"), *Name), F.Since(From, EMatchEventType::EffectApplied).Num(), 0);
		TestFalse(FString::Printf(TEXT("%s: no se pide nada"), *Name), F.State.PendingChoice.IsOpen());
	}
	F.Director(Fenrir);
	F.Launch(F.AddEvent(TEXT("a-casa-el-señalado"), {{App(EEffect::SendHome, EEffectTarget::ChosenRival)}}));
	TestFalse(TEXT("El desterrado no se ofrece como rival"), F.State.PendingChoice.Options.Contains(EchoA));
	F.Expire();

	// Liberación sí.
	TestEqual(TEXT("Liberación sí vale con alguien dentro"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::ReleaseVoid, EchoA, Fenrir)),
		AsInt(EEffectResult::Applied));
	F.Decree(Fenrir, App(EEffect::ReleaseVoid, EEffectTarget::Everyone));
	TestFalse(TEXT("Y lo saca"), F.State.Seats[EchoA].IsInVoid());
	From = F.State.EventLog.Num();
	F.Decree(Fenrir, App(EEffect::ReleaseVoid, EEffectTarget::Everyone));
	TestEqual(TEXT("Sin nadie dentro, Liberación es burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	return true;
}

// CA-DIR-3, R-DIR-4: las tres formas de desatender, con insolencia en todas: un regalo se pierde con burla; un daño sin
// señalar cae sobre uno mismo; la runa a perder sin elegir, al azar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionsDesatenderTest, "Soulbound.Core.Directions.TresDesatenciones",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionsDesatenderTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	// Un regalo: Runa regalada sin elegir.
	int32 From = F.State.EventLog.Num();
	F.Decree(EchoA, App(EEffect::GiftRune, EEffectTarget::ActivePlayer));
	F.Expire();
	TestEqual(TEXT("Regalo: no recibe nada"), F.RuneTotal(EchoA), 0);
	TestEqual(TEXT("Regalo: burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Regalo: insolencia"), F.State.Seats[EchoA].Insolence, 1);

	// Un daño a un rival señalado: cae sobre él. El Eco 3, fuera de casa, es el rival que podía señalar.
	F.MoveAway(EchoA);
	F.MoveAway(EchoB);
	From = F.State.EventLog.Num();
	F.Director(EchoA);
	F.Launch(F.AddEvent(TEXT("a-casa-el-señalado"), {{App(EEffect::SendHome, EEffectTarget::ChosenRival)}}));
	F.Expire();
	const TArray<FMatchEvent> Sent = F.Since(From, EMatchEventType::SentHome);
	TestEqual(TEXT("Daño: cae sobre quien no señaló"), Sent.IsEmpty() ? INDEX_NONE : Sent[0].Seat, EchoA);
	TestEqual(TEXT("Daño: insolencia"), F.State.Seats[EchoA].Insolence, 2);

	// La variante de un castigo propio: qué runa perder.
	F.State.Seats[EchoA].Runes[1] = 1;
	F.State.Seats[EchoA].Runes[4] = 1;
	F.Decree(EchoA, App(EEffect::LoseRune, EEffectTarget::ActivePlayer, 1, EEffectVariant::Chosen));
	F.Expire();
	TestEqual(TEXT("Variante: pierde una al azar"), F.RuneTotal(EchoA), 1);
	TestEqual(TEXT("Variante: insolencia"), F.State.Seats[EchoA].Insolence, 3);
	TestEqual(TEXT("Tres desatenciones en el registro"), F.Since(0, EMatchEventType::WardenIgnored).Num(), 3);
	return true;
}

// CA-COM-1, R-COM-3: "pierdes-un-turno" (decreto de eventos.json): el que acaba de jugar pierde su siguiente turno.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsPierdesUnTurnoTest, "Soulbound.Core.WardenEvents.Ejemplo.PierdesUnTurno",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsPierdesUnTurnoTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Director(EchoA);
	TestEqual(TEXT("Se lanza"), AsInt(F.Launch(TEXT("pierdes-un-turno"))), AsInt(EWardenEventStart::Started));
	TestFalse(TEXT("Es un decreto: no pide nada"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Un turno perdido para el que jugó"), F.State.Seats[EchoA].LostTurns, 1);
	const int32 From = F.State.EventLog.Num();
	for (int32 Turn = 0; Turn < 4 && F.Since(From, EMatchEventType::TurnSkipped).IsEmpty(); ++Turn)
	{
		F.Advance();
		F.FinishTurn();
	}
	const TArray<FMatchEvent> Skipped = F.Since(From, EMatchEventType::TurnSkipped);
	TestEqual(TEXT("Su turno se salta"), Skipped.IsEmpty() ? INDEX_NONE : Skipped[0].Seat, EchoA);
	return true;
}

// CA-COM-1, R-COM-2, R-COM-3: "dos-turnos-o-casa", dilema de eventos.json: dos turnos extra o mandar a casa a quien señales.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsDosTurnosOCasaTest, "Soulbound.Core.WardenEvents.Ejemplo.DosTurnosOCasa",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsDosTurnosOCasaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const FString Id = TEXT("dos-turnos-o-casa");
	F.Director(Fenrir);
	F.Launch(Id);
	TestEqual(TEXT("Dilema para quien dirige"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Dilemma));
	TestEqual(TEXT("A Fenrir"), F.State.PendingChoice.Seat, Fenrir);
	TestEqual(TEXT("Dos opciones"), F.State.PendingChoice.Options, TArray<int32>{0, 1});
	TestEqual(TEXT("Reloj: base más un incremento"), F.State.PendingChoice.DeadlineSeconds, F.Now + 8.0 + 3.0);
	F.Answer(0);
	TestEqual(TEXT("Opción A: dos turnos extra"), F.State.Seats[Fenrir].ExtraTurns, 2);
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());

	F.State.Seats[Fenrir].ExtraTurns = 0;
	F.MoveAway(EchoB);
	F.Director(Fenrir);
	F.Launch(Id);
	F.Answer(1);
	TestEqual(TEXT("Opción B: señala"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::ChosenRival));
	TestEqual(TEXT("Señala al Eco"), AsInt(F.Answer(EchoB)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Vuelve a su Nicho"), F.State.Seats[EchoB].TileId, Soulbound::Rules::HomeTile(F.State, EchoB));
	TestEqual(TEXT("Sin turnos extra"), F.State.Seats[Fenrir].ExtraTurns, 0);

	// Sin contestar: el Warden sortea una opción en su contra, con insolencia.
	F.MoveAway(Fenrir);
	const int32 From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(Id);
	F.Expire();
	const TArray<FMatchEvent> Chosen = F.Since(From, EMatchEventType::DilemmaChosen);
	TestEqual(TEXT("El Warden sortea"), Chosen.IsEmpty() ? INDEX_NONE : Chosen[0].Detail, 1);
	TestEqual(TEXT("Insolencia"), F.State.Seats[Fenrir].Insolence, 1);
	TestEqual(TEXT("Los regalos se descartan"), F.State.Seats[Fenrir].ExtraTurns, 0);
	if (!Chosen.IsEmpty() && Chosen[0].Value == 1)
	{
		TestEqual(TEXT("El daño cae sobre él: a su casa"), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
	}
	TestFalse(TEXT("Sin elecciones dentro de la resolución en contra"), F.State.PendingChoice.IsOpen());
	return true;
}

// CA-COM-1, R-COM-3, R-EFE-13: "duelo-por-todo" se carga con Duelo forzado a dos rivales señalados por todas las runas y se
// aplica (S-015): quien dirige señala a dos, el primero reta al segundo y se juegan todas las runas del perdedor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsDueloPorTodoTest, "Soulbound.Core.WardenEvents.Ejemplo.DueloPorTodo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsDueloPorTodoTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const int32 Index = F.State.WardenEvents.IndexOf(TEXT("duelo-por-todo"));
	if (!TestTrue(TEXT("Está en el archivo"), Index != INDEX_NONE))
	{
		return false;
	}
	const FEffectApplication& Duel = F.State.WardenEvents.Events[Index].Options[0].Applications[0];
	TestEqual(TEXT("Duelo forzado"), AsInt(Duel.Effect), AsInt(EEffect::ForcedDuel));
	TestEqual(TEXT("A dos rivales señalados"), AsInt(Duel.Target), AsInt(EEffectTarget::TwoChosenRivals));
	TestEqual(TEXT("Por todas las runas"), static_cast<int32>(Duel.Variant), static_cast<int32>(EEffectVariant::AllRunes));
	F.State.Seats[Koschei].Runes[0] = 1;
	F.State.Seats[EchoA].Runes[2] = 1;
	F.State.Seats[EchoA].Runes[3] = 1;
	F.Director(Fenrir);
	TestEqual(TEXT("Se aplica"), AsInt(F.Launch(TEXT("duelo-por-todo"))), AsInt(EWardenEventStart::Started));
	TestEqual(TEXT("Señala quien dirige"), F.State.PendingChoice.Seat, Fenrir);
	TestEqual(TEXT("Primero, al retador"), F.State.PendingChoice.Options, TArray<int32>({Koschei, EchoA}));
	F.Answer(EchoA);
	TestEqual(TEXT("Después, al retado"), F.State.PendingChoice.Options, TArray<int32>({Koschei}));
	F.Answer(Koschei);
	TestEqual(TEXT("Se baten"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
	TestTrue(TEXT("Por todas las runas"), F.State.Duel.bAllRunes);
	F.Perform(EchoA, 0.9);
	F.Perform(Koschei, 0.1);
	TestEqual(TEXT("El ganador se lleva todas"), F.RuneTotal(EchoA), 3);
	TestEqual(TEXT("El perdedor se queda sin ninguna"), F.RuneTotal(Koschei), 0);
	return true;
}

// CA-COM-1, R-COM-3, R-EFE-18: "noche": empieza la Noche de los Ecos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsNocheTest, "Soulbound.Core.WardenEvents.Ejemplo.Noche",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsNocheTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.Director(Fenrir);
	F.Launch(TEXT("noche"));
	TestTrue(TEXT("Es de noche"), F.State.IsEchoNight());
	TestEqual(TEXT("Hasta el tope de rondas"), F.State.EchoNightUntilRound, F.State.Round + F.State.Params.EchoNightMaxRounds);
	TestFalse(TEXT("Los Ecos no están confinados"), Soulbound::Rules::MakeMoverTraits(F.State, EchoB).bConfinedToForgotten);
	return true;
}

// CA-COM-1, R-COM-3: "liberacion-y-a-casa": los del Vacío salen y después todos vuelven a casa; cada aplicación con sus válidos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsLiberacionYACasaTest, "Soulbound.Core.WardenEvents.Ejemplo.LiberacionYACasa",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsLiberacionYACasaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	Soulbound::Rules::Banish(F.State, EchoA, false, F.Now);
	F.MoveAway(Fenrir);
	F.MoveAway(EchoB);
	const int32 From = F.State.EventLog.Num();
	F.Director(Koschei);
	F.Launch(TEXT("liberacion-y-a-casa"));
	TestFalse(TEXT("El desterrado sale"), F.State.Seats[EchoA].IsInVoid());
	for (int32 Seat = 0; Seat < F.State.Seats.Num(); ++Seat)
	{
		TestEqual(FString::Printf(TEXT("El asiento %d está en su hogar"), Seat), F.State.Seats[Seat].TileId, Soulbound::Rules::HomeTile(F.State, Seat));
	}
	const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
	TestEqual(TEXT("Liberación una vez y dos Vueltas al hogar"), Applied.Num(), 3);
	TestEqual(TEXT("Primero la Liberación"), Applied.IsEmpty() ? INDEX_NONE : Applied[0].Value, AsInt(EEffect::ReleaseVoid));
	TestEqual(TEXT("Dos vuelven a casa"), F.Since(From, EMatchEventType::SentHome).Num(), 2);
	return true;
}

// CA-COM-1, R-COM-3: "lider-ensena-y-pierde": el líder revela su Brazal y después pierde una runa al azar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsLiderEnsenaTest, "Soulbound.Core.WardenEvents.Ejemplo.LiderEnsenaYPierde",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsLiderEnsenaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.State.Seats[Koschei].Runes[0] = 2;
	F.State.Seats[Koschei].Runes[3] = 1;
	const int32 From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(TEXT("lider-ensena-y-pierde"));
	TestTrue(TEXT("Brazal del líder revelado"), Soulbound::Rules::IsBracerRevealed(F.State, Koschei));
	TestEqual(TEXT("Y pierde una runa"), F.RuneTotal(Koschei), 2);
	int32 Revealed = INDEX_NONE;
	int32 Returned = INDEX_NONE;
	for (int32 Index = From; Index < F.State.EventLog.Num(); ++Index)
	{
		Revealed = F.State.EventLog[Index].Type == EMatchEventType::BracerRevealed && Revealed == INDEX_NONE ? Index : Revealed;
		Returned = F.State.EventLog[Index].Type == EMatchEventType::RuneReturned && Returned == INDEX_NONE ? Index : Returned;
	}
	TestTrue(TEXT("En orden: primero enseña, después pierde"), Revealed != INDEX_NONE && Returned != INDEX_NONE && Revealed < Returned);
	return true;
}

// CA-COM-2, R-COM-2: un dilema de cuatro opciones tiene el reloj base más tres incrementos; agotado, una opción al azar, sus
// daños sobre el activo, sus regalos descartados, sin elecciones dentro e insolencia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsDilemaCuatroTest, "Soulbound.Core.WardenEvents.DilemaDeCuatroOpciones",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsDilemaCuatroTest::RunTest(const FString& Parameters)
{
	// Cada opción, un regalo y un daño con asiento; el sorteo del Warden elige cuál, así que se prueba con varias semillas.
	for (int32 Seed = 1; Seed <= 8; ++Seed)
	{
		FEventFixture F;
		if (!F.StartAfterRace(*this))
		{
			return false;
		}
		F.WardenRandom.Initialize(Seed);
		const FString Id = F.AddEvent(TEXT("cuatro"), {
			{App(EEffect::ExtraTurn, EEffectTarget::ActivePlayer, 1), App(EEffect::SendHome, EEffectTarget::NextPlayer)},
			{App(EEffect::GiftRune, EEffectTarget::ActivePlayer), App(EEffect::LoseTurn, EEffectTarget::ChosenRival, 1)},
			{App(EEffect::EchoNight, EEffectTarget::Echoes), App(EEffect::LoseRune, EEffectTarget::Leader, 1, EEffectVariant::Chosen)},
			{App(EEffect::ReleaseVoid, EEffectTarget::Everyone), App(EEffect::Mark, EEffectTarget::RandomPlayer)},
		});
		F.State.Seats[EchoA].Runes[0] = 2;
		F.MoveAway(EchoA);
		Soulbound::Rules::Banish(F.State, EchoB, false, F.Now);
		const int32 From = F.State.EventLog.Num();
		F.Director(EchoA);
		F.Launch(Id);
		TestEqual(TEXT("Cuatro opciones"), F.State.PendingChoice.Options.Num(), 4);
		TestEqual(TEXT("Reloj: base más tres incrementos"), F.State.PendingChoice.DeadlineSeconds, F.Now + 8.0 + 3 * 3.0);
		F.Expire();

		const TArray<FMatchEvent> Chosen = F.Since(From, EMatchEventType::DilemmaChosen);
		const int32 Option = Chosen.IsEmpty() ? INDEX_NONE : Chosen[0].Value;
		const FString Context = FString::Printf(TEXT("Semilla %d, opción %d"), Seed, Option);
		TestEqual(FString::Printf(TEXT("%s: la sortea el Warden"), *Context), Chosen.IsEmpty() ? INDEX_NONE : Chosen[0].Detail, 1);
		TestEqual(FString::Printf(TEXT("%s: insolencia"), *Context), F.State.Seats[EchoA].Insolence, 1);
		TestFalse(FString::Printf(TEXT("%s: sin elecciones dentro"), *Context), F.State.PendingChoice.IsOpen());
		TestFalse(FString::Printf(TEXT("%s: el evento termina"), *Context), F.State.CurrentEvent.IsActive());
		// Regalos descartados.
		TestEqual(FString::Printf(TEXT("%s: sin turno extra"), *Context), F.State.Seats[EchoA].ExtraTurns, 0);
		TestEqual(FString::Printf(TEXT("%s: sin runa regalada"), *Context), F.Since(From, EMatchEventType::RuneObtained).Num(), 0);
		TestFalse(FString::Printf(TEXT("%s: sin Noche"), *Context), F.State.IsEchoNight());
		TestTrue(FString::Printf(TEXT("%s: sin Liberación"), *Context), F.State.Seats[EchoB].IsInVoid());
		// Daños sobre el activo.
		const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
		for (const FMatchEvent& Event : Applied)
		{
			TestEqual(FString::Printf(TEXT("%s: el daño cae sobre el activo"), *Context), Event.Seat, EchoA);
		}
		switch (Option)
		{
		case 0: TestEqual(TEXT("Vuelta al hogar sobre él"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA)); break;
		case 1: TestEqual(TEXT("Turno perdido sobre él"), F.State.Seats[EchoA].LostTurns, 1); break;
		case 2: TestEqual(TEXT("Pierde una runa, al azar"), F.RuneTotal(EchoA), 1); break;
		case 3: TestEqual(TEXT("Marcarse a sí mismo no vale: burla"), F.Since(From, EMatchEventType::WardenTaunt).Num(), 1); break;
		default: AddError(FString::Printf(TEXT("%s: opción fuera de rango"), *Context)); break;
		}
	}
	return true;
}

// Entre turnos (S-014): un evento con una elección pendiente detiene la máquina hasta resolverse; la elección puede ser de un
// asiento que no es el activo (Runa regalada "al siguiente") y el siguiente turno empieza después. Fuera de BetweenTurns, o
// con otro evento en curso, no se lanza.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsEntreTurnosTest, "Soulbound.Core.WardenEvents.EntreTurnosDetieneLaMaquina",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsEntreTurnosTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const int32 Next = F.NextOf(Fenrir);
	F.Decree(Fenrir, App(EEffect::GiftRune, EEffectTarget::NextPlayer));
	TestEqual(TEXT("La elección es del siguiente"), F.State.PendingChoice.Seat, Next);
	TestEqual(TEXT("Hay evento en curso"), AsInt(F.Launch(TEXT("noche"))), AsInt(EWardenEventStart::EventInProgress));
	const int32 From = F.State.EventLog.Num();
	TestFalse(TEXT("La máquina espera"), F.Advance());
	TestEqual(TEXT("Sin empezar turno"), F.Since(From, EMatchEventType::TurnStarted).Num(), 0);
	TestEqual(TEXT("Sigue entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestEqual(TEXT("Otro asiento no contesta"), AsInt(F.Choose(Fenrir == Next ? Koschei : Fenrir, EChoiceKind::GiftRune, 0)), AsInt(EIntentResult::WrongSeat));
	if (!TestTrue(TEXT("Con tipos que elegir"), !F.State.PendingChoice.Options.IsEmpty()))
	{
		return false;
	}
	TestEqual(TEXT("Contesta el siguiente"), AsInt(F.Answer(F.State.PendingChoice.Options[0])), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Recibe la runa"), F.RuneTotal(Next), 1);
	TestFalse(TEXT("El evento termina"), F.State.CurrentEvent.IsActive());
	TestTrue(TEXT("Y la máquina sigue"), F.Advance());
	TestEqual(TEXT("Empieza el turno siguiente"), F.Since(From, EMatchEventType::TurnStarted).Num(), 1);

	// Con el turno empezado no se lanza.
	TestEqual(TEXT("Fuera de BetweenTurns, no"), AsInt(F.Launch(TEXT("noche"))), AsInt(EWardenEventStart::NotBetweenTurns));
	TestEqual(TEXT("Un id que no existe"), AsInt(Soulbound::Rules::CanApplyEvent(F.State, TEXT("no-existe"))), AsInt(EWardenEventStart::NotBetweenTurns));
	F.FinishTurn();
	TestEqual(TEXT("Entre turnos, un id que no existe"), AsInt(F.Launch(TEXT("no-existe"))), AsInt(EWardenEventStart::UnknownEvent));
	return true;
}

// Registro (S-014): el evento, la opción del dilema, cada aplicación con efecto, dirección, objetivo y quién señaló, y la burla.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsRegistroTest, "Soulbound.Core.WardenEvents.Registro",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsRegistroTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	F.MoveAway(EchoB);
	const int32 From = F.State.EventLog.Num();
	F.Director(Fenrir);
	F.Launch(TEXT("dos-turnos-o-casa"));
	F.Answer(1);
	F.Answer(EchoB);
	const int32 Index = F.State.WardenEvents.IndexOf(TEXT("dos-turnos-o-casa"));

	const TArray<FMatchEvent> Started = F.Since(From, EMatchEventType::WardenEventStarted);
	if (TestEqual(TEXT("Un WardenEventStarted"), Started.Num(), 1))
	{
		TestEqual(TEXT("De quien dirige"), Started[0].Seat, Fenrir);
		TestEqual(TEXT("Con el evento"), Started[0].Value, Index);
		TestEqual(TEXT("Y sus opciones"), Started[0].Detail, 2);
	}
	const TArray<FMatchEvent> Chosen = F.Since(From, EMatchEventType::DilemmaChosen);
	if (TestEqual(TEXT("Un DilemmaChosen"), Chosen.Num(), 1))
	{
		TestEqual(TEXT("De quien dirige"), Chosen[0].Seat, Fenrir);
		TestEqual(TEXT("La opción B"), Chosen[0].Value, 1);
		TestEqual(TEXT("Elegida por él"), Chosen[0].Detail, 0);
	}
	const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
	if (TestEqual(TEXT("Un EffectApplied"), Applied.Num(), 1))
	{
		TestEqual(TEXT("Objetivo"), Applied[0].Seat, EchoB);
		TestEqual(TEXT("Quién señaló"), Applied[0].OtherSeat, Fenrir);
		TestEqual(TEXT("Efecto"), Applied[0].Value, AsInt(EEffect::SendHome));
		TestEqual(TEXT("Dirección"), Applied[0].Detail, AsInt(EEffectTarget::ChosenRival));
	}
	TestEqual(TEXT("Después, el SentHome"), F.Since(From, EMatchEventType::SentHome).Num(), 1);

	const int32 Before = F.State.EventLog.Num();
	F.Decree(Koschei, App(EEffect::LoseRune, EEffectTarget::Everyone, 1, EEffectVariant::Random));
	const TArray<FMatchEvent> Taunts = F.Since(Before, EMatchEventType::WardenTaunt);
	if (TestEqual(TEXT("Una burla"), Taunts.Num(), 1))
	{
		TestEqual(TEXT("A quien dirige"), Taunts[0].Seat, Koschei);
		TestEqual(TEXT("Con el efecto"), Taunts[0].Value, AsInt(EEffect::LoseRune));
		TestEqual(TEXT("Y la dirección"), Taunts[0].Detail, AsInt(EEffectTarget::Everyone));
	}
	return true;
}

// Sorteo (S-014): los eventos no consumen el sorteo de la partida (direcciones al azar, Sello, opción y runa al azar usan el
// del Warden) y el mismo sorteo del Warden da el mismo resultado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenEventsSorteoTest, "Soulbound.Core.WardenEvents.NoConsumeElSorteo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenEventsSorteoTest::RunTest(const FString& Parameters)
{
	auto Play = [this](FEventFixture& F)
	{
		F.State.Seats[Koschei].Runes[0] = 2;
		F.State.Seats[Koschei].Runes[2] = 1;
		F.MoveAway(Fenrir);
		F.MoveAway(Koschei);
		F.Decree(EchoA, App(EEffect::SendHome, EEffectTarget::RandomPlayer));
		F.Decree(EchoA, App(EEffect::PlaceSeal, EEffectTarget::None));
		F.Decree(EchoA, App(EEffect::LoseRune, EEffectTarget::Leader, 1, EEffectVariant::Random));
		F.Director(EchoA);
		F.Launch(TEXT("dos-turnos-o-casa"));
		F.Expire();
	};
	FEventFixture A;
	FEventFixture B;
	if (!A.StartAfterRace(*this) || !B.StartAfterRace(*this))
	{
		return false;
	}
	const FRandomStream MatchRandom = A.Random;
	Play(A);
	Play(B);
	TestEqual(TEXT("El sorteo de la partida no se toca"), A.Random.GetCurrentSeed(), MatchRandom.GetCurrentSeed());
	TestTrue(TEXT("Mismo sorteo del Warden, mismo estado"), SameState(A.State, B.State));
	TestTrue(TEXT("Y se aplicó algo"), A.Since(0, EMatchEventType::EffectApplied).Num() >= 3);
	return true;
}

// Rondas (decisión del autor del 10/10/2026): una ronda es cada vuelta completa del orden de turno. Sube al pasar el orden por
// su primer asiento, también si ese asiento se salta su turno, y nunca con un turno extra. El último que jugó queda anotado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnsRondaTest, "Soulbound.Core.Turns.RondaPorVueltaDelOrden",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurnsRondaTest::RunTest(const FString& Parameters)
{
	FEventFixture F;
	if (!F.StartAfterRace(*this))
	{
		return false;
	}
	const TArray<int32> Order = F.State.TurnOrder;
	auto PlayTurn = [&F]()
	{
		F.Now += 1.0;
		F.Advance();
		const int32 Seat = F.State.ActiveSeat;
		F.FinishTurn();
		return Seat;
	};
	F.State.ActiveSeat = Order[0];
	F.State.TurnPhase = ETurnPhase::BetweenTurns;
	const int32 Round = F.State.Round;
	for (int32 Turn = 0; Turn < Order.Num(); ++Turn)
	{
		const int32 Played = PlayTurn();
		TestEqual(FString::Printf(TEXT("El turno %d es de su asiento"), Turn), Played, Order[Turn]);
		TestEqual(FString::Printf(TEXT("El último que jugó es el %d"), Played), F.State.LastTurnSeat, Played);
		TestEqual(FString::Printf(TEXT("Tras el turno %d"), Turn), F.State.Round, Turn + 1 < Order.Num() ? Round : Round + 1);
	}

	// Un turno extra del último del orden no cuenta; la vuelta, al pasar al primero.
	for (int32 Turn = 0; Turn + 1 < Order.Num(); ++Turn)
	{
		PlayTurn();
	}
	F.State.Seats[Order.Last()].ExtraTurns = 1;
	PlayTurn();
	TestEqual(TEXT("Tras el turno normal del último, con extra pendiente, no"), F.State.Round, Round + 1);
	PlayTurn();
	TestEqual(TEXT("Tras su turno extra, sí"), F.State.Round, Round + 2);

	// El primero del orden se salta su turno: la vuelta cuenta igual.
	F.State.Seats[Order[0]].LostTurns = 1;
	for (int32 Turn = 0; Turn + 1 < Order.Num(); ++Turn)
	{
		PlayTurn();
	}
	TestEqual(TEXT("Con el primero saltándose su turno, cuenta al pasar por él"), F.State.Round, Round + 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
