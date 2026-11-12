#include "DecisionPanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Colores de los paneles de S-027 (StatusText.cpp): verde de la petición al jugador, naranja del Warden, gris de lo secundario. */
	const FLinearColor DecisionGreen = FLinearColor(FColor::Green);
	const FLinearColor DecisionOrange = FLinearColor(FColor::Orange);
	const FLinearColor DecisionGray = FLinearColor(FColor(190, 190, 190));

	/** Margen de cada botón en su celda, por cada lado: entre dos botones queda el doble. */
	constexpr float ButtonSlotMargin = 3.f;

	/** El margen del fondo del panel, por cada lado, en horizontal: lo que el panel le quita al ancho de los botones. */
	constexpr float PanelPaddingX = 12.f;
}

void SDecisionPanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	LargeScale = FMath::Max(InArgs._LargeScale, 1.f);
	TimeFraction = InArgs._TimeFraction;
	MaxWidth = InArgs._MaxWidth;
	OnOption = InArgs._OnOption;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		.Padding(FMargin(PanelPaddingX, 8.f))
		// El panel se queda el clic: el que cae en el fondo, en un texto o en un botón desactivado (la mesa espera al Warden) no llega a la casilla
		// ni a la ficha de detrás. Sin esto, un clic sin manejar sube hasta el viewport y el controlador lo toma como clic en el tablero.
		.Visibility_Lambda([this]() { return View.IsSet() ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SAssignNew(Content, SVerticalBox)
		]
	];
}

void SDecisionPanel::SetView(const TOptional<Soulbound::Presentation::FDecisionView>& NewView)
{
	if (SameLook(View, NewView))
	{
		View = NewView;
		return;
	}
	View = NewView;
	Rebuild();
}

void SDecisionPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// El ancho de los botones no depende de cómo van colocados, así que la decisión no oscila: solo cambia con los botones o con el hueco.
	const bool bFits = ButtonsFitInRow();
	if (ButtonGrid.IsValid() && bFits != bButtonsInRow)
	{
		bButtonsInRow = bFits;
		LayoutButtons(bButtonsInRow);
	}
}

bool SDecisionPanel::ButtonsFitInRow() const
{
	float Widest = 0.f;
	for (const TSharedRef<SWidget>& Button : Buttons)
	{
		Widest = FMath::Max(Widest, static_cast<float>(Button->GetDesiredSize().X));
	}
	return Soulbound::Presentation::DecisionButtonsInRow(Buttons.Num(), Widest, 2.f * ButtonSlotMargin, MaxWidth.Get() - 2.f * PanelPaddingX);
}

void SDecisionPanel::LayoutButtons(bool bInRow)
{
	if (!ButtonGrid.IsValid())
	{
		return;
	}
	ButtonGrid->ClearChildren();
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		ButtonGrid->AddSlot(bInRow ? Index : 0, bInRow ? 0 : Index)
		[
			Buttons[Index]
		];
	}
}

bool SDecisionPanel::SameLook(const TOptional<Soulbound::Presentation::FDecisionView>& A, const TOptional<Soulbound::Presentation::FDecisionView>& B)
{
	if (A.IsSet() != B.IsSet())
	{
		return false;
	}
	return !A.IsSet() || (A->Input == B->Input && A->Title == B->Title && A->Context == B->Context && A->Instruction == B->Instruction
		&& A->Options == B->Options && A->bBlocked == B->bBlocked && A->Lines == B->Lines && A->bHasDeadline == B->bHasDeadline);
}

