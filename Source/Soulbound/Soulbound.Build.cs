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

		// UMG (módulo del motor): el libro de "Cómo se juega" del menú es un widget de UMG escrito en C++ (UHowToPlayBookWidget, S-049, R-HUD-22), el
		// primero del proyecto, para que el autor lo coloque en su pantalla del menú.
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG" });

		// Json: los textos del Grimorio (Content/Data/Textos/grimorio.json), que son de la presentación y no del motor (S-027).
		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
