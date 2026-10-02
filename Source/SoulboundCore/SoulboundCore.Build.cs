using UnrealBuildTool;

// Motor de reglas "sin mundo": estado en USTRUCT y reglas como funciones libres.
// Solo depende de Core y CoreUObject, más Json para cargar datos (el tablero, S-002). Nunca de Engine:
// así se prueba y se simula sin UWorld. JsonUtilities no: depende de más cosas y no hace falta.
public class SoulboundCore : ModuleRules
{
	public SoulboundCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Sin unity build: los archivos de reglas y de tests definen ayudantes con el mismo nombre en espacios anónimos
		// (ReadIntegerField, DomainTouches...). Con unity, que choquen o no depende de qué archivos toca el commit
		// (compilación adaptativa) y la compilación falla según el día. Cada archivo se compila solo (S-008).
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
