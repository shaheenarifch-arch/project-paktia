#include "AI/HavocSquadLogic.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace HavocSquad
{
    EHavocTeam TeamForFaction(const FString& Faction)
    {
        return Faction == TEXT("coalition") ? EHavocTeam::Allied : EHavocTeam::Opposing;
    }

    namespace
    {
        void ReadFloat(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float& InOut)
        {
            double Value = 0.0;
            if (Obj->TryGetNumberField(Key, Value))
            {
                InOut = static_cast<float>(Value);
            }
        }
    }

    bool ParseConfig(const FString& JsonText, FConfig& OutConfig, FString& OutError)
    {
        OutConfig = FConfig();
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            OutError = TEXT("squad-behaviour.json is not valid JSON");
            return false;
        }

        const TSharedPtr<FJsonObject>* Tuning = nullptr;
        if (Root->TryGetObjectField(TEXT("tuning"), Tuning) && Tuning)
        {
            FTuning& T = OutConfig.Tuning;
            ReadFloat(*Tuning, TEXT("squadTickSeconds"), T.SquadTickSeconds);
            ReadFloat(*Tuning, TEXT("loseThreatSeconds"), T.LoseThreatSeconds);
            ReadFloat(*Tuning, TEXT("disengageSeconds"), T.DisengageSeconds);
            ReadFloat(*Tuning, TEXT("fallbackStrength"), T.FallbackStrength);
            ReadFloat(*Tuning, TEXT("regroupSeconds"), T.RegroupSeconds);
            ReadFloat(*Tuning, TEXT("maxFallbackSeconds"), T.MaxFallbackSeconds);
            ReadFloat(*Tuning, TEXT("patrolRadiusCm"), T.PatrolRadiusCm);
            ReadFloat(*Tuning, TEXT("coverSearchRadiusCm"), T.CoverSearchRadiusCm);
            ReadFloat(*Tuning, TEXT("coverGridSpacingCm"), T.CoverGridSpacingCm);
            (*Tuning)->TryGetBoolField(TEXT("useEqsCover"), T.bUseEqsCover);
        }
        OutConfig.Tuning.SquadTickSeconds = FMath::Max(0.05f, OutConfig.Tuning.SquadTickSeconds);
        OutConfig.Tuning.CoverGridSpacingCm = FMath::Max(50.f, OutConfig.Tuning.CoverGridSpacingCm);

        const TArray<TSharedPtr<FJsonValue>>* Phases = nullptr;
        if (Root->TryGetArrayField(TEXT("phases"), Phases) && Phases)
        {
            for (const TSharedPtr<FJsonValue>& PhaseValue : *Phases)
            {
                const TSharedPtr<FJsonObject> Phase = PhaseValue.IsValid() ? PhaseValue->AsObject() : nullptr;
                FString State;
                const TArray<TSharedPtr<FJsonValue>>* Deployments = nullptr;
                if (!Phase.IsValid() || !Phase->TryGetStringField(TEXT("state"), State) || !Phase->TryGetArrayField(TEXT("deployments"), Deployments) || !Deployments)
                {
                    OutError = TEXT("a phase needs 'state' and 'deployments'");
                    return false;
                }
                for (const TSharedPtr<FJsonValue>& DeployValue : *Deployments)
                {
                    const TSharedPtr<FJsonObject> Obj = DeployValue.IsValid() ? DeployValue->AsObject() : nullptr;
                    FDeployment D;
                    D.State = State;
                    FString Sector, Posture;
                    int32 Size = 0;
                    if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("faction"), D.Faction) || !Obj->TryGetStringField(TEXT("sector"), Sector) || !Obj->TryGetNumberField(TEXT("size"), Size))
                    {
                        OutError = FString::Printf(TEXT("a deployment in '%s' needs faction, sector and size"), *State);
                        return false;
                    }
                    D.Sector = FName(*Sector);
                    D.Size = FMath::Clamp(Size, 1, 12);
                    Obj->TryGetStringField(TEXT("posture"), Posture);
                    D.Posture = Posture == TEXT("assault") ? EHavocSquadPosture::Assault : EHavocSquadPosture::Patrol;
                    OutConfig.Deployments.Add(D);
                }
            }
        }
        return true;
    }
}
