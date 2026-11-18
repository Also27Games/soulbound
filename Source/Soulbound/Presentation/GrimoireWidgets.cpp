#include "GrimoireWidgets.h"

#include "GrimoireBook.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<STextBlock> Soulbound::Presentation::GrimoireText(const FGrimoireFont& Font, const TAttribute<int32>& Size, const TAttribute<float>& Wrap, const FString& Text,
	const FLinearColor& Color, bool bBold, float Scale)
{
	// S-048 (corrección 4 del autor del 17/11/2026): con la fuente del Grimorio si la hay; sin ella, la de hoy.
	return SNew(STextBlock)
		.Text(FText::FromString(Text))
		.Font_Lambda([Font, Size, bBold, Scale]() { return Soulbound::Presentation::GrimoireFontInfo(Font, bBold, FMath::RoundToInt(Size.Get() * Scale)); })
		.ColorAndOpacity(Color)
		.WrapTextAt_Lambda([Wrap]() { return Wrap.Get(); });
}

TSharedRef<STextBlock> Soulbound::Presentation::GrimoireFootText(const FGrimoireFont& Font, const TAttribute<int32>& Size, const TAttribute<float>& Wrap,
	const TAttribute<FText>& Text, const FLinearColor& Color, bool bBold)
{
	const TSharedRef<STextBlock> Block = GrimoireText(Font, Size, Wrap, FString(), Color, bBold);
	Block->SetText(Text);
	return Block;
}

TSharedRef<SWidget> Soulbound::Presentation::GrimoireScroll(const TSharedRef<SWidget>& Content)
{
	return SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(GrimoireScrollBarThickness, GrimoireScrollBarThickness))
		.ScrollBarPadding(FMargin(GrimoireScrollBarPadding))
		+ SScrollBox::Slot().Padding(0.f, 0.f, GrimoireScrollGap, 0.f)[Content];
}

TSharedRef<SBorder> Soulbound::Presentation::GrimoireFrame(const TAttribute<FSlateColor>& Color, const TSharedRef<SWidget>& Content)
{
	const TSharedRef<TWeakPtr<SWidget>> Self = MakeShared<TWeakPtr<SWidget>>();
	const TSharedRef<SBorder> Frame = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Color)
		.Padding_Lambda([Self]()
		{
			const TSharedPtr<SWidget> Widget = Self->Pin();
			return FMargin(Soulbound::Presentation::GrimoireFrameWidth(Widget.IsValid() ? Widget->GetCachedGeometry().Scale : 1.f));
		})
		[
			Content
		];
	*Self = Frame;
	return Frame;
}

void Soulbound::Presentation::GrimoireOnPress(const TSharedRef<SWidget>& Widget, TFunction<void()> OnPress)
{
	// Retoques del autor del 18/11/2026: con dos clics rápidos, Slate entrega el segundo solo como doble clic (FSlateApplication::
	// ProcessMouseButtonDoubleClickEvent no lo repite como pulsación); los dos llevan a la misma acción. Cualquier botón del ratón, como antes.
	const FPointerEventHandler Press = FPointerEventHandler::CreateLambda([OnPress](const FGeometry&, const FPointerEvent&)
	{
		if (OnPress)
		{
			OnPress();
		}
		return FReply::Handled();
	});
	Widget->SetOnMouseButtonDown(Press);
	Widget->SetOnMouseDoubleClick(Press);
}
