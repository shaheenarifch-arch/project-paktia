#pragma once

// Loads the versioned game definitions in the repo's data/ folder (the single
// source of truth shared with the TypeScript layer). Nothing is copied into assets.

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HavocMapDef.h"
#include "HavocMissionDef.h"
#include "HavocAircraftDef.h"
#include "HavocVehicleDef.h"
#include "HavocWeaponDef.h"
#include "HavocUnitDef.h"
#include "HavocDataSubsystem.generated.h"

class FJsonObject;

DECLARE_LOG_CATEGORY_EXTERN(LogHavocData, Log, All);

UCLASS(Config = Game)
class MIDNIGHTHAVOC_API UHavocDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool GetMission(FName Id, FHavocMissionDef& OutMission) const;

    /** The campaign mission for an episode number (1-4), whatever its id ("mission_01_..." or "episode_0N_..."). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool FindMissionByEpisode(int32 Episode, FHavocMissionDef& OutMission) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool GetMap(FName Id, FHavocMapDef& OutMap) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool GetAircraft(FName Id, FHavocAircraftDef& OutAircraft) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool GetVehicle(FName Id, FHavocVehicleDef& OutVehicle) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool GetWeapon(FName Id, FHavocWeaponDef& OutWeapon) const;

    /** A unit from data/units (game-balance ratings only). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    bool GetUnit(FName Id, FHavocUnitDef& OutUnit) const;

    /** Infantry-branch units of a faction ("iran", "houthi", "isis"), in file order. Used for AI enemy squads. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    TArray<FHavocUnitDef> GetInfantryUnits(const FString& Faction) const;

    /** Ids of all registered missions, sorted by name. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    TArray<FName> GetAllMissionIds() const;

    /** Resolved absolute data directory (after the config override). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Data")
    FString GetResolvedDataDirectory() const { return ResolvedDataDirectory; }

protected:
    /**
     * Optional override, set in DefaultGame.ini:
     *   [/Script/MidnightHavoc.HavocDataSubsystem]
     *   DataDirectory=D:/path/to/data
     * Relative paths are resolved against the project directory.
     * Empty means <ProjectDir>/../data, then <ProjectDir>/Content/HavocData (packaged builds).
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
    void RegisterUnits(const TSharedRef<FJsonObject>& Json, const FString& File);

    FString ResolvedDataDirectory;

    TMap<FName, FHavocMissionDef> Missions;
    TMap<FName, FHavocMapDef> Maps;
    TMap<FName, FHavocAircraftDef> Aircraft;
    TMap<FName, FHavocVehicleDef> Vehicles;
    TMap<FName, FHavocWeaponDef> Weapons;
    TMap<FName, FHavocUnitDef> Units;
    TArray<FName> UnitOrder;
};
