#include "Player/HavocPlayerCharacter.h"

#include "GameFramework/PlayerController.h"

#include "Assets/HavocAssetSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Combat/HavocWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Input/HavocPlayInput.h"
#include "Player/HavocPlaySettings.h"

AHavocPlayerCharacter::AHavocPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Shooter-style third person: the body follows where the camera looks.
    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    GetCharacterMovement()->MaxWalkSpeedCrouched = 220.f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = TargetArmLength;
    CameraBoom->SocketOffset = FVector(0.f, 55.f, 60.f);
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    Weapon = CreateDefaultSubobject<UHavocWeaponComponent>(TEXT("Weapon"));

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHavocPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyAssetRoles();
}

void AHavocPlayerCharacter::ApplyAssetRoles()
{
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

void AHavocPlayerCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (const UGameInstance* GI = GetGameInstance())
        {
            if (const UHavocPlayInput* Input = GI->GetSubsystem<UHavocPlayInput>())
            {
                Input->ApplyContexts(PC, Input->FootContext, 1);
            }
        }
    }
}

void AHavocPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // Deliberately not calling AHavocCharacter's version: it binds designer-assigned actions only.
    ACharacter::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    const UGameInstance* GI = GetGameInstance();
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    if (!Enhanced || !Input)
    {
        UE_LOG(LogTemp, Error, TEXT("AHavocPlayerCharacter: Enhanced Input is not the default input component. Set DefaultInputComponentClass in DefaultInput.ini."));
        return;
    }

    Enhanced->BindAction(Input->MoveAction, ETriggerEvent::Triggered, this, &AHavocPlayerCharacter::Move);
    Enhanced->BindAction(Input->LookAction, ETriggerEvent::Triggered, this, &AHavocPlayerCharacter::Look);
    Enhanced->BindAction(Input->JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
    Enhanced->BindAction(Input->JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
    Enhanced->BindAction(Input->SprintAction, ETriggerEvent::Started, this, &AHavocPlayerCharacter::StartSprint);
    Enhanced->BindAction(Input->SprintAction, ETriggerEvent::Completed, this, &AHavocPlayerCharacter::StopSprint);
    Enhanced->BindAction(Input->CrouchAction, ETriggerEvent::Started, this, &AHavocPlayerCharacter::ToggleCrouch);
    Enhanced->BindAction(Input->AimAction, ETriggerEvent::Started, this, &AHavocPlayerCharacter::StartAim);
    Enhanced->BindAction(Input->AimAction, ETriggerEvent::Completed, this, &AHavocPlayerCharacter::StopAim);
    Enhanced->BindAction(Input->FireAction, ETriggerEvent::Started, this, &AHavocPlayerCharacter::StartFire);
    Enhanced->BindAction(Input->FireAction, ETriggerEvent::Completed, this, &AHavocPlayerCharacter::StopFire);
    Enhanced->BindAction(Input->ReloadAction, ETriggerEvent::Started, this, &AHavocPlayerCharacter::DoReload);
}

void AHavocPlayerCharacter::Move(const FInputActionValue& Value)
{
    if (IsDead() || !Controller)
    {
        return;
    }
    const FVector2D Movement = Value.Get<FVector2D>();
    const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Movement.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Movement.X);
}

void AHavocPlayerCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LookInput = Value.Get<FVector2D>();
    float Scale = 1.f;
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (const UHavocPlaySettings* Settings = GI->GetSubsystem<UHavocPlaySettings>())
        {
            Scale = Settings->LookSensitivity;
        }
    }
    if (bAiming)
    {
        Scale *= 0.6f;
    }
    AddControllerYawInput(LookInput.X * Scale);
    AddControllerPitchInput(LookInput.Y * Scale);
}

void AHavocPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsLocallyControlled())
    {
        const float Goal = bAiming ? 180.f : TargetArmLength;
        CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Goal, DeltaSeconds, 12.f);
        FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, bAiming ? 60.f : 90.f, DeltaSeconds, 12.f));
        if (bFireHeld)
        {
            FireOnce();
        }
    }
}

void AHavocPlayerCharacter::StartFire() { bFireHeld = !IsDead(); }
void AHavocPlayerCharacter::StopFire() { bFireHeld = false; }

void AHavocPlayerCharacter::FireOnce()
{
    const UWorld* World = GetWorld();
    if (!World || !Controller || IsDead())
    {
        return;
    }
    // Throttle on the client so we do not flood the server; the server validates again.
    if (World->GetTimeSeconds() - LastLocalShot < Weapon->FireInterval)
    {
        return;
    }
    if (Weapon->bReloading || Weapon->AmmoInMagazine <= 0)
    {
        if (Weapon->AmmoInMagazine <= 0 && !Weapon->bReloading)
        {
            DoReload();
        }
        return;
    }
    LastLocalShot = World->GetTimeSeconds();
    const FRotator Aim = Controller->GetControlRotation();
    ServerFire(NextSequence++, Aim.Yaw, Aim.Pitch);
}

void AHavocPlayerCharacter::ServerFire_Implementation(int64 InputSequence, float AimYaw, float AimPitch)
{
    if (IsDead())
    {
        return;
    }
    const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
    // Aiming tightens the cone; the hip is looser.
    Weapon->TryFireFromAim(Origin, AimYaw, AimPitch, InputSequence, bAiming ? 0.f : 1.2f);
}

void AHavocPlayerCharacter::DoReload()
{
    if (!IsDead())
    {
        ServerReload();
    }
}

void AHavocPlayerCharacter::ServerReload_Implementation()
{
    Weapon->StartReload();
}

void AHavocPlayerCharacter::StartAim() { bAiming = true; UpdateSpeed(); }
void AHavocPlayerCharacter::StopAim() { bAiming = false; UpdateSpeed(); }

void AHavocPlayerCharacter::StartSprint()
{
    bSprinting = true;
    UpdateSpeed();
    ServerSetSprint(true);
}

void AHavocPlayerCharacter::StopSprint()
{
    bSprinting = false;
    UpdateSpeed();
    ServerSetSprint(false);
}

void AHavocPlayerCharacter::ServerSetSprint_Implementation(bool bInSprint)
{
    bSprinting = bInSprint;
    UpdateSpeed();
}

void AHavocPlayerCharacter::UpdateSpeed()
{
    float Speed = WalkSpeed;
    if (bAiming)
    {
        Speed = AimWalkSpeed;
    }
    else if (bSprinting && !bIsCrouched)
    {
        Speed = SprintSpeed;
    }
    GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void AHavocPlayerCharacter::ToggleCrouch()
{
    if (bIsCrouched)
    {
        UnCrouch();
    }
    else
    {
        Crouch();
    }
}

void AHavocPlayerCharacter::ReleaseInputState()
{
    bFireHeld = false;
    bAiming = false;
    bSprinting = false;
    UpdateSpeed();
}

void AHavocPlayerCharacter::HandleDeath()
{
    ReleaseInputState();
    GetCharacterMovement()->DisableMovement();
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(TEXT("player:died"), TEXT("{}"));
        }
    }
}

void AHavocPlayerCharacter::Revive(const FTransform& Where)
{
    bIsDead = false;
    Health = MaximumHealth;
    GetMesh()->SetSimulatePhysics(false);
    GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
    GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    SetActorTransform(Where, false, nullptr, ETeleportType::TeleportPhysics);
    Weapon->InitializeFromData();
}
