#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpikeOnlinePlayerController.generated.h"

class UA27OnlineSubsystem;
class UMatchHostSubsystem;
struct FDebugEditOutcome;

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

	// Herramientas de depuración (S-011): llevan la partida al estado que se quiere probar. Solo con autoridad y con la
	// partida iniciada; piden la edición al anfitrión de partida y contestan en pantalla qué hicieron o por qué no. Los
	// argumentos son texto para poder decir qué argumento está mal. En la build de entrega (UE_BUILD_SHIPPING) los
	// comandos siguen declarados, porque Unreal Header Tool no admite UFUNCTION dentro de #if, pero no hacen nada.

	/** SbGiveRune <asiento> <tipo> [n]: da n runas (1 por defecto) de ese tipo, sacadas de la reserva de su Fuente. */
	UFUNCTION(Exec)
	void SbGiveRune(FString Seat, FString RuneType, FString Count);

	/** SbTakeRune <asiento> <tipo> [n]: quita n runas (1 por defecto) y las devuelve a la reserva de su Fuente. */
	UFUNCTION(Exec)
	void SbTakeRune(FString Seat, FString RuneType, FString Count);

	/** SbMove <asiento> <casilla>: coloca la ficha, sin resolver la casilla ni contar como movimiento. */
	UFUNCTION(Exec)
	void SbMove(FString Seat, FString Tile);

	/** SbBond <asiento> <dominio>: vincula al Eco con el Elegido del Dominio, como si hubiera reclamado su Nicho. */
	UFUNCTION(Exec)
	void SbBond(FString Seat, FString Domain);

	/** SbSeal <casilla>: sella con las reglas de Sello. */
	UFUNCTION(Exec)
	void SbSeal(FString Tile);

	/** SbUnseal <casilla>: libera un Sello. */
	UFUNCTION(Exec)
	void SbUnseal(FString Tile);

	/** SbEndRace: termina la carrera por el vínculo ahora. */
	UFUNCTION(Exec)
	void SbEndRace();

	/** SbRoll <n>: fija la próxima tirada (de 1 al máximo de dos dados). */
	UFUNCTION(Exec)
	void SbRoll(FString Value);

	/** SbTrial pass|fail: fija el resultado del próximo minijuego de habilidad del humano. */
	UFUNCTION(Exec)
	void SbTrial(FString Result);

	/** SbDuel win|lose: fija el resultado del próximo duelo en que participe el humano (S-012). */
	UFUNCTION(Exec)
	void SbDuel(FString Result);

	/** SbTurn <asiento>: el siguiente turno que empiece es de ese asiento y el orden sigue desde él. */
	UFUNCTION(Exec)
	void SbTurn(FString Seat);

	/** SbHelp: lista los comandos de depuración con su sintaxis. */
	UFUNCTION(Exec)
	void SbHelp();

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

#if !UE_BUILD_SHIPPING
	/** El anfitrión de partida si se puede editar: con autoridad y con la partida iniciada. Si no, lo dice en pantalla y devuelve null. */
	UMatchHostSubsystem* GetDebugHost() const;

	/** Convierte un argumento en entero. Si no lo es, lo dice en pantalla con Usage y devuelve false. */
	bool ParseDebugInt(const FString& Text, const TCHAR* Name, const TCHAR* Usage, int32& OutValue) const;

	/** Dice en pantalla (y en el log) qué hizo una edición o por qué se rechazó. */
	void ReportDebugEdit(const FDebugEditOutcome& Outcome) const;
#endif

	UFUNCTION()
	void HandleLogin(bool bWasSuccessful, const FString& ErrorMessage);

	UFUNCTION()
	void HandleCreated(bool bWasSuccessful);

	UFUNCTION()
	void HandleJoined(bool bWasSuccessful);

	UFUNCTION()
	void HandleLeft(bool bWasSuccessful);

};
