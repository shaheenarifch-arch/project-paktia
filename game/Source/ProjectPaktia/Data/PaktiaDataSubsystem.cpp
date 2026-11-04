#include "PaktiaDataSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

DEFINE_LOG_CATEGORY(LogPaktiaData);

namespace
{
    const TCHAR* const SettingFictional = TEXT("fictional");
    const TCHAR* const SettingRealWorld = TEXT("real_world_hypothetical");

    /** Converts a JSON object into a USTRUCT. Missing JSON keys keep the struct defaults. */
    template <typename T>
    bool ParseStruct(const TSharedRef<FJsonObject>& Json, T& Out)
    {
        return FJsonObjectConverter::JsonObjectToUStruct<T>(Json, &Out, 0, 0);
    }

    void ReadBool(const FJsonObject& Json, const TCHAR* Key, bool& Out)
    {
        bool Value = false;
        if (Json.TryGetBoolField(Key, Value))
        {
            Out = Value;
        }
    }

    void ReadString(const FJsonObject& Json, const TCHAR* Key, FString& Out)
    {
        FString Value;
        if (Json.TryGetStringField(Key, Value))
        {
            Out = Value;
        }
    }

    template <typename T>
    const T* FindDef(const TMap<FName, T>& Map, FName Id)
    {
        return Map.Find(Id);
    }
}

void UPaktiaDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadAll();
}

void UPaktiaDataSubsystem::LoadAll()
{
    FString Dir = DataDirectory.IsEmpty() ? FPaths::Combine(FPaths::ProjectDir(), TEXT("../data")) : DataDirectory;
    if (FPaths::IsRelative(Dir))
    {
        Dir = FPaths::Combine(FPaths::ProjectDir(), Dir);
    }
    FPaths::CollapseRelativeDirectories(Dir);
    ResolvedDataDirectory = FPaths::ConvertRelativePathToFull(Dir);

    if (!FPaths::DirectoryExists(ResolvedDataDirectory))
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("Data directory '%s' does not exist. No game data loaded. Set DataDirectory under [/Script/ProjectPaktia.PaktiaDataSubsystem] in DefaultGame.ini, or package ../data (see game/README.md)."), *ResolvedDataDirectory);
        return;
    }

    // Maps first: mission validation needs them.
    LoadFolder(TEXT("maps"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterMap(J, F); });
    LoadFolder(TEXT("aircraft"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterAircraft(J, F); });
    LoadFolder(TEXT("vehicles"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterVehicle(J, F); });
    LoadFolder(TEXT("weapons"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterWeapon(J, F); });
    LoadFolder(TEXT("missions"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterMission(J, F); });

    UE_LOG(LogPaktiaData, Log, TEXT("Loaded game data from '%s': %d missions, %d maps, %d aircraft, %d vehicles, %d weapons."),
        *ResolvedDataDirectory, Missions.Num(), Maps.Num(), Aircraft.Num(), Vehicles.Num(), Weapons.Num());
}

void UPaktiaDataSubsystem::LoadFolder(const FString& SubFolder, TFunctionRef<void(const TSharedRef<FJsonObject>&, const FString& File)> Handler)
{
    const FString Folder = FPaths::Combine(ResolvedDataDirectory, SubFolder);
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Folder, TEXT("*.json"), /*Files*/ true, /*Directories*/ false);
    Files.Sort();

    for (const FString& File : Files)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *File))
        {
            UE_LOG(LogPaktiaData, Warning, TEXT("Could not read '%s'; skipping."), *File);
            continue;
        }

        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
        {
            UE_LOG(LogPaktiaData, Warning, TEXT("Could not parse JSON in '%s'; skipping."), *File);
            continue;
        }

        Handler(Json.ToSharedRef(), File);
    }
}

void UPaktiaDataSubsystem::RegisterMap(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FPaktiaMapDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("'%s' is not a valid map definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    if (Def.Setting.IsEmpty())
    {
        Def.Setting = SettingFictional;
    }
    if (Maps.Contains(FName(*Def.Id)))
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("Duplicate map id '%s' in '%s'; later file wins."), *Def.Id, *File);
    }
    Maps.Add(FName(*Def.Id), MoveTemp(Def));
}

