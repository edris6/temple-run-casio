# AGENTS.md: Architecture breakdown of `LuciusTran/templerun`

> **Scope and purpose.** Educational analysis (school project) of the public repository
> https://github.com/LuciusTran/templerun (commit `65964ad`, shallow clone).
> The repo is a **Unity project reconstructed from the Temple Run 1.43.0 Android APK**
> (original game by Imangi Studios). Meshes, textures, audio and UI art are Imangi's
> copyrighted assets. Use this document to *understand how the project is wired*. Do not copy
> the assets into other projects or present them as your own work.
>
> **Method and confidence.** Based on reading the repo docs, the builder and flow scripts,
> the config JSON and file/folder naming. The `.unity`/`.prefab` YAML was not opened one by one.
> Statements marked *(inferred)* come from naming or Unity conventions, not from reading
> that file. Nothing was run in the Unity editor.

---

## 1. One-paragraph summary

The project has two halves. **`Assets/` is the playable game.** It holds assets exported by
AssetRipper, C# partly translated by hand from Ghidra decompilation of the ARM64 IL2CPP binary, and
a thin authored "host" layer (`Assets/Restoration/`) that wires everything into a working loop.
**Everything else (`DecompiledNative/`, `NativeMapWithAddresses/`, `Evidence/`, `Scripts/`,
`Docs/`) is provenance**: proof of where each recovered piece came from. The rule that governs
the code is: *recovered original logic first, authored glue only where recovery is impossible,
and always labelled which is which.*

Original build: Unity 2022.3.62f2, ARM64 IL2CPP, metadata v31.
Development editor: Unity **6000.4.12f1**. Playable entry scene: `Assets/Restoration/Scenes/TempleRun.unity`.

---

## 2. Repository tree (annotated)

```
templerun/
├── AGENTS.md                       Rules for AI agents working in the repo (evidence-first recovery)
├── README.md                       How to open/play, controls, what is verified
├── .agents/skills/templerun-recovery/SKILL.md   Project skill: how to continue recovery
├── .gitignore / .gitattributes     Excludes Library/, Builds/, APKs, *.so, Ghidra project, etc.; LF line endings
│
├── Assets/                         ← THE PLAYABLE UNITY PROJECT
│   ├── Restoration/                Authored host layer (see §4)
│   ├── Scripts/Assembly-CSharp/    ~475 recovered/stubbed C# game classes (see §5)
│   ├── Resources/                  Everything loaded by string at runtime (see §6)
│   ├── Mesh/                       ~74 raw mesh assets
│   ├── Material/  Shader/  Texture2D/  AnimationClip/  GameObject/   Rendering + animation + UI-atlas prefabs
│   ├── Localization/               15 languages as Unity Localization tables
│   ├── Plugins/                    Third-party SDK DLLs (Firebase, AppLovin, Adjust, IAP, ...)
│   └── Scenes/GamePlay.unity       ORIGINAL scene, kept unchanged as reference
│
├── Packages/                       Unity Package Manager manifest + lock
├── ProjectSettings/                Unity editor/runtime settings
│
├── DecompiledNative/               Ghidra pseudocode (.cpp), one folder per class, plus INDEX.md
├── NativeMapWithAddresses/DiffableCs/   Method declarations with native addresses (no bodies)
├── Evidence/                       ~33 JSON reports: provenance, maps, test results
├── Scripts/                        Python/Java tooling that produced and re-applies the above
└── Docs/                           FLOW-MAP, RECOVERY(-STATUS), UI-RECOVERY, LOGIC-AUDIT, APK-WORKFLOW, screenshots
```

---

## 3. How the project was produced (pipeline)

