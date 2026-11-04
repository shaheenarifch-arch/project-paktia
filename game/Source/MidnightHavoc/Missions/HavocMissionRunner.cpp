#include "Missions/HavocMissionRunner.h"

#include "Core/HavocEventSubsystem.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionSystems.h"

DEFINE_LOG_CATEGORY(LogHavocMission);

bool UHavocMissionRunner::Initialize(const FHavocMissionDef& InDefinition, const TMap<FString, FHavocEffectHandler>& InEffects,
    UHavocEventSubsystem* Events, UObject* SystemsProvider, FString& OutError)
{
    // Build into locals first so a failed Initialize leaves the runner untouched.
    TMap<FString, int32> NewIndex;
    for (int32 i = 0; i < InDefinition.States.Num(); ++i)
    {
        NewIndex.Add(InDefinition.States[i].Id, i);
    }
    for (const FHavocMissionStateDef& State : InDefinition.States)
    {
        for (const FString& Effect : State.Effects)
        {
            if (!InEffects.Contains(Effect))
            {
                OutError = FString::Printf(TEXT("Mission %s has no handler for effect %s"), *InDefinition.Id, *Effect);
                UE_LOG(LogHavocMission, Error, TEXT("%s"), *OutError);
                return false;
            }
        }
    }
    const int32* StartIndex = NewIndex.Find(InDefinition.Start);
    if (!StartIndex)
    {
        OutError = FString::Printf(TEXT("Mission %s has no state %s"), *InDefinition.Id, *InDefinition.Start);
        UE_LOG(LogHavocMission, Error, TEXT("%s"), *OutError);
        return false;
    }
    if (SystemsProvider && !Cast<IHavocMissionSystems>(SystemsProvider))
    {
        OutError = FString::Printf(TEXT("Mission %s: systems provider does not implement IHavocMissionSystems"), *InDefinition.Id);
        UE_LOG(LogHavocMission, Error, TEXT("%s"), *OutError);
        return false;
    }

    Definition = InDefinition;
    Effects = InEffects;
    StateIndex = MoveTemp(NewIndex);
    SystemsObject = SystemsProvider;
    EventSubsystem = Events;
    Flags.Reset();
    Trail.Reset();
    Outcome.Reset();
    bHasCheckpoint = false;

    CurrentIndex = *StartIndex;
    Enter(Definition.States[CurrentIndex], FString());
    return true;
}

FString UHavocMissionRunner::GetState() const
{
    return Definition.States.IsValidIndex(CurrentIndex) ? Definition.States[CurrentIndex].Id : FString();
}

FString UHavocMissionRunner::GetObjective() const
{
    return Definition.States.IsValidIndex(CurrentIndex) ? Definition.States[CurrentIndex].Objective : FString();
}

bool UHavocMissionRunner::IsComplete() const
{
    return Definition.States.IsValidIndex(CurrentIndex) && Definition.States[CurrentIndex].bTerminal;
}

TArray<FString> UHavocMissionRunner::GetChoices() const
{
    TArray<FString> Out;
    if (Definition.States.IsValidIndex(CurrentIndex))
    {
        Definition.States[CurrentIndex].Branches.GenerateKeyArray(Out);
    }
    return Out;
}

bool UHavocMissionRunner::HasBranch(const FString& Branch) const
{
    return Definition.States.IsValidIndex(CurrentIndex) && Definition.States[CurrentIndex].Branches.Contains(Branch);
}

void UHavocMissionRunner::SetFlag(const FString& Flag)
{
    Flags.AddUnique(Flag);
}

void UHavocMissionRunner::ClearFlag(const FString& Flag)
{
    Flags.Remove(Flag);
}

TArray<FString> UHavocMissionRunner::Missing() const
{
    TArray<FString> Out;
    if (Definition.States.IsValidIndex(CurrentIndex))
    {
        for (const FString& Flag : Definition.States[CurrentIndex].Requires)
        {
            if (!Flags.Contains(Flag))
            {
                Out.Add(Flag);
            }
        }
    }
    return Out;
}

FString UHavocMissionRunner::Advance()
{
    if (!Definition.States.IsValidIndex(CurrentIndex))
    {
        return FString();
    }
    const FHavocMissionStateDef& Current = Definition.States[CurrentIndex];
    if (IsComplete() || Current.Next.IsEmpty() || !Ready())
    {
        return GetState();
    }
    Go(Current.Next);
    return GetState();
}

FString UHavocMissionRunner::Choose(const FString& Branch)
{
    if (!Definition.States.IsValidIndex(CurrentIndex))
    {
        return FString();
    }
    const FString* Target = Definition.States[CurrentIndex].Branches.Find(Branch);
    if (!Target || Target->IsEmpty())
    {
        UE_LOG(LogHavocMission, Error, TEXT("%s has no branch \"%s\" (choices: %s)"), *GetState(), *Branch,
            *FString::Join(GetChoices(), TEXT(", ")));
        return GetState();
    }
    if (!Ready())
    {
        return GetState();
    }
    const FString TargetId = *Target; // Go() may not invalidate, but copy anyway.
    Go(TargetId);
    return GetState();
}

