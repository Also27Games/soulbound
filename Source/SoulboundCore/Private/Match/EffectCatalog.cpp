#include "Match/EffectCatalog.h"

namespace Soulbound::Rules
{
	const TArray<FEffectDef>& EffectCatalog()
	{
		// TODO(R-EFE-0): sin implementar (S-014, tests en rojo).
		static const TArray<FEffectDef> Catalog;
		return Catalog;
	}

	const FEffectDef& EffectDefOf(EEffect Effect)
	{
		static const FEffectDef Empty;
		return Empty;
	}

	bool IsGift(EEffect Effect) { return false; }

	bool IsEffectImplemented(EEffect Effect) { return false; }

	bool IsGlobalEffect(EEffect Effect) { return false; }

	FString EffectCode(EEffect Effect) { return FString(); }

	FString EffectTargetCode(EEffectTarget Target) { return FString(); }

	bool ParseEffectCode(const FString& Code, EEffect& OutEffect) { return false; }

	bool ParseEffectTargetCode(const FString& Code, EEffectTarget& OutTarget) { return false; }

	bool ParseEffectVariantCode(const FString& Code, EEffectVariant& OutVariant) { return false; }
}
