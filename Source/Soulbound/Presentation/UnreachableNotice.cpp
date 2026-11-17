#include "UnreachableNotice.h"

void Soulbound::Presentation::FUnreachableNotice::Show(const FString& NewText, const FVector2f& NewPosition, double NowReal)
{
	Text = NewText;
	ClickPosition = NewPosition;
	ShownAt = NowReal;
}

void Soulbound::Presentation::FUnreachableNotice::Dismiss()
{
	Text.Reset();
}

FString Soulbound::Presentation::FUnreachableNotice::Current(double NowReal, double Seconds) const
{
	return !Text.IsEmpty() && NowReal - ShownAt < Seconds ? Text : FString();
}

FVector2f Soulbound::Presentation::FUnreachableNotice::Position() const
{
	return ClickPosition;
}

namespace
{
	/** Si dos rectángulos se pisan; que se toquen en un borde no cuenta. */
	bool Overlap(const FBox2f& A, const FBox2f& B)
	{
		return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y;
	}
}

FVector2f Soulbound::Presentation::PlaceNotice(const FVector2f& Click, const FVector2f& Size, const FBox2f& Bounds, const TArray<FBox2f>& Avoid, float Gap)
{
	// Una esquina recortada a Bounds: con un aviso mayor que Bounds, la esquina de Bounds.
	auto Clamp = [&](FVector2f Corner)
	{
		Corner.X = FMath::Clamp(Corner.X, Bounds.Min.X, FMath::Max(Bounds.Min.X, Bounds.Max.X - Size.X));
		Corner.Y = FMath::Clamp(Corner.Y, Bounds.Min.Y, FMath::Max(Bounds.Min.Y, Bounds.Max.Y - Size.Y));
		return Corner;
	};
	auto IsFree = [&](const FVector2f& Corner)
	{
		const FBox2f Box(Corner, Corner + Size);
		return !Avoid.ContainsByPredicate([&Box](const FBox2f& Zone) { return Overlap(Box, Zone); });
	};

	// Los cuatro lados del clic, en orden. En el eje que lo aleja del clic el sitio ha de caber sin recortar (recortarlo lo pondría encima del clic);
	// en el otro eje se recorta.
	struct FCandidate
	{
		FVector2f Corner;
		bool bFits;
	};
	const FVector2f Centered(Click.X - Size.X * 0.5f, Click.Y - Size.Y * 0.5f);
	const FCandidate Candidates[] = {
		{FVector2f(Centered.X, Click.Y - Size.Y - Gap), Click.Y - Size.Y - Gap >= Bounds.Min.Y},
		{FVector2f(Centered.X, Click.Y + Gap), Click.Y + Gap + Size.Y <= Bounds.Max.Y},
		{FVector2f(Click.X - Size.X - Gap, Centered.Y), Click.X - Size.X - Gap >= Bounds.Min.X},
		{FVector2f(Click.X + Gap, Centered.Y), Click.X + Gap + Size.X <= Bounds.Max.X},
	};
	for (const FCandidate& Candidate : Candidates)
	{
		const FVector2f Corner = Clamp(Candidate.Corner);
		if (Candidate.bFits && IsFree(Corner))
		{
			return Corner;
		}
	}

	// Ningún lado vale: la esquina libre más cercana al clic de una rejilla sobre Bounds.
	constexpr float Step = 8.f;
	bool bFound = false;
	FVector2f Best = Clamp(Candidates[0].Corner);
	float BestDistance = TNumericLimits<float>::Max();
	const FVector2f Last(FMath::Max(Bounds.Min.X, Bounds.Max.X - Size.X), FMath::Max(Bounds.Min.Y, Bounds.Max.Y - Size.Y));
	for (float Y = Bounds.Min.Y; Y <= Last.Y + 0.001f; Y += Step)
	{
		for (float X = Bounds.Min.X; X <= Last.X + 0.001f; X += Step)
		{
			const FVector2f Corner = Clamp(FVector2f(X, Y));
			const float Distance = FVector2f::DistSquared(Corner + Size * 0.5f, Click);
			if (Distance < BestDistance && IsFree(Corner))
			{
				bFound = true;
				BestDistance = Distance;
				Best = Corner;
			}
		}
	}
	return bFound ? Best : Clamp(Candidates[0].Corner);
}

TArray<Soulbound::Presentation::EHudZone> Soulbound::Presentation::NoticeAvoidZones(const FHudShown& Shown, bool bDecisionPanelShown)
{
	// S-045: las de la regla mientras se ven; los paneles de depuración, solo si su tecla los enseña. No hay columna (R-HUD-16).
	TArray<EHudZone> Zones;
	if (Shown.bMatchPanel)
	{
		Zones.Add(EHudZone::MatchPanel);
	}
	if (Shown.bGoalPanel)
	{
		Zones.Add(EHudZone::GoalPanel);
	}
	if (bDecisionPanelShown)
	{
		Zones.Add(EHudZone::DecisionPanel);
	}
	if (Shown.bGrimoireButton)
	{
		Zones.Add(EHudZone::GrimoireButton);
	}
	if (Shown.bStatusPanel)
	{
		Zones.Add(EHudZone::StatusPanel);
	}
	if (Shown.bDebugPanel)
	{
		Zones.Add(EHudZone::DebugPanel);
	}
	return Zones;
}

FBox2f Soulbound::Presentation::NoticeBounds(const FVector2f& LayerSize)
{
	// R-HUD-16: el tablero usa todo el ancho; la capa del aviso llena el viewport.
	return FBox2f(FVector2f::ZeroVector, FVector2f(FMath::Max(LayerSize.X, 1.f), FMath::Max(LayerSize.Y, 1.f)));
}
