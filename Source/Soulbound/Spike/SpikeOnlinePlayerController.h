#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpikeOnlinePlayerController.generated.h"

class UA27OnlineSubsystem;

/**
 * Spike, paso 0.5: comandos de consola para probar sesiones por código sin interfaz.
 * En la consola del juego (tecla ` ): SbStatus, SbLogin, SbCreate, SbJoin <codigo>, SbLeave.
 * Código desechable: se borra cuando el Umbral y las barcas (GDD 07) existan.
 */
UCLASS()
class SOULBOUND_API ASpikeOnlinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Muestra estado de login, apodo y código de la sesión actual. */
	UFUNCTION(Exec)
	void SbStatus();

	/** Lanza el login (el subsistema ya lo intenta solo al arrancar). */
	UFUNCTION(Exec)
	void SbLogin();

	/** Crea una sesión de 4 plazas en el mapa actual y muestra el código. */
	UFUNCTION(Exec)
	void SbCreate();

	/** Se une a la sesión con ese código. */
	UFUNCTION(Exec)
	void SbJoin(FString Code);

	/** Abandona la sesión actual. */
	UFUNCTION(Exec)
	void SbLeave();

	/** Modo paso a paso (S-007): SbStepMode 1 lo activa, SbStepMode 0 lo desactiva. Cambia en caliente. */
	UFUNCTION(Exec)
	void SbStepMode(int32 Enabled);

	/** Suelta un turno con el paso a paso activo y la partida detenida (lo mismo que la barra espaciadora). */
	UFUNCTION(Exec)
	void SbStep();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	UFUNCTION(BlueprintCallable)
	virtual void ShowMouseCursorAndInputGame();

private:
	/**
	 * Clic izquierdo (S-005): si hay una casilla bajo el cursor, envía al anfitrión de partida la intención de
	 * destino del asiento humano; si el humano tiene pendiente un salto planar, la casilla es su respuesta (S-009). El
	 * motor decide si vale; aquí no se comprueba nada de las reglas.
	 */
	void OnPrimaryClick();

	/**
	 * Tecla numérica (S-009): contesta la elección pendiente del humano. 1 a 9 eligen la opción con ese número, en el
	 * orden en que la vista las enseña; 0 es "no". El motor decide si vale.
	 */
	void OnChoiceKey(int32 Number);

	/** Tecla R (S-009): el humano pide repetir la tirada (afinidad de Apep). El motor decide si puede. */
	void OnRerollKey();

	/** Barra espaciadora o botón A del mando (S-007): pide al anfitrión el siguiente turno. */
	void OnStepKey();

	UA27OnlineSubsystem* GetOnline() const;
	void Say(const FString& Text) const;

	UFUNCTION()
	void HandleLogin(bool bWasSuccessful, const FString& ErrorMessage);

	UFUNCTION()
	void HandleCreated(bool bWasSuccessful);

	UFUNCTION()
	void HandleJoined(bool bWasSuccessful);

	UFUNCTION()
	void HandleLeft(bool bWasSuccessful);

};
