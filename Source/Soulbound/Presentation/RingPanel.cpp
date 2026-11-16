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

	/** Como DrawCenteredText, pero partiendo el texto por palabras en líneas que no pasen de MaxWidth, cada una centrada. Devuelve el alto total. */
	float DrawCenteredWrapped(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Text, const FSlateFontInfo& Font, float Top,
		const FLinearColor& Color, float MaxWidth)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "));
		float Height = 0.f;
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (!Line.IsEmpty() && Measure->Measure(Candidate, Font).X > MaxWidth)
			{
				Height += DrawCenteredText(Out, Layer, Geometry, Line, Font, Top + Height, Color);
				Line = Word;
			}
			else
			{
				Line = Candidate;
			}
		}
		return Height + DrawCenteredText(Out, Layer, Geometry, Line, Font, Top + Height, Color);
	}

	/**
	 * El título del panel (S-042, R-VIS-30): en una línea si cabe; si no, el nombre de la prueba (hasta los dos puntos, incluidos) en una línea y lo
	 * que te juegas debajo, cada parte partida por palabras si aún no cabe (decisión del autor del 16/11/2026). Devuelve el alto total.
	 */
	float DrawTitle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Title, const FSlateFontInfo& Font, float Top, float MaxWidth)
	{
		if (Title.IsEmpty())
		{
			return 0.f;
		}
		FString Name;
		FString Stake;
		const bool bFits = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Title, Font).X <= MaxWidth;
		if (bFits || !Title.Split(TEXT(": "), &Name, &Stake))
		{
			return DrawCenteredWrapped(Out, Layer, Geometry, Title, Font, Top, FLinearColor::White, MaxWidth);
		}
		const float NameHeight = DrawCenteredWrapped(Out, Layer, Geometry, Name + TEXT(":"), Font, Top, FLinearColor::White, MaxWidth);
		return NameHeight + DrawCenteredWrapped(Out, Layer, Geometry, Stake, Font, Top + NameHeight, FLinearColor::White, MaxWidth);
	}
}

