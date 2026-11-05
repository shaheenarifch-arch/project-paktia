#include "Player/HavocPlaySettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"

void UHavocPlaySettings::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 3.0f);
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    if (FAudioDevice* Device = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr)
    {
        Device->SetTransientPrimaryVolume(MasterVolume);
    }
}

void UHavocPlaySettings::ApplyAndSave()
{
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 3.0f);
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    if (FAudioDevice* Device = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr)
    {
        Device->SetTransientPrimaryVolume(MasterVolume);
    }
    SaveConfig();
}
