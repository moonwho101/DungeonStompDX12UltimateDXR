<div align="center">

# Dungeon Stomp DX12 Ultimate DXR

### A Deterministic 3D Dungeon Crawler Engine Showcase for DirectX 12 Ultimate & DXR

[![License](https://img.shields.io/github/license/moonwho101/DungeonStompDX12UltimateDXR?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue?style=flat-square&logo=windows)](https://github.com/moonwho101/DungeonStompDX12UltimateDXR)
[![DirectX](https://img.shields.io/badge/DirectX-12%20Ultimate-green?style=flat-square&logo=microsoft)](https://devblogs.microsoft.com/directx/announcing-directx-12-ultimate/)
[![C++](https://img.shields.io/badge/language-C%2B%2B-orange?style=flat-square&logo=cplusplus)](https://github.com/moonwho101/DungeonStompDX12UltimateDXR)
[![Visual Studio](https://img.shields.io/badge/VS-2022-purple?style=flat-square&logo=visualstudio)](https://visualstudio.microsoft.com/)

![Dungeon Stomp DX12 DXR](Textures/screenshot52.jpg)

**Most DXR samples stop at a spinning triangle or a Cornell box. Dungeon Stomp is a full, playable, deterministic dungeon crawler engine that puts DirectX 12 Ultimate's headline features to work in a live production codebase — featuring DXR 1.1 inline ray tracing, PBR, Variable Rate Shading, deterministic simulation, and SSAO.**

[Play Now](#quick-start) · [Dungeon Generator](#dungeon-generator) · [Features](#features) · [Screenshots](#screenshots) · [Controls](#controls) · [Deterministic Simulation & Demos](#deterministic-simulation-and-demos) · [Build](#build-from-source) · [Repository](#repository-structure) · [Credits](#credits)

</div>

---

<a id="quick-start"></a>
## ⚡ Quick Start

> **Play immediately** — a pre-compiled `DungeonStomp.exe` is included in `bin/`.

```bash
git clone https://github.com/moonwho101/DungeonStompDX12UltimateDXR.git
cd DungeonStompDX12UltimateDXR/bin
DungeonStomp.exe
```

Run the game from the `bin/` folder; it loads its levels, sounds and shaders using paths relative to that directory.

*Requirements: Windows 10/11 with a DirectX 12 GPU. A DXR 1.1-capable GPU (NVIDIA RTX / AMD RX 6000+ / Intel Arc) is needed for ray tracing (`R`); without one the game falls back to the rasterized renderer. Variable Rate Shading (`T`) also requires hardware support.*

---

<a id="dungeon-generator"></a>
## 🎮 Dungeon Generator (Press F7)

![Procedural-Dungeon-Generation](Textures/screenshot50.jpg)

Press `F7` to open the settings panel, choose a seed (`0` = random) and tile count, then click **Generate Classic Dungeon** or **Generate Enhanced Dungeon**. The layout is written to `level1.map` and loaded immediately. Press `F7` again to close the panel and return to play.

---

<a id="features"></a>
## ✨ Features

### 🚀 Graphics & Engine (DX12 Ultimate)
- **DXR 1.1 Inline Ray Tracing:** `RayQuery` shadow rays, plus a 2-sample single-bounce indirect diffuse (GI) approximation.
- **PBR Material Pipeline:** Cook-Torrance BRDF with a metallic workflow, 28 tuned materials (`bin/materials.txt`) and ACES tone mapping.
- **Variable Rate Shading (VRS):** Optional hardware shading-rate control for performance.
- **Lighting & Effects:** 2048x2048 shadow map, SSAO, up to 32 dynamic lights per scene, normal mapping and atmospheric fog.
- **Engine Tech:** 3-frame buffered rendering, spatial culling, BMFont GPU text rendering, Dear ImGui settings panel and an XAudio2 sound engine.

### ⚔️ Game
- **16 Dungeon Levels:** Hand-crafted campaign levels plus a seed-based procedural dungeon generator.
- **25+ Enemy Types:** Animated MD2 & 3DS monsters with AI, audio cues and loot drops.
- **Weapons & Spells:** Up to 30 weapon/spell slots covering melee, scrolls and a missile system (up to 100 active missiles).
- **Deterministic Simulation:** Seeded PRNG sequence, fixed game-step logic, and deterministic physics & AI ensure 100% reproducible gameplay.
- **Classic RPG Mechanics:** Dice-based combat, level progression, XP, keys, swinging doors, save/load (`F5`/`F6`) and deterministic demo record/playback (`F2`/`F3`).

---

<a id="screenshots"></a>
## 🖼️ Screenshots

<div align="center">

| Real-Time DXR Ray-Traced Shadows | Dynamic Combat Encounter |
|:---:|:---:|
| ![Ray-traced dungeon scene](Textures/screenshot52.jpg) | ![Combat encounter](Textures/screenshot53.jpg) |

<details>
<summary><b>📷 Click to view more screenshots</b></summary>

<br>

| | |
|---|---|
| ![Screenshot](Textures/screenshot23.jpg) | ![Screenshot](Textures/screenshot22.jpg) |
| ![Screenshot](Textures/screenshot25.jpg) | ![Screenshot](Textures/screenshot26.jpg) |

</details>

</div>

---

<a id="controls"></a>
## 🎮 Controls

| Action | Input | Action | Input |
|---|---|---|---|
| **Look / Turn** | `Mouse` | **Cycle Weapons** | `Q` next / `Z` previous, or `Mouse Wheel` |
| **Move Forward / Back** | `W` / `S` (or `Right Click` to move forward) | **Load / Save** | `F5` / `F6` |
| **Strafe** | `A` / `D` | **Record / Play Demo** | `F2` / `F3` |
| **Attack** | `Left Click` | **Settings Panel (Generator & Toggles)** | `F7` |
| **Jump** | `E` | **On-Screen Debug Stats** | `F8` |
| **Open Doors** | `Space` | **Fullscreen (borderless)** | `Alt`+`Enter` or `F11` |
| **Respawn after death** | `Space` | **Quit** | `Esc` |

An Xbox controller is supported but disabled by default; set `g_bUseJoystick` to `true` (`g_bUseMouse` and `g_bUseKeyboard`  to `false`) in `src/DirectInput.cpp` and rebuild.

<details>
<summary><b>🔧 Developer & Feature Hotkeys (Click to expand)</b></summary>

<br>

| Key | Graphics Toggle | Key | Gameplay / Developer |
|:---:|---|:---:|---|
| `R` | DXR Ray Tracing | `G` | Gravity on/off (fly with Numpad `+` / `-` when off) |
| `T` | Variable Rate Shading | `X` | Add 10 experience points |
| `O` | SSAO | `K` | Unlock all weapons & spells |
| `J` | Shadow Map | `]` / `[` | Next / previous level |
| `N` | Normal Maps | `I` | Music on/off |
| `M` | Shadow Map overlay | `P` | Play a random song |
| `V` | VSync | `B` | Camera head bob |
| `H` | Player HUD | | |

</details>

---

<a id="deterministic-simulation-and-demos"></a>
## 🎲 Deterministic Simulation & Demo System

Dungeon Stomp is designed as a **fully deterministic game simulation**. All core game logic—including monster AI pathfinding and target decisions, combat dice rolls, projectile physics, particle behavior, and procedural dungeon generation—runs on a fixed tick clock and a deterministic pseudo-random number generator (PRNG) sequence.

Because every state update is deterministic and reproducible, the engine features a classic Quake/Doom-style **Demo Recording & Playback System**:

- **`F2` Record:** Saves an initial world snapshot to `demo.sav` and records raw frame-by-frame player inputs. Pressing `F2` again stops recording and writes `demo.dem`.
- **`F3` Playback:** Restores the exact starting snapshot and feeds the recorded inputs back through the deterministic simulation tick loop.
- **Exact Parity:** Every monster AI choice, combat hit roll, missile trajectory, and loot drop unfolds with 100% mathematical accuracy compared to the original play session.
- **Deterministic Seed Generation:** Procedural dungeons (`F7` panel or `tools/generate_dungeon.py`) use fixed seed values, ensuring identical dungeon layouts, enemy spawns, and item placements across machines.
- Both demo files are written to `bin/`. Live input is ignored during playback, and `F5`/`F6` save/load hotkeys are temporarily disabled.

---

<a id="build-from-source"></a>
## 🛠️ Build from Source

**Prerequisites:** Visual Studio 2022 (Desktop development with C++, v143 toolset) and the Windows 10/11 SDK.

```powershell
git clone https://github.com/moonwho101/DungeonStompDX12UltimateDXR.git
cd DungeonStompDX12UltimateDXR
msbuild src\DungeonStomp.sln /p:Configuration=Release /p:Platform=x64
```

Or open `src/DungeonStomp.sln` in Visual Studio 2022 and build **Release | x64**.

| Configuration | Output |
|---|---|
| Release | `bin/DungeonStomp.exe` |
| Debug | `bin/DungeonStompDebug.exe` |

Run the executable from `bin/`. HLSL shaders are compiled at startup from the `Shaders/` folder next to it, so keep the repository layout intact.

---

<a id="repository-structure"></a>
## 📂 Repository Structure

- `bin/` — Pre-built executable, level files (`.map`/`.cmp`/`.mod`), sounds and runtime data
- `src/` — Engine & game logic (about 30 C++ files plus bundled Dear ImGui)
- `Common/` — D3D12 helper framework (`d3dApp`, `GameTimer`, `MathHelper`)
- `Shaders/` — HLSL shaders ([Raytracing.hlsl](Shaders/Raytracing.hlsl), PBR, shadows, SSAO)
- `Models/`, `Textures/`, `Sounds/`, `Midi/` — Game assets
- `tools/` — Python asset and dungeon-generation scripts
- `Installer/` — Inno Setup installer script

### Dungeon Generator Scripts
Besides the in-game generator (`F7`), two scripts write a new layout to `bin/level1.map`:

```bash
cd tools
python generate_dungeon.py           # Classic tileset
python generate_dungeonNewObjects.py # Extended tileset
```

---

<a id="credits"></a>
## 🌐 Credits

*Engine architecture builds upon concepts from "Introduction to 3D Game Programming with DirectX 12 - 2nd Edition" by Frank Luna.*

<details>
<summary><b>🎨 MD2 Model Author Credits</b></summary>

<br>

Special thanks to the authors of the classic MD2 models featured in Dungeon Stomp:

- **ALPHA Werewolf** — Andrew "ALPHAwolf" Gilmour
- **Bauul, Hueteotl, Winter's Faerie** — Evil Bastard
- **Centaur** — Scarecrow
- **Bug (Q2)** — Tatey
- **Corpse** — Neuralstasis
- **Demoness (Succubus)** — Pascal "Firebrandt" Jurock
- **Dragon Knight, Ogro** — Michael "Magarnigal" Mellor
- **Fulimo** — Tim
- **Goblin** — Conrad
- **Grey** — RichB
- **Hellspawn** — Alcor
- **Hydralisk** — warlord
- **Ichabod** — Adam Ward (Gixter)
- **Imp** — Paul Interrante & Brad Grace
- **Insect** — Joe "Ebola" Woodrell
- **Morbo/Brawn** — Rowan Crawford (Sumaleth)
- **Necromancer** — Raven Software
- **Necromicus** — Jade Moffatt Jones
- **Ogre** — Didier "The Doctor" Savanah
- **Orc** — Boogieman
- **Perelith Knight** — James Green
- **Phantom, Wraith** — Burnt Kona
- **Purgatori** — Tom Colby
- **Rider** — Blake
- **Sorcerer** — E. Villiers
- **Tentacle** — Marcus Lutz
- **Troll** — Thargar
- **Werewolf** — Brian Yee

</details>

---

## License

This project is open source. See the [LICENSE](LICENSE) file for details.

---

<div align="center">

**Happy Dungeon Stomping — Breeyark! ⚔️**

*If you find this project useful for learning DirectX 12 or DXR, please drop a ⭐ star above!*

[![Star this repo](https://img.shields.io/github/stars/moonwho101/DungeonStompDX12UltimateDXR?style=social)](https://github.com/moonwho101/DungeonStompDX12UltimateDXR/stargazers)

</div>

