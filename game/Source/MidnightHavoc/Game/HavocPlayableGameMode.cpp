#include "Game/HavocPlayableGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Missions/Episode03/HavocEpisode03Flow.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
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
    for (TActorIterator<AHavocEpisode04Flow> It(World); It; ++It)
    {
        return; // one placed in the level already
    }
    for (TActorIterator<AHavocEpisode03Flow> It(World); It; ++It)
    {
        return;
    }
    // Episode 03 (greybox Gardun) when asked: travel option ?Episode=3, command line -HavocEpisode=3,
    // or a map whose name contains "Gardun". Otherwise Episode 04 as before.
    int32 Episode = 0;
    FParse::Value(FCommandLine::Get(), TEXT("HavocEpisode="), Episode);
    if (UGameplayStatics::HasOption(OptionsString, TEXT("Episode")))
    {
        Episode = UGameplayStatics::GetIntOption(OptionsString, TEXT("Episode"), Episode);
    }
    if (Episode == 0 && World->GetMapName().Contains(TEXT("Gardun")))
    {
        Episode = 3;
    }
    if (Episode == 3)
    {
        World->SpawnActor<AHavocEpisode03Flow>(AHavocEpisode03Flow::StaticClass(), FTransform::Identity);
        return;
    }
    World->SpawnActor<AHavocEpisode04Flow>(AHavocEpisode04Flow::StaticClass(), FTransform::Identity);
}
