#include "Vehicles/HavocVehicleBase.h"

#include "Assets/HavocAssetSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/HavocPlayInput.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocVehicle, Log, All);

namespace HavocVehicleMath
{
    double StepSpeed(double Speed, double Throttle, double MaxSpeed, double Accel, double Drag, double Dt)
    {
        const double Target = FMath::Clamp(Throttle, -1.0, 1.0) * MaxSpeed;
        if (FMath::IsNearlyZero(Throttle))
        {
            return FMath::FInterpConstantTo(Speed, 0.0, Dt, Drag);
        }
        return FMath::FInterpConstantTo(Speed, Target, Dt, Accel);
    }

    double SurfaceYawDelta(double Steer, double Speed, double MaxSpeed, double TurnRate, double Dt)
    {
        if (MaxSpeed <= KINDA_SMALL_NUMBER)
        {
            return 0.0;
        }
        const double SpeedFraction = FMath::Clamp(Speed / MaxSpeed, -1.0, 1.0);
        return FMath::Clamp(Steer, -1.0, 1.0) * TurnRate * SpeedFraction * Dt;
    }
}

namespace HavocVehicleMath
{
    double TrackedYawDelta(double Steer, double Speed, double TurnRate, double Dt)
    {
        const double Direction = Speed < -KINDA_SMALL_NUMBER ? -1.0 : 1.0;
        return FMath::Clamp(Steer, -1.0, 1.0) * TurnRate * Direction * Dt;
    }

    EHavocVehicleDamage DamageLevel(double Integrity)
    {
        if (Integrity <= 0.0) return EHavocVehicleDamage::Destroyed;
        if (Integrity <= 0.15) return EHavocVehicleDamage::Disabled;
        if (Integrity <= 0.35) return EHavocVehicleDamage::Critical;
        if (Integrity <= 0.7) return EHavocVehicleDamage::Damaged;
        return EHavocVehicleDamage::Operational;
    }

    double MobilityFactor(EHavocVehicleDamage Level)
    {
        switch (Level)
        {
        case EHavocVehicleDamage::Operational: return 1.0;
        case EHavocVehicleDamage::Damaged: return 0.8;
        case EHavocVehicleDamage::Critical: return 0.5;
        default: return 0.0;
        }
    }

    void FollowInput(const FVector& SelfLocation, const FVector& SelfForward, const FVector& TargetLocation, double Gap, double& OutThrottle, double& OutSteer)
    {
        OutThrottle = 0.0;
        OutSteer = 0.0;
        FVector To = TargetLocation - SelfLocation;
        To.Z = 0.0;
        const double Dist = To.Size();
        FVector Fwd = SelfForward;
        Fwd.Z = 0.0;
        if (Dist <= KINDA_SMALL_NUMBER || !Fwd.Normalize())
        {
            return;
        }
        To /= Dist;
        const double Cross = Fwd.X * To.Y - Fwd.Y * To.X; // > 0: target is to the right (UE yaw turns right when positive)
        const double Dot = FVector::DotProduct(Fwd, To);
        const double Angle = FMath::Atan2(Cross, Dot);
        OutSteer = FMath::Clamp(Angle / (PI * 0.25), -1.0, 1.0);
        if (Dist > Gap)
        {
            const double Close = FMath::Clamp((Dist - Gap) / FMath::Max(Gap, 1.0), 0.0, 1.0);
            // Facing away: creep so wheeled vehicles can swing round; facing the target: speed up with distance.
            OutThrottle = Dot < 0.0 ? 0.3 : FMath::Max(0.3, Close);
        }
    }
}

AHavocVehicleBase::AHavocVehicleBase()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->SetBoxExtent(FVector(250.0, 120.0, 100.0));
    Collision->SetCollisionProfileName(TEXT("Pawn"));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
    SkeletalMesh->SetupAttachment(Collision);
    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Collision);
    CameraBoom->TargetArmLength = 900.f;
    CameraBoom->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
}

void AHavocVehicleBase::BeginPlay()
{
    Super::BeginPlay();

    if (const UGameInstance* GI = GetGameInstance())
    {
        if (const UHavocDataSubsystem* Data = GI->GetSubsystem<UHavocDataSubsystem>())
        {
            FHavocVehicleDef Def;
            if (Data->GetVehicle(VehicleId, Def))
            {
                ApplyDefinition(Def);
            }
            else
            {
                UE_LOG(LogHavocVehicle, Warning, TEXT("Vehicle '%s' is not in data/vehicles; using default stats."), *VehicleId.ToString());
            }
        }
    }
    LoadVisual();
}

