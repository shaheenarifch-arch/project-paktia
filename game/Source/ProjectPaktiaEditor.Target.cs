using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectPaktiaEditorTarget : TargetRules
{
    public ProjectPaktiaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;

        ExtraModuleNames.AddRange(
            new string[]
            {
                "ProjectPaktia"
            }
        );
    }
}
