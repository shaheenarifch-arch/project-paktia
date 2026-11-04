using UnrealBuildTool;
using System.Collections.Generic;

public class MidnightHavocServerTarget : TargetRules
{
    public MidnightHavocServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V5;

        ExtraModuleNames.AddRange(
            new string[]
            {
                "MidnightHavoc"
            }
        );
    }
}