```
 Temple Run 1.43.0 APK/XAPK  (NOT in repo: gitignored *.apk, *.xapk, /Inputs/)
        │
        ├─► AssetRipper ──► Assets/{Mesh,Material,Texture2D,Resources,Scenes,...}
        │                   + Assets/Scripts/Assembly-CSharp (declaration skeleton)
        │
        ├─► Cpp2IL + global-metadata.dat ──► NativeMapWithAddresses/DiffableCs/*.cs
        │                                    Evidence/il2cpp-native-map.json
        │
        └─► Ghidra headless (Scripts/DecompileGameplay.java)
                         └─► DecompiledNative/<Class>/<Method>_<addr>.cpp  (+ INDEX.md)
                                      │
                      human/agent translation, verified against ARM64
                                      ▼
        Assets/Scripts/Assembly-CSharp/*.cs   (recovered method bodies)
        Evidence/csharp-restoration-map.json  (which body came from which native method)
```

Files outside the repo that this pipeline depends on:

| External thing | Why it matters | Where it lives |
|---|---|---|
| The APK/XAPK | Source of every asset and all native code | Gitignored, kept elsewhere by the author |
| `libil2cpp.so` (ARM64 binary) | Contains the compiled game logic; ground truth for the "927 golden cases" | Gitignored (`*.so`); addresses in `Evidence/il2cpp-native-map.json` |
| `global-metadata.dat` | IL2CPP class/method/string names, version 31 | Inside the APK; layout verified by `Scripts/resolve_ui_literals.py` |
| AssetRipper | Unity asset exporter | Local tool install; not in repo |
| Cpp2IL | IL2CPP → managed-signature dumper | Local tool install; not in repo |
| Ghidra | Decompiler | Local; `/GhidraProject/` is gitignored |
| `reverse-engineer-anything` skill / `LuciusTran/Reverse-Engineer-Game` | Generic APK intake tooling that `Scripts/import_apk.py` wraps (`--rea-root` flag) | Separate GitHub repo |
| Unity 6000.4.12f1 editor | Opens the project, runs `Temple Run → Prepare Project` | Installed via Unity Hub |

---

## 4. `Assets/Restoration/`: the authored host layer

This folder is **new code**, not recovered from the binary. It makes the recovered classes playable.
`Docs/FLOW-MAP.md` calls this the "working implementation" column.

| File | What it does |
|---|---|
| `Scenes/TempleRun.unity` | Playable scene, generated by the builder. Contains one `Temple Run Host` GameObject with `TempleRunFlow`. |
| `TempleRunFlow.cs` (257 lines) | Main MonoBehaviour. Bootstraps, loads `TempleRunCatalog` from Resources, runs the game state machine (home → run → pause → death → retry), plays effects via `Resources.Load<AudioClip>("sound/…")`. |
| `TempleRunFlow.Profile.cs` (263) | Local save/profile and run snapshots. `CreateRunner()` (line ~221) instantiates the chosen character prefab and spawns two `monster_prefab` pursuers. Also sets up music and the water prefab. |
| `TempleRunFlow.Track.cs` (300) | Endless-course scheduler: spawns connected path pieces, corners, holes, obstacles and coins, and prunes the ones behind the runner. The docs mark placement and distribution as *authored* because the original generator wasn't recovered. |
| `TempleRunFlow.UI.cs` (40) | Instantiates `Resources/TempleRunOriginalUI.prefab` and hands it to the presenter. |
| `TempleRunFlow.Verification.cs` (22) | Hooks for the `--temple-test --report <file>` batch mode. |
| `TempleRunRules.cs` (102) | Score, coin-bonus thresholds, boost and power-up timer rules. Parts are compared against ARM64 execution. |
| `TempleRunCatalog.cs` (10) | `ScriptableObject` with parallel arrays: `characters[]`, `characterNames[]`, `characterTextures[]`, `originalCharacterIndices[]`. Instance saved at `Assets/Resources/TempleRunCatalog.asset`. |
| `TempleRunCapture.cs` | Screenshot capture for the graphical smoke test. |
| `TempleRunGoldenChecks.cs` | The "927 golden cases" compared with original ARM64 output. |
| `TempleRunLogicAuditChecks.cs` | 52 source-reviewed logic regressions (see `Docs/LOGIC-AUDIT.md`). |
| `TempleRunPlayerChecks.cs` | Real-player gameplay integration checks (save/continue, purchases, corners, biome heights). |
| `Editor/TempleRunProjectBuilder.cs` (133) | Editor-only. Menu **Temple Run → Prepare Project** (`Prepare()`) builds the catalog, adapts materials, builds the UI and **recreates the scene**. `BuildLinux` makes the Linux player in `Builds/Linux/TempleRun`. |
| `Editor/TempleRunUIBuilder.cs` (235) | Editor-only. Builds `TempleRunOriginalUI.prefab` and the objective-row prefab from recovered hierarchy data. |
| `Editor/TempleRunCoreVerification.cs` | Editor-mode core verification. Output in `Evidence/unity-native-core-verification.json`. |
| `UI/TempleRunUIPresenter.cs` (+ `.Input`, `.Store`, `.Catalogs`) | Binds the original UI prefab to the host. Handles panel navigation, pointer/collider dispatch, store and character-select adapters, and objective/death-caption catalogs. |
| `UI/TempleRunUIWidget.cs`, `TempleRunUIClip.cs` | Compatibility renderer standing in for the original NGUI widgets and clipping. |
| `UI/TempleRunUIAtlasData.cs`, `TempleRunUIFontData.cs` | Data holders for recovered sprite atlases and bitmap-font metrics. |
| `UI/TempleRunUIStrings.cs` | English strings. |
| `UI/TempleRunRecoveredUI.shader` | Shader for the recovered UI. |
| `UI/Data/Atlas_<guid>_<id>.asset` | Atlas data keyed by original asset GUIDs. |
| `UI/TempleRunUIFlowChecks.cs` | The 57 UI checks, with 26 scripted pointer clicks through original colliders. |

