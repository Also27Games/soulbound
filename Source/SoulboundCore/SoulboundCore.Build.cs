using UnrealBuildTool;

// Motor de reglas "sin mundo": estado en USTRUCT y reglas como funciones libres.
// Solo depende de Core y CoreUObject, más Json para cargar datos (el tablero, S-002). Nunca de Engine:
// así se prueba y se simula sin UWorld. JsonUtilities no: depende de más cosas y no hace falta.
public class SoulboundCore : ModuleRules
{
	public SoulboundCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Sin unity build: los tests definen ayudantes con el mismo nombre en espacios anónimos (DomainTouches, Join,
		// PowerNames...) y el unity los funde en un solo archivo, donde chocan. Si choca o no depende de qué archivos
		// toque el commit (compilación adaptativa), así que la compilación fallaría según el día. Cada archivo se
		// compila solo (S-008).
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
