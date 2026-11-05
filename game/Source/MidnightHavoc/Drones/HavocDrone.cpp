#include "Drones/HavocDrone.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Drones/HavocDroneDetectable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/HavocPlayInput.h"
#include "EngineUtils.h"
#include "Missions/HavocMissionJson.h"
#include "Net/UnrealNetwork.h"

namespace
{
    constexpr double UnitsPerMeter = 100.0;

    FVector ToMeters(const FVector& Uu) { return Uu / UnitsPerMeter; }
    FVector ToUnits(const FVector& M) { return M * UnitsPerMeter; }
}

AHavocDrone::AHavocDrone()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(40.f);
    Collision->SetCollisionProfileName(TEXT("Pawn"));
    SetRootComponent(Collision);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Collision);
    Camera->SetAutoActivate(false);
    // Recon view: look down and ahead. Piloting does not change the pitch.
    Camera->SetRelativeRotation(FRotator(-40.f, 0.f, 0.f));
}

void AHavocDrone::SetDefinition(const FHavocDroneDefinition& InDefinition)
{
    Core.Definition = InDefinition;
}

void AHavocDrone::BeginPlay()
{
    Super::BeginPlay();
    LastKnownState = Core.State;
    ApplyCamera();
}

void AHavocDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHavocDrone, Core);
    DOREPLIFETIME(AHavocDrone, CameraState);
}

FHavocDroneTelemetry AHavocDrone::GetTelemetry() const
{
    return Core.Telemetry();
}

// ---------------------------------------------------------------- commands

bool AHavocDrone::Launch(const FVector& HomeLocation)
{
    if (!HasAuthority()) { return false; }
    const EHavocDroneState Old = Core.State;
    const bool bLaunched = Core.Launch(ToMeters(HomeLocation));
    if (bLaunched)
    {
        Core.Activate();
        DetectedIds.Reset();
        SetActorLocation(HomeLocation);
        NotifyStateChange(Old);
    }
    return bLaunched;
}

bool AHavocDrone::MoveTo(const FVector& WorldLocation, const FString& Task)
{
    if (!HasAuthority()) { return false; }
    const EHavocDroneState Old = Core.State;
    const bool bOk = Core.MoveTo(ToMeters(WorldLocation), Task);
    NotifyStateChange(Old);
    return bOk;
}

bool AHavocDrone::ReturnHome()
{
    if (!HasAuthority()) { return false; }
    const EHavocDroneState Old = Core.State;
    const bool bOk = Core.ReturnHome();
    NotifyStateChange(Old);
    return bOk;
}

void AHavocDrone::Recover()
{
    if (!HasAuthority()) { return; }
    const EHavocDroneState Old = Core.State;
    Core.Recover();
    if (Core.State == EHavocDroneState::Recovered)
    {
        SetActorLocation(ToUnits(Core.Position));
    }
    NotifyStateChange(Old);
}

void AHavocDrone::ApplyDroneDamage(double Amount)
{
    if (!HasAuthority()) { return; }
    const EHavocDroneState Old = Core.State;
    Core.ApplyDamage(Amount);
    NotifyStateChange(Old);
}

void AHavocDrone::SetCameraActive(bool bActive)
{
    if (!HasAuthority()) { return; }
    CameraState.SetActive(bActive);
    ApplyCamera();
}

void AHavocDrone::SetCameraZoom(double NormalizedZoom)
{
    if (!HasAuthority()) { return; }
    CameraState.SetZoom(NormalizedZoom);
    ApplyCamera();
}

void AHavocDrone::ApplyCamera()
{
    if (!Camera) { return; }
    Camera->SetActive(CameraState.bActive);
    Camera->SetFieldOfView(FMath::Lerp(WideFov, ZoomedFov, static_cast<float>(CameraState.Zoom)));
}

// ---------------------------------------------------------------- tick

void AHavocDrone::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        ApplyPilotInput();
        UpdateServer(DeltaSeconds);
    }
}

// ---------------------------------------------------------------- piloting (playable loop)

void AHavocDrone::PawnClientRestart()
{
    Super::PawnClientRestart();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (const UGameInstance* GI = GetGameInstance())
        {
            if (const UHavocPlayInput* Input = GI->GetSubsystem<UHavocPlayInput>())
            {
                Input->ApplyContexts(PC, Input->DroneContext, 1);
            }
        }
    }
}

void AHavocDrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    const UGameInstance* GI = GetGameInstance();
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    if (!Enhanced || !Input)
    {
        return;
    }
    Enhanced->BindAction(Input->MoveAction, ETriggerEvent::Triggered, this, &AHavocDrone::OnPilotMove);
    Enhanced->BindAction(Input->MoveAction, ETriggerEvent::Completed, this, &AHavocDrone::OnPilotReleased);
    Enhanced->BindAction(Input->RiseAction, ETriggerEvent::Triggered, this, &AHavocDrone::OnPilotRise);
    Enhanced->BindAction(Input->RiseAction, ETriggerEvent::Completed, this, &AHavocDrone::OnPilotReleased);
    Enhanced->BindAction(Input->LookAction, ETriggerEvent::Triggered, this, &AHavocDrone::OnPilotLook);
}

void AHavocDrone::OnPilotMove(const FInputActionValue& Value)
{
    PilotMove = Value.Get<FVector2D>();
    SendPilotInput();
}

void AHavocDrone::OnPilotRise(const FInputActionValue& Value)
{
    PilotRise = Value.Get<float>();
    SendPilotInput();
}

void AHavocDrone::OnPilotReleased()
{
    // Completed fires for whichever action was released; the held state is re-read next Triggered.
    PilotMove = FVector2D::ZeroVector;
    PilotRise = 0.0;
    SendPilotInput();
}

void AHavocDrone::OnPilotLook(const FInputActionValue& Value)
{
    // Yaw is applied locally and replicated by movement; the camera pitch stays fixed.
    AddActorWorldRotation(FRotator(0.f, Value.Get<FVector2D>().X * PilotYawRate, 0.f));
}

void AHavocDrone::SendPilotInput()
{
    if (HasAuthority())
    {
        ServerSetPilotInput_Implementation(PilotMove, PilotRise);
    }
    else
    {
        ServerSetPilotInput(PilotMove, PilotRise);
    }
}

void AHavocDrone::ServerSetPilotInput_Implementation(FVector2D InMove, double InRise)
{
    PilotMove = InMove;
    PilotRise = InRise;
}

void AHavocDrone::ApplyPilotInput()
{
    // Returning (low battery or recalled) is not overridden by stick input.
    if (!Core.IsControllable() || Core.State == EHavocDroneState::Returning)
    {
        return;
    }
    const FVector Input = GetActorForwardVector() * PilotMove.Y + GetActorRightVector() * PilotMove.X + FVector::UpVector * PilotRise;
    if (!Input.IsNearlyZero(0.01))
    {
        MoveTo(GetActorLocation() + Input.GetSafeNormal() * PilotLeadCm, TEXT("PILOT"));
        bPilotWasSteering = true;
    }
    else if (bPilotWasSteering)
    {
        // Released: hold position.
        MoveTo(GetActorLocation(), TEXT("HOLD"));
        bPilotWasSteering = false;
    }
}

double AHavocDrone::ComputeSignal() const
{
    const FVector RelayMeters = RelayActor ? ToMeters(RelayActor->GetActorLocation()) : Core.Home;
    return FHavocDroneSignalManager::Calculate(Core.Position, RelayMeters, Core.Definition.SignalRange, Interference);
}

void AHavocDrone::UpdateServer(float DeltaSeconds)
{
    if (!Core.IsAirborne())
    {
        return;
    }

    const EHavocDroneState Old = Core.State;

    // Same order as the TS update: battery, signal and fallback rules, then low-battery return.
    Core.Update(DeltaSeconds, ComputeSignal());
    FlyToward(DeltaSeconds);

    if (Core.IsControllable())
    {
        SensorTimer += DeltaSeconds;
        if (SensorTimer >= SensorInterval)
        {
            SensorTimer = 0.f;
            ScanForDetectables();
        }
    }

    NotifyStateChange(Old);
    PublishTelemetryChanges();
}

void AHavocDrone::FlyToward(float DeltaSeconds)
{
    FVector Destination;
    if (Core.State == EHavocDroneState::Moving)
    {
        Destination = Core.Target;
    }
    else if (Core.State == EHavocDroneState::Returning)
    {
        Destination = Core.Home;
    }
    else
    {
        return; // holding, degraded, lost, disabled: no commanded motion
    }

    const FVector Previous = Core.Position;
    Core.Position = FHavocDroneFlightController::Step(Core.Position, Destination, Core.Definition.MaxSpeed, DeltaSeconds);

    const FVector Delta = Core.Position - Previous;
    if (!Delta.IsNearlyZero())
    {
        SetActorLocation(ToUnits(Core.Position), /*bSweep*/ true);
        const FVector Flat(Delta.X, Delta.Y, 0.0);
        if (!Flat.IsNearlyZero())
        {
            SetActorRotation(Flat.Rotation());
        }
    }

    // Arrival home completes a return (the TS leaves recover() to the caller; a pawn has no caller).
    if (Core.State == EHavocDroneState::Returning && Core.Position.Equals(Core.Home, 0.01))
    {
        Core.Recover();
    }
}

