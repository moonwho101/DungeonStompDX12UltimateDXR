<div align="center">

# Dungeon Stomp DX12 Ultimate DXR

### A 3D Dungeon Crawler Powered by DirectX 12 Ultimate, DXR Ray Tracing & Physically Based Rendering

[![License](https://img.shields.io/github/license/moonwho101/DungeonStompDX12UltimateDXR?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue?style=flat-square&logo=windows)](https://github.com/moonwho101/DungeonStompDX12UltimateDXR)
[![DirectX](https://img.shields.io/badge/DirectX-12%20Ultimate-green?style=flat-square&logo=microsoft)](https://devblogs.microsoft.com/directx/announcing-directx-12-ultimate/)
[![C++](https://img.shields.io/badge/language-C%2B%2B-orange?style=flat-square&logo=cplusplus)](https://github.com/moonwho101/DungeonStompDX12UltimateDXR)
[![Visual Studio](https://img.shields.io/badge/VS-2022-purple?style=flat-square&logo=visualstudio)](https://visualstudio.microsoft.com/)

![Dungeon Stomp DX12 DXR](Textures/screenshot33.jpg)

**Explore 15 hand-crafted dungeon levels, battle 25+ enemy types, collect weapons & spells, and experience real-time ray-traced lighting — all in an open-source C++ engine.**

[Download & Play](#quick-start) · [Screenshots](#screenshots) · [Build from Source](#build-from-source) · [Controls](#controls) · [Related Projects](#related-projects)

</div>

---

## Highlights

| | Feature | Details |
|---|---|---|
| **Ray Tracing** | DXR inline ray tracing | Real-time ray-traced shadows & global illumination |
| **PBR** | Physically based rendering | Cook-Torrance BRDF with metallic workflow |
| **Shading** | Variable Rate Shading | DX12 Ultimate adaptive performance |
| **Shadows** | Shadow mapping + SSAO | Dynamic shadow casting with ambient occlusion |
| **Levels** | 15 dungeon levels | Full campaign with level progression |
| **Enemies** | 25+ monster types | AI with animated MD2 & 3DS models |
| **Combat** | 30+ weapons & spells | Melee, ranged, and magic with projectile system |
| **Audio** | XAudio2 sound engine | Monster voices, ambient loops, music |

---

## Ray Tracing (DXR) Support

Dungeon Stomp DX12 Ultimate DXR features a modern ray tracing implementation using **DirectX 12 DXR**.

*   **Hardware Compatibility:** Fully supports **NVIDIA RTX** and **AMD Ray Tracing (RT)** capable GPUs.
*   **Inline Ray Tracing (DXR 1.1):** This project leverages the **DirectX 12 Ultimate** feature set, specifically **DXR 1.1 Inline Ray Tracing**.
*   **Shadow Rays:** The engine implements ray-traced **shadow rays** to calculate light visibility. This provides physically accurate, pixel-perfect dynamic shadows that correctly handle overlapping geometry and provide superior visual fidelity compared to standard shadow mapping.

---

## Quick Start

> **No build required** — a pre-built binary is included.

1. **Clone** this repository:
   ```
   git clone https://github.com/moonwho101/DungeonStompDX12UltimateDXR.git
   ```
2. **Run** `bin/DungeonStomp.exe`
3. **Stomp some dungeons!** *Breeyark!*

> **Requirements:** Windows 10/11 with a DirectX 12-capable GPU. A DXR-capable GPU (NVIDIA RTX / AMD RX 6000+) is recommended for ray tracing features.

---

## Screenshots

<div align="center">

![Ray-traced dungeon scene](Textures/screenshot31.jpg)

![Combat encounter](Textures/screenshot30.jpg)

|  |  |
|---|---|
| ![Screenshot](Textures/screenshot23.jpg) | ![Screenshot](Textures/screenshot22.jpg) |
| ![Screenshot](Textures/screenshot25.jpg) | ![Screenshot](Textures/screenshot26.jpg) |

</div>

---

## Rendering Features

**DirectX 12 Ultimate Pipeline:**
- **DXR Ray Tracing** — inline ray tracing for realistic shadow rays and global illumination
- **Variable Rate Shading (VRS)** — adaptive shading rate for performance optimization
- **Physically Based Rendering** — full PBR material pipeline (diffuse albedo, Fresnel, roughness, metallicness)
- **ACES Tone Mapping** — HDR to SDR with cinematic tone curve

**Lighting & Shadows:**
- **Shadow Maps** (2048x2048) with dynamic shadow casting
- **Screen Space Ambient Occlusion (SSAO)** for depth-based occlusion
- **Up to 32 dynamic point/spot lights** per scene
- **Atmospheric fog** with density-based effects

**Textures & Materials:**
- **130+ textures** with DDS mipmap chains
- **Normal maps** with specular in alpha channel
- **Cube maps** for dynamic skybox reflections
- **28+ PBR materials** with tuned roughness and metallic values
- **Alpha transparency & alpha testing**

**Engine:**
- **BMFont GPU text rendering** — fast bitmap font system (AngelCode Hiero format)
- **Camera head bob** — dual sine-wave movement simulation
- **3-frame buffered rendering** with frame resource management
- **Bounding box culling** and spatial cell partitioning

---

## Gameplay Features

- **15 hand-crafted dungeon levels** with campaign progression
- **25+ enemy types** — werewolves, demons, goblins, orcs, hydras, phantoms, and more
- **30+ weapons & spells** with ammunition/magic point tracking
- **Projectile system** — up to 100 simultaneous active missiles
- **Interactive doors & keys** — swing mechanics, locked doors
- **Treasure & loot** — gold drops from defeated enemies
- **Experience & leveling** — XP-based progression system
- **Save/Load** — persistent game state (F5/F6)
- **XBOX controller support** (enable in `DirectInput.cpp`)

---

## Build from Source

**Prerequisites:**
- [Visual Studio 2022 Community](https://visualstudio.microsoft.com/) (or higher)
- Windows SDK with DirectX 12 support
- C++ Desktop Development workload

**Build:**
```powershell
git clone https://github.com/moonwho101/DungeonStompDX12UltimateDXR.git
cd DungeonStompDX12UltimateDXR
msbuild src\DungeonStomp.sln /p:Configuration=Release /p:Platform=x64
```

Or open `src/DungeonStomp.sln` in Visual Studio and build in **Release | x64** for best results.

The compiled binary is output to the `bin/` directory.

---

## Controls

| Action | Input |
|---|---|
| Move | **W A S D** |
| Jump | **E** |
| Open doors | **Space** |
| Attack | **Left mouse button** |
| Move forward | **Right mouse button** |
| Cycle weapons/spells | **Q / Z** or **Mouse wheel** |
| Fullscreen | **Alt+Enter** or **F11** |
| Load game | **F5** |
| Save game | **F6** |

---

## Debug & Developer Controls

<details>
<summary>Click to expand developer hotkeys</summary>

| Key | Action |
|---|---|
| **R** | Toggle DXR (DirectX Raytracing) |
| **T** | Toggle Variable Rate Shading |
| **O** | Toggle SSAO |
| **J** | Toggle Shadow Map |
| **N** | Toggle Normal Map |
| **M** | Show shadow map / SSAO texture |
| **V** | Toggle VSync |
| **B** | Toggle Camera head bob |
| **H** | Toggle Player HUD |
| **G** | Toggle gravity (Numpad +/- to fly up/down) |
| **I** | Disable music |
| **P** | Random music |
| **X** | Give experience points |
| **K** | Give all weapons and spells |
| **]** | Next dungeon level |
| **[** | Previous dungeon level |

</details>

---

## Project Structure

```
├── bin/            # Pre-built game binary & data files (levels, sounds, config)
├── Common/         # D3D12 framework (d3dApp, GameTimer, GeometryGenerator, MathHelper)
├── Models/         # 3DS & MD2 model assets
├── Shaders/        # HLSL shaders (Raytracing, PBR, Shadows, SSAO, NormalMap, Sky)
├── Sounds/         # Ambient loops & sound effects
├── src/            # Game source code (40+ C++ files)
├── Textures/       # 130+ textures (DDS, BMP) with normal maps
└── Installer/      # Inno Setup installer script
```

**Key source files:**

| File | Purpose |
|---|---|
| `src/DungeonStompApp.cpp` | Application entry & D3D12 initialization |
| `src/DungeonStompRender.cpp` | Main rendering pipeline |
| `src/DungeonStompUpdate.cpp` | Per-frame game update loop |
| `src/GameLogic.cpp` | Monster AI, damage, combat logic |
| `src/LoadWorld.cpp` | Level loading & world initialization |
| `src/ImportMD2.cpp` | Animated MD2 model importer |
| `src/Import3DS.cpp` | 3DS model importer |
| `src/DXRHelper.cpp` | DXR acceleration structures & ray tracing setup |
| `Shaders/Raytracing.hlsl` | DXR shader with PBR & global illumination |

---

## Related Projects

| Project | API |
|---|---|
| [Dungeon Stomp DirectX 12](https://github.com/moonwho101/DungeonStompDirectX12) | DirectX 12 (non-DXR) |
| [Dungeon Stomp Vulkan](https://github.com/moonwho101/DungeonStompVulkan) | Vulkan (WIP) |
| [Dungeon Stomp DirectX 7](https://github.com/moonwho101/DungeonStomp) | DirectX 7 (classic) |

---

## Credits

### Reference
- *"Introduction to 3D Game Programming with DirectX 12"* by Frank Luna

### MD2 Model Authors

Dungeon Stomp would not have been possible without the amazing MD2 models made by the following authors:

<details>
<summary>Click to expand full model credits</summary>

| Model | Author |
|---|---|
| ALPHA Werewolf | Andrew "ALPHAwolf" Gilmour |
| Bauul | Evil Bastard |
| Centaur | Scarecrow |
| Bug (Q2) | Tatey |
| Corpse | Neuralstasis |
| Demoness (Succubus) | Pascal "Firebrandt" Jurock |
| Dragon Knight | Michael "Magarnigal" Mellor |
| Fulimo | Tim |
| Goblin | Conrad |
| Grey | RichB |
| Hellspawn | Alcor |
| Hueteotl | Evil Bastard |
| Hydralisk | warlord |
| Ichabod | Adam Ward (Gixter) |
| Imp | Paul Interrante & Brad Grace |
| Insect | Joe "Ebola" Woodrell |
| Morbo/Brawn | Rowan Crawford (Sumaleth) |
| Necromancer | Raven Software |
| Necromicus | Jade Moffatt Jones |
| Ogre | Didier "The Doctor" Savanah |
| Ogro | Michael "Magarnigal" Mellor |
| Orc | Boogieman |
| Perelith Knight | James Green |
| Phantom | Burnt Kona |
| Purgatori | Tom Colby |
| Rider | Blake |
| Sorcerer | E. Villiers |
| Tentacle | Marcus Lutz |
| Troll | Thargar |
| Werewolf | Brian Yee |
| Winter's Faerie | Evil Bastard |
| Wraith | Burnt Kona |

</details>

---

## License

This project is open source. See the [LICENSE](LICENSE) file for details.

---

<div align="center">

**Happy Dungeon Stomping — Breeyark!**

*If you enjoy this project, consider giving it a* ⭐

</div>

