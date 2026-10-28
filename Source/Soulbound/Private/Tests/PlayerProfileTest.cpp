// Tests del perfil del jugador en disco (S-021, A3; glosario "perfil del jugador"): CA-INS-2, "el perfil guardado y vuelto a cargar conserva la
// lista y el ajuste". El perfil no es estado de partida: se prueba solo, en memoria y con una ranura de prueba en disco que se borra al acabar.
// Filtro: "Soulbound.Game.Perfil".

#include "../../Profile/PlayerProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags ProfileTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Una ranura que no es la del juego, para no tocar el perfil de quien prueba. */
	const TCHAR* const ProfileTestSlot = TEXT("perfil-test-s021");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerProfileDefaultsTest, "Soulbound.Game.Perfil.ValoresIniciales", ProfileTestFlags)

bool FPlayerProfileDefaultsTest::RunTest(const FString& Parameters)
{
	// CA-INS-2: el perfil nuevo está vacío y en automático; HasSeen y ForgetInstructions leen y vacían la lista sin tocar el ajuste.
	TStrongObjectPtr<UPlayerProfile> Profile(NewObject<UPlayerProfile>(GetTransientPackage()));
	TestEqual(TEXT("Ajuste inicial automático"), static_cast<int32>(Profile->Hints), static_cast<int32>(EHintsMode::Automatic));
	TestEqual(TEXT("Sin instrucciones vistas"), Profile->InstructionsSeen.Num(), 0);
	TestFalse(TEXT("No ha visto tirar"), Profile->HasSeen(EInstructionMoment::Roll));

	Profile->InstructionsSeen.Add(Soulbound::Rules::InstructionKey(EInstructionMoment::Roll));
	Profile->Hints = EHintsMode::Always;
	TestTrue(TEXT("Ya ha visto tirar"), Profile->HasSeen(EInstructionMoment::Roll));
	TestFalse(TEXT("Y no el duelo"), Profile->HasSeen(EInstructionMoment::Duel));

	Profile->ForgetInstructions();
	TestEqual(TEXT("Olvidar vacía la lista"), Profile->InstructionsSeen.Num(), 0);
	TestFalse(TEXT("Y ya no ha visto tirar"), Profile->HasSeen(EInstructionMoment::Roll));
	TestEqual(TEXT("El ajuste se conserva"), static_cast<int32>(Profile->Hints), static_cast<int32>(EHintsMode::Always));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerProfileRoundTripTest, "Soulbound.Game.Perfil.GuardarYCargar", ProfileTestFlags)

bool FPlayerProfileRoundTripTest::RunTest(const FString& Parameters)
{
	// CA-INS-2: el perfil guardado y vuelto a cargar conserva la lista y el ajuste; en memoria y por disco (ranura de prueba).
	TStrongObjectPtr<UPlayerProfile> Profile(NewObject<UPlayerProfile>(GetTransientPackage()));
	Profile->InstructionsSeen = {Soulbound::Rules::InstructionKey(EInstructionMoment::Roll), Soulbound::Rules::InstructionKey(EInstructionMoment::Orb)};
	Profile->Hints = EHintsMode::Never;

	const TArray<uint8> Bytes = Profile->ToBytes();
	TestTrue(TEXT("Serializa algo"), Bytes.Num() > 0);
	TStrongObjectPtr<UPlayerProfile> FromMemory(UPlayerProfile::FromBytes(Bytes));
	TestEqual(TEXT("En memoria: la lista"), FromMemory->InstructionsSeen, Profile->InstructionsSeen);
	TestEqual(TEXT("En memoria: el ajuste"), static_cast<int32>(FromMemory->Hints), static_cast<int32>(EHintsMode::Never));

	UGameplayStatics::DeleteGameInSlot(ProfileTestSlot, 0);
	TestTrue(TEXT("Guarda en la ranura de prueba"), Profile->SaveToSlot(ProfileTestSlot));
	TStrongObjectPtr<UPlayerProfile> FromDisk(UPlayerProfile::LoadFromSlot(ProfileTestSlot));
	TestEqual(TEXT("En disco: la lista"), FromDisk->InstructionsSeen, Profile->InstructionsSeen);
	TestEqual(TEXT("En disco: el ajuste"), static_cast<int32>(FromDisk->Hints), static_cast<int32>(EHintsMode::Never));
	TestTrue(TEXT("En disco: ha visto tirar"), FromDisk->HasSeen(EInstructionMoment::Roll));
	TestTrue(TEXT("En disco: ha visto el orbe"), FromDisk->HasSeen(EInstructionMoment::Orb));
	UGameplayStatics::DeleteGameInSlot(ProfileTestSlot, 0);

	// Una ranura que no existe da un perfil vacío y automático, nunca nulo.
	TStrongObjectPtr<UPlayerProfile> Missing(UPlayerProfile::LoadFromSlot(ProfileTestSlot));
	TestNotNull(TEXT("Sin ranura, un perfil nuevo"), Missing.Get());
	if (Missing.IsValid())
	{
		TestEqual(TEXT("Vacío"), Missing->InstructionsSeen.Num(), 0);
		TestEqual(TEXT("Y automático"), static_cast<int32>(Missing->Hints), static_cast<int32>(EHintsMode::Automatic));
	}
	return true;
}

#endif
