#include "Missions/HavocEpisodeFlowBase.h"

#include "Cutscene/HavocCutscene.h"
#include "Cutscene/HavocFlowCutscene.h"
#include "Player/HavocPlayerController.h"

void AHavocEpisodeFlowBase::PlayCutscene(bool bIntro, TFunction<void()> Then)
{
    const FString Label = CutsceneLabel();
    HavocFlowCutscene::Play(*this, Cutscene, Cast<AHavocPlayerController>(UGameplayStatics::GetPlayerController(this, 0)),
                            Label, bIntro, CutsceneTargets(bIntro), HavocFlowCutscene::BuildLines(Label, bIntro, Outcome), MoveTemp(Then));
}

void AHavocEpisodeFlowBase::SkipCutscene()
{
    if (Cutscene)
    {
        Cutscene->Skip();
    }
}

bool AHavocEpisodeFlowBase::IsCutsceneActive() const
{
    return Cutscene && Cutscene->IsPlaying();
}

FString AHavocEpisodeFlowBase::GetCutsceneSubtitle() const
{
    return Cutscene ? Cutscene->GetSubtitle() : FString();
}
