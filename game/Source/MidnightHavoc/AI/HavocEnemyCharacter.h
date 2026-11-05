#pragma once

// AI-only enemy infantry for Episode 04 (Iran, Houthi, ISIS units from data/units).
// A unit definition sets health, accuracy and sight (game-balance ratings only). These characters
// can never be possessed by a player: ISIS is AI-only by rule and the other factions are opponents
// in this mission. Coalition units (allied squads, data/units/coalition.json) use the same class with
// Team = Allied; they are AI too. Mesh and rifle come from the item-3 roles Character.Marine (reused until an
// enemy pack is mapped), Weapon.KR47.

#include "CoreMinimal.h"
#include "Characters/HavocCharacter.h"
#include "AI/HavocSquadTypes.h"
#include "Data/HavocUnitDef.h"
#include "HavocEnemyCharacter.generated.h"

class UHavocWeaponComponent;
class USkeletalMeshComponent;

UCLASS()
class MIDNIGHTHAVOC_API AHavocEnemyCharacter : public AHavocCharacter
{
    GENERATED_BODY()

public:
    AHavocEnemyCharacter();

    virtual void PossessedBy(AController* NewController) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Enemy")
    TObjectPtr<UHavocWeaponComponent> Weapon;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Enemy")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    FString UnitId;

    /** "iran", "houthi" or "isis". */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    FString Faction;

    /** Sector the unit was spawned for (kharg_north_beach, ...). Used to decide when a sector is clear. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    FName SectorId;

    /** Allied for coalition units, Opposing for iran, houthi and isis. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    EHavocTeam Team = EHavocTeam::Opposing;

    /** Id from UHavocSquadSubsystem, or -1 when the unit is not in a squad. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    int32 SquadId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    float SightRadiusCm = 5000.f;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Enemy")
    float EngageRangeCm = 3000.f;

    /** Team of any actor: the player's Marine is Allied, enemy characters use Team, other characters count as Opposing. */
    static EHavocTeam TeamOf(const AActor* Actor);

    /** Server: applies a unit definition. Call right after spawning. */
    void ApplyUnitDef(const FHavocUnitDef& Def, FName InSectorId);

protected:
    virtual void BeginPlay() override;
    virtual void HandleDeath() override;
};
