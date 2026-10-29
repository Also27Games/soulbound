#pragma once

#include "CoreMinimal.h"
#include "HintsMode.h"
#include "PlayerProfile.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Warden/WardenLines.h"
#include "PlayerProfileSubsystem.generated.h"

/**
 * El dueño del perfil del jugador (S-021, A3; glosario "perfil del jugador", nombre propuesto): UGameInstanceSubsystem que carga el perfil de la
 * ranura `perfil` al arrancar y lo guarda en cada cambio. La vista lo consulta para saber si enseña una instrucción y si enseña el texto de
 * petición; la política es de Presentation/InstructionTracker.h (funciones puras), aquí solo está el estado y el disco. No es estado de partida.
 * Desde S-022, el ajuste de ayudas se lee y se cambia desde Blueprint (el menú de Opciones de UGT), por el ajuste o por su índice en el combo.
 */
UCLASS(BlueprintType)
class SOULBOUND_API UPlayerProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** El ajuste de ayudas actual. */
	UFUNCTION(BlueprintCallable, Category = "Perfil")
	EHintsMode GetHints() const;

	/** Cambia el ajuste de ayudas y guarda el perfil. */
	UFUNCTION(BlueprintCallable, Category = "Perfil")
	void SetHints(EHintsMode Mode);

	/**
	 * El ajuste de ayudas como posición en el combo de Opciones de UGT (S-022; nombre aprobado): automático 0, siempre 1, nunca 2
	 * (HintsModeToIndex).
	 */
	UFUNCTION(BlueprintCallable, Category = "Perfil")
	int32 GetHintsIndex() const;

	/**
	 * Cambia el ajuste de ayudas a la posición Index del combo de Opciones de UGT y guarda el perfil (S-022; nombre aprobado). Fuera de rango,
	 * automático (HintsModeFromIndex).
	 */
	UFUNCTION(BlueprintCallable, Category = "Perfil")
	void SetHintsIndex(int32 Index);

	/**
	 * Si se enseña la instrucción Moment según el ajuste y el perfil (TeachAndRecord); si se enseña, queda en el perfil y se guarda. Es la única
	 * vía por la que la lista crece.
	 */
	bool TeachInstruction(EInstructionMoment Moment);

	/** Si se enseña el texto de petición ("← tu turno: ...") con el ajuste y el perfil de ahora (ShowsRequestHints). */
	bool ShowsRequestHints() const;

	/** Vacía la lista de instrucciones vistas y guarda el perfil (SbForgetInstructions). */
	void ForgetInstructions();

	/** Las claves vistas, en solo lectura (para la consola). */
	const TArray<FString>& GetInstructionsSeen() const;

private:
	void Save() const;

	UPROPERTY()
	TObjectPtr<UPlayerProfile> Profile;
};
