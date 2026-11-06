#include "HavocDataSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

DEFINE_LOG_CATEGORY(LogHavocData);

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

void UHavocDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadAll();
}

void UHavocDataSubsystem::LoadAll()
{
    // Search order: the DataDirectory override; the repo's data/ folder (<ProjectDir>/../data, used in the
    // editor); then Content/HavocData, the copy that packaged builds ship (made by the PreBuildSteps in
    // MidnightHavoc.uproject and staged by DefaultGame.ini). See game/README.md "Packaged builds".
    TArray<FString> Candidates;
    if (!DataDirectory.IsEmpty())
    {
        Candidates.Add(DataDirectory);
    }
    // ProjectDir() is relative in packaged builds (../../../MidnightHavoc/), so make these absolute here;
    // otherwise the loop below prefixes ProjectDir() a second time and looks five folders too high.
    Candidates.Add(FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("../data")));
    Candidates.Add(FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()), TEXT("HavocData")));

    TArray<FString> Tried;
    ResolvedDataDirectory.Reset();
    for (FString Dir : Candidates)
    {
        if (FPaths::IsRelative(Dir))
        {
            Dir = FPaths::Combine(FPaths::ProjectDir(), Dir);
        }
        FPaths::CollapseRelativeDirectories(Dir);
        Dir = FPaths::ConvertRelativePathToFull(Dir);
        Tried.Add(Dir);
        if (FPaths::DirectoryExists(Dir))
        {
            ResolvedDataDirectory = Dir;
            break;
        }
    }

    if (ResolvedDataDirectory.IsEmpty())
    {
        UE_LOG(LogHavocData, Error, TEXT("No game data directory found (tried: %s). No missions or maps loaded. Set DataDirectory under [/Script/MidnightHavoc.HavocDataSubsystem] in DefaultGame.ini, or see game/README.md \"Packaged builds\"."), *FString::Join(Tried, TEXT(", ")));
        return;
    }
    UE_LOG(LogHavocData, Display, TEXT("Loading game data from '%s'."), *ResolvedDataDirectory);

    // Maps first: mission validation needs them.
    LoadFolder(TEXT("maps"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterMap(J, F); });
    LoadFolder(TEXT("aircraft"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterAircraft(J, F); });
    LoadFolder(TEXT("vehicles"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterVehicle(J, F); });
    LoadFolder(TEXT("weapons"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterWeapon(J, F); });
    LoadFolder(TEXT("units"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterUnits(J, F); });
    LoadFolder(TEXT("missions"), [this](const TSharedRef<FJsonObject>& J, const FString& F) { RegisterMission(J, F); });

    UE_LOG(LogHavocData, Log, TEXT("Loaded game data from '%s': %d missions, %d maps, %d aircraft, %d vehicles, %d weapons, %d units."),
        *ResolvedDataDirectory, Missions.Num(), Maps.Num(), Aircraft.Num(), Vehicles.Num(), Weapons.Num(), Units.Num());
}

void UHavocDataSubsystem::LoadFolder(const FString& SubFolder, TFunctionRef<void(const TSharedRef<FJsonObject>&, const FString& File)> Handler)
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
            UE_LOG(LogHavocData, Warning, TEXT("Could not read '%s'; skipping."), *File);
            continue;
        }

        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
        {
            UE_LOG(LogHavocData, Warning, TEXT("Could not parse JSON in '%s'; skipping."), *File);
            continue;
        }

        Handler(Json.ToSharedRef(), File);
    }
}

