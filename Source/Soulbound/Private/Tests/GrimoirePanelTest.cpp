// Tests del Grimorio como widget de Slate (S-048): cómo atiende los clics y qué textos lleva la hoja del jugador. Sin mundo, sin editor y sin FSlateApplication, como los del panel del
// anillo: se monta una caja y se le mandan los eventos directamente, sin pintar. Filtro: "Soulbound.Game.Presentation.Grimorio".

#include "../../Presentation/GrimoirePanel.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Un evento de ratón con el botón izquierdo, en el origen. */
	FPointerEvent GpLeftClick()
	{
		const TSet<FKey> Pressed{EKeys::LeftMouseButton};
		return FPointerEvent(0, FVector2D::ZeroVector, FVector2D::ZeroVector, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	/** Los textos de todos los STextBlock de un widget y de sus hijos, en orden, sin pintarlo. */
	void GpCollectTexts(const TSharedRef<SWidget>& Widget, TArray<FString>& Out)
	{
		if (Widget->GetType() == TEXT("STextBlock"))
		{
			Out.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			GpCollectTexts(Children->GetChildAt(Index), Out);
		}
	}

	TArray<FString> GpTexts(const TSharedRef<SWidget>& Widget)
	{
		TArray<FString> Out;
		GpCollectTexts(Widget, Out);
		return Out;
	}
}

// CA-HUD-65 (R-HUD-18, R-HUD-10; S-048, retoques del autor del 18/11/2026): una caja con GrimoireOnPress que recibe pulsación, doble clic, pulsación y
// doble clic (lo que entrega Slate con cuatro clics rápidos: el segundo de cada par, solo como doble clic) hace su acción cuatro veces y se queda los
// cuatro eventos. Con una acción vacía (el fondo del panel), también se los queda. Que los botones de página, las pestañas y el fondo usen
// GrimoireOnPress se lee en el código y se comprueba a mano en PIE (story, "Notas para la prueba").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireRapidClicksTest, "Soulbound.Game.Presentation.Grimorio.ClicsRapidos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FGrimoireRapidClicksTest::RunTest(const FString& Parameters)
{
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2f(100.f, 40.f), FSlateLayoutTransform());
	const FPointerEvent Click = GpLeftClick();

	int32 Presses = 0;
	const TSharedRef<SBox> Button = SNew(SBox);
	Soulbound::Presentation::GrimoireOnPress(Button, [&Presses]() { ++Presses; });
	int32 Handled = 0;
	for (int32 Pair = 0; Pair < 2; ++Pair)
	{
		Handled += Button->OnMouseButtonDown(Geometry, Click).IsEventHandled() ? 1 : 0;
		Handled += Button->OnMouseButtonDoubleClick(Geometry, Click).IsEventHandled() ? 1 : 0;
	}
	TestEqual(TEXT("Cuatro clics rápidos, cuatro veces la acción"), Presses, 4);
	TestEqual(TEXT("Se queda los cuatro eventos"), Handled, 4);

	const TSharedRef<SBox> Background = SNew(SBox);
	Soulbound::Presentation::GrimoireOnPress(Background, TFunction<void()>());
	TestTrue(TEXT("El fondo se queda la pulsación"), Background->OnMouseButtonDown(Geometry, Click).IsEventHandled());
	TestTrue(TEXT("El fondo se queda el doble clic"), Background->OnMouseButtonDoubleClick(Geometry, Click).IsEventHandled());
	return true;
}

// CA-HUD-67 (R-HUD-10; S-048, retoques de "La mesa" del 18/11/2026): la hoja del jugador construida no tiene ningún texto "Poderes activos" y sí su
// cabecera nueva, en una línea y sin "ELEGIDO:"; la tarjeta del Elegido (S-029) construida tiene las dos cosas como hoy.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireSheetWithoutActivePowersTest, "Soulbound.Game.Presentation.Grimorio.HojaSinPoderesActivos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FGrimoireSheetWithoutActivePowersTest::RunTest(const FString& Parameters)
{
	using namespace Soulbound::Presentation;
	FGrimoireRow Row;
	Row.Seat = 0;
	Row.bHuman = true;
	Row.bElect = true;
	Row.ElectName = TEXT("Tifón");
	Row.DomainId = 4;
	Row.DomainName = TEXT("La Ruina Tormentosa");
	Row.AffinityName = TEXT("Afinidad");
	Row.AffinityText = TEXT("Lo que hace.");
	for (int32 RuneType = 0; RuneType < 6; ++RuneType)
	{
		FGrimoireRune& Rune = Row.Runes.AddDefaulted_GetRef();
		Rune.RuneType = RuneType;
		Rune.Count = RuneType == 4 ? 1 : 0;
		Rune.bHasPower = true;
		Rune.Power = EPower::Haste;
		Rune.PowerName = TEXT("Celeridad");
		Rune.PowerText = TEXT("Lo que hace el poder.");
		Rune.bActive = RuneType == 4;
	}
	Row.ActivePowers.Add(TEXT("Celeridad"));
	const TArray<FLinearColor> Colors{FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow, FLinearColor::White, FLinearColor::Black};

	const TArray<FString> Sheet = GpTexts(MakeGrimoireCard(Row, 14, Colors, 400.f, FGrimoireFont(), true, TEXT("Insolencia: 0 de 5"), true));
	TestTrue(TEXT("Hoja: su cabecera nueva"), Sheet.Contains(TEXT("Asiento 0 (tú) · Tifón · D4 La Ruina Tormentosa")));
	TestFalse(TEXT("Hoja: sin ELEGIDO:"), Sheet.ContainsByPredicate([](const FString& Text) { return Text.Contains(TEXT("ELEGIDO:")); }));
	TestFalse(TEXT("Hoja: sin Poderes activos"), Sheet.ContainsByPredicate([](const FString& Text) { return Text.Contains(TEXT("Poderes activos")); }));
	TestTrue(TEXT("Hoja: la runa activa lo dice"), Sheet.Contains(TEXT("D4 ×1: Celeridad (ACTIVO)")));

	const TArray<FString> Card = GpTexts(MakeGrimoireCard(Row, 14, Colors, 400.f));
	TestTrue(TEXT("Tarjeta del Elegido: su cabecera de hoy"), Card.Contains(TEXT("Asiento 0 (tú) · ELEGIDO: Tifón · D4 La Ruina Tormentosa")));
	TestTrue(TEXT("Tarjeta del Elegido: con Poderes activos"), Card.Contains(TEXT("Poderes activos: Celeridad")));
	return true;
}

#endif
