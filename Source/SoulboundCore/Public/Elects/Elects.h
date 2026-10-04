#pragma once

#include "CoreMinimal.h"
#include "Elects.generated.h"

/**
 * Poder (glosario "Poder", GDD 04): lo que da una runa a un Elegido según su mapa. Nombres del glosario.
 * Nombre del tipo propuesto en S-008. En S-008 solo Paso Libre tiene efecto en el motor; Celeridad se calcula como
 * poder activo y su efecto sobre los dados es de S-009.
 */
UENUM()
enum class EPower : uint8
{
	/** Duelo (glosario `PowerDuel`). */
	Duel,
	/** Celeridad (glosario `PowerHaste`). */
	Haste,
	/** Paso Libre (glosario `PowerFreePassage`): las casillas selladas son transitables como paso y como destino. */
	FreePassage,
	/** Expulsión (glosario `PowerExpel`). */
	Expel,
	/** Vuelo Planar (glosario `PowerPlanarFlight`). */
	PlanarFlight,
	/** Desvío (glosario `PowerDeflection`). */
	Deflection,
};

/** Afinidad (glosario "Afinidad"): ventaja propia de cada Elegido, siempre activa. Nombre del tipo propuesto en S-008. */
UENUM()
enum class EAffinity : uint8
{
	/** Shuten-dōji: ventaja en duelos (S-010). */
	DuelAdvantage,
	/** Fenrir: Paso Libre siempre (R-ELE-2, S-008). */
	FreePassage,
	/** Apep: repetir tirada una vez. */
	RerollOnce,
	/** Mictlantecuhtli: escape fácil del Vacío. */
	VoidEscape,
	/** Tifón: rechazar el Oráculo. */
	RefuseOracle,
	/** Koschei: una runa a salvo en el trilero (S-010). */
	SafeRune,
};

/** Un Elegido (glosario "Elegido"): su nombre, su afinidad y su mapa fijo de runa a poder (R-ELE-3). Nombre propuesto en S-008. */
USTRUCT()
struct SOULBOUNDCORE_API FElectDef
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	EAffinity Affinity = EAffinity::DuelAdvantage;

	/** Poder que da cada tipo de runa (índice 0 a 5, el Dominio de la runa). Seis entradas en una definición válida. */
	UPROPERTY()
	TArray<EPower> RuneToPower;
};

/** Los seis Elegidos, indexados por su Dominio (de Content/Data/Elects/elegidos.json). Nombre propuesto en S-008. */
USTRUCT()
struct SOULBOUNDCORE_API FElectsTable
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FElectDef> Elects;

	bool IsValidDomain(int32 DomainId) const { return Elects.IsValidIndex(DomainId); }
};
