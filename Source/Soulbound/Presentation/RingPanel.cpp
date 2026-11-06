#include "RingPanel.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Segmentos con que se aproxima un círculo. */
	constexpr int32 CircleSegments = 72;

	/** Un círculo de radio Radius alrededor de Center, como polilínea cerrada. */
	void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Center, float Radius, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2f> Points;
		Points.Reserve(CircleSegments + 1);
		for (int32 Index = 0; Index <= CircleSegments; ++Index)
		{
			const float Angle = 2.f * PI * Index / CircleSegments;
			Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	/** Texto centrado horizontalmente en la línea Top del panel. Devuelve su alto. */
	float DrawCenteredText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Text, const FSlateFontInfo& Font, float Top,
		const FLinearColor& Color)
	{
		if (Text.IsEmpty())
		{
			return 0.f;
		}
		const FVector2f Size = FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font));
		const FVector2f Position((Geometry.GetLocalSize().X - Size.X) * 0.5f, Top);
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Text, Font, ESlateDrawEffect::None, Color);
		return Size.Y;
	}
}

void SRingPanel::Construct(const FArguments& InArgs)
{
	FontSize = FMath::Max(InArgs._FontSize, 8);
	Model = InArgs._Model;
}

FLinearColor SRingPanel::PrecisionColor(double Precision)
{
	const float Clamped = static_cast<float>(FMath::Clamp(Precision, 0.0, 1.0));
	const FLinearColor Red(0.9f, 0.15f, 0.1f);
	const FLinearColor Yellow(0.95f, 0.85f, 0.15f);
	const FLinearColor Green(0.2f, 0.9f, 0.3f);
	return Clamped < 0.5f ? FLinearColor::LerpUsingHSV(Red, Yellow, Clamped * 2.f) : FLinearColor::LerpUsingHSV(Yellow, Green, (Clamped - 0.5f) * 2.f);
}

FVector2D SRingPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// Cuadrado proporcional a la letra: a 18 puntos, 420 píxeles de ancho.
	const float Side = 420.f * FontSize / 18.f;
	return FVector2D(Side, Side * 1.15f);
}

int32 SRingPanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FRingPanelModel Now = Model ? Model() : FRingPanelModel();
	if (!Now.bVisible)
	{
		return LayerId;
	}
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Scale = FontSize / 18.f;

	// Fondo.
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
		FLinearColor(0.f, 0.f, 0.f, 0.72f));
	const int32 Layer = LayerId + 1;

	// Título y contador.
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, FMath::RoundToInt(FontSize * 0.75f)));
	const FSlateFontInfo BigFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(FontSize * 1.6f));
	float Top = 12.f * Scale;
	Top += DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Now.Title, TitleFont, Top, FLinearColor::White) + 4.f * Scale;
	Top += DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Now.Counter, SmallFont, Top, FLinearColor(0.8f, 0.8f, 0.8f));

	// La runa, la guía de la zona buena y el anillo.
	const FVector2f Center(Size.X * 0.5f, Size.Y * 0.5f);
	const float RuneRadius = 34.f * Scale;
	const float MaxRadius = FMath::Min(Size.X, Size.Y) * 0.36f;
	if (Now.bShowRing)
	{
		DrawCircle(OutDrawElements, Layer, AllottedGeometry, Center, RuneRadius, FLinearColor(0.55f, 0.75f, 1.f), 7.f * Scale);
		DrawCircle(OutDrawElements, Layer, AllottedGeometry, Center, RuneRadius * 0.45f, FLinearColor(0.55f, 0.75f, 1.f), 4.f * Scale);
		const float ZoneRadius = RuneRadius + (MaxRadius - RuneRadius) * static_cast<float>(FMath::Clamp(Now.ZoneFraction, 0.0, 1.0));
		DrawCircle(OutDrawElements, Layer, AllottedGeometry, Center, ZoneRadius, FLinearColor(0.2f, 0.9f, 0.3f, 0.35f), 2.f * Scale);
		const float RingRadius = RuneRadius + (MaxRadius - RuneRadius) * static_cast<float>(1.0 - FMath::Clamp(Now.CloseFraction, 0.0, 1.0));
		DrawCircle(OutDrawElements, Layer, AllottedGeometry, Center, RingRadius, FLinearColor::White, 4.f * Scale);
	}

	// El texto grande: bajo la runa durante la prueba; en el centro con el resultado.
	const float BigTop = Now.bShowRing ? Center.Y + MaxRadius + 4.f * Scale : Center.Y - FontSize * 1.2f;
	DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Now.Big, BigFont, BigTop, Now.BigColor);

	// Una marca por pulso, abajo: gris si falta, del color de su precisión si ya se resolvió.
	const int32 Count = Now.Pulses.Num();
	const float Dot = 9.f * Scale;
	const float Gap = 10.f * Scale;
	const float RowWidth = Count * (2.f * Dot) + FMath::Max(Count - 1, 0) * Gap;
	const float DotsY = Size.Y - 22.f * Scale;
	for (int32 Pulse = 0; Pulse < Count; ++Pulse)
	{
		const FVector2f DotCenter((Size.X - RowWidth) * 0.5f + Dot + Pulse * (2.f * Dot + Gap), DotsY);
		if (Now.Pulses[Pulse].IsSet())
		{
			const FLinearColor Color = PrecisionColor(Now.Pulses[Pulse].GetValue());
			DrawCircle(OutDrawElements, Layer, AllottedGeometry, DotCenter, Dot * 0.5f, Color, Dot);
		}
		else
		{
			DrawCircle(OutDrawElements, Layer, AllottedGeometry, DotCenter, Dot, FLinearColor(0.5f, 0.5f, 0.5f), 2.f * Scale);
		}
	}
	return Layer;
}
