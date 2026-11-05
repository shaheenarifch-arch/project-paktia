#include "HavocAssetSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogHavocAssets);

void UHavocAssetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FString IniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("HavocAssets.ini"));
    const int32 Count = LoadFromConfigFile(IniPath);
    UE_LOG(LogHavocAssets, Log, TEXT("Mapped %d asset roles from '%s'."), Count, *IniPath);
}

int32 UHavocAssetSubsystem::LoadFromConfigFile(const FString& IniPath)
{
    RolePaths.Reset();
    if (!FPaths::FileExists(IniPath))
    {
        UE_LOG(LogHavocAssets, Warning, TEXT("'%s' not found. No asset roles mapped. See game/docs/fab-assets.md."), *IniPath);
        return 0;
    }

    FConfigFile Config;
    Config.Read(IniPath);
    const FConfigSection* Section = Config.FindSection(TEXT("HavocAssets"));
    if (!Section)
    {
        UE_LOG(LogHavocAssets, Warning, TEXT("'%s' has no [HavocAssets] section."), *IniPath);
        return 0;
    }

    for (const TPair<FName, FConfigValue>& Pair : *Section)
    {
        const FString Value = Pair.Value.GetValue().TrimStartAndEnd();
        if (Value.IsEmpty())
        {
            continue;
        }
        RolePaths.Add(Pair.Key, FSoftObjectPath(Value));
    }
    return RolePaths.Num();
}

bool UHavocAssetSubsystem::GetAssetPath(FName Role, FSoftObjectPath& OutPath) const
{
    if (const FSoftObjectPath* Found = RolePaths.Find(Role))
    {
        OutPath = *Found;
        return true;
    }
    UE_LOG(LogHavocAssets, Warning, TEXT("No asset mapped for role '%s' in HavocAssets.ini."), *Role.ToString());
    return false;
}

UObject* UHavocAssetSubsystem::LoadAsset(FName Role) const
{
    FSoftObjectPath Path;
    if (!GetAssetPath(Role, Path))
    {
        return nullptr;
    }
    UObject* Asset = Path.TryLoad();
    if (!Asset)
    {
        UE_LOG(LogHavocAssets, Warning, TEXT("Role '%s' -> '%s' could not be loaded. Add the pack from Fab and fix the path in HavocAssets.ini."), *Role.ToString(), *Path.ToString());
    }
    return Asset;
}

TArray<FName> UHavocAssetSubsystem::GetRoles() const
{
    TArray<FName> Roles;
    RolePaths.GetKeys(Roles);
    return Roles;
}
