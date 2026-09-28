using UnrealBuildTool;

// Motor de reglas "sin mundo": estado en USTRUCT y reglas como funciones libres.
// Solo depende de Core y CoreUObject, más Json para cargar datos (el tablero, S-002). Nunca de Engine:
// así se prueba y se simula sin UWorld. JsonUtilities no: depende de más cosas y no hace falta.
public class SoulboundCore : ModuleRules
{
	public SoulboundCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