void UPaktiaDataSubsystem::RegisterAircraft(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FPaktiaAircraftDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("'%s' is not a valid aircraft definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    ReadBool(*Json, TEXT("enabled"), Def.bEnabled);
    ReadString(*Json, TEXT("class"), Def.ClassName);
    Aircraft.Add(FName(*Def.Id), MoveTemp(Def));
}

void UPaktiaDataSubsystem::RegisterVehicle(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FPaktiaVehicleDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("'%s' is not a valid vehicle definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    ReadBool(*Json, TEXT("enabled"), Def.bEnabled);
    ReadString(*Json, TEXT("class"), Def.ClassName);
    Vehicles.Add(FName(*Def.Id), MoveTemp(Def));
}

void UPaktiaDataSubsystem::RegisterWeapon(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FPaktiaWeaponDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("'%s' is not a valid weapon definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    ReadBool(*Json, TEXT("enabled"), Def.bEnabled);
    Weapons.Add(FName(*Def.Id), MoveTemp(Def));
}

void UPaktiaDataSubsystem::RegisterMission(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FPaktiaMissionDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("'%s' is not a valid mission definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    if (Def.Setting.IsEmpty())
    {
        Def.Setting = SettingFictional;
    }

    // Per-state booleans ("checkpoint", "terminal") are filled by hand.
    const TArray<TSharedPtr<FJsonValue>>* StatesJson = nullptr;
    if (Json->TryGetArrayField(TEXT("states"), StatesJson) && StatesJson)
    {
        for (int32 Index = 0; Index < StatesJson->Num() && Index < Def.States.Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* StateObject = nullptr;
            if ((*StatesJson)[Index].IsValid() && (*StatesJson)[Index]->TryGetObject(StateObject) && StateObject)
            {
                ReadBool(**StateObject, TEXT("checkpoint"), Def.States[Index].bCheckpoint);
                ReadBool(**StateObject, TEXT("terminal"), Def.States[Index].bTerminal);
            }
        }
    }

    // Setting rule, same as scripts/validate-contracts.mjs.
    const bool bKnownSetting = Def.Setting == SettingFictional || Def.Setting == SettingRealWorld;
    if (!bKnownSetting)
    {
        UE_LOG(LogPaktiaData, Error, TEXT("Mission '%s' ('%s'): unknown setting '%s'. Not registered."), *Def.Id, *File, *Def.Setting);
        return;
    }
    if (Def.Setting == SettingFictional && !Def.bFictional)
    {
        UE_LOG(LogPaktiaData, Error, TEXT("Mission '%s' ('%s'): setting is 'fictional' but fictional is false. Not registered."), *Def.Id, *File);
        return;
    }
    if (Def.Setting == SettingRealWorld && Def.bFictional)
    {
        UE_LOG(LogPaktiaData, Error, TEXT("Mission '%s' ('%s'): setting is 'real_world_hypothetical' but fictional is true. Not registered."), *Def.Id, *File);
        return;
    }
    if (!Def.Map.IsEmpty())
    {
        const FPaktiaMapDef* MapDef = FindDef(Maps, FName(*Def.Map));
        if (!MapDef)
        {
            UE_LOG(LogPaktiaData, Error, TEXT("Mission '%s' ('%s'): unknown map '%s'. Not registered."), *Def.Id, *File, *Def.Map);
            return;
        }
        if (MapDef->Setting != Def.Setting)
        {
            UE_LOG(LogPaktiaData, Error, TEXT("Mission '%s' ('%s'): map '%s' setting '%s' does not match the mission setting '%s'. Not registered."), *Def.Id, *File, *Def.Map, *MapDef->Setting, *Def.Setting);
            return;
        }
    }

    if (Missions.Contains(FName(*Def.Id)))
    {
        UE_LOG(LogPaktiaData, Warning, TEXT("Duplicate mission id '%s' in '%s'; later file wins."), *Def.Id, *File);
    }
    Missions.Add(FName(*Def.Id), MoveTemp(Def));
}

bool UPaktiaDataSubsystem::GetMission(FName Id, FPaktiaMissionDef& OutMission) const
{
    if (const FPaktiaMissionDef* Found = Missions.Find(Id)) { OutMission = *Found; return true; }
    return false;
}

bool UPaktiaDataSubsystem::GetMap(FName Id, FPaktiaMapDef& OutMap) const
{
    if (const FPaktiaMapDef* Found = Maps.Find(Id)) { OutMap = *Found; return true; }
    return false;
}

bool UPaktiaDataSubsystem::GetAircraft(FName Id, FPaktiaAircraftDef& OutAircraft) const
{
    if (const FPaktiaAircraftDef* Found = Aircraft.Find(Id)) { OutAircraft = *Found; return true; }
    return false;
}

bool UPaktiaDataSubsystem::GetVehicle(FName Id, FPaktiaVehicleDef& OutVehicle) const
{
    if (const FPaktiaVehicleDef* Found = Vehicles.Find(Id)) { OutVehicle = *Found; return true; }
    return false;
}

bool UPaktiaDataSubsystem::GetWeapon(FName Id, FPaktiaWeaponDef& OutWeapon) const
{
    if (const FPaktiaWeaponDef* Found = Weapons.Find(Id)) { OutWeapon = *Found; return true; }
    return false;
}

TArray<FName> UPaktiaDataSubsystem::GetAllMissionIds() const
{
    TArray<FName> Ids;
    Missions.GenerateKeyArray(Ids);
    Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    return Ids;
}
