// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Soulbound : ModuleRules
{
	public Soulbound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "SoulboundCore" });

		// Also27Online: plugin propio de sesiones por código (Plugins/A27Online). Solo lo usa el spike de momento.
		PrivateDependencyModuleNames.AddRange(new string[] { "Also27Online" });

		// Slate y SlateCore: el panel de crónica y la leyenda de la vista de depuración (S-007).
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// EngineSettings: UGameMapsSettings, para volver al mapa del menú (GameDefaultMap) al acabar la partida (S-022).
		PrivateDependencyModuleNames.AddRange(new string[] { "EngineSettings" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
