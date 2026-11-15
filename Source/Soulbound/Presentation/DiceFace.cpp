#include "DiceFace.h"

#include "Match/TurnRules.h"

TArray<FVector2f> Soulbound::Presentation::DiceFacePips(int32 Value)
{
	// Los tres ejes de la cara, por el lado de la cara. Corrección del autor del 15/11/2026: los puntos, más pequeños (DicePipDiameterRatio) y más
	// separados: entre dos puntos queda al menos un diámetro libre y los del borde quedan a más de un diámetro y medio del borde.
	constexpr float Near = 0.22f;
	constexpr float Mid = 0.5f;
	constexpr float Far = 0.78f;
	const FVector2f TopLeft(Near, Near), TopRight(Far, Near), BottomLeft(Near, Far), BottomRight(Far, Far), Center(Mid, Mid);
	switch (Value)
	{
	case 1: return {Center};
	case 2: return {TopLeft, BottomRight};
	case 3: return {TopLeft, Center, BottomRight};
	case 4: return {TopLeft, TopRight, BottomLeft, BottomRight};
	case 5: return {TopLeft, TopRight, Center, BottomLeft, BottomRight};
	case 6: return {FVector2f(Near, Near), FVector2f(Far, Near), FVector2f(Near, Mid), FVector2f(Far, Mid), FVector2f(Near, Far), FVector2f(Far, Far)};
	default: return {};
	}
}

FBox2f Soulbound::Presentation::DiceFaceUV(int32 Value)
{
	if (Value < 1 || Value > Soulbound::Dice::Faces)
	{
		return FBox2f(ForceInit);
	}
	const float Width = 1.f / Soulbound::Dice::Faces;
	return FBox2f(FVector2f((Value - 1) * Width, 0.f), FVector2f(Value == Soulbound::Dice::Faces ? 1.f : Value * Width, 1.f));
}
