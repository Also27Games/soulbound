// Tests del texto de "cómo se juega" (S-022): el cargador del archivo de Content/Data/Textos y el texto para el bloque de UGT. El texto es del
// autor; los tests no miran su contenido, solo sus apartados, y los títulos exigidos salen de la constante, no se escriben aquí. Sin mundo.
// Filtro: "Soulbound.Game.Presentation.ComoSeJuega".

#include "../../Presentation/HowToPlay.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags HowToPlayTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Un texto con los títulos exigidos en el orden Order (posiciones en HowToPlayHeadings) y "(pendiente)" bajo cada uno. */
	FString HowToPlayWith(const TArray<int32>& Order)
	{
		FString Text;
		for (const int32 Heading : Order)
		{
			Text += Soulbound::Presentation::HowToPlayHeadings()[Heading] + TEXT("\n\n(pendiente)\n\n");
		}
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayRepositoryFileTest, "Soulbound.Game.Presentation.ComoSeJuega.ArchivoDelRepositorio", HowToPlayTestFlags)

bool FHowToPlayRepositoryFileTest::RunTest(const FString& Parameters)
{
	// CA-CSJ-1 (S-022): el cargador acepta el archivo del repositorio y devuelve texto no vacío con los tres títulos, en orden.
	const TArray<FString>& Headings = Soulbound::Presentation::HowToPlayHeadings();
	TestEqual(TEXT("Tres títulos exigidos"), Headings.Num(), 3);
	const FHowToPlayLoadResult Result = Soulbound::Presentation::LoadHowToPlayText(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath));
	if (!TestTrue(FString::Printf(TEXT("El archivo del repositorio se carga (%s)"), *Result.Message), Result.IsSuccess()))
	{
		return false;
	}
	TestFalse(TEXT("Texto no vacío"), Result.Text.TrimStartAndEnd().IsEmpty());
	int32 From = 0;
	for (const FString& Heading : Headings)
	{
		const int32 At = Result.Text.Find(Heading, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
		TestTrue(FString::Printf(TEXT("%s, después del anterior"), *Heading), At != INDEX_NONE);
		From = At == INDEX_NONE ? From : At + Heading.Len();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayMissingFileTest, "Soulbound.Game.Presentation.ComoSeJuega.ArchivoQueNoExiste", HowToPlayTestFlags)

bool FHowToPlayMissingFileTest::RunTest(const FString& Parameters)
{
	// CA-CSJ-1 (S-022): sin archivo, el error de archivo, con la ruta en el motivo; nunca lanza.
	const FString Missing = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Textos/no-existe.md"));
	const FHowToPlayLoadResult Result = Soulbound::Presentation::LoadHowToPlayText(Missing);
	TestEqual(TEXT("Error de archivo"), static_cast<int32>(Result.Error), static_cast<int32>(EHowToPlayError::FileNotFound));
	TestTrue(TEXT("El motivo nombra el archivo"), Result.Message.Contains(TEXT("no-existe.md")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayHeadingsTest, "Soulbound.Game.Presentation.ComoSeJuega.TitulosEnOrden", HowToPlayTestFlags)

bool FHowToPlayHeadingsTest::RunTest(const FString& Parameters)
{
	// CA-CSJ-1 (S-022): los tres títulos en orden se aceptan; si falta uno, si están cambiados de orden o si el texto está vacío, MissingHeading.
	if (!TestEqual(TEXT("Tres títulos exigidos"), Soulbound::Presentation::HowToPlayHeadings().Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Los tres títulos, en orden"), static_cast<int32>(Soulbound::Presentation::ParseHowToPlayText(HowToPlayWith({0, 1, 2})).Error),
		static_cast<int32>(EHowToPlayError::None));
	TestEqual(TEXT("Falta el segundo"), static_cast<int32>(Soulbound::Presentation::ParseHowToPlayText(HowToPlayWith({0, 2})).Error),
		static_cast<int32>(EHowToPlayError::MissingHeading));
	TestEqual(TEXT("Cambiados de orden"), static_cast<int32>(Soulbound::Presentation::ParseHowToPlayText(HowToPlayWith({1, 0, 2})).Error),
		static_cast<int32>(EHowToPlayError::MissingHeading));
	TestEqual(TEXT("Vacío"), static_cast<int32>(Soulbound::Presentation::ParseHowToPlayText(FString()).Error), static_cast<int32>(EHowToPlayError::MissingHeading));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayDisplayTextTest, "Soulbound.Game.Presentation.ComoSeJuega.TextoParaUGT", HowToPlayTestFlags)

bool FHowToPlayDisplayTextTest::RunTest(const FString& Parameters)
{
	// S-022 (decisión del autor del 29/10/2026): el bloque de texto de UGT no entiende Markdown; los títulos van sin "## " y el resto, igual.
	const FString Display = Soulbound::Presentation::HowToPlayDisplayText(TEXT("## Uno\n\nTexto con # dentro.\n\n## Dos\n(pendiente)\n"));
	TestEqual(TEXT("Sin almohadillas en los títulos"), Display, FString(TEXT("Uno\n\nTexto con # dentro.\n\nDos\n(pendiente)\n")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
