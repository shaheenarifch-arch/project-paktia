using UnrealBuildTool;
using System.Collections.Generic;

public class MidnightHavocEditorTarget : TargetRules
{
    public MidnightHavocEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;

        ExtraModuleNames.AddRange(
            new string[]
            {
                "MidnightHavoc"
            }
        );
    }
}