**Warning (from the repo's own AGENTS.md):** `Prepare Project` **overwrites the playable scene**, so save customised scenes under another name first.

---

## 5. `Assets/Scripts/Assembly-CSharp/`: recovered game classes

About 475 `.cs` files. The directory name is the original managed assembly (`Assembly-CSharp`). Each file is one original class.
Bodies are either translated from native code or still declarations only. The docs state that *a declaration/stub is not an implementation*.

| Group | Key files | Role |
|---|---|---|
| **Player** | `CharacterPlayer.cs` (451 lines), `CharacterPlayer.Recovered.cs` (66), `CommonPlayer.cs`, `PlayerManager.cs`, `PsuedoRunner.cs` | Jump / slide / stumble state, tilt, boost, wings, invincibility, scoring hooks. The `.Recovered.cs` partial holds native-derived bodies. Fields `CharacterModel`, `AnimateObject`, `RenderObject`, `AttachmentsRenderObjects` are filled by the host in `CreateRunner()`. |
| **World** | `PathElement.cs` (596), `Region2D.cs`, `Obstacle.cs`, `MovingObject*.cs`, `DynamicElement.cs`, `BonusItem.cs`, `Enemy.cs` | Ground tiles, corners, holes, obstacle collision regions, coins and pickups, the pursuers. Prefabs: `temple/path/PathElement.prefab`, `utility/Region2D.prefab`. |
| **Orchestration** | `GameController.cs` (1,284; `public static GameController SharedInstance`), `GameInput`, `PCInput`, `TouchInput`, `GameCamera`, `AudioManager`, `RecordManager` | Original top-level controller (startup, ads/analytics init coroutines, retry), input abstraction, camera, audio, objective registration (56 objectives). Intro/controller orchestration is only partly restored. |
| **UI panels** | `GameGUI`, `GameGUITotem`, `EndGameUI`, `PauseMenu`, `MainMenu`, `StoreGUI`, `StoreItem`, `CharacterSelectGUI`, `BasePanel` | Original screen logic. Callback targets come from serialized fields in the UI prefab. |
| **NGUI (legacy UI toolkit)** | `UIWidget`, `UISprite`, `UILabel`, `UIFilledSprite`, `UISlider`, `UICheckbox`, `UIAnchor`, `UIStack`, `UIButtonMessage(2)`, `UIButtonScale` | Third-party NGUI classes inside the game. Engine behaviour is only partly recovered, so `Restoration/UI` supplies compatibility code. |
| **Config data** | `PlayerConfigData`, `CoinSpawnConfigData`, `ABTest*ConfigData` | Typed config. Values come from `Resources/TempleRunNativeConfig.json`. |
| **Localization** | `TR1LocalizeStringMainMenuButtons`, etc. | Binds UI labels to the localization tables. |

Related folders:
- `Assets/Plugins/*.dll`: precompiled third-party SDKs (Firebase Analytics, Crashlytics, RemoteConfig, Installations; Google Mobile Ads and UMP; AppLovin MAX; Adjust; Unity IAP "Purchasing"; Addressables; Unity Localization; Unity Services; Notifications; Newtonsoft.Json; `__Generated.dll`). **Their source is proprietary and not in the repo.** Per the docs they are unavailable or offline in this build, and the store explicitly reports purchases as unavailable.
- `Assets/Resources/AppLovinSettings.asset` and `Resources/applovin/`: ad-SDK config.
- `Assets/Resources/BillingMode.json`: IAP mode flag.

---

## 6. Where models, textures and data are stored

### 6.1 Models (3D)

1. **Raw meshes**: `Assets/Mesh/*.asset` (Unity YAML `Mesh` objects with sub-meshes and a bind pose). Characters include `Astronaut_mesh`, `Dragon_Queen_mesh` (+ crown), `Nutcracker_mesh`, `Halloween_Werewolf01`, `Halloween_Runner_mesh` (+ hat), `Jean_Benitez_Captian_mesh`, `Jean_Benitez_Frost_Pirate_*`, `Mystic_Monk_Skin_Dragon_Flame_mesh` and `Nidhi_Nirmal01`. Scenery includes `Box01/06/07` and `Object01…08-Pivot`.
2. **Prefabs** under `Assets/Resources/prefabs/`: mesh + material + skeleton + animation.

   | Path | Contents |
   |---|---|
   | `temple/characters/` (85 files) | Default runner `Jhony_Export3_prefab.prefab` ("Guy Dangerous"), `monster_prefab` (pursuers), `extra characters/resources/*_Export1_prefab` (other runners) |
   | `temple/path/` | `PathElement.prefab`, `Decoration.prefab` |
   | `temple/walls/` (288) | Cliff and wall pieces, e.g. `cliffStraightHorz`, `cliffStraightTreeHorz` |
   | `temple/trees/` (85) | `cliff`, `cliff2`, `cliff3`, trees |
   | `temple/temple/` | `temple_prefab`, `anniversary*_temple_prefab` (Low/Medium/High) |
   | `temple/pickups/` | `coinBlue/Red/Yellow_prefab` |
   | `gui/` | `Main Menu Root`, `Character Select GUI Root`, `SaveMeGUI`, `Particle`, ad-debug prefabs |
   | `utility/` | `Region2D`, `Scenery Object`, `Safety Net Visual` |

3. **Exported sub-mesh data**: top-level `Assets/Resources/<name>_export1_prefab_N.asset`, e.g. `bb_…`, `china_…`, `conquis_…` *(inferred from naming: per-character sub-meshes of the "Export" prefabs)*. They are kept in `Resources/` so the prefabs that reference them are bundled and loadable.
4. **Skin textures**: `Assets/Texture2D/` (e.g. `playerTexture.png`, `enemyTexture.png`, portrait sheets). Materials are in `Assets/Material/`; shaders in `Assets/Shader/` (Imangi `Unlit_Opaque`, `MachuLightmap_Opaque VertexFog`, anniversary vertex-displacement).
5. **Animations**: `Assets/AnimationClip/` (Run, Run01…, Jump, Slide Start/Loop, Stumble, Idle, Opening). These are legacy `Animation` clips, not Animator controllers.

### 6.2 How a character model reaches the screen

```
Assets/Scenes/GamePlay.unity   (original; CharacterPlayer.Characters / .Textures arrays)
        │ extracted into
        ▼
Resources/TempleRunCharacterBindings.json
   { originalIndex, prefabGuid, textureGuid, prefab path, texture path, displayName }
        │ read by
        ▼
Editor: TempleRunProjectBuilder.Prepare()
   - finds prefabs in Assets/Resources with a SkinnedMeshRenderer (Jhony forced first)
   - matches each to a binding via CharacterKey(path)
        │ writes
        ▼
Resources/TempleRunCatalog.asset   (TempleRunCatalog ScriptableObject)
        │ Resources.Load<TempleRunCatalog>("TempleRunCatalog")
        ▼
Runtime: TempleRunFlow.CreateRunner()
   - Instantiate(catalog.characters[Profile.character]) under a "Native-derived CharacterPlayer"
   - assign Runner.CharacterModel / AnimateObject / RenderObject
   - clone materials and set mainTexture = catalog.characterTextures[index]
   - spawn 2x prefabs/temple/characters/monster_prefab, 25 units behind
```

### 6.3 Other data

| Data | Location |
|---|---|
| Runner/coin/store/control constants | `Assets/Resources/TempleRunNativeConfig.json` (`CoinSpawn`, `Controls`, `Runner`, `Store`) |
| Store prices (5 upgrade tracks, utilities, 15 characters, 3 wallpapers) | `Store` section of that JSON |
| Death illustrations and captions | `Assets/Resources/deaths/` (~79 files, per-character and per-death-type PNGs + material) |
| Sounds / music | `Assets/Resources/sound/` (wav), `Assets/Resources/music/` (ogg) |
| Bitmap fonts | `Resources/Arial_Rounded_White.txt`, `Cheboygan_*2x.txt`, `Assets/GameObject/Font *.prefab` |
| NGUI materials | `Resources/ngui materials/*.mat` (Countdown, Effects, EndGame, Font, Game Interface, Tutorial, NoAds) |
| UI atlases | `Assets/GameObject/*Atlas.prefab`, `Assets/Material/*`, `Restoration/UI/Data/Atlas_*.asset` |
| Localization | `Assets/Localization/*.asset` (15 locales; `TempleRunLocTable_<lang>` + shared data), `Resources/config/LanguageMap.json` |
| Player save data | Written at runtime by `TempleRunFlow.Profile.cs`: local profile and run snapshot |

---

## 7. Unity-proprietary and "invisible" connections

Several things make this project work that are **not visible as ordinary code files**.

1. **`.meta` files and GUIDs.** Every asset has a sibling `.meta` with a 32-hex `guid`, e.g. `Jhony_Export3_prefab.prefab.meta` has `guid: 73c879a1686f8f04f81ad97ac97972e9`. Prefabs, scenes and materials refer to each other by GUID and fileID, never by path. That is why `AGENTS.md` insists: *"preserve GUIDs during refactoring."* Moving or deleting a `.meta` silently breaks references, and `Evidence/generated-missing-meta.json` records ones that had to be generated.
2. **Unity YAML serialization.** `.unity`, `.prefab`, `.mat`, `.asset` and `.anim` are YAML (`%YAML 1.1`, `!u!43` = Mesh). The MonoBehaviour fields serialized in them (e.g. `UIButtonMessage` target and function name) are the real wiring for UI callbacks. `.gitattributes` forces LF line endings on them to keep diffs stable.
3. **`Resources/` folder magic.** Anything under any `Resources/` is bundled and loadable by string path (`Resources.Load("prefabs/temple/characters/monster_prefab")`). The code depends on these path strings, so renaming a folder breaks runtime loading with no compile error.
4. **ScriptableObjects as glue.** `TempleRunCatalog.asset` is generated by the editor script and read at runtime. It does not exist until **Prepare Project** has run.
5. **Editor vs runtime split.** `Assets/Restoration/Editor/` only compiles into the editor (Unity "Editor" folder rule). It uses `AssetDatabase`, `MenuItem` and `EditorSceneManager`, none of which exist in a built player.
6. **Assembly names.** `Assembly-CSharp` and `Assembly-CSharp-firstpass` are Unity's default script assemblies. Original plugins in `Assets/Plugins/*.dll` are referenced by managed assembly name and GUID.
7. **Packages.** `Packages/manifest.json` pulls only Unity built-in modules (`com.unity.modules.*`), the Linux toolchain/SDK packages and `multiplayer.center`. `packages-lock.json` pins resolved versions.
8. **ProjectSettings.** `ProjectVersion.txt` pins the editor. `TagManager`, `InputManager`, `QualitySettings`, `GraphicsSettings` and `ProjectSettings.asset` hold layers, input axes, quality tiers and build/player settings. Layers and tags used by the colliders live here.
9. **Lighting.** `Assets/Scenes/GamePlay/LightingData.asset` and `Settings.lighting.lighting` are baked lightmap data for the original scene.
10. **IL2CPP.** The shipped game is C# compiled to C++ and then to ARM64. Original method bodies exist only as machine code in `libil2cpp.so`. This is why `DecompiledNative/*.cpp` is Ghidra *pseudocode*, and why native addresses come in three coordinate systems (Cpp2IL file offset, ELF linked address, Ghidra import address) that must not be converted with a blind delta.
11. **Proprietary SDK services.** Firebase, AppLovin MAX, Adjust, Unity IAP and Google Mobile Ads call remote servers in the original game. Here they are present as DLLs but treated as offline.

---

## 8. Provenance files: what each does

| Path | Purpose |
|---|---|
| `DecompiledNative/<Class>/<Method>_<addr>.cpp` | Ghidra pseudocode with method-byte verification. About 600 files, e.g. `GameController` 154, `PathElement` 71, `EndGameUI` 39, `CharacterPlayer` 38. |
| `DecompiledNative/INDEX.md` | Table: class, signature, file offset, link to the `.cpp`. |
| `NativeMapWithAddresses/DiffableCs/` | Declarations only (no playable code), with addresses, for diffing against recovered `.cs`. |
| `Evidence/il2cpp-native-map.json` | Method → native address map. |
| `Evidence/csharp-restoration-map.json`, `ui-csharp-restoration.json` | Which C# bodies were restored from which native methods. |
| `Evidence/native-decompilation.json`, `ui-native-decompilation.json` | Scope and limits of the decompilation. |
| `Evidence/native-constants.json`, `config-provenance.json` | Origin of numbers in `TempleRunNativeConfig.json`. |
| `Evidence/input-provenance.json`, `apk-inventory.json`, `rea-input-inspection.json` | APK identity (hashes) and contents. Check these before native analysis. |
| `Evidence/asset-export.json`, `unity-export-inspection.json`, `unity-upgrade.json` | AssetRipper export results and the upgrade to the Unity 6 editor. |
| `Evidence/ui-recovery.json`, `ui-source-evidence.json`, `ui-native-string-literals.json`, `ui-english-strings.json`, `ui-objective-catalog.json` | UI hierarchy, literals, English strings and the 56-objective list. |
| `Evidence/material-adaptations.json` | Shader/material changes made so the assets render in Unity 6. |
| `Evidence/native-logic-verification.json`, `unity-native-core-verification.json`, `player-smoke.json`, `player-graphical-smoke.json`, `player-graphical-capture.json`, `ui-flow.json`, `ui-verification.json`, `ui-project-verification.json`, `final-project-inspection.json`, `logic-audit.json`, `playable-restoration.json`, `environment-recovery.json`, `apk-automation-verification.json` | Test and verification reports. Their README claims (927 ARM64 golden cases, 52 logic regressions, 57 UI checks) come from these. |
| `Scripts/import_apk.py` | Wrapper that runs the external REA tool on a new APK into an *external* workspace. |
| `Scripts/export_assets.py` | AssetRipper/Cpp2IL driver. |
| `Scripts/build_native_map.py` | Builds `NativeMapWithAddresses/DiffableCs`. |
| `Scripts/DecompileGameplay.java` | Ghidra headless script that writes `DecompiledNative/`. |
| `Scripts/export_ui_evidence.py`, `recover_ui_catalogs.py`, `resolve_ui_literals.py` | UI evidence, catalog and string-literal recovery. |
| `Scripts/restore_gameplay.py`, `restore_ui.py` | Re-apply reviewed bodies to the C# files (confirmed present). The docs warn they can overwrite developer edits. |
| `Scripts/verify_native_logic.py` | Runs recovered logic against the original ARM64 code with the Unicorn emulator (`unicorn` Python package). Produces `Evidence/native-logic-verification.json`. |
| `Docs/FLOW-MAP.md` | Table mapping each responsibility to its original owner class, the working implementation and the boundary. |
| `Docs/RECOVERY-STATUS.md`, `RECOVERY.md` | Current gaps and method. |
| `Docs/UI-RECOVERY.md`, `LOGIC-AUDIT.md`, `APK-WORKFLOW.md` | UI provenance, corrected discrepancies, importer usage. |

---

## 9. Runtime flow (end to end)

```
Unity opens Assets/Restoration/Scenes/TempleRun.unity
  └─ "Temple Run Host" → TempleRunFlow.Awake/Start
       ├─ Resources.Load<TempleRunCatalog>            (fails with "Run Temple Run > Prepare Project" if missing)
       ├─ load profile / config JSON
       ├─ TempleRunFlow.UI: Instantiate(Resources "TempleRunOriginalUI")
       │     └─ TempleRunUIPresenter binds original panels (MainMenu, GameGUI, PauseMenu, EndGameUI, StoreGUI, CharacterSelectGUI)
       │         └─ UIButtonMessage callbacks fire via collider raycast → recovered panel methods
       ├─ Home: player clicks the PLAY! idol
       ├─ CreateRunner(): character prefab + CharacterPlayer + 2 pursuers
       ├─ Track scheduler spawns PathElements, corners, obstacles, coins (Region2D collision predicates)
       ├─ Per frame: input adapter → CharacterPlayer (jump/slide/stumble/turn) → TempleRunRules (score, coins, power-ups)
       ├─ Death → EndGameUI shows character/death-specific illustration + caption (Resources/deaths)
       └─ Revive (wings) / retry / home; run snapshot saved locally
```

Controls (desktop): turn = arrows/Q/E or swipe, tilt = A/D, jump = Space/W/Up, slide = S/Down,
pause = P/Esc, wings = F or double-tap. Dev helpers: `C` continue snapshot, `R` revive, `1/2/3` biome.

---

## 10. Known gaps (from the repo's own docs)

- Objective completion rules, progression and lifetime counters are unresolved. The 56 objectives are registered but cannot complete.
- Camera and guardian choreography is partial. Intro/controller orchestration is partial.
- Course placement and distribution is authored, not the original algorithm.
- NGUI engine behaviour is only partly recovered, and store scrolling is a partial compatibility layer.
- Android and physical-input behaviour are not verified. Remote payments and online services are offline.

---

## 11. Suggested reading order for your own project

1. `Docs/FLOW-MAP.md`, then `Docs/RECOVERY-STATUS.md`
2. `Assets/Restoration/TempleRunFlow.cs` and `.Profile.cs`: how a game loop is composed
3. `Assets/Restoration/Editor/TempleRunProjectBuilder.cs`: editor automation and the catalog pattern
4. `Assets/Scripts/Assembly-CSharp/CharacterPlayer.cs` and `.Recovered.cs`
5. `PathElement.cs`, `Region2D.cs`: geometry and collision
6. `Resources/TempleRunNativeConfig.json` and `TempleRunCharacterBindings.json`: data-driven design
7. One `.meta` file and one `.prefab` YAML: to see GUID-based linking first-hand
