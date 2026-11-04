#include "Aircraft/HavocFalconHelicopter.h"

#include "Components/StaticMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Missions/HavocMissionJson.h"
#include "Net/UnrealNetwork.h"

namespace
{
    constexpr double GravityAccel = 980.0; // cm/s^2

    FString EnumName(const UEnum* Enum, int64 Value)
    {
        return Enum ? Enum->GetNameStringByValue(Value) : FString();
    }
}

AHavocFalconHelicopter::AHavocFalconHelicopter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    Body->SetCollisionProfileName(TEXT("PhysicsActor"));

    AircraftId = FName(TEXT("falcon_helicopter"));
    Callsign = TEXT("FALCON-1");
}

void AHavocFalconHelicopter::BeginPlay()
{
    Super::BeginPlay();

    Body->SetLinearDamping(LinearDamping);
    Body->SetAngularDamping(AngularDamping);

    Helicopter.Id = FName(*Callsign.ToLower());
    Helicopter.Callsign = Callsign;

    // Seats come from data/aircraft/falcon-helicopter.json.
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (const UHavocDataSubsystem* Data = GI->GetSubsystem<UHavocDataSubsystem>())
        {
            FHavocAircraftDef Def;
            if (Data->GetAircraft(AircraftId, Def))
            {
                Seats = Def.Seats;
                Crash.Crew.AssignSeats(Seats);
            }
        }
    }
}

void AHavocFalconHelicopter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHavocFalconHelicopter, Helicopter);
    DOREPLIFETIME(AHavocFalconHelicopter, Crash);
}

// ---------------------------------------------------------------- input

void AHavocFalconHelicopter::PawnClientRestart()
{
    Super::PawnClientRestart();

    if (const APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (MappingContext)
                {
                    Subsystem->ClearAllMappings();
                    Subsystem->AddMappingContext(MappingContext, MappingPriority);
                }
            }
        }
    }
}

void AHavocFalconHelicopter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (CollectiveAction)
        {
            Input->BindAction(CollectiveAction, ETriggerEvent::Triggered, this, &AHavocFalconHelicopter::OnCollective);
            Input->BindAction(CollectiveAction, ETriggerEvent::Completed, this, &AHavocFalconHelicopter::OnInputReleased);
        }
        if (CyclicAction)
        {
            Input->BindAction(CyclicAction, ETriggerEvent::Triggered, this, &AHavocFalconHelicopter::OnCyclic);
            Input->BindAction(CyclicAction, ETriggerEvent::Completed, this, &AHavocFalconHelicopter::OnInputReleased);
        }
        if (YawAction)
        {
            Input->BindAction(YawAction, ETriggerEvent::Triggered, this, &AHavocFalconHelicopter::OnYaw);
            Input->BindAction(YawAction, ETriggerEvent::Completed, this, &AHavocFalconHelicopter::OnInputReleased);
        }
    }
}

void AHavocFalconHelicopter::OnCollective(const FInputActionValue& Value)
{
    CollectiveInput = FMath::Clamp(static_cast<double>(Value.Get<float>()), -1.0, 1.0);
    SendInput();
}

void AHavocFalconHelicopter::OnCyclic(const FInputActionValue& Value)
{
    CyclicInput = Value.Get<FVector2D>().ClampAxes(-1.0, 1.0);
    SendInput();
}

void AHavocFalconHelicopter::OnYaw(const FInputActionValue& Value)
{
    YawInput = FMath::Clamp(static_cast<double>(Value.Get<float>()), -1.0, 1.0);
    SendInput();
}

void AHavocFalconHelicopter::OnInputReleased()
{
    // Completed fires per action; zero everything that is no longer held. Held actions re-trigger next frame.
    CollectiveInput = 0.0;
    CyclicInput = FVector2D::ZeroVector;
    YawInput = 0.0;
    SendInput();
}

void AHavocFalconHelicopter::SendInput()
{
    if (!HasAuthority() && IsLocallyControlled())
    {
        ServerSetFlightInput(CollectiveInput, CyclicInput, YawInput);
    }
}

void AHavocFalconHelicopter::ServerSetFlightInput_Implementation(double InCollective, FVector2D InCyclic, double InYaw)
{
    CollectiveInput = FMath::Clamp(InCollective, -1.0, 1.0);
    CyclicInput = InCyclic.ClampAxes(-1.0, 1.0);
    YawInput = FMath::Clamp(InYaw, -1.0, 1.0);
}

