#include "LegendPanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

float Soulbound::Presentation::LegendChipTopPadding(float LineHeight, float ChipSize)
{
	// TODO(R-HUD-10): stub de los tests en rojo (el cuadrito, arriba del todo); se implementa en el commit siguiente.
	return 0.f;
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

	for (const FLegendRow& Row : Rows)
	{
		// S-048 (ronda de maquetación del 18/11/2026): una fila bOnePerLine pone sus muestras una debajo de otra, con el cuadrito y el texto empezando a
		// la misma altura; las demás, repartidas por la línea mientras quepan, como antes.
		TSharedRef<SWrapBox> Wrapped = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 1.f));
		TSharedRef<SVerticalBox> Stacked = SNew(SVerticalBox);
		for (const FLegendChip& Chip : Row.Chips)
		{
			const TSharedRef<SWidget> Sample =
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f); })
					.HeightOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f); })
					[
						SNew(SColorBlock).Color(Chip.Color)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Chip]() { return FText::FromString(Chip.TextGetter ? Chip.TextGetter() : Chip.Text); })
					.WrapTextAt_Lambda(TextWidth(Size, true))
					.Font_Lambda([Font]() { return Font("Regular"); })
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
				];
			if (Row.bOnePerLine)
			{
				Stacked->AddSlot().AutoHeight().Padding(0.f, 1.f)[Sample];
			}
			else
			{
				Wrapped->AddSlot()[Sample];
			}
		}
		const TSharedRef<SWidget> Chips = Row.bOnePerLine ? StaticCastSharedRef<SWidget>(Stacked) : StaticCastSharedRef<SWidget>(Wrapped);
		Body->AddSlot().AutoHeight().Padding(0.f, 1.f)
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
