// Tests del perfil del jugador en disco (S-021, A3; glosario "perfil del jugador"): CA-INS-2, "el perfil guardado y vuelto a cargar conserva la
// lista y el ajuste". El perfil no es estado de partida: se prueba solo, en memoria y con una ranura de prueba en disco que se borra al acabar.
// Filtro: "Soulbound.Game.Perfil".

#include "../../Profile/PlayerProfile.h"
#include "../../Profile/PlayerProfileSubsystem.h"
#include "Engine/GameInstance.h"
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
	// CA-INS-2 y CA-AYU-2 (R-AYU-1): el perfil nuevo está vacío y en siempre; HasSeen y ForgetInstructions leen y vacían la lista sin tocar el ajuste.
	TStrongObjectPtr<UPlayerProfile> Profile(NewObject<UPlayerProfile>(GetTransientPackage()));
	TestEqual(TEXT("Ajuste inicial siempre"), static_cast<int32>(Profile->Hints), static_cast<int32>(EHintsMode::Always));
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
		TestEqual(TEXT("Y siempre"), static_cast<int32>(Missing->Hints), static_cast<int32>(EHintsMode::Always));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerProfileUnreadableTest, "Soulbound.Game.Perfil.DatosIlegibles", ProfileTestFlags)

bool FPlayerProfileUnreadableTest::RunTest(const FString& Parameters)
{
	// CA-AYU-2 (R-AYU-1): unos datos que no se pueden leer dan un perfil nuevo: en siempre y con la lista vacía, nunca nulo.
	// Un archivo vacío (lo que deja una escritura cortada). Bytes arbitrarios no sirven: el motor lee un FName inválido y aborta el comandlet.
	TStrongObjectPtr<UPlayerProfile> Profile(UPlayerProfile::FromBytes(TArray<uint8>()));
	TestNotNull(TEXT("Con datos ilegibles, un perfil nuevo"), Profile.Get());
	if (Profile.IsValid())
	{
		TestEqual(TEXT("En siempre"), static_cast<int32>(Profile->Hints), static_cast<int32>(EHintsMode::Always));
		TestEqual(TEXT("Y sin instrucciones"), Profile->InstructionsSeen.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerProfileSubsystemDefaultTest, "Soulbound.Game.Perfil.SubsistemaSinPerfil", ProfileTestFlags)

bool FPlayerProfileSubsystemDefaultTest::RunTest(const FString& Parameters)
{
	// CA-AYU-2 (R-AYU-1): sin perfil cargado (el subsistema sin Initialize), GetHints da el mismo valor inicial que el perfil nuevo: siempre.
	// UGameInstanceSubsystem es Within = GameInstance: cuelga de un UGameInstance transitorio, sin mundo.
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GetTransientPackage()));
	TStrongObjectPtr<UPlayerProfileSubsystem> Subsystem(NewObject<UPlayerProfileSubsystem>(GameInstance.Get()));
	TestEqual(TEXT("Sin perfil, siempre"), static_cast<int32>(Subsystem->GetHints()), static_cast<int32>(EHintsMode::Always));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerProfileKeepsSettingTest, "Soulbound.Game.Perfil.ConservaElAjuste", ProfileTestFlags)

bool FPlayerProfileKeepsSettingTest::RunTest(const FString& Parameters)
{
	// CA-AYU-3 (R-AYU-1): un perfil en automático, guardado y vuelto a cargar, sigue en automático (y lo mismo con nunca), en memoria y por disco.
	// Es una guarda: automático ya volvía como automático antes de S-038; tras el cambio de valor inicial pasa a ser lo que se escribe en el archivo.
	for (const EHintsMode Mode : {EHintsMode::Automatic, EHintsMode::Never})
	{
		const FString Name = Mode == EHintsMode::Automatic ? TEXT("automático") : TEXT("nunca");
		TStrongObjectPtr<UPlayerProfile> Profile(NewObject<UPlayerProfile>(GetTransientPackage()));
		Profile->Hints = Mode;

		TStrongObjectPtr<UPlayerProfile> FromMemory(UPlayerProfile::FromBytes(Profile->ToBytes()));
		TestEqual(FString::Printf(TEXT("En memoria, %s"), *Name), static_cast<int32>(FromMemory->Hints), static_cast<int32>(Mode));

		UGameplayStatics::DeleteGameInSlot(ProfileTestSlot, 0);
		TestTrue(TEXT("Guarda en la ranura de prueba"), Profile->SaveToSlot(ProfileTestSlot));
		TStrongObjectPtr<UPlayerProfile> FromDisk(UPlayerProfile::LoadFromSlot(ProfileTestSlot));
		TestEqual(FString::Printf(TEXT("En disco, %s"), *Name), static_cast<int32>(FromDisk->Hints), static_cast<int32>(Mode));
		UGameplayStatics::DeleteGameInSlot(ProfileTestSlot, 0);
	}
	return true;
}

#endif
