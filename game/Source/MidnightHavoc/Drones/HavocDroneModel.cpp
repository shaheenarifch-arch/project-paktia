#include "Drones/HavocDroneModel.h"

// ---------------------------------------------------------------- definitions

FHavocDroneDefinition FHavocDroneDefinition::MakeRaven(const FString& InId)
{
    FHavocDroneDefinition D;
    D.Id = InId;
    D.DisplayName = TEXT("Raven-1");
    D.Role = EHavocDroneRole::Recon;
    D.MaxSpeed = 28.0;
    D.SensorRange = 450.0;
    D.BatteryDrainPerSecond = 0.002;
    D.SignalRange = 1600.0;
    D.AvailableCommands = { TEXT("LAUNCH"), TEXT("MOVE"), TEXT("OBSERVE"), TEXT("TRACK"), TEXT("MARK"), TEXT("RETURN") };
    D.FallbackBehavior = EHavocLostLinkBehavior::ReturnHome;
    return D;
}

FHavocDroneDefinition FHavocDroneDefinition::MakeWarden(const FString& InId)
{
    FHavocDroneDefinition D;
    D.Id = InId;
    D.DisplayName = TEXT("Warden-1");
    D.Role = EHavocDroneRole::Surveillance;
    D.MaxSpeed = 22.0;
    D.SensorRange = 600.0;
    D.BatteryDrainPerSecond = 0.0015;
    D.SignalRange = 2200.0;
    D.AvailableCommands = { TEXT("LAUNCH"), TEXT("MOVE"), TEXT("ORBIT"), TEXT("OBSERVE"), TEXT("TRACK"), TEXT("RETURN") };
    D.FallbackBehavior = EHavocLostLinkBehavior::HoldPosition;
    return D;
}

FHavocDroneDefinition FHavocDroneDefinition::MakeTalon(const FString& InId)
{
    FHavocDroneDefinition D;
    D.Id = InId;
    D.DisplayName = TEXT("Talon-1");
    D.Role = EHavocDroneRole::PrecisionSupport;
    D.MaxSpeed = 34.0;
    D.SensorRange = 300.0;
    D.BatteryDrainPerSecond = 0.003;
    D.SignalRange = 1800.0;
    D.AvailableCommands = { TEXT("DEPLOY"), TEXT("ABORT"), TEXT("RETURN") };
    D.FallbackBehavior = EHavocLostLinkBehavior::MissionDefined;
    return D;
}

// ---------------------------------------------------------------- battery

double FHavocDroneBattery::Consume(double Amount)
{
    Charge = FMath::Clamp(Charge - FMath::Max(0.0, Amount), 0.0, 1.0);
    return Charge;
}

double FHavocDroneBattery::Recharge(double Amount)
{
    Charge = FMath::Clamp(Charge + FMath::Max(0.0, Amount), 0.0, 1.0);
    return Charge;
}

// ---------------------------------------------------------------- signal / flight / sensor

double FHavocDroneSignalManager::Calculate(const FVector& DronePosition, const FVector& RelayPosition, double MaxRange, double Interference)
{
    const double RangeFactor = 1.0 - FVector::Dist(DronePosition, RelayPosition) / FMath::Max(MaxRange, 1.0);
    return FMath::Clamp(RangeFactor - FMath::Clamp(Interference, 0.0, 1.0), 0.0, 1.0);
}

EHavocSignalState FHavocDroneSignalManager::Classify(double Signal)
{
    if (Signal <= 0.0) { return EHavocSignalState::Lost; }
    if (Signal < 0.15) { return EHavocSignalState::Critical; }
    if (Signal < 0.4) { return EHavocSignalState::Degraded; }
    if (Signal < 0.75) { return EHavocSignalState::Stable; }
    return EHavocSignalState::Strong;
}

FVector FHavocDroneFlightController::Step(const FVector& Current, const FVector& Target, double Speed, double DeltaSeconds)
{
    const double Remaining = FVector::Dist(Current, Target);
    if (Remaining == 0.0 || Remaining <= Speed * DeltaSeconds)
    {
        return Target;
    }
    const double Ratio = (Speed * DeltaSeconds) / Remaining;
    return Current + (Target - Current) * Ratio;
}

