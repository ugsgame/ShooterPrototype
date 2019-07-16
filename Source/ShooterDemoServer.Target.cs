// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class ShooterDemoServerTarget : TargetRules
{
	public ShooterDemoServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		//bUsesSteam = true;

		ExtraModuleNames.Add("ShooterDemo");
	}
}
