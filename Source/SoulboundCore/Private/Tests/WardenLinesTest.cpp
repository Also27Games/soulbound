// Tests de la tabla de frases del Warden y del sorteo de la voz (S-021, GDD 06): el cargador de frases.json y sus claves exigidas
// (CA-VOZ-2, R-VOZ-5) y la elección de variante con sorteo propio (CA-WAR-9, R-WAR-9). Filtro: "Soulbound.Core.Voice".

#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Warden/WardenLines.h"
#include "Warden/WardenVoice.h"
#include "VoiceFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Tabla con una entrada Key de Variants variantes. */
	FWardenLineTable OneEntry(const FString& Key, int32 Variants)
	{
		FWardenLineTable Table;
		FWardenLineEntry& Entry = Table.Entries.AddDefaulted_GetRef();
		Entry.Id = Key;
		for (int32 Variant = 0; Variant < Variants; ++Variant)
		{
			Entry.Lines.Add(FString::Printf(TEXT("variante %d"), Variant));
		}
		return Table;
	}

	/** Cien sorteos seguidos de Key sobre una partida con la semilla de la voz Seed; devuelve las variantes. */
	TArray<int32> HundredDraws(const FWardenLineTable& Table, const FString& Key, int32 Seed)
	{
		FMatchState State;
		State.WardenLines = Table;
		State.Warden.VoiceSeed = Seed;
		TArray<int32> Variants;
		for (int32 Draw = 0; Draw < 100; ++Draw)
		{
			Variants.Add(Soulbound::Rules::DrawWardenLine(State, Key).Variant);
		}
		return Variants;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceLinesLoaderTest, "Soulbound.Core.Voice.CargadorDeFrases",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceLinesLoaderTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-2 (R-VOZ-5): el cargador acepta el frases.json del repositorio y falla, nombrando la clave, si falta cualquiera de las
	// claves exigidas o si una variante está vacía. La lista exigida sale de los enums y de las tablas, no de una lista escrita a mano.
	FWardenEventTable Events;
	FElectsTable Elects;
	if (!Soulbound::Tests::LoadTestWardenEvents(*this, Events) || !Soulbound::Tests::LoadTestElects(*this, Elects))
	{
		return false;
	}
	const TArray<FString> Required = Soulbound::Rules::RequiredWardenLineKeys(Events, Elects);

	// La lista se construye de las tablas y los enums: un evento de algún saco, un tipo de comentario (once: la presentación de los Elegidos
	// no es un comentario), un momento de instrucción, tres gravedades y la Furia, catorce programadas (inicio, corrupción, cuatro
	// posiciones del orden, fin de carrera con Ecos y con todos vinculados, aspirante, dos avisos, fin de tiempo, victoria y nadie vinculado) y una presentación por Elegido.
	// Los eventos que no están en ningún saco pueden tener frase, pero no se exige (decisión del autor del 28/10/2026).
	const int32 RemarkCount = static_cast<int32>(EWardenRemarkKind::NotThisTime) + 1;
	const int32 InstructionCount = static_cast<int32>(EInstructionMoment::Insolence) + 1;
	const int32 InBag = Events.Events.FilterByPredicate([](const FWardenEventDef& Event) { return Event.bInBag; }).Num();
	TestEqual(TEXT("Once tipos de comentario"), RemarkCount, 11);
	TestTrue(TEXT("Hay eventos fuera de los sacos (los de S-014) y dentro"), InBag > 0 && InBag < Events.Events.Num());
	TestEqual(TEXT("Una clave por evento de saco, tipo de comentario, momento, gravedad con la Furia, programada y Elegido"), Required.Num(),
		InBag + RemarkCount + InstructionCount + 4 + 14 + Elects.Elects.Num());
	TestFalse(TEXT("La presentación no es un comentario"), Required.Contains(TEXT("comentario:presentacion")));
	TestEqual(TEXT("Sin claves repetidas"), TSet<FString>(Required).Num(), Required.Num());
	for (const FWardenEventDef& Event : Events.Events)
	{
		TestEqual(FString::Printf(TEXT("Se exige la frase del evento %s solo si está en un saco"), *Event.Id), Required.Contains(Soulbound::Rules::WardenEventKey(Event.Id)), Event.bInBag);
	}
	for (int32 Kind = 0; Kind < RemarkCount; ++Kind)
	{
		TestTrue(FString::Printf(TEXT("Se exige el comentario %d"), Kind), Required.Contains(Soulbound::Rules::WardenRemarkKey(static_cast<EWardenRemarkKind>(Kind))));
	}
	for (int32 Moment = 0; Moment < InstructionCount; ++Moment)
	{
		TestTrue(FString::Printf(TEXT("Se exige la instrucción %d"), Moment), Required.Contains(Soulbound::Rules::InstructionKey(static_cast<EInstructionMoment>(Moment))));
	}
	for (int32 Position = 1; Position <= Soulbound::Warden::OrderLineCount; ++Position)
	{
		TestTrue(FString::Printf(TEXT("Se exige la posición %d del orden"), Position), Required.Contains(Soulbound::Rules::ScheduledOrderKey(Position)));
	}
	for (const FString& Key : {Soulbound::Warden::LineStart, Soulbound::Warden::LineCorruption, Soulbound::Warden::LineRaceEnd, Soulbound::Warden::LineRaceEndAll, Soulbound::Warden::LineAspirant,
		Soulbound::Warden::LineLastMinute, Soulbound::Warden::LineTenSeconds, Soulbound::Warden::LineTimeOut, Soulbound::Warden::LineVictory, Soulbound::Warden::LineNobodyBonded, Soulbound::Warden::LineFury})
	{
		TestTrue(FString::Printf(TEXT("Se exige %s"), *Key), Required.Contains(Key));
	}
	for (int32 Domain = 0; Domain < Elects.Elects.Num(); ++Domain)
	{
		TestTrue(FString::Printf(TEXT("Se exige la presentación del Elegido %d"), Domain), Required.Contains(Soulbound::Rules::IntroductionKey(Domain)));
	}

	// El archivo del repositorio carga con todas las claves.
	const FWardenLinesLoadResult Loaded = Soulbound::Rules::LoadWardenLinesFromFile(Soulbound::Tests::WardenLinesPath(), Required);
	if (!TestTrue(FString::Printf(TEXT("frases.json carga (%s)"), *Loaded.Message), Loaded.IsSuccess()))
	{
		return false;
	}
	TestEqual(TEXT("Entran todas las entradas del archivo"), Loaded.Table.Entries.Num(), Required.Num());

	// Las frases de los eventos fuera de los sacos son opcionales: sin ellas carga, y con ellas también.
	FWardenLineTable WithoutOptional = Loaded.Table;
	for (const FWardenEventDef& Event : Events.Events)
	{
		if (!Event.bInBag)
		{
			WithoutOptional.Entries.RemoveAll([&Event](const FWardenLineEntry& Entry) { return Entry.Id == Soulbound::Rules::WardenEventKey(Event.Id); });
		}
	}
	TestTrue(TEXT("Sin las frases de los eventos sin saco, carga"), Soulbound::Rules::ValidateWardenLines(WithoutOptional, Required).IsSuccess());
	FWardenLineTable WithOptional = Loaded.Table;
	FWardenLineEntry& Extra = WithOptional.Entries.AddDefaulted_GetRef();
	Extra.Id = Soulbound::Rules::WardenEventKey(Events.Events.FindByPredicate([](const FWardenEventDef& Event) { return !Event.bInBag; })->Id);
	Extra.Lines = {TEXT("una frase opcional")};
	TestTrue(TEXT("Con ellas (o con cualquier clave de más), también"), Soulbound::Rules::ValidateWardenLines(WithOptional, Required).IsSuccess());

	// Si falta cualquiera de las claves, falla nombrándola.
	for (const FString& Key : Required)
	{
		FWardenLineTable Without = Loaded.Table;
		Without.Entries.RemoveAll([&Key](const FWardenLineEntry& Entry) { return Entry.Id == Key; });
		const FWardenLinesLoadResult Result = Soulbound::Rules::ValidateWardenLines(Without, Required);
		TestEqual(FString::Printf(TEXT("Sin %s: falta una clave"), *Key), static_cast<int32>(Result.Error), static_cast<int32>(EWardenLinesError::MissingKey));
		TestTrue(FString::Printf(TEXT("Sin %s: el motivo nombra la clave"), *Key), Result.Message.Contains(Key));
	}

	// Una variante vacía, una entrada sin variantes o una clave repetida, también.
	FWardenLineTable WithEmpty = Loaded.Table;
	WithEmpty.Entries[3].Lines.Add(TEXT(""));
	FWardenLinesLoadResult Result = Soulbound::Rules::ValidateWardenLines(WithEmpty, Required);
	TestEqual(TEXT("Una variante vacía se rechaza"), static_cast<int32>(Result.Error), static_cast<int32>(EWardenLinesError::EmptyLine));
	TestTrue(TEXT("El motivo nombra la clave"), Result.Message.Contains(WithEmpty.Entries[3].Id));
	FWardenLineTable NoVariants = Loaded.Table;
	NoVariants.Entries[4].Lines.Reset();
	Result = Soulbound::Rules::ValidateWardenLines(NoVariants, Required);
	TestEqual(TEXT("Una entrada sin variantes se rechaza"), static_cast<int32>(Result.Error), static_cast<int32>(EWardenLinesError::EmptyLine));
	FWardenLineTable Duplicated = Loaded.Table;
	Duplicated.Entries.Add(Loaded.Table.Entries[0]);
	Result = Soulbound::Rules::ValidateWardenLines(Duplicated, Required);
	TestEqual(TEXT("Una clave repetida se rechaza"), static_cast<int32>(Result.Error), static_cast<int32>(EWardenLinesError::DuplicateKey));
	TestTrue(TEXT("El motivo nombra la clave"), Result.Message.Contains(Loaded.Table.Entries[0].Id));

	// El texto que no es una lista de entradas, o que tiene un tipo equivocado, se rechaza sin lanzar nada.
	for (const TCHAR* Bad : {TEXT("no es json"), TEXT("{\"lines\": []}"), TEXT("[{\"id\": 3, \"lines\": [\"a\"]}]"), TEXT("[{\"id\": \"a\", \"lines\": \"a\"}]"), TEXT("[{\"id\": \"a\"}]")})
	{
		TestEqual(FString::Printf(TEXT("\"%s\" es MalformedData"), Bad), static_cast<int32>(Soulbound::Rules::LoadWardenLinesFromJsonString(Bad, {}).Error),
			static_cast<int32>(EWardenLinesError::MalformedData));
	}
	TestEqual(TEXT("Un archivo que no existe"), static_cast<int32>(Soulbound::Rules::LoadWardenLinesFromFile(TEXT("no/existe.json"), {}).Error),
		static_cast<int32>(EWardenLinesError::FileNotFound));
	TestTrue(TEXT("Una lista válida con las claves que se piden carga"),
		Soulbound::Rules::LoadWardenLinesFromJsonString(TEXT("[{\"id\": \"a\", \"lines\": [\"uno\", \"dos\"]}]"), {TEXT("a")}).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceCuesTest, "Soulbound.Core.Voice.AcotacionesDeAudio",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceCuesTest::RunTest(const FString& Parameters)
{
	// R-VOZ-5: las acotaciones de audio van entre corchetes y la presentación las quita del subtítulo, con los espacios que dejan.
	TestEqual(TEXT("Sin acotaciones, igual"), Soulbound::Rules::StripLineCues(TEXT("Otra vez tú.")), FString(TEXT("Otra vez tú.")));
	TestEqual(TEXT("Una al principio"), Soulbound::Rules::StripLineCues(TEXT("[risa] Otra vez tú.")), FString(TEXT("Otra vez tú.")));
	TestEqual(TEXT("Una en medio"), Soulbound::Rules::StripLineCues(TEXT("Otra [pausa] vez tú.")), FString(TEXT("Otra vez tú.")));
	TestEqual(TEXT("Una al final y dos seguidas"), Soulbound::Rules::StripLineCues(TEXT("Otra vez tú. [risa][suspiro]")), FString(TEXT("Otra vez tú.")));
	TestEqual(TEXT("Solo una acotación"), Soulbound::Rules::StripLineCues(TEXT("[risa]")), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceDrawTest, "Soulbound.Core.Voice.SorteoSinRepetir",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceDrawTest::RunTest(const FString& Parameters)
{
	// CA-WAR-9 (R-WAR-9): cien sorteos seguidos de una clave con tres variantes, nunca la misma dos veces seguidas y todas salen; con una
	// variante, siempre esa y sin sorteo; la secuencia es la misma con la misma semilla y distinta con otra; sin la clave, nada.
	const FString Key = TEXT("evento:prueba");
	const FWardenLineTable Three = OneEntry(Key, 3);
	const TArray<int32> Draws = HundredDraws(Three, Key, 11);
	TestEqual(TEXT("Cien sorteos"), Draws.Num(), 100);
	bool bRepeated = false;
	TSet<int32> Seen;
	for (int32 Index = 0; Index < Draws.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Sorteo %d: una variante que existe"), Index), Draws[Index] >= 0 && Draws[Index] < 3);
		bRepeated |= Index > 0 && Draws[Index] == Draws[Index - 1];
		Seen.Add(Draws[Index]);
	}
	TestFalse(TEXT("Nunca dos veces seguidas la misma"), bRepeated);
	TestEqual(TEXT("Salen las tres"), Seen.Num(), 3);
	TestTrue(TEXT("Misma semilla, misma secuencia"), HundredDraws(Three, Key, 11) == Draws);
	TestTrue(TEXT("Otra semilla, otra secuencia"), HundredDraws(Three, Key, 12) != Draws);

	// Con una variante, siempre esa y sin gastar sorteos; con dos, alternan.
	{
		FMatchState State;
		State.WardenLines = OneEntry(Key, 1);
		State.Warden.VoiceSeed = 11;
		for (int32 Draw = 0; Draw < 20; ++Draw)
		{
			const FWardenLineChoice Choice = Soulbound::Rules::DrawWardenLine(State, Key);
			TestTrue(TEXT("Con una variante, la 0 de la entrada 0"), Choice.IsValid() && Choice.EntryIndex == 0 && Choice.Variant == 0);
		}
		TestEqual(TEXT("Con una variante no hay sorteo"), State.Warden.VoiceDraws, 0);
	}
	{
		const TArray<int32> Two = HundredDraws(OneEntry(Key, 2), Key, 3);
		bool bAlternates = true;
		for (int32 Index = 1; Index < Two.Num(); ++Index)
		{
			bAlternates &= Two[Index] != Two[Index - 1];
		}
		TestTrue(TEXT("Con dos variantes, alternan"), bAlternates);
	}
	{
		// Cada clave recuerda la suya, y la voz no gasta el sorteo del Warden.
		FMatchState State;
		State.WardenLines = OneEntry(Key, 3);
		FWardenLineEntry& Other = State.WardenLines.Entries.AddDefaulted_GetRef();
		Other.Id = TEXT("evento:otra");
		Other.Lines = {TEXT("x"), TEXT("y"), TEXT("z")};
		State.Warden.Seed = 77;
		State.Warden.VoiceSeed = 5;
		const int32 WardenDraws = State.Warden.Draws;
		for (int32 Draw = 0; Draw < 30; ++Draw)
		{
			Soulbound::Rules::DrawWardenLine(State, Key);
			Soulbound::Rules::DrawWardenLine(State, TEXT("evento:otra"));
		}
		TestEqual(TEXT("El sorteo del Warden no se gasta"), State.Warden.Draws, WardenDraws);
		TestEqual(TEXT("La voz lleva su cuenta: un sorteo por frase"), State.Warden.VoiceDraws, 60);
		TestEqual(TEXT("Recuerda la última de cada clave"), State.Warden.LastLineVariant.Num(), 2);
		TestFalse(TEXT("Sin la clave, una elección inválida"), Soulbound::Rules::DrawWardenLine(State, TEXT("evento:no-existe")).IsValid());
		FMatchState Silent;
		TestFalse(TEXT("La tabla vacía no habla"), Soulbound::Rules::DrawWardenLine(Silent, Key).IsValid());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
