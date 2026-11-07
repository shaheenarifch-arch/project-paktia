using UnrealBuildTool;

public class MidnightHavoc : ModuleRules
{
    public MidnightHavoc(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Compile each .cpp on its own: several files keep same-named helpers in anonymous namespaces
        // (StateFromString, AttachVisual, ...), which collide when Unreal merges files into unity blobs.
        // Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/module-properties-in-unreal-engine
        bUseUnity = false;

        // Resolve "Folder/File.h" style includes (e.g. "Aircraft/HavocAircraftTypes.h")
        // from the module root, since headers are not split into Public/ and Private/.
        PublicIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "EnhancedInput",
                "NetCore",
                "UMG",
                "AIModule",
                "StateTreeModule",
                "GameplayStateTreeModule",
                "NavigationSystem",
                "GameplayTasks",
                "Niagara",
                "Json",
                "JsonUtilities"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
            }
        );
    }
}
