#include "Game/HavocPlayableGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Campaign/HavocCampaign.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"

AHavocPlayableGameMode::AHavocPlayableGameMode()
{
    DefaultPawnClass = AHavocPlayerCharacter::StaticClass();
    PlayerControllerClass = AHavocPlayerController::StaticClass();
}

void AHavocPlayableGameMode::StartPlay()
{
    Super::StartPlay();
    UWorld* World = GetWorld();
    // Which episode: travel option ?Episode=N (menu "Play Episode N"), command line -HavocSmokeEpisode=NN or
    // -HavocEpisode=N, else the episode the map belongs to (UHavocCampaign::EpisodeMapPath), else Episode 04.
    int32 Episode = 0;
    FParse::Value(FCommandLine::Get(), TEXT("HavocEpisode="), Episode);
    FString SmokeEpisode;
    if (FParse::Value(FCommandLine::Get(), TEXT("HavocSmokeEpisode="), SmokeEpisode) && SmokeEpisode.IsNumeric())
    {
        Episode = FCString::Atoi(*SmokeEpisode);
    }
    if (UGameplayStatics::HasOption(OptionsString, TEXT("Episode")))
    {
        Episode = UGameplayStatics::GetIntOption(OptionsString, TEXT("Episode"), Episode);
    }
    if (Episode < UHavocCampaign::FirstEpisode || Episode > UHavocCampaign::LastEpisode)
    {
        Episode = UHavocCampaign::EpisodeForMap(World->GetMapName());
    }
    if (Episode == 0)
    {
        Episode = 4;
    }
    UClass* FlowClass = UHavocCampaign::EpisodeFlowClass(Episode);
    for (TActorIterator<AActor> It(World, FlowClass); It; ++It)
    {
        return; // one placed in the level already
    }
    UE_LOG(LogTemp, Display, TEXT("HavocGameMode: map %s, spawning the Episode %02d flow (%s)"), *World->GetMapName(), Episode, *FlowClass->GetName());
    World->SpawnActor<AActor>(FlowClass, FTransform::Identity);
}
