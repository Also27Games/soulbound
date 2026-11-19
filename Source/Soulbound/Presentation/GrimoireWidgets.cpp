#include "GrimoireWidgets.h"

#include "Engine/Texture2D.h"
#include "GrimoireBook.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
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

TSharedRef<SWidget> Soulbound::Presentation::GrimoireColumns(const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right, const TAttribute<float>& LeftWidth,
	const TAttribute<float>& SpineWidth, EVerticalAlignment RightAlign)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride_Lambda([LeftWidth]() { return FOptionalSize(LeftWidth.Get()); })[Left]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride_Lambda([SpineWidth]() { return FOptionalSize(SpineWidth.Get()); })
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(RightAlign)[Right];
}

TSharedRef<SWidget> Soulbound::Presentation::GrimoireWatermark(const FSlateBrush* Brush, float Opacity, float SizeFraction, bool bRight,
	TFunction<FGrimoireBookLayout()> Layout, TFunction<bool()> Shown)
{
	if (!Brush || !Layout)
	{
		return SNullWidget::NullWidget;
	}
	// El sitio de la marca, en coordenadas del panel (GrimoireWatermarks); el contenido empieza en Content.Min, así que se resta.
	const auto Mark = [Brush, SizeFraction, bRight, Layout]() -> TOptional<FBox2f>
	{
		const FGrimoireBookLayout Book = Layout();
		const FVector2f Texture(Brush->ImageSize.X, Brush->ImageSize.Y);
		const FGrimoireWatermarks Marks = GrimoireWatermarks(Book, Texture, SizeFraction);
		if (!Marks.bShown)
		{
			return TOptional<FBox2f>();
		}
		const FBox2f Box = bRight ? Marks.Right : Marks.Left;
		return FBox2f(Box.Min - Book.Content.Min, Box.Max - Book.Content.Min);
	};
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding_Lambda([Mark]() { const TOptional<FBox2f> Box = Mark(); return Box.IsSet() ? FMargin(Box->Min.X, Box->Min.Y, 0.f, 0.f) : FMargin(0.f); })
		// Siempre que se vea, lleve la página imágenes o no, también en la hoja vacía de la última vista.
		.Visibility_Lambda([Mark, Shown]() { return (!Shown || Shown()) && Mark().IsSet() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SBox)
			.WidthOverride_Lambda([Mark]() { const TOptional<FBox2f> Box = Mark(); return FOptionalSize(Box.IsSet() ? Box->GetSize().X : 0.f); })
			.HeightOverride_Lambda([Mark]() { const TOptional<FBox2f> Box = Mark(); return FOptionalSize(Box.IsSet() ? Box->GetSize().Y : 0.f); })
			[
				SNew(SImage)
				.Image(Brush)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Opacity, 0.f, 1.f)))
			]
		];
}

FSlateBrush Soulbound::Presentation::GrimoireTextureBrush(UTexture2D* Texture, ESlateBrushDrawType::Type DrawAs, const FMargin& Margin)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	// Correcciones tras probar la maquetación de S-048 (18/11/2026): el tamaño de su fuente, no el de una textura que aún se compila.
	Brush.ImageSize = GrimoireTextureSize(Texture);
	Brush.DrawAs = DrawAs;
	Brush.Margin = Margin;
	return Brush;
}