// ---------------------------------------------------------------- flight

void AHavocFalconHelicopter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        ApplyFlightForces(DeltaSeconds);
    }
}

void AHavocFalconHelicopter::ApplyFlightForces(float DeltaSeconds)
{
    if (!Body || !Body->IsSimulatingPhysics())
    {
        return;
    }

    // A crashed or destroyed aircraft only falls.
    const EHavocHelicopterDamageLevel Level = Helicopter.Damage.GetLevel();
    if (Helicopter.FlightState == EHavocHelicopterFlightState::Crashed ||
        Helicopter.FlightState == EHavocHelicopterFlightState::Recovered ||
        Level == EHavocHelicopterDamageLevel::Destroyed)
    {
        return;
    }

    const double EngineFactor = Helicopter.Damage.EngineCondition;
    const double ControlFactor = Helicopter.Damage.FlightControl;

    // Collective: rate-limited level, lift = gravity * (level / hover level), scaled by engine condition.
    CollectiveLevel = FMath::Clamp(CollectiveLevel + CollectiveInput * CollectiveRate * DeltaSeconds, 0.0, 1.0);
    const FVector Up = GetActorUpVector();
    const double LiftAccel = GravityAccel * (CollectiveLevel / HoverCollective) * EngineFactor;
    // Divide by the tilt so the vertical component stays the commanded lift (clamped so a flipped aircraft cannot blow up).
    const double Tilt = FMath::Max(FVector::DotProduct(Up, FVector::UpVector), 0.5);
    Body->AddForce(Up * (LiftAccel / Tilt), NAME_None, /*bAccelChange*/ true);

    // Cyclic: command a target up-vector tilted toward the stick direction, torque the body toward it.
    // Axis = Up x Target rotates Up toward Target. Without input the target is straight up (self-levelling).
    const FVector Forward = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.0).GetSafeNormal();
    const FVector Right = FVector(GetActorRightVector().X, GetActorRightVector().Y, 0.0).GetSafeNormal();
    const double TiltAmount = FMath::Tan(FMath::DegreesToRadians(MaxTiltDegrees)) * ControlFactor;
    const FVector TargetUp = (FVector::UpVector + (Forward * CyclicInput.Y + Right * CyclicInput.X) * TiltAmount).GetSafeNormal();
    const FVector TiltAxis = FVector::CrossProduct(Up, TargetUp);
    Body->AddTorqueInRadians(TiltAxis * TiltResponse, NAME_None, /*bAccelChange*/ true);

    // Yaw: torque about world up (positive = clockwise seen from above).
    Body->AddTorqueInRadians(FVector::UpVector * (YawInput * YawAcceleration * ControlFactor), NAME_None, /*bAccelChange*/ true);
}

// ---------------------------------------------------------------- helpers

bool AHavocFalconHelicopter::CanMutate() const
{
    return HasAuthority();
}

bool AHavocFalconHelicopter::IsInRecoveryZone(const FVector& WorldPosition) const
{
    return Crash.RecoveryZone.Contains(WorldPosition, RecoveryRadius);
}

void AHavocFalconHelicopter::PublishEvent(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(FName(EventName), Payload);
        }
    }
}

void AHavocFalconHelicopter::NotifyDamage()
{
    const FHavocHelicopterDamageSnapshot Snapshot = Helicopter.Damage.Snapshot();

    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("aircraft"), Helicopter.Callsign);
    Json->SetStringField(TEXT("level"), EnumName(StaticEnum<EHavocHelicopterDamageLevel>(), static_cast<int64>(Snapshot.Level)));
    Json->SetNumberField(TEXT("flightControl"), Snapshot.FlightControl);
    Json->SetNumberField(TEXT("engineCondition"), Snapshot.EngineCondition);
    Json->SetNumberField(TEXT("structuralCondition"), Snapshot.StructuralCondition);
    PublishEvent(TEXT("helicopter:damage"), HavocMissionJson::ToString(Json));

    OnDamageChanged.Broadcast(Snapshot);
}