// ---------------------------------------------------------------- sensor

void AHavocDrone::ScanForDetectables()
{
    UWorld* World = GetWorld();
    if (!World) { return; }

    const FVector Origin = Core.Position;
    TArray<FHavocSensorCandidate> Candidates;
    TMap<FName, TWeakObjectPtr<AActor>> Owners;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        const IHavocDroneDetectable* Detectable = Cast<IHavocDroneDetectable>(Actor);
        if (!Detectable) { continue; }

        FHavocSensorCandidate C;
        C.Id = Detectable->GetDetectableId();
        C.Position = ToMeters(Actor->GetActorLocation());
        C.Classification = Detectable->GetDetectableClassification();
        C.bVisible = Detectable->IsDetectableVisible();
        Candidates.Add(C);
        Owners.Add(C.Id, Actor);
    }

    const FName DroneName(*Core.Definition.Id);
    for (const FHavocSensorCandidate& Hit : FHavocDroneSensor::Scan(Origin, Core.Definition.SensorRange, Candidates))
    {
        bool bAlreadyInSet = false;
        DetectedIds.Add(Hit.Id, &bAlreadyInSet);
        if (bAlreadyInSet) { continue; }

        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("droneId"), Core.Definition.Id);
        Json->SetStringField(TEXT("hazardId"), Hit.Id.ToString());
        Json->SetStringField(TEXT("classification"), Hit.Classification);
        PublishEvent(TEXT("drone:detected"), HavocMissionJson::ToString(Json));

        if (AActor* Owner = Owners.FindRef(Hit.Id).Get())
        {
            if (IHavocDroneDetectable* Detectable = Cast<IHavocDroneDetectable>(Owner))
            {
                Detectable->OnDetectedByDrone(DroneName);
            }
        }
    }
}

// ---------------------------------------------------------------- events

void AHavocDrone::PublishEvent(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(FName(EventName), Payload);
        }
    }
}

void AHavocDrone::NotifyStateChange(EHavocDroneState OldState)
{
    if (OldState != Core.State)
    {
        OnDroneStateChanged.Broadcast(OldState, Core.State);
    }
    LastKnownState = Core.State;
}

void AHavocDrone::PublishTelemetryChanges()
{
    const int32 BatteryPercent = FMath::RoundToInt(Core.Battery.Charge * 100.0);
    if (BatteryPercent != LastBatteryPercent)
    {
        LastBatteryPercent = BatteryPercent;
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("droneId"), Core.Definition.Id);
        Json->SetNumberField(TEXT("battery"), Core.Battery.Charge);
        Json->SetBoolField(TEXT("low"), Core.Battery.IsLow());
        Json->SetBoolField(TEXT("critical"), Core.Battery.IsCritical());
        PublishEvent(TEXT("drone:battery"), HavocMissionJson::ToString(Json));
    }

    const int32 SignalPercent = FMath::RoundToInt(Core.Signal * 100.0);
    if (SignalPercent != LastSignalPercent)
    {
        LastSignalPercent = SignalPercent;
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("droneId"), Core.Definition.Id);
        Json->SetNumberField(TEXT("signal"), Core.Signal);
        PublishEvent(TEXT("drone:signal"), HavocMissionJson::ToString(Json));
    }
}

void AHavocDrone::OnRep_Core()
{
    NotifyStateChange(LastKnownState);
    PublishTelemetryChanges();
}

// ---------------------------------------------------------------- subclasses

AHavocRavenDrone::AHavocRavenDrone()
{
    SetDefinition(FHavocDroneDefinition::MakeRaven());
}

AHavocWardenDrone::AHavocWardenDrone()
{
    SetDefinition(FHavocDroneDefinition::MakeWarden());
}

bool AHavocWardenDrone::SurveilArea(const FVector& WorldCenter)
{
    return MoveTo(WorldCenter, TEXT("SURVEIL_AREA"));
}

AHavocTalonDrone::AHavocTalonDrone()
{
    SetDefinition(FHavocDroneDefinition::MakeTalon());
}

void AHavocTalonDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHavocTalonDrone, bMissionAuthorized);
}

void AHavocTalonDrone::AuthorizeMission()
{
    if (HasAuthority())
    {
        bMissionAuthorized = true;
    }
}

void AHavocTalonDrone::AbortMission()
{
    if (HasAuthority())
    {
        bMissionAuthorized = false;
        ReturnHome();
    }
}
