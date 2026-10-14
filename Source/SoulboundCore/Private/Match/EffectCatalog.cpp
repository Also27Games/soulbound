#include "Match/EffectCatalog.h"

#include "UObject/Class.h"

namespace
{
	using T = EEffectTarget;
	using S = EEffectSeverity;
	using V = EEffectVariant;

	FEffectDef Def(EEffect Effect, S Severity, TArray<T> Targets, bool bImplemented, bool bTakesAmount = false, TArray<V> Variants = {})
	{
		FEffectDef Entry;
		Entry.Effect = Effect;
		Entry.Severity = Severity;
		Entry.AllowedTargets = MoveTemp(Targets);
		Entry.bImplemented = bImplemented;
		Entry.bTakesAmount = bTakesAmount;
		Entry.Variants = MoveTemp(Variants);
		return Entry;
	}

	/** La tabla de R-EFE-0 (GDD 05), en el orden de EEffect. */
	TArray<FEffectDef> MakeCatalog()
	{
		return {
			Def(EEffect::Halt, S::Light, {T::ActivePlayer}, false),
			Def(EEffect::RevealBracer, S::Light, {T::ActivePlayer, T::ChosenRival, T::Leader, T::Everyone}, true),
			Def(EEffect::ForcedOracle, S::Light, {T::ActivePlayer, T::ChosenRival, T::NextPlayer}, false),
			Def(EEffect::SendHome, S::Medium, {T::ActivePlayer, T::ChosenRival, T::Leader, T::NextPlayer, T::Everyone, T::RandomPlayer}, true),
			Def(EEffect::LoseTurn, S::Medium, {T::ActivePlayer, T::ChosenRival, T::Leader, T::NextPlayer, T::RandomPlayer}, true, true),
			// El Sello no va a ningún asiento: el Warden elige la casilla (decisión del autor del 12/10/2026: sin dirección).
			Def(EEffect::PlaceSeal, S::Medium, {T::None}, true),
			Def(EEffect::SealAgainstRival, S::Medium, {T::ChosenRival, T::Leader}, true),
			Def(EEffect::Mark, S::Medium, {T::ChosenRival, T::Leader, T::RandomPlayer}, true),
			Def(EEffect::ShadowLaw, S::Medium, {T::Echoes}, true),
			Def(EEffect::WardenTrial, S::Medium, {T::ActivePlayer, T::ChosenRival, T::NextPlayer}, false),
			Def(EEffect::LoseRune, S::Severe, {T::ActivePlayer, T::ChosenRival, T::Leader, T::Everyone, T::RandomPlayer}, true, false, {V::Random, V::Chosen}),
			// "Activo roba a rival señalado, a todos o al azar": quien roba es siempre el activo; la dirección es la víctima.
			Def(EEffect::DecreedTheft, S::Severe, {T::ChosenRival, T::Everyone, T::RandomPlayer}, false),
			// "Activo contra rival señalado; dos rivales señalados; dos al azar". Lo que está en juego, en la variante.
			Def(EEffect::ForcedDuel, S::Severe, {T::ChosenRival, T::TwoChosenRivals, T::RandomPlayer}, false, false, {V::OneRune, V::AllRunes}),
			Def(EEffect::Banish, S::Severe, {T::ActivePlayer, T::ChosenRival, T::Leader, T::RandomPlayer}, true),
			Def(EEffect::ExtraTurn, S::Gift, {T::ActivePlayer, T::NextPlayer}, true, true),
			Def(EEffect::GiftRune, S::Gift, {T::ActivePlayer, T::NextPlayer, T::RandomPlayer}, true),
			// "Todos los del Vacío" (decisión del autor del 12/10/2026): la dirección Todos, con el filtro de quien está dentro.
			Def(EEffect::ReleaseVoid, S::Gift, {T::Everyone}, true),
			Def(EEffect::EchoNight, S::Gift, {T::Echoes}, true),
		};
	}

	/** Códigos de los datos de cada dirección, en el orden de EEffectTarget; None no tiene. */
	const TCHAR* const TargetCodes[] = {
		TEXT("ActivePlayer"), TEXT("ChosenRival"), TEXT("TwoChosenRivals"), TEXT("Leader"), TEXT("NextPlayer"), TEXT("Everyone"), TEXT("RandomPlayer"), TEXT("Echoes"),
	};

	/** Códigos de los datos de cada variante, en el orden de EEffectVariant; None no tiene. */
	const TCHAR* const VariantCodes[] = {TEXT(""), TEXT("random"), TEXT("chosen"), TEXT("one"), TEXT("all")};
}

namespace Soulbound::Rules
{
	const TArray<FEffectDef>& EffectCatalog()
	{
		static const TArray<FEffectDef> Catalog = MakeCatalog();
		return Catalog;
	}

	const FEffectDef& EffectDefOf(EEffect Effect)
	{
		return EffectCatalog()[static_cast<int32>(Effect)];
	}

	bool IsGift(EEffect Effect)
	{
		return EffectDefOf(Effect).Severity == EEffectSeverity::Gift;
	}

	bool IsEffectImplemented(EEffect Effect)
	{
		return EffectDefOf(Effect).bImplemented;
	}

	bool IsGlobalEffect(EEffect Effect)
	{
		return Effect == EEffect::PlaceSeal || Effect == EEffect::ShadowLaw || Effect == EEffect::EchoNight || Effect == EEffect::ReleaseVoid;
	}

	FString EffectCode(EEffect Effect)
	{
		// El código en los datos es el nombre del valor de la enumeración (R-EFE-0: los códigos de la tabla).
		return StaticEnum<EEffect>()->GetNameStringByValue(static_cast<int64>(Effect));
	}

	FString EffectTargetCode(EEffectTarget Target)
	{
		const int32 Index = static_cast<int32>(Target);
		return Index < UE_ARRAY_COUNT(TargetCodes) ? FString(TargetCodes[Index]) : FString();
	}

	// Los códigos se leen distinguiendo mayúsculas: son identificadores de datos, como en la tabla del GDD.
	bool ParseEffectCode(const FString& Code, EEffect& OutEffect)
	{
		for (int32 Index = 0; Index < Soulbound::Effects::EffectCount; ++Index)
		{
			if (EffectCode(static_cast<EEffect>(Index)).Equals(Code, ESearchCase::CaseSensitive))
			{
				OutEffect = static_cast<EEffect>(Index);
				return true;
			}
		}
		return false;
	}

	bool ParseEffectTargetCode(const FString& Code, EEffectTarget& OutTarget)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TargetCodes); ++Index)
		{
			if (Code.Equals(TargetCodes[Index], ESearchCase::CaseSensitive))
			{
				OutTarget = static_cast<EEffectTarget>(Index);
				return true;
			}
		}
		return false;
	}

	bool ParseEffectVariantCode(const FString& Code, EEffectVariant& OutVariant)
	{
		for (int32 Index = 1; Index < UE_ARRAY_COUNT(VariantCodes); ++Index)
		{
			if (Code.Equals(VariantCodes[Index], ESearchCase::CaseSensitive))
			{
				OutVariant = static_cast<EEffectVariant>(Index);
				return true;
			}
		}
		return false;
	}
}
