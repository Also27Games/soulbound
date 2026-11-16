// Tests del panel del anillo como widget de Slate (S-042): dónde coloca la fila de "Empezar". Sin mundo, sin editor y sin FSlateApplication: se
// monta el widget, se calcula su tamaño (SlatePrepass) y se colocan sus hijos en un rectángulo (ArrangeChildren), sin pintar. La fila es una caja de
// tamaño fijo en vez de los textos de verdad, que necesitan el medidor de letra de la aplicación. Filtro: "Soulbound.Game.Presentation.Anillo".

#include "../../Presentation/RingPanel.h"
#include "../../Presentation/RingPanelModel.h"
#include "Layout/ArrangedChildren.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Layout/SBox.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Dónde queda la fila en el panel: esquina de arriba a la izquierda y tamaño, en unidades del panel. */
	struct FPlacedRow
	{
		FVector2f Position = FVector2f::ZeroVector;
		FVector2f Size = FVector2f::ZeroVector;
	};

	/** Coloca los hijos del panel en un rectángulo de Size con origen en 0 y devuelve dónde queda la fila; vacío si no coloca exactamente uno. */
	TOptional<FPlacedRow> ArrangeStartRow(const TSharedRef<SRingPanel>& Panel, const FVector2f& Size)
	{
		FArrangedChildren Arranged(EVisibility::Visible);
		Panel->ArrangeChildren(FGeometry::MakeRoot(Size, FSlateLayoutTransform()), Arranged);
		if (Arranged.Num() != 1)
		{
			return {};
		}
		const FGeometry& Row = Arranged[0].Geometry;
		return FPlacedRow{FVector2f(Row.GetAbsolutePosition()), FVector2f(Row.GetLocalSize())};
	}
}

// CA-MIN-17 (el panel; corrección del autor del 16/11/2026 tras la segunda prueba en PIE), R-MIN-19: la fila de "Empezar" la coloca el propio panel,
// dentro de él, en la franja de RingPanelLayout calculada con su tamaño de verdad: centrada a lo ancho y en el alto de la franja, entre el círculo y la
// barra. En PIE salía por debajo del panel: la vista la ponía en un hueco de SOverlay con margen y centrado, y Slate centra sumando el margen de arriba
// menos el de abajo, no la mitad (LayoutUtils.h, AlignChild), así que bajaba unos 205 puntos de más con la letra 20. Lo que no cubría el test de la
// colocación (EmpezarDebajoDelAro): comprobaba las cifras de RingPanelLayout, no dónde acababa la fila al colocarla Slate. Montaje: la letra de la
// configuración (20) y una fila de 330 x 56, lo que mide la de verdad (botón de 20 puntos con su margen y, al lado, dos líneas de 16). Se coloca con el
// tamaño que pide el panel y con uno más alto, por si quien lo contiene lo estira: la fila sigue la franja del panel, no una cuenta hecha aparte.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingPanelStartRowTest, "Soulbound.Game.Presentation.Anillo.FilaDeEmpezarDentroDelPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingPanelStartRowTest::RunTest(const FString& Parameters)
{
	constexpr int32 FontSize = 20;
	const FVector2f RowSize(330.f, 56.f);
	const TSharedRef<SRingPanel> Panel = SNew(SRingPanel)
		.FontSize(FontSize)
		[
			SNew(SBox)
			.WidthOverride(RowSize.X)
			.HeightOverride(RowSize.Y)
		];
	Panel->SlatePrepass(1.f);

	const float Width = Soulbound::Presentation::RingPanelWidth(FontSize);
	const float Height = Width * Soulbound::Presentation::RingPanelHeightRatio;
	TestEqual(TEXT("El panel pide su ancho, sin contar la fila"), static_cast<float>(Panel->GetDesiredSize().X), Width, 0.01f);
	TestEqual(TEXT("Y su alto"), static_cast<float>(Panel->GetDesiredSize().Y), Height, 0.01f);

	for (const float Extra : {0.f, 100.f})
	{
		const FVector2f Size(Width, Height + Extra);
		const FRingPanelLayout Layout = Soulbound::Presentation::RingPanelLayout(Size.X, Size.Y, 0.f, FontSize / 18.f);
		const FString Case = FString::Printf(TEXT("Panel de %.0f x %.0f"), Size.X, Size.Y);
		const TOptional<FPlacedRow> Row = ArrangeStartRow(Panel, Size);
		if (!TestTrue(Case + TEXT(": coloca la fila"), Row.IsSet()))
		{
			continue;
		}
		const float Top = Row->Position.Y;
		const float Bottom = Top + Row->Size.Y;
		TestTrue(Case + TEXT(": la fila queda dentro del panel"), Top >= 0.f && Bottom <= Size.Y && Row->Position.X >= 0.f && Row->Position.X + Row->Size.X <= Size.X);
		TestTrue(Case + TEXT(": y dentro de su franja, entre el círculo y la barra"), Top >= Layout.StartBandTop - 0.01f && Bottom <= Layout.StartBandBottom + 0.01f);
		TestEqual(Case + TEXT(": centrada en el alto de la franja"), Top, (Layout.StartBandTop + Layout.StartBandBottom - Row->Size.Y) * 0.5f, 0.01f);
		TestEqual(Case + TEXT(": centrada a lo ancho"), Row->Position.X, (Size.X - Row->Size.X) * 0.5f, 0.01f);
		TestTrue(Case + TEXT(": con el tamaño que pide"), Row->Size.Equals(RowSize, 0.01f));
	}
	return true;
}

#endif