void UHavocDataSubsystem::RegisterMap(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FHavocMapDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogHavocData, Warning, TEXT("'%s' is not a valid map definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    if (Def.Setting.IsEmpty())
    {
        Def.Setting = SettingFictional;
    }
    if (Maps.Contains(FName(*Def.Id)))
    {
        UE_LOG(LogHavocData, Warning, TEXT("Duplicate map id '%s' in '%s'; later file wins."), *Def.Id, *File);
    }
    Maps.Add(FName(*Def.Id), MoveTemp(Def));
}

void UHavocDataSubsystem::RegisterAircraft(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FHavocAircraftDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogHavocData, Warning, TEXT("'%s' is not a valid aircraft definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    ReadBool(*Json, TEXT("enabled"), Def.bEnabled);
    ReadString(*Json, TEXT("class"), Def.ClassName);
    Aircraft.Add(FName(*Def.Id), MoveTemp(Def));
}

void UHavocDataSubsystem::RegisterVehicle(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FHavocVehicleDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogHavocData, Warning, TEXT("'%s' is not a valid vehicle definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    ReadBool(*Json, TEXT("enabled"), Def.bEnabled);
    ReadString(*Json, TEXT("class"), Def.ClassName);
    Vehicles.Add(FName(*Def.Id), MoveTemp(Def));
}

void UHavocDataSubsystem::RegisterWeapon(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FHavocWeaponDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogHavocData, Warning, TEXT("'%s' is not a valid weapon definition (parse failed or missing id); skipping."), *File);
        return;
    }
    ReadBool(*Json, TEXT("fictional"), Def.bFictional);
    ReadBool(*Json, TEXT("enabled"), Def.bEnabled);
    Weapons.Add(FName(*Def.Id), MoveTemp(Def));
}

void UHavocDataSubsystem::RegisterUnits(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (!Json->TryGetArrayField(TEXT("units"), Array) || !Array)
    {
        UE_LOG(LogHavocData, Warning, TEXT("'%s' has no units array; skipping."), *File);
        return;
    }
    auto Rating = [](const TSharedPtr<FJsonObject>& Object, const TCHAR* Group, const TCHAR* Key, int32 Default)
    {
        const TSharedPtr<FJsonObject>* Child = nullptr;
        int32 Value = Default;
        if (Object->TryGetObjectField(Group, Child) && Child)
        {
            (*Child)->TryGetNumberField(Key, Value);
        }
        return Value;
    };
    for (const TSharedPtr<FJsonValue>& Item : *Array)
    {
        const TSharedPtr<FJsonObject> Object = Item.IsValid() ? Item->AsObject() : nullptr;
        FHavocUnitDef Def;
        if (!Object.IsValid() || !Object->TryGetStringField(TEXT("id"), Def.Id) || Def.Id.IsEmpty())
        {
            continue;
        }
        ReadString(*Object, TEXT("name"), Def.Name);
        ReadString(*Object, TEXT("faction"), Def.Faction);
        ReadString(*Object, TEXT("branch"), Def.Branch);
        ReadString(*Object, TEXT("role"), Def.Role);
        Object->TryGetNumberField(TEXT("crew"), Def.Crew);
        Def.AntiInfantry = Rating(Object, TEXT("firepower"), TEXT("antiInfantry"), Def.AntiInfantry);
        Def.Optics = Rating(Object, TEXT("sensors"), TEXT("optics"), Def.Optics);
        Object->TryGetNumberField(TEXT("stealth"), Def.Stealth);
        const FName Key(*Def.Id);
        if (!Units.Contains(Key))
        {
            UnitOrder.Add(Key);
        }
        Units.Add(Key, MoveTemp(Def));
    }
}

void UHavocDataSubsystem::RegisterMission(const TSharedRef<FJsonObject>& Json, const FString& File)
{
    FHavocMissionDef Def;
    if (!ParseStruct(Json, Def) || Def.Id.IsEmpty())
    {
        UE_LOG(LogHavocData, Warning, TEXT("'%s' is not a valid mission definition (parse failed or missing id); skipping."), *File);
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
        UE_LOG(LogHavocData, Error, TEXT("Mission '%s' ('%s'): unknown setting '%s'. Not registered."), *Def.Id, *File, *Def.Setting);
        return;
    }
    if (Def.Setting == SettingFictional && !Def.bFictional)
    {
        UE_LOG(LogHavocData, Error, TEXT("Mission '%s' ('%s'): setting is 'fictional' but fictional is false. Not registered."), *Def.Id, *File);
        return;
    }
    if (Def.Setting == SettingRealWorld && Def.bFictional)
    {
        UE_LOG(LogHavocData, Error, TEXT("Mission '%s' ('%s'): setting is 'real_world_hypothetical' but fictional is true. Not registered."), *Def.Id, *File);
        return;
    }
    if (!Def.Map.IsEmpty())
    {
        const FHavocMapDef* MapDef = FindDef(Maps, FName(*Def.Map));
        if (!MapDef)
        {
            UE_LOG(LogHavocData, Error, TEXT("Mission '%s' ('%s'): unknown map '%s'. Not registered."), *Def.Id, *File, *Def.Map);
            return;
        }
        if (MapDef->Setting != Def.Setting)
        {
            UE_LOG(LogHavocData, Error, TEXT("Mission '%s' ('%s'): map '%s' setting '%s' does not match the mission setting '%s'. Not registered."), *Def.Id, *File, *Def.Map, *MapDef->Setting, *Def.Setting);
            return;
        }
    }

    if (Missions.Contains(FName(*Def.Id)))
    {
        UE_LOG(LogHavocData, Warning, TEXT("Duplicate mission id '%s' in '%s'; later file wins."), *Def.Id, *File);
    }
    Missions.Add(FName(*Def.Id), MoveTemp(Def));
}

bool UHavocDataSubsystem::GetMission(FName Id, FHavocMissionDef& OutMission) const
{
    if (const FHavocMissionDef* Found = Missions.Find(Id)) { OutMission = *Found; return true; }
    return false;
}

bool UHavocDataSubsystem::GetMap(FName Id, FHavocMapDef& OutMap) const
{
    if (const FHavocMapDef* Found = Maps.Find(Id)) { OutMap = *Found; return true; }
    return false;
}

bool UHavocDataSubsystem::GetAircraft(FName Id, FHavocAircraftDef& OutAircraft) const
{
    if (const FHavocAircraftDef* Found = Aircraft.Find(Id)) { OutAircraft = *Found; return true; }
    return false;
}

bool UHavocDataSubsystem::GetVehicle(FName Id, FHavocVehicleDef& OutVehicle) const
{
    if (const FHavocVehicleDef* Found = Vehicles.Find(Id)) { OutVehicle = *Found; return true; }
    return false;
}

bool UHavocDataSubsystem::GetWeapon(FName Id, FHavocWeaponDef& OutWeapon) const
{
    if (const FHavocWeaponDef* Found = Weapons.Find(Id)) { OutWeapon = *Found; return true; }
    return false;
}

bool UHavocDataSubsystem::GetUnit(FName Id, FHavocUnitDef& OutUnit) const
{
    if (const FHavocUnitDef* Found = Units.Find(Id)) { OutUnit = *Found; return true; }
    return false;
}

TArray<FHavocUnitDef> UHavocDataSubsystem::GetInfantryUnits(const FString& Faction) const
{
    TArray<FHavocUnitDef> Result;
    for (const FName& Key : UnitOrder)
    {
        const FHavocUnitDef& Unit = Units[Key];
        if (Unit.Faction == Faction && Unit.Branch == TEXT("infantry"))
        {
            Result.Add(Unit);
        }
    }
    return Result;
}

TArray<FName> UHavocDataSubsystem::GetAllMissionIds() const
{
    TArray<FName> Ids;
    Missions.GenerateKeyArray(Ids);
    Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    return Ids;
}
