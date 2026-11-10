#include "DecisionPanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Colores de los paneles de S-027 (StatusText.cpp): verde de la petición al jugador, naranja del Warden, gris de lo secundario. */
	const FLinearColor DecisionGreen = FLinearColor(FColor::Green);
	const FLinearColor DecisionOrange = FLinearColor(FColor::Orange);
	const FLinearColor DecisionGray = FLinearColor(FColor(190, 190, 190));
}

void SDecisionPanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	LargeScale = FMath::Max(InArgs._LargeScale, 1.f);
	TimeFraction = InArgs._TimeFraction;
	OnOption = InArgs._OnOption;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		.Padding(FMargin(12.f, 8.f))
		// Solo los botones reciben clics; el fondo no, para que no se coma el clic de lo que tenga detrás fuera de los botones.
		.Visibility_Lambda([this]() { return View.IsSet() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
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

bool SDecisionPanel::SameLook(const TOptional<Soulbound::Presentation::FDecisionView>& A, const TOptional<Soulbound::Presentation::FDecisionView>& B)
{
	if (A.IsSet() != B.IsSet())
	{
		return false;
	}
	return !A.IsSet() || (A->Input == B->Input && A->Title == B->Title && A->Context == B->Context && A->Instruction == B->Instruction
		&& A->Options == B->Options && A->bBlocked == B->bBlocked);
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

	// Título: qué se decide, con la letra grande del panel, como la petición del panel de estado.
	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Shown.Title))
		.Font(Font("Bold", Large))
		.ColorAndOpacity(DecisionGreen)
		.AutoWrapText(true)
	];
	if (!Shown.Context.IsEmpty())
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Shown.Context))
			.Font(Font("Regular", 1.f))
			.ColorAndOpacity(DecisionGray)
			.AutoWrapText(true)
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
			.AutoWrapText(true)
		];
	}

	// Un botón por opción, con su tecla en negrita delante. No toman el foco: las teclas siguen llegando al controlador.
	if (!Shown.Options.IsEmpty())
	{
		TSharedRef<SWrapBox> Buttons = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
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
			Buttons->AddSlot()
			[
				SNew(SButton)
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
				]
			];
		}
		Content->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)
		[
			Buttons
		];
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
	else
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SBox)
			.HeightOverride_Lambda([Size]() { return FOptionalSize(FMath::Max(Size.Get() * 0.5f, 4.f)); })
			[
				SNew(SProgressBar)
				.Percent_Lambda([this]() { return TOptional<float>(FMath::Clamp(TimeFraction.Get(), 0.f, 1.f)); })
				.FillColorAndOpacity(DecisionGreen)
			]
		];
	}
}
