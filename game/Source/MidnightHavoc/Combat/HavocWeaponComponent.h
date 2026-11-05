#pragma once

// Hitscan rifle for the playable loop (player and AI). Weapon data (magazine size, fire modes) comes
// from data/weapons/<WeaponId>.json through UHavocDataSubsystem; the JSON has no numeric damage, so
// damage, rate of fire and reload time are abstract game-balance values set here (EditDefaultsOnly).
//
// Implements MidnightHavoc::ICombatAuthority: the server validates every fire intent (ammo, reload,
// fire interval, increasing input sequence) before applying damage. Call TryFire* on the server only.
// Publishes "combat:shot" and "combat:impact" on UHavocEventSubsystem (names proposed in
// docs/audio-voice-plan.md section 2.5) and plays the item-3 FX/audio roles through UHavocAudioSubsystem.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatAuthority.h"
#include "HavocWeaponComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHavocWeaponChangedDelegate);

UCLASS(ClassGroup = (Havoc), meta = (BlueprintSpawnableComponent))
class MIDNIGHTHAVOC_API UHavocWeaponComponent : public UActorComponent, public MidnightHavoc::ICombatAuthority
{
    GENERATED_BODY()

public:
    UHavocWeaponComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Id in data/weapons (default kr47). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    FName WeaponId;

    /** Abstract game-balance damage per hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    float DamagePerHit = 20.f;

    /** Seconds between shots. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    float FireInterval = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    float ReloadSeconds = 2.0f;

    /** Maximum trace range in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    float Range = 60000.f;

    /** Magazines carried besides the one loaded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    int32 SpareMagazines = 4;

    /** Random cone half-angle in degrees (AI uses a larger one). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Weapon")
    float SpreadDegrees = 0.6f;

    UPROPERTY(ReplicatedUsing = OnRep_Ammo, BlueprintReadOnly, Category = "Havoc|Weapon")
    int32 AmmoInMagazine = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Ammo, BlueprintReadOnly, Category = "Havoc|Weapon")
    int32 ReserveAmmo = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Ammo, BlueprintReadOnly, Category = "Havoc|Weapon")
    bool bReloading = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Weapon")
    int32 MagazineCapacity = 30;

    UPROPERTY(BlueprintAssignable, Category = "Havoc|Weapon")
    FHavocWeaponChangedDelegate OnAmmoChanged;

    /** Server: loads the weapon definition and fills the magazines. Safe to call again to resupply. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Weapon")
    void InitializeFromData();

    /** Server: fire along a yaw/pitch (degrees) from Origin. Returns true if a shot was taken. */
    bool TryFireFromAim(const FVector& Origin, float AimYaw, float AimPitch, int64 InputSequence, float ExtraSpreadDegrees = 0.f);

    /** Server: begin a reload if the magazine is not full and there is reserve. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Weapon")
    bool StartReload();

    /** Server: add magazines (e.g. a resupply). */
    void AddSpareMagazines(int32 Count);

    // ---- MidnightHavoc::ICombatAuthority ----
    virtual bool ValidateFireIntent(const MidnightHavoc::FFireIntent& Intent) override;
    virtual void ApplyValidatedFireIntent(const MidnightHavoc::FFireIntent& Intent) override;

    /** Origin used by ApplyValidatedFireIntent (set by TryFireFromAim). */
    FVector PendingOrigin = FVector::ZeroVector;
    float PendingSpread = 0.f;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnRep_Ammo();

private:
    void FinishReload();
    void NotifyAmmo();

    int64 LastSequence = -1;
    double LastShotTime = -1000.0;
    FTimerHandle ReloadTimer;
};
