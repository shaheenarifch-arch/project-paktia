#include "Combat/HavocWeaponComponent.h"

#include "AI/HavocAIController.h"
#include "Audio/HavocAudioSubsystem.h"
#include "Characters/HavocCharacter.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "AI/HavocEnemyCharacter.h"
#include "Player/HavocPlayerCharacter.h"
#include "Perception/AISense_Hearing.h"
#include "TimerManager.h"
#include "World/HavocTerminalPart.h"

namespace
{
    UHavocEventSubsystem* GetEvents(const UWorld* World)
    {
        const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
        return GI ? GI->GetSubsystem<UHavocEventSubsystem>() : nullptr;
    }
}

UHavocWeaponComponent::UHavocWeaponComponent()
    : WeaponId(TEXT("kr47"))
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHavocWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHavocWeaponComponent, AmmoInMagazine);
    DOREPLIFETIME(UHavocWeaponComponent, ReserveAmmo);
    DOREPLIFETIME(UHavocWeaponComponent, bReloading);
}

void UHavocWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        InitializeFromData();
    }
}

void UHavocWeaponComponent::InitializeFromData()
{
    MagazineCapacity = 30;
    const UWorld* World = GetWorld();
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    FHavocWeaponDef Def;
    if (Data && Data->GetWeapon(WeaponId, Def) && Def.bEnabled && Def.MagazineCapacity > 0)
    {
        MagazineCapacity = Def.MagazineCapacity;
        // Single-fire-only weapons fire slower; automatic keeps the configured interval.
        if (!Def.FireModes.Contains(TEXT("automatic")))
        {
            FireInterval = FMath::Max(FireInterval, 0.35f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UHavocWeaponComponent: weapon '%s' not found or disabled in data/weapons; using %d rounds."), *WeaponId.ToString(), MagazineCapacity);
    }
    AmmoInMagazine = MagazineCapacity;
    ReserveAmmo = MagazineCapacity * FMath::Max(0, SpareMagazines);
    bReloading = false;
    NotifyAmmo();
}

void UHavocWeaponComponent::AddSpareMagazines(int32 Count)
{
    ReserveAmmo += MagazineCapacity * FMath::Max(0, Count);
    NotifyAmmo();
}

bool UHavocWeaponComponent::ValidateFireIntent(const MidnightHavoc::FFireIntent& Intent)
{
    const UWorld* World = GetWorld();
    if (!World || Intent.WeaponSlot != 0 || Intent.InputSequence <= LastSequence)
    {
        return false;
    }
    if (bReloading || AmmoInMagazine <= 0)
    {
        return false;
    }
    return World->GetTimeSeconds() - LastShotTime >= FireInterval * 0.9;
}

void UHavocWeaponComponent::ApplyValidatedFireIntent(const MidnightHavoc::FFireIntent& Intent)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner)
    {
        return;
    }
    LastSequence = Intent.InputSequence;
    LastShotTime = World->GetTimeSeconds();
    --AmmoInMagazine;

    FRotator Aim(Intent.AimPitch, Intent.AimYaw, 0.f);
    const float Spread = FMath::Max(0.f, SpreadDegrees + PendingSpread);
    const FVector Direction = FMath::VRandCone(Aim.Vector(), FMath::DegreesToRadians(Spread));
    const FVector Start = PendingOrigin;
    const FVector End = Start + Direction * Range;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(HavocShot), /*bTraceComplex*/ false, Owner);
    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    if (UHavocAudioSubsystem* Audio = World->GetSubsystem<UHavocAudioSubsystem>())
    {
        Audio->PlayRole(TEXT("Audio.Weapon.Rifle"), Start);
        Audio->PlayRole(TEXT("FX.Muzzle"), Start, Direction.Rotation());
    }
    UAISense_Hearing::ReportNoiseEvent(World, Start, 1.f, Owner, 0.f, TEXT("Gunshot"));
    if (UHavocEventSubsystem* Events = GetEvents(World))
    {
        Events->Publish(TEXT("combat:shot"), FString::Printf(TEXT("{\"shooter\":\"%s\",\"weapon\":\"%s\"}"), *Owner->GetName(), *WeaponId.ToString()));
    }

    if (bHit && Hit.GetActor())
    {
        AActor* Target = Hit.GetActor();
        bool bDamaged = false;
        if (AHavocCharacter* Character = Cast<AHavocCharacter>(Target))
        {
            // Rounds hurt the other team only: the player and allied squads against opposing units. No friendly fire.
            // Decided by class, not controller, so the Marine stays a valid target while the Raven is flown.
            if (AHavocEnemyCharacter::TeamOf(Owner) != AHavocEnemyCharacter::TeamOf(Character))
            {
                Character->ApplyAuthoritativeDamage(DamagePerHit);
                bDamaged = true;
                if (AHavocAIController* AI = Cast<AHavocAIController>(Character->GetController()))
                {
                    AI->NotifyShotBy(Owner);
                }
            }
        }
        else
        {
            // Stray fire into the oil terminal damages that facility (Episode 04 rule: keep it intact).
            AHavocTerminalPart* Part = Cast<AHavocTerminalPart>(Target);
            if (!Part && Target->GetAttachParentActor())
            {
                Part = Cast<AHavocTerminalPart>(Target->GetAttachParentActor());
            }
            if (Part)
            {
                Part->ApplyTerminalDamage(0.004f);
            }
        }
        if (UHavocEventSubsystem* Events = GetEvents(World))
        {
            Events->Publish(TEXT("combat:impact"), FString::Printf(TEXT("{\"target\":\"%s\",\"damaged\":%s}"), *Target->GetName(), bDamaged ? TEXT("true") : TEXT("false")));
        }
    }
    NotifyAmmo();

    if (AmmoInMagazine <= 0)
    {
        StartReload();
    }
}

bool UHavocWeaponComponent::TryFireFromAim(const FVector& Origin, float AimYaw, float AimPitch, int64 InputSequence, float ExtraSpreadDegrees)
{
    MidnightHavoc::FFireIntent Intent{ InputSequence, 0, AimYaw, AimPitch };
    if (!ValidateFireIntent(Intent))
    {
        return false;
    }
    PendingOrigin = Origin;
    PendingSpread = ExtraSpreadDegrees;
    ApplyValidatedFireIntent(Intent);
    return true;
}

bool UHavocWeaponComponent::StartReload()
{
    UWorld* World = GetWorld();
    if (!World || bReloading || AmmoInMagazine >= MagazineCapacity || ReserveAmmo <= 0)
    {
        return false;
    }
    bReloading = true;
    NotifyAmmo();
    World->GetTimerManager().SetTimer(ReloadTimer, this, &UHavocWeaponComponent::FinishReload, ReloadSeconds, false);
    return true;
}

void UHavocWeaponComponent::FinishReload()
{
    const int32 Needed = MagazineCapacity - AmmoInMagazine;
    const int32 Moved = FMath::Min(Needed, ReserveAmmo);
    AmmoInMagazine += Moved;
    ReserveAmmo -= Moved;
    bReloading = false;
    NotifyAmmo();
}

void UHavocWeaponComponent::OnRep_Ammo()
{
    OnAmmoChanged.Broadcast();
}

void UHavocWeaponComponent::NotifyAmmo()
{
    OnAmmoChanged.Broadcast();
}
