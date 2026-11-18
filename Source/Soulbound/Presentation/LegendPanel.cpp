#include "LegendPanel.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/**
	 * La altura de una línea de texto con esa letra, en unidades de Slate (S-048, retoques de "La mesa" del 18/11/2026): la que mide el servicio de
	 * fuentes de Slate al pintar. Sin aplicación de Slate o sin renderizador (un test), cero: el cuadrito se queda arriba.
	 */
	float LegendLineHeight(const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return 0.f;
		}
		const FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		return Renderer ? static_cast<float>(Renderer->GetFontMeasureService()->GetMaxCharacterHeight(Font)) : 0.f;
	}
}

float Soulbound::Presentation::LegendChipTopPadding(float LineHeight, float ChipSize)
{
	// Retoques de "La mesa" del 18/11/2026: centrado en la primera línea; si el cuadrito no es más bajo que la línea, arriba del todo.
	if (LineHeight <= KINDA_SMALL_NUMBER || ChipSize >= LineHeight)
	{
		return 0.f;
	}
	return (LineHeight - ChipSize) * 0.5f;
}

void SLegendPanel::Construct(const FArguments& InArgs)
{
	// S-027 (R-HUD-3): la letra se lee del atributo cada vez que se pinta, para que cambie con la resolución sin rehacer el panel.
	FontSize = InArgs._FontSize;
	// S-035: el ancho al que se parten los textos.
	WrapWidth = InArgs._WrapWidth;
	TextFont = InArgs._Font;
	Heading = InArgs._Heading;
	SAssignNew(Body, SVerticalBox);
	if (InArgs._Background)
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
			.Padding(FMargin(8.f, 6.f))
			[
				Body.ToSharedRef()
			]
		];
	}
	else
	{
		ChildSlot
		[
			Body.ToSharedRef()
		];
	}
	SetRows(InArgs._Rows);
}