FString UHavocMissionRunner::Fail()
{
    if (!IsComplete() && !Definition.FailState.IsEmpty())
    {
        Go(Definition.FailState);
    }
    return GetState();
}

bool UHavocMissionRunner::RestoreCheckpoint()
{
    return bHasCheckpoint && RestoreCheckpointData(LastCheckpoint);
}

bool UHavocMissionRunner::RestoreCheckpointData(const FHavocMissionCheckpoint& Checkpoint)
{
    if (Checkpoint.MissionId != Definition.Id)
    {
        return false;
    }
    const int32* Index = StateIndex.Find(Checkpoint.State);
    if (!Index)
    {
        UE_LOG(LogHavocMission, Error, TEXT("Mission %s has no state %s"), *Definition.Id, *Checkpoint.State);
        return false;
    }
    CurrentIndex = *Index;
    Flags = Checkpoint.Flags;
    Trail = Checkpoint.History;
    Outcome.Reset();
    if (IHavocMissionSystems* Systems = Cast<IHavocMissionSystems>(SystemsObject.Get()))
    {
        Systems->RestoreSystems(Checkpoint.SystemsJson);
    }
    LastCheckpoint = Checkpoint;
    bHasCheckpoint = true;

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("missionId"), Definition.Id);
    Payload->SetStringField(TEXT("to"), GetState());
    Publish(TEXT("mission:state"), Payload);
    OnState.Broadcast(FString(), GetState());
    return true;
}

bool UHavocMissionRunner::Ready()
{
    const TArray<FString> Missed = Missing();
    if (Missed.Num() > 0)
    {
        const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("missionId"), Definition.Id);
        Payload->SetStringField(TEXT("state"), GetState());
        Payload->SetArrayField(TEXT("missing"), HavocMissionJson::StringArray(Missed));
        Publish(TEXT("mission:blocked"), Payload);
        OnBlocked.Broadcast(GetState(), Missed);
        return false;
    }
    return true;
}

bool UHavocMissionRunner::Go(const FString& Id)
{
    const int32* Index = StateIndex.Find(Id);
    if (!Index)
    {
        UE_LOG(LogHavocMission, Error, TEXT("Mission %s has no state %s"), *Definition.Id, *Id);
        return false;
    }
    const FString From = GetState();
    CurrentIndex = *Index;
    Enter(Definition.States[CurrentIndex], From);
    return true;
}

void UHavocMissionRunner::Enter(const FHavocMissionStateDef& State, const FString& From)
{
    Trail.Add(State.Id);
    for (const FString& Effect : State.Effects)
    {
        if (const FHavocEffectHandler* Handler = Effects.Find(Effect))
        {
            if (*Handler)
            {
                (*Handler)(State);
            }
        }
    }

    {
        const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("missionId"), Definition.Id);
        if (!From.IsEmpty())
        {
            Payload->SetStringField(TEXT("from"), From);
        }
        Payload->SetStringField(TEXT("to"), State.Id);
        Publish(TEXT("mission:state"), Payload);
        OnState.Broadcast(From, State.Id);
    }

    if (State.bCheckpoint)
    {
        LastCheckpoint.MissionId = Definition.Id;
        LastCheckpoint.State = State.Id;
        LastCheckpoint.Flags = Flags;
        LastCheckpoint.History = Trail;
        LastCheckpoint.SystemsJson = SnapshotSystems();
        bHasCheckpoint = true;

        const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("missionId"), Definition.Id);
        Payload->SetObjectField(TEXT("checkpoint"), CheckpointToJson(LastCheckpoint));
        Publish(TEXT("mission:checkpoint"), Payload);
        OnCheckpoint.Broadcast(LastCheckpoint);
    }

    if (State.bTerminal && !State.Outcome.IsEmpty())
    {
        Outcome = State.Outcome;
        const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("missionId"), Definition.Id);
        Payload->SetStringField(TEXT("state"), State.Id);
        Payload->SetStringField(TEXT("outcome"), State.Outcome);
        Publish(TEXT("mission:complete"), Payload);
        OnComplete.Broadcast(State.Id, State.Outcome);
    }
}

FString UHavocMissionRunner::SnapshotSystems() const
{
    if (const IHavocMissionSystems* Systems = Cast<IHavocMissionSystems>(SystemsObject.Get()))
    {
        return Systems->SnapshotSystems();
    }
    return TEXT("{}");
}

void UHavocMissionRunner::Publish(const TCHAR* EventName, const TSharedRef<FJsonObject>& Payload)
{
    if (EventSubsystem)
    {
        EventSubsystem->Publish(FName(EventName), HavocMissionJson::ToString(Payload));
    }
}

TSharedRef<FJsonObject> UHavocMissionRunner::CheckpointToJson(const FHavocMissionCheckpoint& Checkpoint)
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("missionId"), Checkpoint.MissionId);
    Json->SetStringField(TEXT("state"), Checkpoint.State);
    Json->SetArrayField(TEXT("flags"), HavocMissionJson::StringArray(Checkpoint.Flags));
    Json->SetArrayField(TEXT("history"), HavocMissionJson::StringArray(Checkpoint.History));
    Json->SetObjectField(TEXT("systems"), HavocMissionJson::FromString(Checkpoint.SystemsJson));
    return Json;
}
