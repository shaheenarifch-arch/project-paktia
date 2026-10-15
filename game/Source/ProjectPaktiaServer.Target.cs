using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectPaktiaServerTarget : TargetRules
{
    public ProjectPaktiaServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V5;

        ExtraModuleNames.AddRange(
            new string[]
            {
                "ProjectPaktia"
            }
        );
    }
}
