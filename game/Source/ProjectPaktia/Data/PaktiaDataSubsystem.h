#pragma once

// Loads the versioned game definitions in the repo's data/ folder (the single
// source of truth shared with the TypeScript layer). Nothing is copied into assets.

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PaktiaMapDef.h"
#include "PaktiaMissionDef.h"
#include "PaktiaAircraftDef.h"
#include "PaktiaVehicleDef.h"
#include "PaktiaWeaponDef.h"
#include "PaktiaDataSubsystem.generated.h"

class FJsonObject;

DECLARE_LOG_CATEGORY_EXTERN(LogPaktiaData, Log, All);

UCLASS(Config = Game)
class PROJECTPAKTIA_API UPaktiaDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    bool GetMission(FName Id, FPaktiaMissionDef& OutMission) const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    bool GetMap(FName Id, FPaktiaMapDef& OutMap) const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    bool GetAircraft(FName Id, FPaktiaAircraftDef& OutAircraft) const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    bool GetVehicle(FName Id, FPaktiaVehicleDef& OutVehicle) const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    bool GetWeapon(FName Id, FPaktiaWeaponDef& OutWeapon) const;

    /** Ids of all registered missions, sorted by name. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    TArray<FName> GetAllMissionIds() const;

    /** Resolved absolute data directory (after the config override). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Data")
    FString GetResolvedDataDirectory() const { return ResolvedDataDirectory; }

protected:
    /**
     * Optional override, set in DefaultGame.ini:
     *   [/Script/ProjectPaktia.PaktiaDataSubsystem]
     *   DataDirectory=D:/path/to/data
     * Relative paths are resolved against the project directory.
     * Empty means <ProjectDir>/../data.
     */
    UPROPERTY(Config)
    FString DataDirectory;

private:
    void LoadAll();
    void LoadFolder(const FString& SubFolder, TFunctionRef<void(const TSharedRef<FJsonObject>&, const FString& File)> Handler);
    void RegisterMission(const TSharedRef<FJsonObject>& Json, const FString& File);
    void RegisterMap(const TSharedRef<FJsonObject>& Json, const FString& File);
    void RegisterAircraft(const TSharedRef<FJsonObject>& Json, const FString& File);
    void RegisterVehicle(const TSharedRef<FJsonObject>& Json, const FString& File);
    void RegisterWeapon(const TSharedRef<FJsonObject>& Json, const FString& File);

    FString ResolvedDataDirectory;

    TMap<FName, FPaktiaMissionDef> Missions;
    TMap<FName, FPaktiaMapDef> Maps;
    TMap<FName, FPaktiaAircraftDef> Aircraft;
    TMap<FName, FPaktiaVehicleDef> Vehicles;
    TMap<FName, FPaktiaWeaponDef> Weapons;
};