void SDecisionPanel::Rebuild()
{
	if (!Content.IsValid())
	{
		return;
	}
	Content->ClearChildren();
	if (!View.IsSet())
	{
		return;
	}
	const TAttribute<int32> Size = FontSize;
	const float Large = LargeScale;
	auto Font = [Size](const char* Style, float Scale)
	{
		return TAttribute<FSlateFontInfo>::CreateLambda([Size, Style, Scale]()
		{
			return FCoreStyle::GetDefaultFontStyle(Style, FMath::Max(FMath::RoundToInt(Size.Get() * Scale), 1));
		});
	};
	const Soulbound::Presentation::FDecisionView& Shown = View.GetValue();
	// R-DEC-13 (S-033, corrección del autor del 12/11/2026): los textos se parten al ancho libre del hueco menos el margen del fondo, no con
	// AutoWrapText, que parte al ancho con que se pintó antes (el mismo defecto que la frase del Warden, R-HUD-3).
	auto Wrap = [this]() { return FMath::Max(MaxWidth.Get() - 2.f * PanelPaddingX, 1.f); };

	// S-033 (R-DEC-7): la espera a otro asiento y el resultado de una votación no son una decisión del humano: título en el gris de lo secundario,
	// sin botones y con la barra, si la lleva, en gris. Solo la decisión propia va en verde.
	const bool bOwnDecision = Shown.Input != Soulbound::Presentation::EDecisionInput::Wait && Shown.Input != Soulbound::Presentation::EDecisionInput::Result;
	const FLinearColor Accent = bOwnDecision ? DecisionGreen : DecisionGray;

	// Título: qué se decide, con la letra grande del panel, como la petición del panel de estado.
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Shown.Title))
		.Font(Font("Bold", Large))
		.ColorAndOpacity(Accent)
		.WrapTextAt_Lambda(Wrap)
	];
	if (!Shown.Context.IsEmpty())
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Shown.Context))
			.Font(Font("Regular", 1.f))
			.ColorAndOpacity(DecisionGray)
			.WrapTextAt_Lambda(Wrap)
		];
	}
	if (!Shown.Instruction.IsEmpty())
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Shown.Instruction))
			.Font(Font("Regular", 1.f))
			.ColorAndOpacity(FLinearColor::White)
			.WrapTextAt_Lambda(Wrap)
		];
	}

	// Una línea por voto en el resultado de una votación (S-033, R-DEC-8).
	for (const FString& Line : Shown.Lines)
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Line))
			.Font(Font("Regular", 1.f))
			.ColorAndOpacity(FLinearColor::White)
			.WrapTextAt_Lambda(Wrap)
		];
	}

	// Un botón por opción, con su tecla en negrita delante. No toman el foco: las teclas siguen llegando al controlador. Todos del ancho del más
	// largo, en fila si caben y si no en columna (corrección del autor del 11/11/2026): una rejilla uniforme, que Tick recoloca.
	Buttons.Reset();
	ButtonGrid.Reset();
	if (!Shown.Options.IsEmpty())
	{
		const bool bEnabled = !Shown.bBlocked;
		for (const Soulbound::Presentation::FDecisionOption& Option : Shown.Options)
		{
			TSharedRef<SHorizontalBox> Label = SNew(SHorizontalBox);
			if (!Option.Key.IsEmpty())
			{
				Label->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock).Text(FText::FromString(Option.Key)).Font(Font("Bold", Large))
				];
			}
			Label->AddSlot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(Option.Text)).Font(Font("Regular", Large))
			];
			Buttons.Add(SNew(SButton)
				.IsFocusable(false)
				.IsEnabled(bEnabled)
				.ContentPadding(FMargin(10.f, 4.f))
				.OnClicked_Lambda([this, Option]()
				{
					OnOption.ExecuteIfBound(Option);
					return FReply::Handled();
				})
				[
					Label
				]);
		}
		Content->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 3.f, 0.f, 0.f)
		[
			SAssignNew(ButtonGrid, SUniformGridPanel).SlotPadding(FMargin(ButtonSlotMargin))
		];
		// Hasta el primer tick no se sabe lo que mide cada botón: se empieza en fila y Tick lo corrige si no cabe.
		bButtonsInRow = true;
		LayoutButtons(bButtonsInRow);
	}

	// El reloj: una barra que se vacía, sin números; mientras la mesa espera al Warden, el texto en su sitio (R-DEC-3).
	if (Shown.bBlocked)
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Soulbound::Presentation::DecisionBlockedText))
			.Font(Font("Bold", 1.f))
			.ColorAndOpacity(DecisionOrange)
		];
	}
	else if (Shown.bHasDeadline)
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SBox)
			.HeightOverride_Lambda([Size]() { return FOptionalSize(FMath::Max(Size.Get() * 0.5f, 4.f)); })
			[
				SNew(SProgressBar)
				.Percent_Lambda([this]() { return TOptional<float>(FMath::Clamp(TimeFraction.Get(), 0.f, 1.f)); })
				// El relleno por defecto de SProgressBar ya es azul y el color solo lo tiñe: con un pincel blanco el color es el que se pide
				// (corrección del autor del 11/11/2026, R-DEC-7).
				.FillImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.FillColorAndOpacity(Accent)
			]
		];
	}
}
