using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectPaktiaTarget : TargetRules
{
    public ProjectPaktiaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;

        ExtraModuleNames.AddRange(
            new string[]
            {
                "ProjectPaktia"
            }
        );
    }
}
