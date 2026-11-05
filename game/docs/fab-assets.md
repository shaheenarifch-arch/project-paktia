# Fab / Megascans asset checklist

Real art and audio only: no placeholders, no drawn art. Shaheen adds the free packs below to his
Fab library, adds them to the project, then points `Config/HavocAssets.ini` at the imported assets.
`UHavocAssetSubsystem` (`Source/MidnightHavoc/Assets/`) reads that file, so swapping a pack is
config-only.

Help:

- Using Fab in Unreal Engine: <https://dev.epicgames.com/documentation/en-us/fab/using-fab-in-unreal-engine>
- Fab overview: <https://dev.epicgames.com/documentation/en-us/fab/fab-overview>
- Quixel Megascans in UE: <https://dev.epicgames.com/documentation/en-us/fab/using-megascans-in-unreal-engine>
- Referencing assets by path: <https://dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine>

## How to add a pack

1. Open the link, sign in with the Epic account, and click **Add to My Library** (only items
   marked **Free** or with a free Megascans/Quixel licence).
2. In the editor open **Window > Fab** (or the Fab plugin), find the pack under **My Library**
   and click **Add to Project** (Megascans: use **Quixel Bridge/Fab** export).
3. Move or import it into `Content/ThirdParty/<Pack>` (target folders below).
4. Right-click the asset > **Copy Reference**, paste the path into `Config/HavocAssets.ini`
   next to the matching role.

`Content/ThirdParty/`, `Content/Megascans/` and `Content/MSPresets/` are git-ignored; do not
commit marketplace assets (licences usually forbid redistribution). Binary asset types that do
get committed (`.uasset`, `.umap`, `.uexp`, `.ubulk`, `.wav`, ...) are Git LFS via `.gitattributes`.

## Links

The links are Fab searches so they never go stale; pick the top free result by the named
publisher (Quixel Megascans, Epic Games, or a Free-badged listing) and check the price reads
**Free** and the licence reads **Standard / Fab licence** before adding it. Prices and
listings change; re-check before adding.

Licence note for all rows: Megascans on Fab are free for use in Unreal Engine projects;
other listings use the Fab Standard License (personal/commercial use in a game, no
redistribution of the raw assets). Read each listing's licence box.

| Group | Pack to find | Why it is needed | Role keys (`HavocAssets.ini`) | Target folder |
| --- | --- | --- | --- | --- |
| Terrain | [Megascans desert sand surfaces](https://www.fab.com/search?q=megascans%20desert%20sand&sort_by=-relevance&listing_types=material) | Kharg-style coastal ground | `Terrain.Sand` | `Content/ThirdParty/Megascans/Surfaces` |
| Terrain | [Megascans rock surfaces and cliffs](https://www.fab.com/search?q=megascans%20rock%20cliff) | Cliffs, shoreline, rock faces | `Terrain.Rock`, `Props.Rocks` | `Content/ThirdParty/Megascans/3D_Assets` |
| Scrub | [Desert scrub / dry plants](https://www.fab.com/search?q=desert%20scrub%20plants&listing_types=3d-model) | Sparse vegetation | `Foliage.Scrub` | `Content/ThirdParty/Megascans/3D_Plants` |
| Industrial | [Oil storage tanks](https://www.fab.com/search?q=industrial%20storage%20tank%20free) | Terminal tank farm | `Props.Tank` | `Content/ThirdParty/Industrial/Tanks` |
| Industrial | [Industrial pipes](https://www.fab.com/search?q=industrial%20pipes%20free) | Terminal pipework | `Props.Pipes` | `Content/ThirdParty/Industrial/Pipes` |
| Industrial | [Pier / jetty / dock](https://www.fab.com/search?q=pier%20dock%20jetty%20free) | Loading pier | `Props.Pier` | `Content/ThirdParty/Industrial/Pier` |
| Industrial | [Shipping containers](https://www.fab.com/search?q=shipping%20container%20free) | Yard cover | `Props.Container` | `Content/ThirdParty/Industrial/Containers` |
| Industrial | [Fencing / barriers](https://www.fab.com/search?q=chain%20link%20fence%20free) | Perimeter | `Props.Fence` | `Content/ThirdParty/Industrial/Fencing` |
| Character | [Soldier character + animations](https://www.fab.com/search?q=soldier%20character%20animations%20free) | Playable marine and rigged animations (look for a UE5 mannequin-compatible rig) | `Character.Marine`, `Character.Marine.AnimBP` | `Content/ThirdParty/Soldier` |
| Weapons | [Assault rifle (rigged/animated)](https://www.fab.com/search?q=rifle%20weapon%20free) | Weapon for the KR-47 role | `Weapon.KR47` | `Content/ThirdParty/Weapons/Rifle` |
| Weapons | [Weapon FX (muzzle flash, tracers, impacts)](https://www.fab.com/search?q=weapon%20fx%20muzzle%20niagara%20free) | Firing feedback | `FX.Muzzle` | `Content/ThirdParty/WeaponFX` |
| Aircraft | [Helicopter](https://www.fab.com/search?q=helicopter%20free) | Falcon helicopter | `Aircraft.Falcon` | `Content/ThirdParty/Helicopter` |
| Vessels | [Small boats](https://www.fab.com/search?q=small%20boat%20free) | Strait traffic | `Vessel.SmallBoat` | `Content/ThirdParty/Boats` |
| Vessels | [Oil tanker ship](https://www.fab.com/search?q=oil%20tanker%20ship%20free) | Tanker escort | `Vessel.Tanker` | `Content/ThirdParty/Boats` |
| FX | [Explosions](https://www.fab.com/search?q=explosion%20niagara%20free) | Terminal and hazard hits | `FX.Explosion.Large` | `Content/ThirdParty/VFX` |
| FX | [Smoke and fire](https://www.fab.com/search?q=smoke%20fire%20niagara%20free) | Damage states, crash | `FX.Smoke` | `Content/ThirdParty/VFX` |
| FX | [Water splash / ocean FX](https://www.fab.com/search?q=water%20splash%20niagara%20free) | Boats, impacts (the Water plugin already provides the sea) | `FX.Water.Splash` | `Content/ThirdParty/VFX` |
| Audio | [Ambient sea / wind / industrial](https://www.fab.com/search?q=ambient%20sound%20sea%20wind%20free&listing_types=audio) | Level atmosphere | `Audio.Ambient.Coast` | `Content/ThirdParty/Audio/Ambient` |
| Audio | [Weapon and explosion sounds](https://www.fab.com/search?q=weapon%20sound%20effects%20free&listing_types=audio) | Gunfire and blasts | `Audio.Weapon.Rifle`, `Audio.Explosion` | `Content/ThirdParty/Audio/Weapons` |

## Rules

- Real assets only. If a role has no pack yet, it stays unmapped (null + `LogHavocAssets`
  warning); never substitute drawn or generated placeholders.
- Episode 04 keeps `real_world_hypothetical` rules: no real people, no real equipment specs
  or targeting data; Aegis stays abstract. Role names such as `Weapon.KR47` and
  `Aircraft.Falcon` are the game's fictional names, whatever the asset looks like.
- ISIS stays AI-only.

## Using the roles from C++

```cpp
if (UHavocAssetSubsystem* Assets = GetGameInstance()->GetSubsystem<UHavocAssetSubsystem>())
{
    UNiagaraSystem* Boom = Assets->LoadAs<UNiagaraSystem>(TEXT("FX.Explosion.Large"));
}
```

(`UNiagaraSystem` needs the `Niagara` module in `MidnightHavoc.Build.cs`; `LoadAs` works for any
`UObject` type already linked.) Reference: <https://dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine>
