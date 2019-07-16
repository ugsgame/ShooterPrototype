// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShooterGame : ModuleRules
{
	public ShooterGame(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivatePCHHeaderFile = "ShooterGame.h";

        PrivateIncludePaths.AddRange(
			new string[] { 
				"ShooterGame/",
                "ShooterGame/AI",
                "ShooterGame/Bots",
                "ShooterGame/Effects",
                "ShooterGame/Pickups",
                "ShooterGame/Player",
                "ShooterGame/Sound",
                "ShooterGame/Weapons",
                "ShooterGame/Online",
                "ShooterGame/UI",
				"ShooterGame/UI/Menu",
				"ShooterGame/UI/Style",
				"ShooterGame/UI/Widgets",
            }
		);

        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"AssetRegistry",
                "AIModule",
				"GameplayTasks",
                "Landscape",
                "PhysXVehicles",
                "NavigationSystem",
            }
		);

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore",
				"Slate",
				"SlateCore",
				"ShooterGameLoadingScreen",
				"Json"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"OnlineSubsystemNull",
				"NetworkReplayStreaming",
				"NullNetworkReplayStreaming",
				"HttpNetworkReplayStreaming"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"NetworkReplayStreaming"
			}
		);
	}
}
