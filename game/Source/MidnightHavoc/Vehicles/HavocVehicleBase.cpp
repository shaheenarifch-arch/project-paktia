#include "Vehicles/HavocVehicleBase.h"

#include "Assets/HavocAssetSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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

bool AHavocVehicleBase::CanEnter(const AController* Controller) const
{
    if (!Controller || Driver)
    {
        return false;
    }
    const APawn* Pawn = Controller->GetPawn();
    if (!Pawn || Pawn == this)
    {
        return false;
    }
    return FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(EnterRange);
}

bool AHavocVehicleBase::Enter(AController* Controller)
{
    if (!HasAuthority() || !CanEnter(Controller))
    {
        return false;
    }
    APawn* Previous = Controller->GetPawn();
    Driver = Controller;
    PreviousPawn = Previous;
    Throttle = Steer = Lift = 0.0;

    // The on-foot pawn stays in the world but is hidden and inert while its player drives.
    Previous->SetActorHiddenInGame(true);
    Previous->SetActorEnableCollision(false);

    Controller->Possess(this);
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

AHavocVehicleBase* AHavocVehicleBase::FindEnterable(const AController* Controller)
{
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn || !Pawn->GetWorld())
    {
        return nullptr;
    }
    AHavocVehicleBase* Best = nullptr;
    double BestDist = TNumericLimits<double>::Max();
    for (TActorIterator<AHavocVehicleBase> It(Pawn->GetWorld()); It; ++It)
    {
        if (!It->CanEnter(Controller))
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