void AHavocVehicleBase::ApplyDefinition(const FHavocVehicleDef& Def)
{
    Stats = Def.Stats;
    Seats = Def.Seats;
}

void AHavocVehicleBase::LoadVisual()
{
    if (AssetRole.IsNone())
    {
        return;
    }
    const UGameInstance* GI = GetGameInstance();
    const UHavocAssetSubsystem* Assets = GI ? GI->GetSubsystem<UHavocAssetSubsystem>() : nullptr;
    if (!Assets)
    {
        return;
    }
    // A missing asset is logged by the asset subsystem (LogHavocAssets) and the vehicle stays invisible but playable.
    UObject* Asset = Assets->LoadAsset(AssetRole);
    if (UStaticMesh* StaticAsset = Cast<UStaticMesh>(Asset))
    {
        Mesh->SetStaticMesh(StaticAsset);
    }
    else if (USkeletalMesh* SkeletalAsset = Cast<USkeletalMesh>(Asset))
    {
        SkeletalMesh->SetSkeletalMesh(SkeletalAsset);
    }
}

// ---------------------------------------------------------------- enter / exit

bool AHavocVehicleBase::CanEnter(const AController* InController) const
{
    if (!InController || Driver || IsDestroyed())
    {
        return false;
    }
    const APawn* Pawn = InController->GetPawn();
    if (!Pawn || Pawn == this)
    {
        return false;
    }
    return FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(EnterRange);
}

bool AHavocVehicleBase::Enter(AController* InController)
{
    if (!HasAuthority() || !CanEnter(InController))
    {
        return false;
    }
    APawn* Previous = InController->GetPawn();
    Driver = InController;
    PreviousPawn = Previous;
    Throttle = Steer = Lift = 0.0;

    // The on-foot pawn stays in the world but is hidden and inert while its player drives.
    Previous->SetActorHiddenInGame(true);
    Previous->SetActorEnableCollision(false);

    InController->Possess(this);
    return true;
}

bool AHavocVehicleBase::Exit()
{
    if (!HasAuthority() || !Driver)
    {
        return false;
    }
    AController* Leaving = Driver;
    APawn* Previous = PreviousPawn;
    Driver = nullptr;
    PreviousPawn = nullptr;
    Throttle = Steer = Lift = 0.0;
    Speed = 0.0;

    if (Previous)
    {
        Previous->SetActorLocation(GetActorTransform().TransformPosition(ExitOffset), false, nullptr, ETeleportType::TeleportPhysics);
        Previous->SetActorHiddenInGame(false);
        Previous->SetActorEnableCollision(true);
        Leaving->Possess(Previous);
    }
    else
    {
        Leaving->UnPossess();
    }
    return true;
}

// ---------------------------------------------------------------- damage

void AHavocVehicleBase::ApplyVehicleDamage(float Amount)
{
    if (!HasAuthority() || IsDestroyed() || Amount <= 0.f)
    {
        return;
    }
    Integrity = FMath::Max(0.f, FMath::RoundToFloat((Integrity - Amount) * 1000.f) / 1000.f);
    if (IsDestroyed())
    {
        Throttle = Steer = Lift = 0.0;
        Exit();
    }
    PublishDamage();
}

bool AHavocVehicleBase::RepairVehicle(float Amount)
{
    if (!HasAuthority() || IsDestroyed())
    {
        return false;
    }
    Integrity = FMath::Min(1.f, Integrity + FMath::Max(0.f, Amount));
    PublishDamage();
    return true;
}

void AHavocVehicleBase::PublishDamage() const
{
    if (ConvoyId.IsNone())
    {
        return;
    }
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            const UEnum* Enum = StaticEnum<EHavocVehicleDamage>();
            const FString Level = Enum ? Enum->GetNameStringByValue(static_cast<int64>(GetDamageLevel())) : FString();
            Events->Publish(FName(TEXT("convoy:vehicle")),
                FString::Printf(TEXT("{\"id\":\"%s\",\"integrity\":%.3f,\"level\":\"%s\"}"), *ConvoyId.ToString(), Integrity, *Level));
        }
    }
}

void AHavocVehicleBase::UnPossessed()
{
    Super::UnPossessed();
    if (Driver)
    {
        if (PreviousPawn)
        {
            PreviousPawn->SetActorHiddenInGame(false);
            PreviousPawn->SetActorEnableCollision(true);
        }
        Driver = nullptr;
        PreviousPawn = nullptr;
        Throttle = Steer = Lift = Speed = 0.0;
    }
}

