#include "AI/HavocEnemyCharacter.h"

#include "AI/HavocAIController.h"
#include "Assets/HavocAssetSubsystem.h"
#include "Combat/HavocWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "AI/HavocSquadLogic.h"
#include "Player/HavocPlayerCharacter.h"

AHavocEnemyCharacter::AHavocEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass = AHavocAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = 380.f;

    Weapon = CreateDefaultSubobject<UHavocWeaponComponent>(TEXT("Weapon"));
    Weapon->FireInterval = 0.25f;
    Weapon->SpareMagazines = 99; // AI does not run dry
    Weapon->SpreadDegrees = 2.5f;

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHavocEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    const UGameInstance* GI = GetGameInstance();
    const UHavocAssetSubsystem* Assets = GI ? GI->GetSubsystem<UHavocAssetSubsystem>() : nullptr;
    if (!Assets)
    {
        return;
    }
    if (USkeletalMesh* Body = Assets->LoadAs<USkeletalMesh>(TEXT("Character.Marine")))
    {
        GetMesh()->SetSkeletalMesh(Body);
        GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
        GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        if (UClass* Anim = Cast<UClass>(Assets->LoadAsset(TEXT("Character.Marine.AnimBP"))))
        {
            GetMesh()->SetAnimInstanceClass(Anim);
        }
    }
    if (USkeletalMesh* Rifle = Assets->LoadAs<USkeletalMesh>(TEXT("Weapon.KR47")))
    {
        WeaponMesh->SetSkeletalMesh(Rifle);
    }
}

EHavocTeam AHavocEnemyCharacter::TeamOf(const AActor* Actor)
{
    if (Actor && Actor->IsA<AHavocPlayerCharacter>())
    {
        return EHavocTeam::Allied;
    }
    if (const AHavocEnemyCharacter* Unit = Cast<AHavocEnemyCharacter>(Actor))
    {
        return Unit->Team;
    }
    return EHavocTeam::Opposing;
}

void AHavocEnemyCharacter::PossessedBy(AController* NewController)
{
    if (NewController && NewController->IsPlayerController())
    {
        UE_LOG(LogTemp, Error, TEXT("AHavocEnemyCharacter: enemy units are AI-only (ISIS never playable); player possession refused."));
        return;
    }
    Super::PossessedBy(NewController);
}

void AHavocEnemyCharacter::ApplyUnitDef(const FHavocUnitDef& Def, FName InSectorId)
{
    UnitId = Def.Id;
    Faction = Def.Faction;
    Team = HavocSquad::TeamForFaction(Def.Faction);
    SectorId = InSectorId;

    SetMaximumHealthAndFill(40.f + 8.f * Def.AntiInfantry);
    SightRadiusCm = 4500.f + 700.f * Def.Optics;
    EngageRangeCm = 2200.f + 250.f * Def.AntiInfantry;
    Weapon->DamagePerHit = 3.f + 1.0f * Def.AntiInfantry;
    Weapon->SpreadDegrees = FMath::Clamp(4.0f - 0.25f * Def.AntiInfantry, 1.2f, 4.0f);
}

void AHavocEnemyCharacter::HandleDeath()
{
    if (AController* C = GetController())
    {
        C->StopMovement();
        C->UnPossess();
    }
    GetCharacterMovement()->DisableMovement();
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetLifeSpan(20.f);

    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(TEXT("ai:killed"), FString::Printf(TEXT("{\"unit\":\"%s\",\"faction\":\"%s\",\"sector\":\"%s\"}"), *UnitId, *Faction, *SectorId.ToString()));
        }
    }
}
