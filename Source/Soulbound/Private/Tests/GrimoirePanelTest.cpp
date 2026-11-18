// Tests del Grimorio como widget de Slate (S-048): cómo atiende los clics. Sin mundo, sin editor y sin FSlateApplication, como los del panel del
// anillo: se monta una caja y se le mandan los eventos directamente, sin pintar. Filtro: "Soulbound.Game.Presentation.Grimorio".

#include "../../Presentation/GrimoirePanel.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Layout/SBox.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Un evento de ratón con el botón izquierdo, en el origen. */
	FPointerEvent GpLeftClick()
	{
		const TSet<FKey> Pressed{EKeys::LeftMouseButton};
		return FPointerEvent(0, FVector2D::ZeroVector, FVector2D::ZeroVector, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
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

#endif
