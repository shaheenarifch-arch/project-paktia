#include "Campaign/HavocCampaign.h"

#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocCampaign, Log, All);

void UHavocCampaign::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadProgress();
}

FString UHavocCampaign::EpisodeName(int32 Episode)
{
    switch (Episode)
    {
    case 1: return TEXT("Operation Mountain Shield");
    case 2: return TEXT("Operation Silent Corridor");
    case 3: return TEXT("Operation Broken Citadel");
    case 4: return TEXT("Operation Open Strait");
    default: return FString();
    }
}

int32 UHavocCampaign::OutcomeRank(const FString& Outcome)
{
    if (Outcome == TEXT("FAILED")) { return 0; }
    if (Outcome == TEXT("PARTIAL")) { return 1; }
    if (Outcome == TEXT("SUCCESS")) { return 2; }
    return -1;
}

const FHavocEpisodeResult* UHavocCampaign::Find(int32 Episode) const
{
    return Results.FindByPredicate([Episode](const FHavocEpisodeResult& R) { return R.Episode == Episode; });
}

bool UHavocCampaign::Record(int32 Episode, const FString& Outcome, const FHavocCarryover& Carryover, bool bSave)
{
    if (Episode < FirstEpisode || Episode > LastEpisode || OutcomeRank(Outcome) < 0)
    {
        UE_LOG(LogHavocCampaign, Warning, TEXT("Record ignored: episode %d outcome '%s'"), Episode, *Outcome);
        return false;
    }
    FHavocEpisodeResult* Prior = Results.FindByPredicate([Episode](const FHavocEpisodeResult& R) { return R.Episode == Episode; });
    // A replay never downgrades a better earlier result.
    if (Prior && OutcomeRank(Prior->Outcome) > OutcomeRank(Outcome))
    {
        return false;
    }
    FHavocEpisodeResult& Slot = Prior ? *Prior : Results.AddDefaulted_GetRef();
    Slot.Episode = Episode;
    Slot.Outcome = Outcome;
    Slot.Carryover = Carryover;
    if (bSave)
    {
        SaveProgress();
    }
    return true;
}

FString UHavocCampaign::GetOutcome(int32 Episode) const
{
    const FHavocEpisodeResult* R = Find(Episode);
    return R ? R->Outcome : FString();
}

bool UHavocCampaign::GetCarryover(int32 Episode, FHavocCarryover& OutCarryover) const
{
    const FHavocEpisodeResult* R = Find(Episode);
    if (R)
    {
        OutCarryover = R->Carryover;
    }
    return R != nullptr;
}

bool UHavocCampaign::IsPassed(int32 Episode) const
{
    const int32 Rank = OutcomeRank(GetOutcome(Episode));
    return Rank == 1 || Rank == 2;
}

bool UHavocCampaign::IsUnlocked(int32 Episode) const
{
    if (Episode < FirstEpisode || Episode > LastEpisode)
    {
        return false;
    }
    return Episode == FirstEpisode || bUnlockAll || IsPassed(Episode - 1);
}

int32 UHavocCampaign::GetNextEpisode() const
{
    for (int32 Episode = FirstEpisode; Episode <= LastEpisode; ++Episode)
    {
        if (!IsPassed(Episode))
        {
            return Episode;
        }
    }
    return 0;
}

FString UHavocCampaign::StatusText(int32 Episode) const
{
    const FString Outcome = GetOutcome(Episode);
    if (!Outcome.IsEmpty())
    {
        return FString::Printf(TEXT("Completed: %s"), *Outcome);
    }
    return IsUnlocked(Episode) ? TEXT("Unlocked") : TEXT("Locked");
}

bool UHavocCampaign::WasCrewRecoveredInEpisode01() const
{
    FHavocCarryover C;
    return GetCarryover(1, C) ? C.bCrewRecovered : true;
}

bool UHavocCampaign::SaveProgress() const
{
    UHavocCampaignSave* Save = Cast<UHavocCampaignSave>(UGameplayStatics::CreateSaveGameObject(UHavocCampaignSave::StaticClass()));
    if (!Save)
    {
        return false;
    }
    Save->Results = Results;
    Save->bUnlockAll = bUnlockAll;
    const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
    if (!bOk)
    {
        UE_LOG(LogHavocCampaign, Warning, TEXT("Could not save campaign progress to slot '%s'"), *SlotName);
    }
    return bOk;
}

bool UHavocCampaign::LoadProgress()
{
    Results.Reset();
    bUnlockAll = false;
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        return false;
    }
    const UHavocCampaignSave* Save = Cast<UHavocCampaignSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save)
    {
        UE_LOG(LogHavocCampaign, Warning, TEXT("Campaign save '%s' is unreadable; starting fresh"), *SlotName);
        return false;
    }
    // Keep only valid entries, one per episode (a hand-edited or old file must not break the rules).
    for (const FHavocEpisodeResult& R : Save->Results)
    {
        if (R.Episode >= FirstEpisode && R.Episode <= LastEpisode && OutcomeRank(R.Outcome) >= 0 && !Find(R.Episode))
        {
            Results.Add(R);
        }
    }
    bUnlockAll = Save->bUnlockAll;
    return true;
}

void UHavocCampaign::ResetProgress()
{
    Results.Reset();
    bUnlockAll = false;
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
}

void UHavocCampaign::SetUnlockAll(bool bValue)
{
    bUnlockAll = bValue;
    SaveProgress();
}
