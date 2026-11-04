using UnrealBuildTool;
using System.Collections.Generic;

public class MidnightHavocTarget : TargetRules
{
    public MidnightHavocTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;

        ExtraModuleNames.AddRange(
            new string[]
            {
                "MidnightHavoc"
            }
        );
    }
}
