#pragma once

#include "CoreMinimal.h"
#include "EffectCatalog.generated.h"

namespace Soulbound::Effects
{
	/** Efectos del catálogo (R-EFE-0): la lista es cerrada por regla, no un valor de ajuste. */
	inline constexpr int32 EffectCount = 18;

	/**
	 * Cantidad máxima de una aplicación de Turno perdido o Turno extra (R-EFE-5, R-EFE-15: uno o dos). Es la regla que acota
	 * los datos; el tope de turnos acumulados es otro (FMatchParams::MaxLostTurns, MaxExtraTurns).
	 */
	inline constexpr int32 MaxAmount = 2;
}

/**
 * Efecto (glosario `Effect`, R-EFE-0, GDD 05): consecuencia atómica que el Warden impone o regala. Lista cerrada de
 * dieciocho, en el orden de la tabla del GDD; el nombre de cada valor es su código en los datos (eventos.json).
 * Nombre propuesto en S-014.
 */
UENUM()
enum class EEffect : uint8
{
	/** Alto (`Halt`, R-TAB-28, R-EFE-1): leve. Su único efecto es el corte del turno: solo vale en un golpe que corta (S-016). */
	Halt,
	/** Revelar Brazal (`RevealBracer`, R-EFE-2): leve. Los tipos de runa del objetivo, visibles unas rondas. */
	RevealBracer,
	/** Consulta forzada al Oráculo (`ForcedOracle`): leve. Abre una consulta del Oráculo (S-017, R-EFE-3). */
	ForcedOracle,
	/** Vuelta al hogar (`SendHome`, R-EFE-4): medio. */
	SendHome,
	/** Turno perdido (`LoseTurn`, R-EFE-5): medio. */
	LoseTurn,
	/** Sello (`PlaceSeal`, R-EFE-6): medio. El Warden sella una casilla válida. */
	PlaceSeal,
	/** Sello contra rival (`SealAgainstRival`, R-EFE-7): medio. */
	SealAgainstRival,
	/** Marcado (`Mark`, R-EFE-8): medio. */
	Mark,
	/** Ley de Sombras (`ShadowLaw`, R-EFE-9): medio. Todos los Ecos a su hogar; termina la Noche. */
	ShadowLaw,
	/** Prueba del Warden (`WardenTrial`, R-EFE-10): medio. S-015. */
	WardenTrial,
	/** Perder una runa (`LoseRune`, R-EFE-11): grave. */
	LoseRune,
	/** Robo decretado (`DecreedTheft`, R-EFE-12): grave. S-015. */
	DecreedTheft,
	/** Duelo forzado (`ForcedDuel`, R-EFE-13): grave. S-015. */
	ForcedDuel,
	/** El Vacío (`Banish`, R-EFE-14): grave. */
	Banish,
	/** Turno extra (`ExtraTurn`, R-EFE-15): regalo. */
	ExtraTurn,
	/** Runa regalada (`GiftRune`, R-EFE-16): regalo. */
	GiftRune,
	/** Liberación (`ReleaseVoid`, R-EFE-17): regalo. */
	ReleaseVoid,
	/** Noche de los Ecos (`EchoNight`, R-EFE-18): regalo. */
	EchoNight,
};

/**
 * Dirección (glosario `EffectTarget`, R-DIR-1): a quién le pasa un efecto. Las ocho de la regla y None, que no es una
 * dirección: el efecto no va a ningún asiento (el Sello, que elige el Warden; decisión del autor del 12/10/2026: en los
 * datos, sin campo `target`). El nombre de cada valor es su código en los datos. Nombre propuesto en S-014.
 */
UENUM()
enum class EEffectTarget : uint8
{
	/** Activo: quien dirige el evento; entre turnos, el último que jugó (R-DIR-1). */
	ActivePlayer,
	/** Rival señalado: el activo elige entre los válidos, con el reloj corto. */
	ChosenRival,
	/** Dos rivales señalados (solo el Duelo forzado, S-015): quien dirige señala dos, uno tras otro; el primero es el retador. */
	TwoChosenRivals,
	/** Líder (glosario `Leader`): más tipos distintos; con empate, más runas; si persiste, todos los empatados. */
	Leader,
	/** Siguiente: el que sigue a quien dirige en el orden de turno. */
	NextPlayer,
	/** Todos: los cuatro asientos (en Liberación, los que están en el Vacío). */
	Everyone,
	/** Al azar: uno de los válidos, con el sorteo del Warden. */
	RandomPlayer,
	/** Ecos: los Eco Perdido (Ley de Sombras, Noche de los Ecos). */
	Echoes,
	/** Sin dirección: el efecto no va a ningún asiento (el Sello). */
	None,
};

