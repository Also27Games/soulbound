#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HintsMode.h"
#include "Warden/WardenLines.h"
#include "PlayerProfile.generated.h"

/**
 * El perfil del jugador en disco (S-021, A3; glosario "perfil del jugador", nombre propuesto): lo que esta máquina recuerda de quien juega. No es
 * estado de partida: no entra en FMatchState, no se replica y no lo ve SoulboundCore. Lleva las instrucciones que el Warden ya le ha enseñado y el
 * ajuste de ayudas. Las instrucciones se guardan como su clave de texto (`instruccion:tirar`), no como el valor del enum, para que reordenar
 * el enum no corrompa perfiles viejos.
 */
UCLASS()
class SOULBOUND_API UPlayerProfile : public USaveGame
{
	GENERATED_BODY()

public:
	/** La ranura del perfil en disco. */
	static const FString DefaultSlot;

	/** Las claves de las instrucciones que ya ha visto (`instruccion:<momento>`, InstructionKey). */
	UPROPERTY()
	TArray<FString> InstructionsSeen;

	/** El ajuste de ayudas. Valor inicial Automatic. */
	UPROPERTY()
	EHintsMode Hints = EHintsMode::Automatic;

	/** Si ya ha visto la instrucción Moment. */
	bool HasSeen(EInstructionMoment Moment) const;

	/** Vacía la lista de instrucciones vistas (SbForgetInstructions); el ajuste se conserva. */
	void ForgetInstructions();

	/** Guarda el perfil en la ranura Slot. Devuelve si se guardó. */
	bool SaveToSlot(const FString& Slot) const;

	/** El perfil de la ranura Slot, o uno vacío (Automatic, sin instrucciones) si no existe o no se puede leer. Nunca nulo. */
	static UPlayerProfile* LoadFromSlot(const FString& Slot);

	/** El perfil serializado en memoria, y su vuelta: lo mismo que pasa por el disco, sin tocarlo (para los tests). */
	TArray<uint8> ToBytes() const;
	static UPlayerProfile* FromBytes(const TArray<uint8>& Bytes);
};
