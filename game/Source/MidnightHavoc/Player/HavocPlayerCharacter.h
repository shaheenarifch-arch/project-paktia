#pragma once

// Third-person Marine for Episode 04. Extends AHavocCharacter (health, damage, replication) with a
// camera boom, sprint / crouch / aim, the KR-47 (UHavocWeaponComponent, data/weapons/kr47.json) and
// Enhanced Input built in code by UHavocPlayInput (no editor assets needed). Health and damage go
// through the server: the fire intent is sent by RPC and validated by the weapon's ICombatAuthority.
// Mesh, animation Blueprint and rifle mesh come from the item-3 roles Character.Marine,
// Character.Marine.AnimBP and Weapon.KR47 (Config/HavocAssets.ini); if a role is missing the Marine
// is still playable (invisible body) and the log says which role to fill in.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine

#include "CoreMinimal.h"
#include "Characters/HavocCharacter.h"
#include "InputActionValue.h"
#include "HavocPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UHavocWeaponComponent;
class USkeletalMeshComponent;

UCLASS()
class MIDNIGHTHAVOC_API AHavocPlayerCharacter : public AHavocCharacter
{
    GENERATED_BODY()

public:
    AHavocPlayerCharacter();

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Player")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Player")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Player")
    TObjectPtr<UHavocWeaponComponent> Weapon;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Player")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Player")
    float WalkSpeed = 450.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Player")
    float SprintSpeed = 750.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Havoc|Player")
    float AimWalkSpeed = 250.f;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Player")
    bool bAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Player")
    bool bSprinting = false;

    /** Server: put the Marine back on its feet at a transform with full health and ammo (checkpoint respawn). */
    void Revive(const FTransform& Where);

    /** Local: stop firing/aiming (menus, possessing another pawn). */
    void ReleaseInputState();

protected:
    virtual void BeginPlay() override;
    virtual void Move(const FInputActionValue& Value) override;
    virtual void Look(const FInputActionValue& Value) override;
    virtual void HandleDeath() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(Server, Reliable)
    void ServerFire(int64 InputSequence, float AimYaw, float AimPitch);

    UFUNCTION(Server, Reliable)
    void ServerReload();

    UFUNCTION(Server, Reliable)
    void ServerSetSprint(bool bInSprint);

private:
    void ApplyAssetRoles();
    void StartFire();
    void StopFire();
    void FireOnce();
    void StartAim();
    void StopAim();
    void StartSprint();
    void StopSprint();
    void ToggleCrouch();
    void DoReload();
    void UpdateSpeed();

    bool bFireHeld = false;
    int64 NextSequence = 1;
    double LastLocalShot = -1000.0;
    float TargetArmLength = 350.f;
};