void AHavocFalconHelicopter::NotifyCrashState()
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("aircraft"), Helicopter.Callsign);
    Json->SetStringField(TEXT("sequence"), EnumName(StaticEnum<EHavocCrashSequenceState>(), static_cast<int64>(Crash.State)));
    Json->SetStringField(TEXT("flightState"), EnumName(StaticEnum<EHavocHelicopterFlightState>(), static_cast<int64>(Helicopter.FlightState)));
    PublishEvent(TEXT("helicopter:state"), HavocMissionJson::ToString(Json));

    OnCrashSequenceChanged.Broadcast(Crash.State);
}

void AHavocFalconHelicopter::OnRep_Helicopter()
{
    NotifyDamage();
}

void AHavocFalconHelicopter::OnRep_Crash()
{
    NotifyCrashState();
}

// ---------------------------------------------------------------- damage / flight state

void AHavocFalconHelicopter::ApplyDamage(double FlightControl, double Engine, double Structure)
{
    if (!CanMutate()) { return; }
    Helicopter.Damage.ApplyDamage(FlightControl, Engine, Structure);
    NotifyDamage();
}

void AHavocFalconHelicopter::ReportDistress()
{
    if (!CanMutate()) { return; }
    Helicopter.ReportDistress();
    NotifyCrashState();
}

void AHavocFalconHelicopter::AttemptEmergencyLanding(const FVector& WorldPosition)
{
    if (!CanMutate()) { return; }
    Helicopter.AttemptEmergencyLanding(WorldPosition);
    NotifyCrashState();
}

void AHavocFalconHelicopter::RecoverAircraft()
{
    if (!CanMutate()) { return; }
    Helicopter.RecoverAircraft();
    NotifyCrashState();
}

// ---------------------------------------------------------------- crash sequence

void AHavocFalconHelicopter::BeginInbound(const FVector& WorldPosition)
{
    if (!CanMutate()) { return; }
    Crash.BeginInbound(Helicopter, WorldPosition);
    NotifyCrashState();
}

void AHavocFalconHelicopter::TriggerDistress()
{
    if (!CanMutate()) { return; }
    Crash.TriggerDistress(Helicopter);
    NotifyDamage();
    NotifyCrashState();
}

void AHavocFalconHelicopter::LoseSignal()
{
    if (!CanMutate()) { return; }
    Crash.LoseSignal();
    NotifyCrashState();
}

void AHavocFalconHelicopter::RegisterCrash(const FVector& WorldPosition)
{
    if (!CanMutate()) { return; }
    Crash.RegisterCrash(Helicopter, WorldPosition);
    NotifyCrashState();
}

void AHavocFalconHelicopter::BeginSearch()
{
    if (!CanMutate()) { return; }
    Crash.BeginSearch();
    NotifyCrashState();
}

void AHavocFalconHelicopter::LocateCrashSite(const FVector& WorldPosition)
{
    if (!CanMutate()) { return; }
    Crash.LocateCrashSite(WorldPosition);
    NotifyCrashState();
}

void AHavocFalconHelicopter::SecureCrashSite()
{
    if (!CanMutate()) { return; }
    Crash.SecureCrashSite();
    NotifyCrashState();
}

void AHavocFalconHelicopter::BeginCrewRecovery()
{
    if (!CanMutate()) { return; }
    Crash.BeginCrewRecovery();
    NotifyCrashState();
}

void AHavocFalconHelicopter::StartRecoveryDefense()
{
    if (!CanMutate()) { return; }
    Crash.StartRecoveryDefense();
    NotifyCrashState();
}

bool AHavocFalconHelicopter::CompleteEvacuation()
{
    if (!CanMutate()) { return false; }
    const bool bDone = Crash.CompleteEvacuation();
    if (bDone)
    {
        NotifyCrashState();
    }
    return bDone;
}

void AHavocFalconHelicopter::Abandon()
{
    if (!CanMutate()) { return; }
    Crash.Abandon();
    NotifyCrashState();
}

bool AHavocFalconHelicopter::LocateCrew(FName CrewId, EHavocCrewCondition Condition)
{
    if (!CanMutate()) { return false; }
    return Crash.Crew.Locate(CrewId, Condition);
}

bool AHavocFalconHelicopter::RecoverCrew(FName CrewId)
{
    if (!CanMutate()) { return false; }
    return Crash.Crew.Recover(CrewId);
}
