// Tests de los datos del Warden (S-016, CA-SAC-1, R-SAC-2): Content/Data/Warden/warden.json carga con sus valores iniciales
// y rechaza con motivo lo que no tiene sentido; Content/Data/Events/eventos.json trae los 35 eventos de los sacos del GDD 06,
// con nombre, acto, canal, entrega, pesos, usos, ventanas y corte, y los once de prueba fuera de los sacos. Los rechazos se
// prueban cambiando una sola cosa del archivo real. Se lanzan con Scripts/test.ps1.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 AsInt(EWardenParamsError Error) { return static_cast<int32>(Error); }

	/**
	 * El texto de warden.json con From sustituido por To una sola vez; si From no está, lo apunta como error del test y
	 * devuelve vacío. Así cada rechazo cambia una sola cosa del archivo real.
	 */
	FString WardenJsonWith(FAutomationTestBase& Test, const FString& From, const FString& To)
	{
		FString Json;
		if (!Test.TestTrue(TEXT("warden.json se lee"), FFileHelper::LoadFileToString(Json, *Soulbound::Tests::WardenParamsPath())))
		{
			return FString();
		}
		const int32 At = Json.Find(From, ESearchCase::CaseSensitive);
		if (!Test.TestTrue(FString::Printf(TEXT("warden.json contiene \"%s\""), *From), At != INDEX_NONE))
		{
			return FString();
		}
		return Json.Left(At) + To + Json.Mid(At + From.Len());
	}

	/** Comprueba que warden.json con From cambiado por To se rechaza con Expected y con motivo. */
	void ExpectRejected(FAutomationTestBase& Test, const FString& From, const FString& To, EWardenParamsError Expected)
	{
		const FString Json = WardenJsonWith(Test, From, To);
		if (Json.IsEmpty())
		{
			return;
		}
		const FWardenParamsLoadResult Result = Soulbound::Rules::LoadWardenParamsFromJsonString(Json);
		Test.TestEqual(FString::Printf(TEXT("\"%s\" en lugar de \"%s\": rechazo por la regla que toca"), *To, *From), AsInt(Result.Error), AsInt(Expected));
		Test.TestFalse(TEXT("Con motivo para el desarrollador"), Result.Message.IsEmpty());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenParamsFileTest, "Soulbound.Core.Warden.Data.ArchivoDelWardenCarga",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenParamsFileTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1 (R-WAR-2, R-WAR-3, R-WAR-4d, R-WAR-5, R-INS-2): warden.json carga y valida con los valores iniciales de la story.
	FWardenParams Params;
	if (!Soulbound::Tests::LoadTestWardenParams(*this, Params))
	{
		return false;
	}
	if (!TestEqual(TEXT("Tres actos"), Params.Acts.Num(), Soulbound::Warden::ActCount))
	{
		return false;
	}
	const double Starts[] = {0.0, 300.0, 1620.0};
	const double Probabilities[] = {0.20, 0.35, 0.50};
	const double Weights[][3] = {{40, 20, 40}, {45, 25, 30}, {50, 25, 25}};
	const double Targets[] = {0.15, 0.25, 0.35};
	for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
	{
		const FWardenActParams& Data = Params.Acts[Act];
		TestEqual(FString::Printf(TEXT("Acto %d: inicio"), Act + 1), Data.StartSeconds, Starts[Act]);
		TestEqual(FString::Printf(TEXT("Acto %d: probabilidad de intervenir"), Act + 1), Data.InterveneProbability, Probabilities[Act]);
		TestEqual(FString::Printf(TEXT("Acto %d: objetivo de cortes"), Act + 1), Data.CutTarget, Targets[Act]);
		if (TestEqual(FString::Printf(TEXT("Acto %d: tres pesos de canal"), Act + 1), Data.ChannelWeights.Num(), Soulbound::Warden::ChannelCount))
		{
			for (int32 Channel = 0; Channel < Soulbound::Warden::ChannelCount; ++Channel)
			{
				TestEqual(FString::Printf(TEXT("Acto %d: peso del canal %d"), Act + 1, Channel), Data.ChannelWeights[Channel], Weights[Act][Channel]);
			}
		}
	}
	TestEqual(TEXT("Fin de la partida"), Params.MatchEndSeconds, 2400.0);
	TestEqual(TEXT("Intervalo"), Params.EvaluationIntervalSeconds, 15.0);
	TestEqual(TEXT("Variación"), Params.EvaluationVariationSeconds, 5.0);
	TestEqual(TEXT("Garantía"), Params.GuaranteeSeconds, 120.0);
	TestEqual(TEXT("Margen"), Params.MarginSeconds, 12.0);

	// R-INS-2: niveles 0 y 1, 70/25/5; 2 y 3, 30/50/20; 4, la Furia.
	if (TestEqual(TEXT("Tres filas en la tabla de insolencia"), Params.InsolenceTable.Num(), 3))
	{
		TestTrue(TEXT("Fila 1: niveles 0 y 1, 70/25/5"), Params.InsolenceTable[0].Levels == TArray<int32>{0, 1} && Params.InsolenceTable[0].SeverityPercents == TArray<int32>{70, 25, 5});
		TestTrue(TEXT("Fila 2: niveles 2 y 3, 30/50/20"), Params.InsolenceTable[1].Levels == TArray<int32>{2, 3} && Params.InsolenceTable[1].SeverityPercents == TArray<int32>{30, 50, 20});
		TestTrue(TEXT("Fila 3: nivel 4, la Furia"), Params.InsolenceTable[2].Levels == TArray<int32>{4} && Params.InsolenceTable[2].bFury);
	}
	TestTrue(TEXT("Y valida por sí sola"), Soulbound::Rules::ValidateWardenParams(Params).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenParamsActsTest, "Soulbound.Core.Warden.Data.RechazaActosNoCrecientes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenParamsActsTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1: actos no crecientes (el III empieza antes que el II), o el primero que no empieza en 0.
	ExpectRejected(*this, TEXT("\"startSeconds\": 1620"), TEXT("\"startSeconds\": 200"), EWardenParamsError::ActsNotIncreasing);
	ExpectRejected(*this, TEXT("\"startSeconds\": 0"), TEXT("\"startSeconds\": 10"), EWardenParamsError::ActsNotIncreasing);
	ExpectRejected(*this, TEXT("\"matchEndSeconds\": 2400"), TEXT("\"matchEndSeconds\": 1000"), EWardenParamsError::ActsNotIncreasing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenParamsProbabilityTest, "Soulbound.Core.Warden.Data.RechazaProbabilidadFueraDeRango",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenParamsProbabilityTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1: probabilidades fuera de [0, 1], la de intervenir y el objetivo de cortes.
	ExpectRejected(*this, TEXT("\"interveneProbability\": 0.35"), TEXT("\"interveneProbability\": 1.35"), EWardenParamsError::ProbabilityOutOfRange);
	ExpectRejected(*this, TEXT("\"interveneProbability\": 0.20"), TEXT("\"interveneProbability\": -0.2"), EWardenParamsError::ProbabilityOutOfRange);
	ExpectRejected(*this, TEXT("\"cutTarget\": 0.35"), TEXT("\"cutTarget\": 1.5"), EWardenParamsError::ProbabilityOutOfRange);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenParamsWeightTest, "Soulbound.Core.Warden.Data.RechazaPesosNegativos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenParamsWeightTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1: un peso de canal negativo; y todos a cero, que no deja elegir canal.
	ExpectRejected(*this, TEXT("\"scene\": 25, \"remark\": 30"), TEXT("\"scene\": -25, \"remark\": 30"), EWardenParamsError::NegativeWeight);
	ExpectRejected(*this, TEXT("{\"strike\": 40, \"scene\": 20, \"remark\": 40}"), TEXT("{\"strike\": 0, \"scene\": 0, \"remark\": 0}"), EWardenParamsError::NegativeWeight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenParamsInsolenceTest, "Soulbound.Core.Warden.Data.RechazaTablaDeInsolencia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenParamsInsolenceTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1: una fila que no suma 100; y niveles que faltan o se repiten.
	ExpectRejected(*this, TEXT("\"severe\": 5}"), TEXT("\"severe\": 4}"), EWardenParamsError::InsolenceTableInvalid);
	ExpectRejected(*this, TEXT("\"levels\": [2, 3]"), TEXT("\"levels\": [2]"), EWardenParamsError::InsolenceTableInvalid);
	ExpectRejected(*this, TEXT("\"levels\": [4]"), TEXT("\"levels\": [3]"), EWardenParamsError::InsolenceTableInvalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenParamsDurationTest, "Soulbound.Core.Warden.Data.RechazaMargenOGarantiaNoPositivos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenParamsDurationTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1: margen o garantía no positivos; y el intervalo con su variación, que tampoco pueden dejar de avanzar.
	ExpectRejected(*this, TEXT("\"marginSeconds\": 12"), TEXT("\"marginSeconds\": 0"), EWardenParamsError::NonPositiveDuration);
	ExpectRejected(*this, TEXT("\"guaranteeSeconds\": 120"), TEXT("\"guaranteeSeconds\": -1"), EWardenParamsError::NonPositiveDuration);
	ExpectRejected(*this, TEXT("\"evaluationIntervalSeconds\": 15"), TEXT("\"evaluationIntervalSeconds\": 0"), EWardenParamsError::NonPositiveDuration);
	ExpectRejected(*this, TEXT("\"evaluationVariationSeconds\": 5"), TEXT("\"evaluationVariationSeconds\": 15"), EWardenParamsError::NonPositiveDuration);
	ExpectRejected(*this, TEXT("\"marginSeconds\": 12,"), TEXT(""), EWardenParamsError::MalformedData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenBagsFileTest, "Soulbound.Core.Warden.Data.TreintaYCincoEventosEnLosSacos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenBagsFileTest::RunTest(const FString& Parameters)
{
	// CA-SAC-1 (R-SAC-2): los 35 eventos del GDD 06 cargan y validan contra el catálogo, con su saco; los once de prueba de
	// S-014 y S-015 quedan fuera de los sacos.
	FWardenEventTable Table;
	if (!Soulbound::Tests::LoadTestWardenEvents(*this, Table))
	{
		return false;
	}
	int32 PerAct[Soulbound::Warden::ActCount] = {};
	int32 OutOfBags = 0;
	for (const FWardenEventDef& Event : Table.Events)
	{
		if (!Event.bInBag)
		{
			++OutOfBags;
			continue;
		}
		++PerAct[static_cast<int32>(Event.Act)];
		TestFalse(FString::Printf(TEXT("%s: tiene nombre"), *Event.Id), Event.Name.IsEmpty());
		TestNotEqual(FString::Printf(TEXT("%s: canal golpe o escena"), *Event.Id), static_cast<int32>(Event.Channel), static_cast<int32>(EWardenChannel::Remark));
		TestEqual(FString::Printf(TEXT("%s: los golpes cortan y las escenas no"), *Event.Id), Event.bCutsTurn, Event.Channel == EWardenChannel::Strike);
		TestEqual(FString::Printf(TEXT("%s: tres pesos"), *Event.Id), Event.Weights.Num(), Soulbound::Warden::ActCount);
	}
	TestEqual(TEXT("Seis en el saco del Acto I"), PerAct[0], 6);
	TestEqual(TEXT("Veintiuno en el del Acto II"), PerAct[1], 21);
	TestEqual(TEXT("Ocho en el del Acto III"), PerAct[2], 8);
	TestEqual(TEXT("Los once de prueba, fuera de los sacos"), OutOfBags, 11);

	auto Find = [&Table](const TCHAR* Id) -> const FWardenEventDef*
	{
		const int32 Index = Table.IndexOf(Id);
		return Table.Events.IsValidIndex(Index) ? &Table.Events[Index] : nullptr;
	};
	const FWardenEventDef* Pause = Find(TEXT("pausa"));
	const FWardenEventDef* Banish = Find(TEXT("destierro"));
	const FWardenEventDef* AllOrNothing = Find(TEXT("todo-o-nada"));
	const FWardenEventDef* Wall = Find(TEXT("primer-muro"));
	const FWardenEventDef* Orb = Find(TEXT("el-orbe-llama"));
	if (!TestTrue(TEXT("Están Pausa, Destierro, Todo o nada, Primer muro y El orbe llama"), Pause && Banish && AllOrNothing && Wall && Orb))
	{
		return false;
	}
	TestEqual(TEXT("Pausa: nombre"), Pause->Name, FString(TEXT("Pausa")));
	TestEqual(TEXT("Pausa: entrega \"any\""), static_cast<int32>(Pause->Delivery), static_cast<int32>(EEventDelivery::Any));
	TestTrue(TEXT("Pausa: ventana hasta el segundo 150"), Pause->IsInWindow(150.0) && !Pause->IsInWindow(151.0));
	TestTrue(TEXT("Pausa: peso 1 en su acto y los siguientes"), Pause->Weights == TArray<double>{1.0, 1.0, 1.0});
	TestTrue(TEXT("Destierro: peso 0,3 desde el Acto II"), Banish->Weights == TArray<double>{0.0, 0.3, 0.3});
	TestEqual(TEXT("Destierro: usos 2"), Banish->MaxUses, 2);
	TestTrue(TEXT("Todo o nada: ventana desde el segundo 1920"), !AllOrNothing->IsInWindow(1919.0) && AllOrNothing->IsInWindow(1920.0) && AllOrNothing->IsInWindow(2400.0));
	TestTrue(TEXT("Todo o nada: dos rivales señalados, todas las runas"), AllOrNothing->Options[0].Applications[0].Target == EEffectTarget::TwoChosenRivals
		&& AllOrNothing->Options[0].Applications[0].Variant == EEffectVariant::AllRunes);
	TestTrue(TEXT("Primer muro: escena del Acto I, el Sello"), Wall->Act == EAct::ActOne && Wall->Channel == EWardenChannel::Scene
		&& Wall->Options[0].Applications[0].Effect == EEffect::PlaceSeal);
	TestEqual(TEXT("El orbe llama: Consulta forzada"), static_cast<int32>(Orb->Options[0].Applications[0].Effect), static_cast<int32>(EEffect::ForcedOracle));
	for (const TCHAR* Id : {TEXT("marcha-atras"), TEXT("cosecha"), TEXT("milagro")})
	{
		const FWardenEventDef* Event = Find(Id);
		TestTrue(FString::Printf(TEXT("%s: usos 1"), Id), Event && Event->MaxUses == 1);
	}
	const FWardenEventDef* Trade = Find(TEXT("trueque"));
	TestTrue(TEXT("Trueque: dilema de cuatro"), Trade && Trade->Options.Num() == 4);
	const FWardenEventDef* TestEvent = Find(TEXT("pierdes-un-turno"));
	TestTrue(TEXT("Un evento de prueba no tiene acto ni nombre"), TestEvent && !TestEvent->bInBag && TestEvent->Name.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenBagsRejectTest, "Soulbound.Core.Warden.Data.SacoMalFormadoSeRechaza",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenBagsRejectTest::RunTest(const FString& Parameters)
{
	// R-SAC-2: los campos del saco se validan como el resto del evento; se lee entero o se rechaza entero.
	auto Load = [](const FString& Bag)
	{
		const FString Json = FString::Printf(TEXT("{\"events\": [{\"id\": \"e\", \"name\": \"E\", \"text\": \"t\", \"delivery\": \"direct\", %s, ")
			TEXT("\"options\": [[{\"effect\": \"LoseTurn\", \"target\": \"ActivePlayer\"}]]}]}"), *Bag);
		return Soulbound::Rules::LoadWardenEventsFromJsonString(Json);
	};
	const FWardenEventsLoadResult Valid = Load(TEXT("\"act\": \"II\", \"channel\": \"strike\""));
	if (!TestTrue(FString::Printf(TEXT("Un saco bien formado carga (%s)"), *Valid.Message), Valid.IsSuccess()))
	{
		return false;
	}
	const FWardenEventDef& Event = Valid.Table.Events[0];
	TestTrue(TEXT("Con su acto y su canal"), Event.bInBag && Event.Act == EAct::ActTwo && Event.Channel == EWardenChannel::Strike && Event.bCutsTurn);
	TestTrue(TEXT("Peso por defecto: 0 antes de su acto, 1 en él y después"), Event.Weights == TArray<double>{0.0, 1.0, 1.0});
	TestEqual(TEXT("Usos sin límite por defecto"), Event.MaxUses, 0);

	const TCHAR* Wrong[] = {
		TEXT("\"act\": \"IV\", \"channel\": \"strike\""),
		TEXT("\"act\": \"II\""),
		TEXT("\"act\": \"II\", \"channel\": \"remark\""),
		TEXT("\"act\": \"II\", \"channel\": \"strike\", \"weights\": [0, -1, 1]"),
		TEXT("\"act\": \"II\", \"channel\": \"strike\", \"weights\": [1, 1]"),
		TEXT("\"act\": \"II\", \"channel\": \"strike\", \"maxUses\": 0"),
		TEXT("\"act\": \"II\", \"channel\": \"strike\", \"window\": {\"from\": 200, \"to\": 100}"),
		TEXT("\"act\": \"II\", \"channel\": \"strike\", \"window\": {\"from\": -5}"),
	};
	for (const TCHAR* Bag : Wrong)
	{
		const FWardenEventsLoadResult Result = Load(Bag);
		TestEqual(FString::Printf(TEXT("%s: se rechaza por el saco"), Bag), static_cast<int32>(Result.Error), static_cast<int32>(EWardenEventsError::InvalidBag));
		TestFalse(TEXT("Con motivo"), Result.Message.IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