TArray<FHavocSensorCandidate> FHavocDroneSensor::Scan(const FVector& Origin, double Range, const TArray<FHavocSensorCandidate>& Candidates)
{
    TArray<FHavocSensorCandidate> Out;
    for (const FHavocSensorCandidate& C : Candidates)
    {
        if (C.bVisible && FVector::Dist(Origin, C.Position) <= Range)
        {
            Out.Add(C);
        }
    }
    return Out;
}

// ---------------------------------------------------------------- core (DroneBase.ts)

bool FHavocDroneCore::IsControllable() const
{
    switch (State)
    {
    case EHavocDroneState::Stowed:
    case EHavocDroneState::Recovered:
    case EHavocDroneState::Disabled:
    case EHavocDroneState::Destroyed:
    case EHavocDroneState::SignalLost:
        return false;
    default:
        return true;
    }
}

bool FHavocDroneCore::IsAirborne() const
{
    return State != EHavocDroneState::Stowed && State != EHavocDroneState::Recovered && State != EHavocDroneState::Destroyed;
}

bool FHavocDroneCore::Launch(const FVector& HomePosition)
{
    if (State != EHavocDroneState::Stowed && State != EHavocDroneState::Recovered)
    {
        return false;
    }
    Home = HomePosition;
    Position = HomePosition;
    State = EHavocDroneState::Launching;
    return true;
}

void FHavocDroneCore::Activate()
{
    if (State == EHavocDroneState::Launching)
    {
        State = EHavocDroneState::Active;
    }
}

bool FHavocDroneCore::MoveTo(const FVector& InTarget, const FString& InTask)
{
    if (!IsControllable())
    {
        return false;
    }
    Target = InTarget;
    Task = InTask;
    State = EHavocDroneState::Moving;
    return true;
}

bool FHavocDroneCore::ReturnHome()
{
    switch (State)
    {
    case EHavocDroneState::Destroyed:
    case EHavocDroneState::Disabled:
    case EHavocDroneState::Stowed:
    case EHavocDroneState::Recovered:
        return false;
    default:
        break;
    }
    Task = TEXT("RETURN_HOME");
    State = EHavocDroneState::Returning;
    return true;
}

void FHavocDroneCore::Recover()
{
    if (State != EHavocDroneState::Returning)
    {
        return;
    }
    Position = Home;
    Task.Empty();
    State = EHavocDroneState::Recovered;
}

void FHavocDroneCore::ApplyDamage(double Amount)
{
    if (State == EHavocDroneState::Destroyed)
    {
        return;
    }
    if (Amount >= 1.0)
    {
        State = EHavocDroneState::Destroyed;
    }
    else if (Amount >= 0.5)
    {
        State = EHavocDroneState::Disabled;
    }
}

void FHavocDroneCore::Update(double DeltaSeconds, double SignalStrength)
{
    if (!IsAirborne())
    {
        return;
    }
    Battery.Consume(Definition.BatteryDrainPerSecond * DeltaSeconds);
    Signal = FMath::Clamp(SignalStrength, 0.0, 1.0);

    if (Signal <= 0.0)
    {
        HandleSignalLost();
    }
    else if (Signal < 0.3 && State != EHavocDroneState::Returning && State != EHavocDroneState::Destroyed)
    {
        State = EHavocDroneState::SignalDegraded;
    }

    if (Battery.Charge <= 0.1 && IsControllable())
    {
        ReturnHome();
    }
}

void FHavocDroneCore::HandleSignalLost()
{
    State = EHavocDroneState::SignalLost;
    switch (Definition.FallbackBehavior)
    {
    case EHavocLostLinkBehavior::ReturnHome:
        State = EHavocDroneState::Returning;
        Task = TEXT("LOST_LINK_RETURN");
        break;
    case EHavocLostLinkBehavior::Land:
        State = EHavocDroneState::Disabled;
        Task = TEXT("LAND");
        break;
    case EHavocLostLinkBehavior::HoldPosition:
        Task = TEXT("HOLD");
        break;
    case EHavocLostLinkBehavior::MissionDefined:
        Task = TEXT("AWAIT_MISSION_RULE");
        break;
    }
}

FHavocDroneTelemetry FHavocDroneCore::Telemetry() const
{
    FHavocDroneTelemetry T;
    T.Id = Definition.Id;
    T.DisplayName = Definition.DisplayName;
    T.Role = Definition.Role;
    T.State = State;
    T.Position = Position;
    T.Battery = Battery.Charge;
    T.Signal = Signal;
    T.Task = Task;
    return T;
}
