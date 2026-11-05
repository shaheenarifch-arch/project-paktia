#include "Audio/HavocAudioSubsystem.h"

#include "Assets/HavocAssetSubsystem.h"
#include "Components/AudioComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Missions/HavocMissionJson.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/HavocPlaySettings.h"
#include "Sound/SoundBase.h"

namespace
{
    const TCHAR* const AudioEvents[] = { TEXT("hazard:cleared"), TEXT("tanker:damaged"), TEXT("falcon:support") };
}

void UHavocAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (UGameInstance* GI = InWorld.GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            for (const TCHAR* Name : AudioEvents)
            {
                Events->Subscribe(FName(Name), this, GET_FUNCTION_NAME_CHECKED(UHavocAudioSubsystem, HandleHavocEvent));
            }
        }
    }
}

void UHavocAudioSubsystem::Deinitialize()
{
    const UWorld* World = GetWorld();
    if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            for (const TCHAR* Name : AudioEvents)
            {
                Events->Unsubscribe(FName(Name), this, GET_FUNCTION_NAME_CHECKED(UHavocAudioSubsystem, HandleHavocEvent));
            }
        }
    }
    Super::Deinitialize();
}

UObject* UHavocAudioSubsystem::ResolveRole(FName Role)
{
    if (const TObjectPtr<UObject>* Found = Cache.Find(Role))
    {
        return *Found;
    }
    if (Missing.Contains(Role))
    {
        return nullptr;
    }
    const UWorld* World = GetWorld();
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    const UHavocAssetSubsystem* Assets = GI ? GI->GetSubsystem<UHavocAssetSubsystem>() : nullptr;
    UObject* Asset = Assets ? Assets->LoadAsset(Role) : nullptr; // logs its own warning
    if (!Asset)
    {
        Missing.Add(Role);
        return nullptr;
    }
    Cache.Add(Role, Asset);
    return Asset;
}

bool UHavocAudioSubsystem::PlayRole(FName Role, const FVector& Location, const FRotator& Rotation)
{
    UWorld* World = GetWorld();
    UObject* Asset = World ? ResolveRole(Role) : nullptr;
    if (!Asset)
    {
        return false;
    }
    if (USoundBase* Sound = Cast<USoundBase>(Asset))
    {
        UGameplayStatics::PlaySoundAtLocation(World, Sound, Location);
        return true;
    }
    if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset))
    {
        // Accessibility (docs/vfx-plan.md): "reduce flashes" drops the big explosion layer.
        const UGameInstance* GI = World->GetGameInstance();
        const UHavocPlaySettings* Settings = GI ? GI->GetSubsystem<UHavocPlaySettings>() : nullptr;
        if (Settings && Settings->bReduceFlashes && Role == TEXT("FX.Explosion.Large"))
        {
            return false;
        }
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, Rotation);
        return true;
    }
    return false;
}

bool UHavocAudioSubsystem::PlayRole2D(FName Role)
{
    UObject* Asset = GetWorld() ? ResolveRole(Role) : nullptr;
    if (USoundBase* Sound = Cast<USoundBase>(Asset))
    {
        UGameplayStatics::PlaySound2D(GetWorld(), Sound);
        return true;
    }
    return false;
}

void UHavocAudioSubsystem::StartAmbient()
{
    if (Ambient && Ambient->IsPlaying())
    {
        return;
    }
    if (USoundBase* Sound = Cast<USoundBase>(GetWorld() ? ResolveRole(TEXT("Audio.Ambient.Coast")) : nullptr))
    {
        Ambient = UGameplayStatics::SpawnSound2D(GetWorld(), Sound);
    }
}

void UHavocAudioSubsystem::StopAmbient()
{
    if (Ambient)
    {
        Ambient->Stop();
        Ambient = nullptr;
    }
}

void UHavocAudioSubsystem::HandleHavocEvent(FName EventName, const FString& Payload)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Payload);
    double X = 0, Y = 0, Z = 0;
    Json->TryGetNumberField(TEXT("x"), X);
    Json->TryGetNumberField(TEXT("y"), Y);
    Json->TryGetNumberField(TEXT("z"), Z);
    const FVector Where(X, Y, Z);

    if (EventName == TEXT("hazard:cleared"))
    {
        PlayRole(TEXT("FX.Explosion.Large"), Where);
        PlayRole(TEXT("Audio.Explosion"), Where);
        PlayRole(TEXT("FX.Water.Splash"), Where);
    }
    else if (EventName == TEXT("tanker:damaged"))
    {
        PlayRole(TEXT("FX.Smoke"), Where);
        PlayRole(TEXT("Audio.Explosion"), Where);
    }
}
