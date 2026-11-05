#pragma once

// Maps game roles (e.g. "Terrain.Sand", "Aircraft.Falcon") to soft asset paths read from
// Config/HavocAssets.ini, so swapping a Fab / Megascans pack is config-only.
// Guide: game/docs/fab-assets.md
// Help: https://dev.epicgames.com/documentation/en-us/fab/using-fab-in-unreal-engine

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "HavocAssetSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHavocAssets, Log, All);

UCLASS()
class MIDNIGHTHAVOC_API UHavocAssetSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Soft path for a role. Returns false (and logs) if the role is not mapped. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Assets")
    bool GetAssetPath(FName Role, FSoftObjectPath& OutPath) const;

    /** Synchronously loads the asset for a role; null (with a warning) if unmapped or not imported. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Assets")
    UObject* LoadAsset(FName Role) const;

    template <typename T>
    T* LoadAs(FName Role) const { return Cast<T>(LoadAsset(Role)); }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Assets")
    TArray<FName> GetRoles() const;

    /** Reads the [HavocAssets] section of an ini file, replacing current mappings. Returns number of roles mapped. */
    int32 LoadFromConfigFile(const FString& IniPath);

private:
    TMap<FName, FSoftObjectPath> RolePaths;
};
