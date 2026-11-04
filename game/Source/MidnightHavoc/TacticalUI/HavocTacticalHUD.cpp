#include "TacticalUI/HavocTacticalHUD.h"

#include "Core/HavocEventSubsystem.h"
#include "Drones/HavocDroneModel.h"
#include "Aircraft/HavocHelicopterModel.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionSubsystem.h"

namespace
{
    const TCHAR* const HudEvents[] = {
        TEXT("mission:state"),
        TEXT("mission:blocked"),
        TEXT("mission:checkpoint"),
        TEXT("mission:complete"),
        TEXT("drone:battery"),
        TEXT("drone:signal"),
        TEXT("helicopter:damage"),
    };

    UHavocEventSubsystem* GetEvents(const UUserWidget* Widget)
    {
        const UWorld* World = Widget->GetWorld();
        const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
        return GI ? GI->GetSubsystem<UHavocEventSubsystem>() : nullptr;
    }

    double NumberOr(const FJsonObject& Json, const TCHAR* Field, double Default)
    {
        double Value = Default;
        Json.TryGetNumberField(Field, Value);
        return Value;
    }
}

void UHavocTacticalHUD::NativeConstruct()
{
    Super::NativeConstruct();

    if (UHavocEventSubsystem* Events = GetEvents(this))
    {
        for (const TCHAR* Name : HudEvents)
        {
            Events->Subscribe(FName(Name), this, GET_FUNCTION_NAME_CHECKED(UHavocTacticalHUD, HandleHavocEvent));
        }
    }
    RefreshFromMission();
}

void UHavocTacticalHUD::NativeDestruct()
{
    if (UHavocEventSubsystem* Events = GetEvents(this))
    {
        for (const TCHAR* Name : HudEvents)
        {
            Events->Unsubscribe(FName(Name), this, GET_FUNCTION_NAME_CHECKED(UHavocTacticalHUD, HandleHavocEvent));
        }
    }
    Super::NativeDestruct();
}

UHavocMissionSubsystem* UHavocTacticalHUD::GetMissionSubsystem() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
}

void UHavocTacticalHUD::RefreshFromMission()
{
    const UHavocMissionSubsystem* Missions = GetMissionSubsystem();
    if (!Missions)
    {
        return;
    }
    const FString Objective = Missions->GetObjective();
    if (!Objective.IsEmpty())
    {
        OnObjectiveChanged(Objective, FString());
    }
    const TArray<FString> Choices = Missions->GetChoices();
    if (Choices.Num() > 0)
    {
        OnChoicesAvailable(Choices);
    }
}

void UHavocTacticalHUD::HandleHavocEvent(FName EventName, const FString& Payload)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Payload);
    const FString Name = EventName.ToString();

    if (Name == TEXT("mission:state"))
    {
        FString StateId;
        Json->TryGetStringField(TEXT("to"), StateId);
        const UHavocMissionSubsystem* Missions = GetMissionSubsystem();
        OnObjectiveChanged(Missions ? Missions->GetObjective() : FString(), StateId);
        if (Missions)
        {
            const TArray<FString> Choices = Missions->GetChoices();
            if (Choices.Num() > 0)
            {
                OnChoicesAvailable(Choices);
            }
        }
    }
    else if (Name == TEXT("mission:blocked"))
    {
        FString StateId;
        TArray<FString> Missing;
        Json->TryGetStringField(TEXT("state"), StateId);
        Json->TryGetStringArrayField(TEXT("missing"), Missing);
        OnMissionBlocked(StateId, Missing);
    }
    else if (Name == TEXT("mission:checkpoint"))
    {
        FString StateId;
        const TSharedPtr<FJsonObject>* Checkpoint = nullptr;
        if (Json->TryGetObjectField(TEXT("checkpoint"), Checkpoint) && Checkpoint)
        {
            (*Checkpoint)->TryGetStringField(TEXT("state"), StateId);
        }
        OnCheckpoint(StateId);
    }
    else if (Name == TEXT("mission:complete"))
    {
        FString StateId, Outcome;
        Json->TryGetStringField(TEXT("state"), StateId);
        Json->TryGetStringField(TEXT("outcome"), Outcome);
        OnMissionComplete(Outcome, StateId);
    }
    else if (Name == TEXT("drone:battery"))
    {
        FString DroneId;
        bool bLow = false, bCritical = false;
        Json->TryGetStringField(TEXT("droneId"), DroneId);
        Json->TryGetBoolField(TEXT("low"), bLow);
        Json->TryGetBoolField(TEXT("critical"), bCritical);
        OnDroneBattery(DroneId, NumberOr(*Json, TEXT("battery"), 0.0), bLow, bCritical);
    }
    else if (Name == TEXT("drone:signal"))
    {
        FString DroneId;
        Json->TryGetStringField(TEXT("droneId"), DroneId);
        const double Signal = NumberOr(*Json, TEXT("signal"), 0.0);
        OnDroneSignal(DroneId, Signal, FHavocDroneSignalManager::Classify(Signal));
    }
    else if (Name == TEXT("helicopter:damage"))
    {
        FString Callsign;
        Json->TryGetStringField(TEXT("aircraft"), Callsign);
        FHavocHelicopterDamageState Damage;
        Damage.FlightControl = NumberOr(*Json, TEXT("flightControl"), 1.0);
        Damage.EngineCondition = NumberOr(*Json, TEXT("engineCondition"), 1.0);
        Damage.StructuralCondition = NumberOr(*Json, TEXT("structuralCondition"), 1.0);
        const FHavocHelicopterDamageSnapshot Snapshot = Damage.Snapshot();
        OnHelicopterDamage(Callsign, Snapshot.Level, Snapshot);
    }
}

// ---------------------------------------------------------------- Episode 04 getters

bool UHavocTacticalHUD::HasEpisode04Data() const
{
    const UHavocMissionSubsystem* Missions = GetMissionSubsystem();
    return Missions && Missions->GetOpenStrait() != nullptr;
}

int32 UHavocTacticalHUD::GetReinforcements() const
{
    const UHavocMissionSubsystem* Missions = GetMissionSubsystem();
    const UHavocOpenStraitMission* Mission = Missions ? Missions->GetOpenStrait() : nullptr;
    return Mission ? Mission->GetReinforcements() : 0;
}

double UHavocTacticalHUD::GetTerminalIntegrity() const
{
    const UHavocMissionSubsystem* Missions = GetMissionSubsystem();
    const UHavocOpenStraitMission* Mission = Missions ? Missions->GetOpenStrait() : nullptr;
    return Mission ? Mission->GetResult().TerminalIntegrity : 0.0;
}

double UHavocTacticalHUD::GetSupplyFlow() const
{
    const UHavocMissionSubsystem* Missions = GetMissionSubsystem();
    const UHavocOpenStraitMission* Mission = Missions ? Missions->GetOpenStrait() : nullptr;
    return Mission ? Mission->GetResult().SupplyFlow : 0.0;
}
