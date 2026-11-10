#include "Cutscene/HavocFlowCutscene.h"

#include "Cutscene/HavocCutscene.h"
#include "Engine/World.h"
#include "Player/HavocPlayerController.h"

namespace HavocFlowCutscene
{
    TArray<FString> BuildLines(const FString& Episode, bool bIntro, const FString& Outcome)
    {
        if (Episode == TEXT("01"))
        {
            if (bIntro)
            {
                return { TEXT("Paktia Highlands. A fictional mountain country."),
                         TEXT("Hold Checkpoint Echo, then set up the relay at Ridge Station."),
                         TEXT("Learn the Raven drone first; Falcon-2 is on its way.") };
            }
            return { Outcome == TEXT("SUCCESS") ? TEXT("Ridge Station holds and the Falcon-2 crew is home.")
                   : Outcome == TEXT("FAILED") ? TEXT("Ridge Station is lost.")
                   : TEXT("Ridge Station holds, but the Falcon-2 crew was left behind."),
                     TEXT("Operation Mountain Shield is over. Check your results.") };
        }
        if (Episode == TEXT("02"))
        {
            if (bIntro)
            {
                return { TEXT("The Silent Corridor. The convoy forms up at Checkpoint Echo."),
                         TEXT("Place Relay Kilo with the Raven, then escort the convoy past the bridge."),
                         TEXT("South Gate is the way out.") };
            }
            return { Outcome == TEXT("SUCCESS") ? TEXT("The convoy reached South Gate.")
                   : Outcome == TEXT("FAILED") ? TEXT("The convoy was lost in the corridor.")
                   : TEXT("The convoy got through, but not whole."),
                     TEXT("Operation Silent Corridor is over. Check your results.") };
        }
        if (Episode == TEXT("03"))
        {
            if (bIntro)
            {
                return { TEXT("Gardun. The citadel is the last stronghold."),
                         TEXT("Take the sectors in order and protect the medical station."),
                         TEXT("Break the enemy's hold on the citadel.") };
            }
            return { Outcome == TEXT("SUCCESS") ? TEXT("The citadel is secured.")
                   : Outcome == TEXT("FAILED") ? TEXT("The assault failed. The citadel holds.")
                   : TEXT("The citadel is taken, at a cost."),
                     TEXT("Operation Broken Citadel is over. Check your results.") };
        }
        return {};
    }

    void Play(AActor& Owner, TObjectPtr<AHavocCutscene>& Slot, AHavocPlayerController* Player,
              const FString& Episode, bool bIntro, const TArray<FVector>& Targets,
              const TArray<FString>& Lines, TFunction<void()> Then)
    {
        UWorld* World = Owner.GetWorld();
        if (!Slot && World)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Slot = World->SpawnActor<AHavocCutscene>(AHavocCutscene::StaticClass(), Owner.GetActorTransform(), Params);
        }
        if (!Slot)
        {
            UE_LOG(LogHavocCutscene, Warning, TEXT("Cutscene could not start (no camera actor)"));
            if (Then) { Then(); }
            return;
        }
        Slot->Begin(Player, FString::Printf(TEXT("%s %s"), *Episode, bIntro ? TEXT("intro") : TEXT("outro")),
                    HavocCutsceneMath::BuildFlyover(Targets, Lines, 5.f),
                    [Then = MoveTemp(Then)](bool) { if (Then) { Then(); } });
    }
}