/** Gravedad de un efecto (R-EFE-0): leve, media, grave o regalo. Nombre propuesto en S-014. */
UENUM()
enum class EEffectSeverity : uint8
{
	Light,
	Medium,
	Severe,
	Gift,
};

/**
 * Variante de una aplicación (S-014, nombre propuesto): qué forma toma el efecto, para los que tienen más de una. En los
 * datos, el campo `variant`. Perder una runa: "random" o "chosen" (R-EFE-11). Duelo forzado: "one" o "all" (lo que está en
 * juego, decisión del autor del 12/10/2026; S-015). None en el resto.
 */
UENUM()
enum class EEffectVariant : uint8
{
	None,
	/** Perder una runa al azar. */
	Random,
	/** Perder una runa a elección del objetivo, con el reloj corto; si no elige, al azar. */
	Chosen,
	/** Duelo forzado por una runa. */
	OneRune,
	/** Duelo forzado por todas las runas. */
	AllRunes,
};

/**
 * Entrada del catálogo de efectos (R-EFE-0, S-014, nombre propuesto): gravedad, direcciones permitidas, si admite cantidad
 * y qué variantes. El catálogo es una tabla fija del motor (EffectCatalog), no un dato.
 */
struct SOULBOUNDCORE_API FEffectDef
{
	EEffect Effect = EEffect::Halt;
	EEffectSeverity Severity = EEffectSeverity::Light;
	/** Direcciones permitidas (tabla del GDD 05); el Sello, solo None. */
	TArray<EEffectTarget> AllowedTargets;
	/** Si admite cantidad de 1 a Soulbound::Effects::MaxAmount (Turno perdido, Turno extra); el resto, siempre 1. */
	bool bTakesAmount = false;
	/** Variantes admitidas; la primera es la de por defecto. Vacío: solo None. */
	TArray<EEffectVariant> Variants;
	/** Si el motor lo aplica: los 18 desde S-017, con la Consulta forzada (R-EFE-3). Un efecto de otra story devolvería NotImplemented. */
	bool bImplemented = false;
};

/** Catálogo de efectos (R-EFE-0): la tabla fija y los códigos de los datos. */
namespace Soulbound::Rules
{
	/** Las dieciocho entradas del catálogo, en el orden de EEffect (la entrada i es la del efecto i). */
	SOULBOUNDCORE_API const TArray<FEffectDef>& EffectCatalog();

	/** Entrada del catálogo del efecto Effect. */
	SOULBOUNDCORE_API const FEffectDef& EffectDefOf(EEffect Effect);

	/** Si el efecto es un regalo (gravedad Gift); los demás son dañinos (R-DIR-4, R-COM-2). */
	SOULBOUNDCORE_API bool IsGift(EEffect Effect);

	/** Si el efecto se aplica en esta story (FEffectDef::bImplemented). */
	SOULBOUNDCORE_API bool IsEffectImplemented(EEffect Effect);

	/**
	 * Si el efecto no va a asientos sino a la partida entera: Sello, Ley de Sombras, Noche de los Ecos y Liberación. Se aplica
	 * una sola vez, con su comprobación global, y su evento EffectApplied no lleva asiento.
	 */
	SOULBOUNDCORE_API bool IsGlobalEffect(EEffect Effect);

	/** Código del efecto en los datos ("SendHome"...). */
	SOULBOUNDCORE_API FString EffectCode(EEffect Effect);

	/** Código de la dirección en los datos ("ChosenRival"...); None da "". */
	SOULBOUNDCORE_API FString EffectTargetCode(EEffectTarget Target);

	/** Lee un código de efecto. Devuelve false si no es ninguno de los dieciocho. */
	SOULBOUNDCORE_API bool ParseEffectCode(const FString& Code, EEffect& OutEffect);

	/** Lee un código de dirección (una de las ocho). Devuelve false si no es ninguna. */
	SOULBOUNDCORE_API bool ParseEffectTargetCode(const FString& Code, EEffectTarget& OutTarget);

	/** Lee un código de variante ("random", "chosen", "one", "all"). Devuelve false si no es ninguna. */
	SOULBOUNDCORE_API bool ParseEffectVariantCode(const FString& Code, EEffectVariant& OutVariant);
}
