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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	/**
	 * Clic izquierdo (S-005): si hay una casilla bajo el cursor, envía al anfitrión de partida la intención de
	 * destino del asiento humano. El motor decide si vale; aquí no se comprueba nada de las reglas.
	 */
	void OnPrimaryClick();

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