AHavocVehicleBase* AHavocVehicleBase::FindEnterable(const AController* InController)
{
    const APawn* Pawn = InController ? InController->GetPawn() : nullptr;
    if (!Pawn || !Pawn->GetWorld())
    {
        return nullptr;
    }
    AHavocVehicleBase* Best = nullptr;
    double BestDist = TNumericLimits<double>::Max();
    for (TActorIterator<AHavocVehicleBase> It(Pawn->GetWorld()); It; ++It)
    {
        if (!It->CanEnter(InController))
        {
            continue;
        }
        const double D = FVector::DistSquared(Pawn->GetActorLocation(), It->GetActorLocation());
        if (D < BestDist)
        {
            BestDist = D;
            Best = *It;
        }
    }
    return Best;
}

// ---------------------------------------------------------------- movement

void AHavocVehicleBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority())
    {
        StepMovement(DeltaSeconds);
    }
}

void AHavocVehicleBase::MoveSwept(const FVector& Delta, const FQuat& NewRotation)
{
    FHitResult Hit;
    AddActorWorldOffset(Delta, true, &Hit);
    SetActorRotation(NewRotation);
    if (Hit.bBlockingHit)
    {
        Speed = 0.0; // stopped by geometry
    }
}

void AHavocVehicleBase::SetInput(double InThrottle, double InSteer, double InLift)
{
    Throttle = FMath::Clamp(InThrottle, -1.0, 1.0);
    Steer = FMath::Clamp(InSteer, -1.0, 1.0);
    Lift = FMath::Clamp(InLift, -1.0, 1.0);
}

void AHavocVehicleBase::ServerSetInput_Implementation(double InThrottle, double InSteer, double InLift)
{
    SetInput(InThrottle, InSteer, InLift);
}

// ---------------------------------------------------------------- input

void AHavocVehicleBase::PawnClientRestart()
{
    Super::PawnClientRestart();

    APlayerController* PC = Cast<APlayerController>(GetController());
    const UGameInstance* GI = GetGameInstance();
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    if (PC && Input)
    {
        Input->ApplyContexts(PC, MappingContext ? MappingContext.Get() : Input->VehicleContext.Get());
    }
}

void AHavocVehicleBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    const UGameInstance* GI = GetGameInstance();
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    if (!Enhanced || !Input)
    {
        return;
    }
    Enhanced->BindAction(Input->ThrottleAction, ETriggerEvent::Triggered, this, &AHavocVehicleBase::OnThrottle);
    Enhanced->BindAction(Input->ThrottleAction, ETriggerEvent::Completed, this, &AHavocVehicleBase::OnReleased);
    Enhanced->BindAction(Input->SteerAction, ETriggerEvent::Triggered, this, &AHavocVehicleBase::OnSteer);
    Enhanced->BindAction(Input->SteerAction, ETriggerEvent::Completed, this, &AHavocVehicleBase::OnReleased);
    Enhanced->BindAction(Input->RiseAction, ETriggerEvent::Triggered, this, &AHavocVehicleBase::OnLift);
    Enhanced->BindAction(Input->RiseAction, ETriggerEvent::Completed, this, &AHavocVehicleBase::OnReleased);
    Enhanced->BindAction(Input->LookAction, ETriggerEvent::Triggered, this, &AHavocVehicleBase::OnLook);
}

void AHavocVehicleBase::OnThrottle(const FInputActionValue& Value) { Throttle = FMath::Clamp(static_cast<double>(Value.Get<float>()), -1.0, 1.0); SendInput(); }
void AHavocVehicleBase::OnSteer(const FInputActionValue& Value) { Steer = FMath::Clamp(static_cast<double>(Value.Get<float>()), -1.0, 1.0); SendInput(); }
void AHavocVehicleBase::OnLift(const FInputActionValue& Value) { Lift = FMath::Clamp(static_cast<double>(Value.Get<float>()), -1.0, 1.0); SendInput(); }

void AHavocVehicleBase::OnLook(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>();
    AddControllerYawInput(Look.X);
    AddControllerPitchInput(Look.Y);
}

void AHavocVehicleBase::OnReleased()
{
    // Completed fires per action; zero everything, held actions re-trigger next frame.
    Throttle = Steer = Lift = 0.0;
    SendInput();
}

void AHavocVehicleBase::SendInput()
{
    if (!HasAuthority())
    {
        ServerSetInput(Throttle, Steer, Lift);
    }
}
