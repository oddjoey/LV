# 🔧 LV: Engine Builder

A first-person engine building game, made on **Lordi**, a custom C++ / Vulkan game engine.

You run a small garage. You start with a worn-out **LV-20 Inline-4** core and **$4000**. Measure every part down to the micron, machine what's out of spec, buy what's missing, put the engine together with the right clearances, and run it on the dyno. Nothing is shown to you that you haven't measured yourself: the parts have real (hidden) geometry, and your tools have real resolution and error.

---

## 📑 Contents

- [Getting started](#-getting-started)
- [How to play](#-how-to-play)
- [Controls](#-controls)
- [The workshop screens](#-the-workshop-screens)
- [Parts, tolerances and measuring](#-parts-tolerances-and-measuring)
- [Machines](#-machines)
- [Tools](#-tools)
- [The shop and the world](#-the-shop-and-the-world)
- [Settings](#-settings)
- [Saving](#-saving)
- [Developer tools](#-developer-tools)
- [The Lordi engine](#-the-lordi-engine)
- [Project layout](#-project-layout)
- [Known limitations](#-known-limitations)
- [License](#-license)

---

## 🚀 Getting started

### Requirements

- **Windows 10/11, x64**
- **Visual Studio 2026** (platform toolset v145) with the C++ desktop workload
- A GPU with **Vulkan 1.4** support (a discrete GPU is required)
- An **NVIDIA RTX** GPU for DLSS (optional: the game runs without it)

### Libraries

Everything lives in `Libraries/`. Several of them are git submodules; the rest are copied in. `Libraries/` is git-ignored apart from the submodules.

| Library | Folder | Used for |
|---|---|---|
| [Vulkan SDK](https://vulkan.lunarg.com/) | `Libraries/vulkan` | Graphics API, validation layers, `glslc` shader compiler |
| [GLFW 3.4](https://www.glfw.org/) | `Libraries/glfw-3.4` | Window, input, fullscreen |
| [Dear ImGui](https://github.com/ocornut/imgui) | `Libraries/imgui` (submodule) | All UI: HUD, menus, workshop screens |
| [glm](https://github.com/g-truc/glm) | `Libraries/glm` (submodule) | Vector and matrix maths |
| [magic_enum](https://github.com/Neargye/magic_enum) | `Libraries/magic_enum` (submodule) | Enum names in logs |
| [stb](https://github.com/nothings/stb) | `Libraries/stb` | Image loading |
| [termcolor](https://github.com/ikalnytskyi/termcolor) | `Libraries/termcolor` | Coloured console logs |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | `Libraries/tinyobjloader` | `.obj` models |
| [ufbx](https://github.com/ufbx/ufbx) | `Libraries/ufbx` | `.fbx` models |
| [NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS) | `Libraries/DLSS` | DLSS Super Resolution (310.9.16) |

### Setup

1. Clone and pull the submodules:
   ```bash
   git clone https://github.com/oddjoey/LV.git
   cd LV
   git submodule update --init --recursive
   ```
2. Put the Vulkan SDK in `Libraries/vulkan` and GLFW 3.4 in `Libraries/glfw-3.4` (with `lib-vc2026`). Set `VULKAN_SDK` to the Vulkan folder.
3. Get the DLSS SDK. Only the headers and Windows libraries are needed:
   ```bash
   cd Libraries
   git clone --depth 1 --filter=blob:none --sparse https://github.com/NVIDIA/DLSS DLSS
   cd DLSS
   git sparse-checkout set --no-cone /include /lib/Windows_x86_64 /LICENSE.txt /README.md
   ```
4. Open `LV.sln`, pick **Debug | x64**, and build. A post-build step copies `nvngx_dlss.dll` next to `LV.exe`.
5. Run from the project folder (the working directory has to be the repository root, because shaders and resources are loaded from relative paths).

### Shaders

The compiled SPIR-V is checked in under `Engine/Graphics/Shaders/Compiled/`. After changing a shader, run `Engine/Graphics/Shaders/compile.bat`. It builds:

| Output | Source | Use |
|---|---|---|
| `vert.spv`, `frag.spv` | `shader.vert`, `shader.frag` | The scene, drawn straight to the window |
| `vert_mv.spv`, `frag_mv.spv` | same, with `-DMOTION_VECTORS` | The scene with motion vectors, for DLSS |
| `shadow.spv` | `shadow.vert` | Sun shadow maps (depth only) |
| `composite_vert.spv`, `composite_frag.spv` | `composite.vert`, `composite.frag` | Puts DLSS's output on screen |

`scene_data.glsl` holds the per-frame lighting block shared by the scene shaders. It must match `SceneUniforms` in `Engine/Graphics/Lighting.h`.

---

## 🎮 How to play

The loop:

1. **Look around the garage.** Your core's parts sit on the shelf and the pallet rack, and your starting tools hang on the tool board.
2. **Measure.** Take a tool in hand, aim at a part, and left-click. Bores, journals, decks, valves, springs: everything has a spec.
3. **Inspect on the bench.** Set a part on the workbench and press E there. The Bench screen lists every feature, its spec, your reading and the verdict.
4. **Machine** what's worn: set the part up in the right machine with E and cut it from that machine's screen. Material only comes off.
5. **Buy** what's missing or scrap: the parts computer (E at the desk) is the store. Deliveries arrive in a box by the roll-up door; open it with E.
6. **Assemble** on the engine stand, then check the clearances that only exist once parts are together (bearing oil clearance, ring gap, deck clearance, installed spring height).
7. **Run it on the dyno.** You get a power curve, oil pressure, blow-by, compression and a list of findings. A bad build fails on the pull.
8. **Sell** the finished engine (priced on its dyno pull), or keep improving it.

The **Backpack** screen (Tab) shows an engine checklist: how many of each part are fitted, how many spares you have, and what you still need to buy.

---

## ⌨️ Controls

| Key | Action |
|---|---|
| W A S D | Walk |
| Shift | Run |
| Space | Jump |
| Mouse | Look around |
| E | Pick up, open deliveries, use stations, fit parts, open/close the roll-up door |
| Left click | Measure with the tool in your hand (otherwise the same as E) |
| Right click | Set what you're holding down where you're aiming |
| G | Drop what you're holding |
| Q | Stow what you're holding in your backpack (or back on its shelf) |
| 1 – 0 | Take a hotbar item in hand, or put it back |
| Tab | Open/close your backpack (closes any workshop screen) |
| F | Move equipment (machines, bench, stand, shelves, rack, tool board). R rotates, F cancels |
| R (hold) | Turn the engine on the stand over by hand |
| Z / X (hold) | Roll the engine on its stand to get at it from underneath |
| Esc | Close the screen, or open the pause menu |
| F3 | Developer menu |
| Home | Lordi's engine debug window |

In the backpack: drag to move, **R** while dragging to turn an item, drag out of the window to drop, right-click for options.

---

## 🖥️ The workshop screens

One window with tabs. Stations open their own tab with E. Tab and Esc go back to the shop floor.

| Tab | Opened by | What it shows |
|---|---|---|
| **Backpack** | Tab | A Tarkov-style grid (10 × 6 cells) and the hotbar. Beside it: details of the item under the cursor (material, quality, condition, size class, value, how much of it you've measured; for tools, precision, speed and resolution), a **Tidy up** button, and the engine checklist with a "still to buy" list. |
| **Bench** | E at the workbench | Only parts actually sitting on the bench. Every feature grouped by area, with spec, your reading, finish, verdict and which tool to measure it with. Click a feature for the full spec and readings. |
| **Machine** (named after it) | E at a machine | Appears only while you work at a machine. The machine (operation, finish it leaves, time), the part set up in it, and only the features that machine can cut, each marked green, red or grey by your last reading. Then the cut itself: target size (with one-click size classes), time estimate from your own reading, and the button. |
| **Engine Stand** | E at the stand | Fitting parts into slots, and the assembled checks: oil clearances, ring gaps, piston-to-wall, deck clearance, compression, valve installed height and spring pressure. |
| **Dyno Cell** | E at the dyno | The pull: a live RPM sweep, torque and power curves, oil pressure, blow-by, compression and findings. |
| **Store** | E at the parts computer, or the tool board | Parts (choose quality, condition and size class), tools and machines, each with a description and ratings. |

The log at the bottom records everything that happens, with the shop clock.

### Feature details

Click a feature (on the Bench or in a Machine tab) for two side-by-side panels:

- **Spec**: diameter or size range, each oversize class with its exact size, max taper, max out-of-round, and the finish range.
- **Your last reading**: diameter (with the size class it falls in), taper, out-of-round, finish, the verdict and the tool used. Each value is green or red against spec, and greyed out if the part has been cut since.
- **Gauge readings** (round features): the table of every reading by depth (top, middle, bottom) on the thrust and pin axes, with out-of-round at each depth.

---

## 📏 Parts, tolerances and measuring

Every part is generated (`Game/Simulation/PartFactory`) with true geometry: sizes, taper, out-of-round, flatness, runout, surface finish and wear, according to its **quality** (budget, OEM or performance), **condition** (new, used, damaged) and **size class** (standard, or oversize/undersize). Used parts are worn; damaged ones carry a reason.

A **feature** (`Game/Simulation/Feature.h`) is one measurable thing on a part: a bore, a journal, a deck, a valve stem, a spring force. Each has:

- a **spec** (tolerance band), plus allowed **size classes** for things that can be machined oversize/undersize;
- limits for **taper**, **out-of-round** and **surface finish** (Ra) where they matter;
- the **operations** that can cut it.

Round features are read at three depths on two axes (thrust and pin). The verdict lists every issue: undersize, oversize, taper, out-of-round, rough, glazed, warped, runout, or beyond the last size class (scrap or sleeve).

**Measuring is physical.** Take the tool in hand and aim at the part; markers show everything that tool can measure from where you stand. Each reading has the tool's resolution and repeatability error, so a $40 caliper and a $450 bore gauge give genuinely different answers. Readings go stale when the part is machined.

Some clearances only exist once parts are assembled. These are checked on the engine stand: oil clearance (Plastigauge), ring gap (ring squared in its bore, feeler gauge), deck clearance (dial indicator at TDC) and installed spring height.

The engine's true clearances are what the **dyno** runs on, whether or not you measured them.

Units can be switched between metric and imperial in the workshop window or in settings.

---

## ⚙️ Machines

Parts are only machined while set up in the right machine: hold the part and press E at the machine. Each cut costs shop time and can't be undone.

| Machine | Price | Owned at start | Does |
|---|---|---|---|
| Hand Ring Filer | $150 | ✅ | Opens piston ring end gaps |
| Drill-Driven Cylinder Hone | $300 | ✅ | Hones bores; cheap, but geometry depends on your hands |
| Torque-Plate Power Hone | $4500 | | Hones with a torque plate, so the bore is shaped as it will be when assembled |
| Portable Boring Bar | $3500 | | Bores oversize in one pass; leaves a rough finish, so hone afterwards |
| Crankshaft Grinder | $12000 | | Regrinds journals undersize (you'll need undersize bearings) |
| Belt Journal Polisher | $600 | | Takes a few microns off and leaves a mirror finish |
| Surface Grinder | $6000 | | Decks blocks and heads flat (every cut changes deck height or chamber volume) |
| Valve Guide Reamer Set | $180 | ✅ | Reams worn guides straight and round for oversize-stem valves |
| Valve Seat Cutter | $900 | | Three-angle cutters; trues the seat but sinks it a little deeper each cut |
| Valve Refacer | $1400 | | Regrinds valve faces true; eats margin and the valve sits deeper |

Hones charge time by the amount of stock removed, so measure first to get a time estimate. Machines you haven't bought aren't in the shop; once bought, they appear along the wall and can be moved with F.

---

## 📐 Tools

| Tool | Price | Owned at start |
|---|---|---|
| Digital Caliper | $40 | ✅ |
| Telescoping Gauge | $60 | ✅ |
| Small Hole Gauge Set | $45 | ✅ |
| Straightedge & Feelers | $80 | ✅ |
| Plastigauge | $15 | ✅ |
| Feeler Gauge Set | $25 | ✅ |
| Outside Micrometer Set | $350 | |
| Dial Bore Gauge | $450 | |
| Dial Small-Bore Gauge | $380 | |
| Runout Gauge & V-Blocks | $140 | |
| Valve Spring Tester | $250 | |
| Burette & CC Plate | $90 | |
| Surface Profilometer | $1500 | |
| Dial Indicator & Deck Bridge | $120 | |
| Installed Height Micrometer | $90 | |

Tools are rated for **precision** and **speed**. The bench always suggests the best tool you own for each feature. Tools hang on the tool board; E at the board hangs the tool in your hand back up, or opens the tool store.

---

## 🏠 The shop and the world

- **The garage** (`Game/World/GarageLayout.h` holds every dimension): metal siding, a pitched shingle roof, ten windows, six ceiling lamps and a roll-up door (E to open/close; it's open at the start).
- **Equipment**: workbench, parts shelf, pallet rack, tool board, engine stand, machines, dyno cell and parts computer. All of it can be moved with F.
- **Outside**: a fenced yard with trees, a driveway with a parked car, bins and oil drums, a sidewalk, a road with lane markings, street lights and houses across the street.
- **Physics**: items fall, land and stack, and can be set down anywhere. Walls stop you and anything you throw. You can't reach through walls or windows.
- **Holding things**: whatever is in your hand is held first person, low and to the right of your view, and moves with the camera.
- **Deliveries** arrive as a box by the door; open it with E and the parts come out where it stood.
- **Day and night**: the sun crosses the sky (sunrise 06:00, sunset 20:00), the sky and fog change colour, street and porch lights come on at dusk, and there's moonlight at night. A full day takes 30 real minutes by default. The clock is in the top right.

---

## 🎛️ Settings

Esc → **Settings**. Changes apply immediately and are saved to `game_settings.cfg` when you leave the menu.

**Game:** mouse sensitivity, invert Y, field of view, metric/imperial, control hints, FPS counter.

**Video:**

| Setting | Notes |
|---|---|
| Display mode | Windowed, borderless fullscreen or exclusive fullscreen |
| Resolution | Any mode your monitor supports (borderless always uses the desktop's) |
| V-Sync, frame rate limit | Limit 0 = unlimited |
| NVIDIA DLSS | Off, DLAA, Quality, Balanced, Performance, Ultra Performance. Greyed out (with the reason) on GPUs that can't run it. Shows the resolution it renders at. |
| Anti-aliasing (MSAA) | 1×/2×/4×/8×, takes effect on restart, not used while DLSS is on |
| Shadows | Off / Low / Medium / High (1024 / 2048 / 4096 shadow maps) |
| Shadow distance, view distance, brightness | |

**Time of day:** set the time, turn the cycle on/off, and set the length of a day.

---

## 💾 Saving

Pause menu → **Save Game** / **Load Game** / **New Game**, and **Save and Quit** on the quit screen. The game loads your save automatically when it starts.

The save is `savegame.lvsave`, a versioned text file (`LVSAVE 1`) holding:

- money, shop hours and the log;
- every part's true geometry, condition, value, name, your readings, and any machining done to it;
- the engine on the stand and the last dyno pull;
- where every part, tool and delivery is, the backpack layout and hotbar;
- equipment positions, the door, the time of day, and where you're standing.

The whole file is read and checked before anything is replaced, so a damaged save shows an error and leaves your game alone. Saves are written to a temporary file and renamed, so a crash can't leave half a save. `savegame.lvsave` and `game_settings.cfg` are git-ignored.

---

## 🛠️ Developer tools

**F3** opens the developer menu (nothing in it is saved):

- **X-ray part geometry**: the workshop screens show each part's true geometry (in blue) next to your readings.
- **Bounding boxes**: colour-coded wireframes: walls (red), what blocks walking (magenta), surfaces things can be set on (green), equipment (cyan), deliveries (orange), parts and tools (yellow). The menu shows the legend.

**Home** opens Lordi's own debug window (engine stats, performance).

Debug builds run with the Vulkan validation layers on and log everything to the console.

---

## 🧱 The Lordi engine

`Engine/` is the engine; `Game/` is the game built on it. The game plugs in through `Engine/Game.h` (`OnLoadAssets`, `OnStart`, `OnUpdate`, `OnGUI`, `OnShutdown`), and `main.cpp` hands an `EngineBuilderGame` to `Lordi`.

### Systems

| System | Files | Does |
|---|---|---|
| Lordi | `Engine/Lordi.*` | Starts everything, runs the main loop (with an optional frame cap), shuts down |
| Graphics | `Engine/Graphics/Graphics.*` | Vulkan instance, device, swap chain, render passes, pipelines, buffers, descriptors; display mode and V-Sync changes at runtime |
| Renderer | `Engine/Graphics/Renderer.*` | Records each frame: shadow passes, the scene (solid, then glass back to front), DLSS, then the UI |
| Lighting | `Engine/Graphics/Lighting.h`, `GraphicsLighting.cpp` | Sun/moon, sky, ambient, fog, exposure, up to 32 point lights, cascaded shadow maps |
| DLSS | `Engine/Graphics/DLSS.*`, `GraphicsUpscaling.cpp` | NVIDIA NGX setup and the upscaling render path |
| Models & textures | `Engine/Graphics/Models/*` | Model and texture managers (one shared vertex/index buffer, a bindless texture array), `.obj`/`.fbx` loading, and `MeshBuilder` for procedural meshes (boxes, cylinders, spheres, cones…) |
| Objects | `Engine/Objects/*` | Entities with position/rotation/scale, objects with a model, texture, tint, emissive glow, shading mode (lit/unlit/sky) and shadow casting; cameras |
| Input, settings, logging, performance | `Engine/Utility/*` | Keyboard/mouse, cursor lock, graphics settings, coloured logs, frame timings |
| UI | `Engine/Graphics/IMGUI/*` | Dear ImGui, drawn in its own subpass over the scene |

### Rendering

- **Lighting** is forward shaded in `shader.frag`. The sun and an outdoor sky/ground ambient light the outside. One interior box (the garage) gets its own ambient light. Point lights can be limited to inside, outside or everywhere, so shop lamps don't light the yard and street lights don't light the shop. Fog and an ACES tone map with exposure finish it off.
- **Shadows**: two sun cascades in a depth array. The first is sharp up close (parts on the bench), the second covers the yard out to the shadow distance. Each cascade is snapped to whole shadow texels so shadows don't crawl as you move. Lookups use normal offsetting and 3×3 PCF.
- **Glass** is drawn after everything solid, sorted back to front, with blending and no depth writes.
- **DLSS**: when on, the scene is drawn at the lower render resolution into colour, motion vector and depth targets. The projection is jittered with a Halton (2,3) sequence, and textures get a negative mip bias. DLSS upscales to the window size, and a full-screen pass puts the result on the swap chain under the UI. Motion vectors come from each object's previous model matrix and the previous unjittered view-projection. Loading a save or starting a new game resets DLSS's history.

### Video options at runtime

Display mode and resolution (`glfwSetWindowMonitor`), V-Sync (swap chain rebuild), shadow quality (shadow map rebuild), DLSS mode (render targets and DLSS feature rebuild) and the frame cap all change live. MSAA is read from the settings at startup.

---

## 🗂️ Project layout

```
LV/
├── main.cpp                    Window/video settings from game_settings.cfg, then starts Lordi with the game
├── Engine/                     Lordi
│   ├── Lordi.*, Game.h         Main loop and the game interface
│   ├── Graphics/               Vulkan, renderer, lighting, shadows, DLSS, models, textures, ImGui
│   │   └── Shaders/            GLSL sources, compile.bat, Compiled/*.spv
│   ├── Objects/                Entities, objects, cameras
│   └── Utility/                Input, settings, logger, performance, helpers
├── Game/
│   ├── EngineBuilderGame.*     Ties it all together: input modes, menus, saving, time of day
│   ├── GameSettings.*          The player's settings file
│   ├── Simulation/             The engine-building model: parts, features, tolerances, materials,
│   │                           tools, machines, assembly, dyno, the workshop (money, clock, store)
│   ├── World/                  The 3D shop: layout, models, procedural textures, lighting,
│   │                           physics, measuring, moving equipment, the HUD, the outdoors
│   ├── Inventory/              The backpack grid and hotbar
│   ├── UI/                     Workshop screens, backpack, pause menu, dev menu, icons, ratings
│   └── Save/                   Save and load
├── Resources/                  Models and textures
├── Libraries/                  Third-party libraries (see Getting started)
└── LV.sln, LV.vcxproj          Visual Studio project
```

Files written while playing (both git-ignored): `game_settings.cfg` and `savegame.lvsave`.

---

## ⚠️ Known limitations

- Only **Debug | x64** is set up with the project-relative library paths; the other configurations still point at old absolute paths.
- MSAA changes need a restart.
- Held items can poke through a wall if you stand right against it.
- Point lights don't cast shadows (the inside/outside light zones stand in for that).
- DLSS needs an NVIDIA RTX GPU and a recent driver; on anything else the option is greyed out.

---

## 🧑‍💻 License

This project is released under the **MIT License**. The NVIDIA DLSS SDK in `Libraries/DLSS` is under NVIDIA's own license (`Libraries/DLSS/LICENSE.txt`).