void SRingPanel::Construct(const FArguments& InArgs)
{
	FontSize = FMath::Max(InArgs._FontSize, 8);
	Model = InArgs._Model;
	RuneBrush = InArgs._RuneBrush;
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
	// Proporcional a la letra: a 18 puntos, 420 píxeles de ancho. Desde S-042, más alto que ancho (RingPanelHeightRatio; antes 1,15): el título con
	// lo que te juegas puede ocupar dos o tres líneas (R-VIS-30) y bajo el aro va la franja de "Empezar" (R-MIN-19).
	const float Side = Soulbound::Presentation::RingPanelWidth(FontSize);
	return FVector2D(Side, Side * Soulbound::Presentation::RingPanelHeightRatio);
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

	// Título y, debajo, la instrucción (cada línea centrada y por separado, R-VIS-17) o la nota del resultado.
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, FMath::RoundToInt(FontSize * 0.75f)));
	const FSlateFontInfo BigFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(FontSize * 1.6f));
	float Top = 12.f * Scale;
	// S-042 (R-VIS-30): el título dice qué te juegas y puede no caber en una línea; se parte (DrawTitle) dentro del ancho con un margen a cada lado.
	Top += DrawTitle(OutDrawElements, Layer, AllottedGeometry, Now.Title, TitleFont, Top, Size.X - 24.f * Scale) + 4.f * Scale;
	for (const FString& Line : Now.Instruction)
	{
		Top += DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Line, SmallFont, Top, FLinearColor(0.8f, 0.8f, 0.8f));
	}
	Top += DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Now.Note, SmallFont, Top, FLinearColor(0.8f, 0.8f, 0.8f));

	// La franja verde de la zona buena, la runa y el aro (R-VIS-14, R-VIS-18, R-VIS-19). Orden de pintado: franja, runa, aro. Desde S-042 el aro va
	// entre los textos y la franja de "Empezar", que la vista llena con el botón mientras la prueba espera (RingPanelLayout).
	const FRingPanelLayout Layout = Soulbound::Presentation::RingPanelLayout(Size.X, Size.Y, Top, Scale);
	const float RuneRadius = 34.f * Scale;
	const float MaxRadius = Layout.MaxRadius;
	const FVector2f Center(Size.X * 0.5f, Layout.CenterY);
	if (Now.bShowRing)
	{
		// La franja va del borde de la runa al radio en que el aro entra en la zona (el cierre 1 - ZoneFraction): un círculo grueso a media distancia. Su color y opacidad vienen de la configuración (RingZoneColor).
		const float ZoneRadius = static_cast<float>(Soulbound::Presentation::RingInnerRadius(1.0 - FMath::Clamp(Now.ZoneFraction, 0.0, 1.0), RuneRadius, MaxRadius));
		if (ZoneRadius > RuneRadius)
		{
			DrawCircle(OutDrawElements, Layer, AllottedGeometry, Center, (RuneRadius + ZoneRadius) * 0.5f, Now.ZoneColor, ZoneRadius - RuneRadius);
		}

		// La runa: la imagen teñida, del tamaño del diámetro de siempre; sin textura, un disco relleno del mismo color (un círculo cuyo grosor es su radio).
		const FSlateBrush* Brush = RuneBrush ? RuneBrush() : nullptr;
		if (Brush && Brush->GetResourceObject())
		{
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(2.f * RuneRadius), FSlateLayoutTransform(Center - FVector2f(RuneRadius))),
				Brush, ESlateDrawEffect::None, Now.RuneColor);
		}
		else
		{
			DrawCircle(OutDrawElements, Layer + 1, AllottedGeometry, Center, RuneRadius * 0.5f, Now.RuneColor, RuneRadius);
		}

		// El aro: blanco fuera de la zona buena; verde y más grueso dentro. Se pinta con el radio de su borde interior (RingInnerRadius), así que
		// toca la runa exactamente en la coincidencia; la polilínea es simétrica, y su centro va medio grosor más afuera.
		const float Thickness = (Now.bInSweetZone ? 7.f : 4.f) * Scale;
		const float InnerRadius = static_cast<float>(Soulbound::Presentation::RingInnerRadius(Now.CloseFraction, RuneRadius, MaxRadius));
		DrawCircle(OutDrawElements, Layer + 2, AllottedGeometry, Center, InnerRadius + Thickness * 0.5f,
			Now.bInSweetZone ? FLinearColor(0.2f, 0.95f, 0.3f) : FLinearColor::White, Thickness);
	}

	// El resultado ("SUPERADA", "FALLADA") en el centro, solo al acabar. Durante la prueba no hay texto de puntuación (R-VIS-17).
	DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Now.Big, BigFont, Center.Y - FontSize * 1.2f, Now.BigColor);

	// La barra de progreso bajo el anillo (R-VIS-16): el fondo, el relleno del color de la última precisión y la marca del objetivo.
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const float BarWidth = Size.X * 0.7f;
	const float BarHeight = 12.f * Scale;
	const FVector2f BarPosition((Size.X - BarWidth) * 0.5f, Layout.BarTop);
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(BarWidth, BarHeight), FSlateLayoutTransform(BarPosition)), White,
		ESlateDrawEffect::None, FLinearColor(0.25f, 0.25f, 0.25f, 0.9f));
	const float FillWidth = BarWidth * static_cast<float>(FMath::Clamp(Now.ProgressFill, 0.0, 1.0));
	if (FillWidth > 0.f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(FillWidth, BarHeight), FSlateLayoutTransform(BarPosition)), White,
			ESlateDrawEffect::None, Now.ProgressColor);
	}
	if (Now.ProgressMark.IsSet())
	{
		const float MarkWidth = 3.f * Scale;
		const float MarkX = BarPosition.X + BarWidth * static_cast<float>(FMath::Clamp(Now.ProgressMark.GetValue(), 0.0, 1.0)) - MarkWidth * 0.5f;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 2,
			AllottedGeometry.ToPaintGeometry(FVector2f(MarkWidth, BarHeight + 8.f * Scale), FSlateLayoutTransform(FVector2f(MarkX, BarPosition.Y - 4.f * Scale))), White,
			ESlateDrawEffect::None, FLinearColor::White);
	}

	// Una marca por pulso, abajo: gris si falta, del color de su precisión si ya se resolvió.
	const int32 Count = Now.Pulses.Num();
	const float Dot = 9.f * Scale;
	const float Gap = 10.f * Scale;
	const float RowWidth = Count * (2.f * Dot) + FMath::Max(Count - 1, 0) * Gap;
	const float DotsY = Layout.DotsY;
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
	return Layer + 2;
}
