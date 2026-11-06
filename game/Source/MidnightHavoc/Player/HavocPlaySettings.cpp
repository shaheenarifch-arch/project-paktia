#include "Player/HavocPlaySettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "Input/HavocPlayInput.h"

void UHavocPlaySettings::Initialize(FSubsystemCollectionBase& Collection)
{
    // The input subsystem reads the key bindings from here, so this must exist first.
    Super::Initialize(Collection);
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 3.0f);
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    GraphicsQuality = FMath::Clamp(GraphicsQuality, 0, 3);
    if (FAudioDevice* Device = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr)
    {
        Device->SetTransientPrimaryVolume(MasterVolume);
    }
    // Quality is applied here but not saved: Unreal's own GameUserSettings file is left to the engine.
    if (UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        User->SetOverallScalabilityLevel(GraphicsQuality);
    }
}

void UHavocPlaySettings::ApplyAndSave()
{
    LookSensitivity = FMath::Clamp(LookSensitivity, 0.2f, 3.0f);
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    GraphicsQuality = FMath::Clamp(GraphicsQuality, 0, 3);
    if (FAudioDevice* Device = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr)
    {
        Device->SetTransientPrimaryVolume(MasterVolume);
    }
    if (UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        User->SetOverallScalabilityLevel(GraphicsQuality);
        User->ApplySettings(false);
    }
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocPlayInput* Input = GI->GetSubsystem<UHavocPlayInput>())
        {
            Input->ApplyBindings(*this);
        }
    }
    SaveConfig();
}

bool UHavocPlaySettings::IsHintSeen(EHavocHintKind Kind) const
{
    switch (Kind)
    {
    case EHavocHintKind::Drone: return bHintDroneSeen;
    case EHavocHintKind::Vehicle: return bHintVehicleSeen;
    default: return bHintHelicopterSeen;
    }
}

void UHavocPlaySettings::MarkHintSeen(EHavocHintKind Kind)
{
    switch (Kind)
    {
    case EHavocHintKind::Drone: bHintDroneSeen = true; break;
    case EHavocHintKind::Vehicle: bHintVehicleSeen = true; break;
    default: bHintHelicopterSeen = true; break;
    }
    SaveConfig();
}

void UHavocPlaySettings::ResetToDefaults()
{
    bReduceFlashes = false;
    LookSensitivity = 1.0f;
    MasterVolume = 1.0f;
    GraphicsQuality = 2;
    bSubtitles = true;
    InteractKey = EKeys::E;
    ReloadKey = EKeys::R;
    RavenKey = EKeys::T;
    FalconKey = EKeys::G;
    ReturnKey = EKeys::Tab;
    bHintDroneSeen = bHintVehicleSeen = bHintHelicopterSeen = false; // hints show again
    ApplyAndSave();
}

const TCHAR* UHavocPlaySettings::QualityName(int32 Level)
{
    switch (FMath::Clamp(Level, 0, 3))
    {
    case 0: return TEXT("Low");
    case 1: return TEXT("Medium");
    case 2: return TEXT("High");
    default: return TEXT("Epic");
    }
}

FString UHavocPlaySettings::HelpFor(FName SettingId)
{
    static const TMap<FName, FString> Help = {
        { TEXT("quality"), TEXT("Overall graphics quality. Lower it if the frame rate drops; the change applies at once.") },
        { TEXT("flashes"), TEXT("Skips the large explosion effect and camera shake. Turn on if flashes or shaking bother you.") },
        { TEXT("subtitles"), TEXT("Shows mission and radio messages as text at the screen centre and the hint line at the bottom.") },
        { TEXT("sensitivity"), TEXT("How fast the camera turns with the mouse. 0.2 is slow, 3.0 is fast.") },
        { TEXT("volume"), TEXT("Master volume for all sound, 0 to 100 percent.") },
        { TEXT("bind"), TEXT("Choose a row, press Enter or A, then press the new key. Esc or B cancels. Another action using that key is swapped.") },
        { TEXT("reset"), TEXT("Puts every setting above back to its default. First-time hints show again.") },
        { TEXT("howtoplay"), TEXT("Every key for walking, vehicles, the Raven drone and the Falcon helicopter, read from your current key bindings.") },
    };
    const FString* Found = Help.Find(SettingId);
    return Found ? *Found : FString();
}