void SLegendPanel::SetRows(const TArray<FLegendRow>& Rows)
{
	// S-044 (R-VIS-21): lo mismo que ya se pinta no se toca. Las filas con muestras vivas (TextGetter) nunca son iguales, pero la columna solo las da
	// una vez, al construirse.
	if (!Body.IsValid() || (bHasRows && Rows == LastRows))
	{
		return;
	}
	bHasRows = true;
	LastRows = Rows;
	Body->ClearChildren();

	const TAttribute<int32> Size = FontSize;
	// S-035: el ancho al que se parten los textos. Sale del de la columna menos el margen del panel (8 a cada lado) y, en una ficha, la sangría
	// de la fila (8) y el cuadro de color con su separación (letra + 2 + 3). Con WrapTextAt: AutoWrapText dentro de un SWrapBox cortaba por la derecha.
	const TAttribute<float> ColumnWidth = WrapWidth;
	const auto TextWidth = [ColumnWidth](const TAttribute<int32>& InSize, bool bChip)
	{
		return [ColumnWidth, InSize, bChip]()
		{
			const float Column = ColumnWidth.Get(0.f);
			return Column > 0.f ? FMath::Max(Column - 16.f - (bChip ? 8.f + static_cast<float>(InSize.Get()) + 5.f : 0.f), 1.f) : 0.f;
		};
	};
	// S-048 (corrección 4 del autor del 17/11/2026): la fuente del Grimorio si la hay; sin ella, la de hoy (GrimoireFontInfo).
	const Soulbound::Presentation::FGrimoireFont Face = TextFont;
	const auto Font = [Size, Face](const ANSICHAR* Style) { return Soulbound::Presentation::GrimoireFontInfo(Face, FCStringAnsi::Strcmp(Style, "Bold") == 0, Size.Get()); };
	if (!Heading.IsEmpty())
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Heading))
			.Font_Lambda([Font]() { return Font("Bold"); })
			.ColorAndOpacity(FLinearColor::White)
		];
	}

	using Soulbound::Presentation::LegendChipGap;
	using Soulbound::Presentation::LegendSubChipGapX;
	using Soulbound::Presentation::LegendSubChipGapY;
	// Lo que mide el cuadrito con su separación del texto: el texto de una muestra empieza ahí, y las muestras de debajo (SubChips), también.
	const auto ChipAdvance = [Size]() { return static_cast<float>(Size.Get()) + 2.f + 3.f; };
	// Una muestra: el cuadrito y su texto. Retoques de "La mesa" del 18/11/2026 (R-HUD-10): el cuadrito, arriba y centrado en la primera línea del
	// texto (LegendChipTopPadding, con la altura de la línea medida al pintar), para que no quede en medio cuando el texto se parte.
	const auto MakeSample = [Size, Font](const FLinearColor& Color, const TAttribute<FText>& Text, bool bBold, TFunction<float()> Wrap) -> TSharedRef<SWidget>
	{
		const ANSICHAR* Style = bBold ? "Bold" : "Regular";
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			.Padding(TAttribute<FMargin>::CreateLambda([Size, Font, Style]()
			{
				const float Chip = static_cast<float>(Size.Get()) + 2.f;
				return FMargin(0.f, Soulbound::Presentation::LegendChipTopPadding(LegendLineHeight(Font(Style)), Chip), 3.f, 0.f);
			}))
			[
				SNew(SBox)
				.WidthOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f); })
				.HeightOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f); })
				[
					SNew(SColorBlock).Color(Color)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text(Text)
				.WrapTextAt_Lambda([Wrap]() { return Wrap(); })
				.Font_Lambda([Font, Style]() { return Font(Style); })
				.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
			];
	};
	// Las muestras de debajo empiezan bajo el texto de la suya: su ancho, el de una muestra menos el cuadrito de la de arriba.
	const auto ChipWidth = TextWidth(Size, true);
	const TFunction<float()> SubChipWidth = [ChipWidth, ChipAdvance]() { const float Width = ChipWidth(); return Width > 0.f ? FMath::Max(Width - ChipAdvance(), 1.f) : 0.f; };

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const FLegendRow& Row = Rows[RowIndex];
		// S-048 (ronda de maquetación del 18/11/2026): una fila bOnePerLine pone sus muestras una debajo de otra, con el cuadrito y el texto empezando a
		// la misma altura; las demás, repartidas por la línea mientras quepan, como antes. Desde los retoques de "La mesa", con LegendChipGap entre una
		// y la siguiente.
		TSharedRef<SWrapBox> Wrapped = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, LegendChipGap));
		TSharedRef<SVerticalBox> Stacked = SNew(SVerticalBox);
		for (int32 ChipIndex = 0; ChipIndex < Row.Chips.Num(); ++ChipIndex)
		{
			const FLegendChip& Chip = Row.Chips[ChipIndex];
			const TAttribute<FText> Text = TAttribute<FText>::CreateLambda([Chip]() { return FText::FromString(Chip.TextGetter ? Chip.TextGetter() : Chip.Text); });
			TSharedRef<SWidget> Sample = MakeSample(Chip.Color, Text, Chip.bBold, ChipWidth);
			// Retoques de "La mesa" del 18/11/2026 (R-HUD-10): las muestras de debajo (las runas de un Brazal revelado), bajo su texto, repartidas por la
			// línea mientras quepan.
			if (!Chip.SubChips.IsEmpty())
			{
				TSharedRef<SWrapBox> Subs = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(LegendSubChipGapX, LegendSubChipGapY));
				for (const FLegendSubChip& Sub : Chip.SubChips)
				{
					Subs->AddSlot()[MakeSample(Sub.Color, FText::FromString(Sub.Text), false, SubChipWidth)];
				}
				Sample = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Sample]
					+ SVerticalBox::Slot().AutoHeight().Padding(TAttribute<FMargin>::CreateLambda([ChipAdvance]() { return FMargin(ChipAdvance(), LegendSubChipGapY, 0.f, 0.f); }))[Subs];
			}
			if (Row.bOnePerLine)
			{
				Stacked->AddSlot().AutoHeight().Padding(0.f, ChipIndex > 0 ? LegendChipGap : 0.f, 0.f, 0.f)[Sample];
			}
			else
			{
				Wrapped->AddSlot()[Sample];
			}
		}
		const TSharedRef<SWidget> Chips = Row.bOnePerLine ? StaticCastSharedRef<SWidget>(Stacked) : StaticCastSharedRef<SWidget>(Wrapped);
		// Retoques de "La mesa" del 18/11/2026: LegendRowGap entre una fila y la siguiente (antes, 2).
		Body->AddSlot().AutoHeight().Padding(0.f, RowIndex > 0 ? Soulbound::Presentation::LegendRowGap : 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				// S-027 (R-HUD-3): el título parte en líneas en vez de cortarse por la derecha cuando la columna es estrecha (1024x768).
				SNew(STextBlock)
				.Text(FText::FromString(Row.Title))
				.Font_Lambda([Font]() { return Font("Bold"); })
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.4f))
				.WrapTextAt_Lambda(TextWidth(Size, false))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 0.f, 0.f)
			[
				Chips
			]
		];
	}
}
