#include "Missions/PaktiaMissionRunner.h"

#include "Core/PaktiaEventSubsystem.h"
#include "Missions/PaktiaMissionJson.h"
#include "Missions/PaktiaMissionSystems.h"

DEFINE_LOG_CATEGORY(LogPaktiaMission);

bool UPaktiaMissionRunner::Initialize(const FPaktiaMissionDef& InDefinition, const TMap<FString, FPaktiaEffectHandler>& InEffects,
    UPaktiaEventSubsystem* Events, UObject* SystemsProvider, FString& OutError)
{
    // Build into locals first so a failed Initialize leaves the runner untouched.
    TMap<FString, int32> NewIndex;
    for (int32 i = 0; i < InDefinition.States.Num(); ++i)
    {
        NewIndex.Add(InDefinition.States[i].Id, i);
    }
    for (const FPaktiaMissionStateDef& State : InDefinition.States)
    {
        for (const FString& Effect : State.Effects)
        {
            if (!InEffects.Contains(Effect))
            {
                OutError = FString::Printf(TEXT("Mission %s has no handler for effect %s"), *InDefinition.Id, *Effect);
                UE_LOG(LogPaktiaMission, Error, TEXT("%s"), *OutError);
                return false;
            }
        }
    }
    const int32* StartIndex = NewIndex.Find(InDefinition.Start);
    if (!StartIndex)
    {
        OutError = FString::Printf(TEXT("Mission %s has no state %s"), *InDefinition.Id, *InDefinition.Start);
        UE_LOG(LogPaktiaMission, Error, TEXT("%s"), *OutError);
        return false;
    }
    if (SystemsProvider && !Cast<IPaktiaMissionSystems>(SystemsProvider))
    {
        OutError = FString::Printf(TEXT("Mission %s: systems provider does not implement IPaktiaMissionSystems"), *InDefinition.Id);
        UE_LOG(LogPaktiaMission, Error, TEXT("%s"), *OutError);
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

FString UPaktiaMissionRunner::GetState() const
{
    return Definition.States.IsValidIndex(CurrentIndex) ? Definition.States[CurrentIndex].Id : FString();
}

FString UPaktiaMissionRunner::GetObjective() const
{
    return Definition.States.IsValidIndex(CurrentIndex) ? Definition.States[CurrentIndex].Objective : FString();
}

bool UPaktiaMissionRunner::IsComplete() const
{
    return Definition.States.IsValidIndex(CurrentIndex) && Definition.States[CurrentIndex].bTerminal;
}

TArray<FString> UPaktiaMissionRunner::GetChoices() const
{
    TArray<FString> Out;
    if (Definition.States.IsValidIndex(CurrentIndex))
    {
        Definition.States[CurrentIndex].Branches.GenerateKeyArray(Out);
    }
    return Out;
}

bool UPaktiaMissionRunner::HasBranch(const FString& Branch) const
{
    return Definition.States.IsValidIndex(CurrentIndex) && Definition.States[CurrentIndex].Branches.Contains(Branch);
}

void UPaktiaMissionRunner::SetFlag(const FString& Flag)
{
    Flags.AddUnique(Flag);
}

void UPaktiaMissionRunner::ClearFlag(const FString& Flag)
{
    Flags.Remove(Flag);
}

TArray<FString> UPaktiaMissionRunner::Missing() const
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

FString UPaktiaMissionRunner::Advance()
{
    if (!Definition.States.IsValidIndex(CurrentIndex))
    {
        return FString();
    }
    const FPaktiaMissionStateDef& Current = Definition.States[CurrentIndex];
    if (IsComplete() || Current.Next.IsEmpty() || !Ready())
    {
        return GetState();
    }
    Go(Current.Next);
    return GetState();
}

FString UPaktiaMissionRunner::Choose(const FString& Branch)
{
    if (!Definition.States.IsValidIndex(CurrentIndex))
    {
        return FString();
    }
    const FString* Target = Definition.States[CurrentIndex].Branches.Find(Branch);
    if (!Target || Target->IsEmpty())
    {
        UE_LOG(LogPaktiaMission, Error, TEXT("%s has no branch \"%s\" (choices: %s)"), *GetState(), *Branch,
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

FString UPaktiaMissionRunner::Fail()
{
    if (!IsComplete() && !Definition.FailState.IsEmpty())
    {
        Go(Definition.FailState);
    }
    return GetState();
}

bool UPaktiaMissionRunner::RestoreCheckpoint()
{
    return bHasCheckpoint && RestoreCheckpointData(LastCheckpoint);
}

bool UPaktiaMissionRunner::RestoreCheckpointData(const FPaktiaMissionCheckpoint& Checkpoint)
{
    if (Checkpoint.MissionId != Definition.Id)
    {
        return false;
    }
    const int32* Index = StateIndex.Find(Checkpoint.State);
    if (!Index)
    {
        UE_LOG(LogPaktiaMission, Error, TEXT("Mission %s has no state %s"), *Definition.Id, *Checkpoint.State);
        return false;
    }
    CurrentIndex = *Index;
    Flags = Checkpoint.Flags;
    Trail = Checkpoint.History;
    Outcome.Reset();
    if (IPaktiaMissionSystems* Systems = Cast<IPaktiaMissionSystems>(SystemsObject.Get()))
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

bool UPaktiaMissionRunner::Ready()
{
    const TArray<FString> Missed = Missing();
    if (Missed.Num() > 0)
    {
        const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("missionId"), Definition.Id);
        Payload->SetStringField(TEXT("state"), GetState());
        Payload->SetArrayField(TEXT("missing"), PaktiaMissionJson::StringArray(Missed));
        Publish(TEXT("mission:blocked"), Payload);
        OnBlocked.Broadcast(GetState(), Missed);
        return false;
    }
    return true;
}

bool UPaktiaMissionRunner::Go(const FString& Id)
{
    const int32* Index = StateIndex.Find(Id);
    if (!Index)
    {
        UE_LOG(LogPaktiaMission, Error, TEXT("Mission %s has no state %s"), *Definition.Id, *Id);
        return false;
    }
    const FString From = GetState();
    CurrentIndex = *Index;
    Enter(Definition.States[CurrentIndex], From);
    return true;
}

void UPaktiaMissionRunner::Enter(const FPaktiaMissionStateDef& State, const FString& From)
{
    Trail.Add(State.Id);
    for (const FString& Effect : State.Effects)
    {
        if (const FPaktiaEffectHandler* Handler = Effects.Find(Effect))
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

FString UPaktiaMissionRunner::SnapshotSystems() const
{
    if (const IPaktiaMissionSystems* Systems = Cast<IPaktiaMissionSystems>(SystemsObject.Get()))
    {
        return Systems->SnapshotSystems();
    }
    return TEXT("{}");
}

void UPaktiaMissionRunner::Publish(const TCHAR* EventName, const TSharedRef<FJsonObject>& Payload)
{
    if (EventSubsystem)
    {
        EventSubsystem->Publish(FName(EventName), PaktiaMissionJson::ToString(Payload));
    }
}

TSharedRef<FJsonObject> UPaktiaMissionRunner::CheckpointToJson(const FPaktiaMissionCheckpoint& Checkpoint)
{
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("missionId"), Checkpoint.MissionId);
    Json->SetStringField(TEXT("state"), Checkpoint.State);
    Json->SetArrayField(TEXT("flags"), PaktiaMissionJson::StringArray(Checkpoint.Flags));
    Json->SetArrayField(TEXT("history"), PaktiaMissionJson::StringArray(Checkpoint.History));
    Json->SetObjectField(TEXT("systems"), PaktiaMissionJson::FromString(Checkpoint.SystemsJson));
    return Json;
}
