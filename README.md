# 🧩 Lordi: a Vulkan game engine

A lightweight C++20 / Vulkan engine, built from scratch for learning and for real games. It handles the window, the GPU, lighting and shadows, NVIDIA DLSS, models and textures, objects and cameras, input and ImGui; a game plugs in through one small interface.

**Built with Lordi:** [LV-EngineBuilder](https://github.com/oddjoey/LV-EngineBuilder), a first-person engine building game.

---

## 🚀 Features

- **Vulkan 1.4** renderer with validation layers in debug builds
- **Lighting**: sun/moon, sky and ground ambient, one interior zone with its own ambient, up to 32 point lights (limited to inside, outside or everywhere), exponential fog, ACES tone mapping with exposure
- **Sun shadows**: two cascades in a depth array, texel-snapped so they don't crawl, normal-offset lookups, 3×3 PCF
- **Glass**: transparent objects drawn after solid ones, sorted back to front
- **NVIDIA DLSS Super Resolution** (DLAA to Ultra Performance) with motion vectors, jitter and mip bias; greyed out where unsupported
- **Runtime video options**: windowed/borderless/fullscreen, resolution, V-Sync, shadow quality, DLSS mode and a frame cap all change live; MSAA at startup
- **Models**: `.obj` and `.fbx` loading, plus `MeshBuilder` for procedural meshes (boxes, cylinders, tubes, spheres, cones, helices, plates with holes, triangles)
- **Textures**: from image files or generated in code, all in one bindless array
- **Objects**: position/rotation/scale, model, texture, tint, emissive glow, lit/unlit/sky shading, shadow casting, visibility, picking
- **Dear ImGui** drawn over the scene at full resolution, an engine debug window (Home), coloured console logging and frame timing

---

## 🧰 Requirements

- **Windows 10/11, x64**
- **Visual Studio 2026** (toolset v145) with the C++ desktop workload
- A discrete GPU with **Vulkan 1.4**
- An **NVIDIA RTX** GPU for DLSS (optional)

### Libraries

All third-party code lives in `Libraries/`, which is git-ignored apart from the submodules.

| Library | Folder | Used for |
|---|---|---|
| [Vulkan SDK](https://vulkan.lunarg.com/) | `Libraries/vulkan` | Graphics API, validation layers, `glslc` |
| [GLFW 3.4](https://www.glfw.org/) | `Libraries/glfw-3.4` | Window, input, fullscreen |
| [Dear ImGui](https://github.com/ocornut/imgui) | `Libraries/imgui` (submodule) | UI |
| [glm](https://github.com/g-truc/glm) | `Libraries/glm` (submodule) | Maths |
| [magic_enum](https://github.com/Neargye/magic_enum) | `Libraries/magic_enum` (submodule) | Enum names in logs |
| [stb](https://github.com/nothings/stb) | `Libraries/stb` | Image loading |
| [termcolor](https://github.com/ikalnytskyi/termcolor) | `Libraries/termcolor` | Coloured console output |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | `Libraries/tinyobjloader` | `.obj` models |
| [ufbx](https://github.com/ufbx/ufbx) | `Libraries/ufbx` | `.fbx` models |
| [NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS) | `Libraries/DLSS` | DLSS Super Resolution (310.9.16) |

---

## 🔧 Building

1. Clone with the submodules:
   ```bash
   git clone --recurse-submodules https://github.com/oddjoey/LV.git
   ```
2. Put the Vulkan SDK in `Libraries/vulkan` and GLFW 3.4 in `Libraries/glfw-3.4` (with `lib-vc2026`). Set `VULKAN_SDK` to the Vulkan folder.
3. Get the DLSS SDK (only the headers and Windows libraries are needed):
   ```bash
   cd Libraries
   git clone --depth 1 --filter=blob:none --sparse https://github.com/NVIDIA/DLSS DLSS
   cd DLSS
   git sparse-checkout set --no-cone /include /lib/Windows_x86_64 /LICENSE.txt /README.md
   ```
4. Open `LV.sln`, pick **Debug | x64**, build and run from the repository root. A post-build step copies `nvngx_dlss.dll` next to the executable.

`main.cpp` is a small sample game: a block turning in the sun on a stone floor, with shadows.

### Shaders

Compiled SPIR-V is checked in under `Engine/Graphics/Shaders/Compiled/`. After changing a shader, run `Engine/Graphics/Shaders/compile.bat`:

| Output | Source | Use |
|---|---|---|
| `vert.spv`, `frag.spv` | `shader.vert`, `shader.frag` | The scene, drawn straight to the window |
| `vert_mv.spv`, `frag_mv.spv` | same, with `-DMOTION_VECTORS` | The scene with motion vectors, for DLSS |
| `shadow.spv` | `shadow.vert` | Sun shadow maps (depth only) |
| `composite_vert.spv`, `composite_frag.spv` | `composite.vert`, `composite.frag` | Puts DLSS's output on screen |

`scene_data.glsl` is the per-frame lighting block shared by the scene shaders; it must match `SceneUniforms` in `Engine/Graphics/Lighting.h`.

---

## 🎮 Making a game with Lordi

A game derives from `Game` (`Engine/Game.h`) and hands itself to Lordi:

```cpp
class MyGame : public Game
{
public:
    // Before the GPU upload: register models, textures, objects and cameras here.
    void OnLoadAssets() override;
    // Every engine system is up, just before the first frame.
    void OnStart() override;
    void OnUpdate(float deltaTime) override;
    // Inside the ImGui frame.
    void OnGUI() override;
    void OnShutdown() override;
};

int main()
{
    auto lordi = Lordi::Get();

    GraphicsSettings graphics {};
    graphics.sWindowName = "My Game";
    graphics.uiWindowWidth = 1280;
    graphics.uiWindowHeight = 720;
    graphics.eVSync = VSyncOptions::ENABLED;
    graphics.eMSAA = MSAAOptions::X4;
    graphics.sEngineRoot = "LV/";   // where Lordi is, relative to the working directory
    lordi->GetSettings()->SetGraphicsSettings(graphics);

    lordi->SetGame(std::make_shared<MyGame>());
    lordi->Start();
}
```

### As a submodule

The intended layout keeps Lordi in its own repository and adds it to the game as a submodule:

```
MyGame/
├── LV/            ← this repository (git submodule), with its Libraries/ folder
├── Game/          ← your code
├── main.cpp
└── MyGame.vcxproj
```

- Add Lordi: `git submodule add https://github.com/oddjoey/LV.git LV`
- Include engine headers as `"LV/Engine/..."` with the game's folder on the include path, and the libraries from `LV/Libraries/...`.
- Compile Lordi's `Engine/**/*.cpp`, ImGui's sources (`imgui*.cpp`, the GLFW and Vulkan backends, `misc/cpp/imgui_stdlib.cpp`) and `ufbx.c` into the game's project.
- Link `vulkan-1.lib`, `glfw3.lib` and `nvsdk_ngx_d.lib` (`nvsdk_ngx_d_dbg.lib` in debug), and copy `nvngx_dlss.dll` next to the executable.
- Set `GraphicsSettings::sEngineRoot` to `"LV/"` and run from the game's folder, so Lordi finds its compiled shaders.

[LV-EngineBuilder](https://github.com/oddjoey/LV-EngineBuilder) is set up exactly like this and is a complete example.

### The main calls

| What | How |
|---|---|
| A procedural model | `MeshBuilder b; b.SetColor(...).AddBox(...); ModelManager::Get()->CreateModel(b.Build("Name"))->GetIndex()` |
| A model from a file | `ModelManager::Get()->CreateModel(Model(name, path, options))` |
| A texture | `TextureManager::Get()->CreateTexture({ name, path })`, or `CreateTexture(name, width, height, rgbaPixels)` |
| An object | `ObjectManager::Get()->CreateObject(Object(position, rotation, scale, modelIndex, textureIndex))`, then `SetTint`, `SetEmissive`, `SetShading`, `SetCastsShadows`, `SetVisible`… |
| A camera | `CameraManager::Get()->CreateCamera(...)`, `SetActiveCamera(camera)` |
| Lighting | Fill a `SceneLighting` (sun, sky, interior box, fog, exposure, point lights) and call `Renderer::Get()->SetSceneLighting(...)` every frame it changes |
| Shadows | `Renderer::Get()->SetShadowSettings({ resolution, distance })` (resolution 0 turns them off) |
| Video | `Graphics::Get()->SetDisplayMode(...)`, `SetVSync(...)`, `SetDLSSMode(...)`, `Lordi::Get()->SetFrameLimit(...)` |
| After a cut | `Renderer::Get()->ResetTemporalHistory()` so DLSS doesn't blend in the old view |
| Input | `InputManager::Get()`: key/mouse state, presses, cursor lock |

Models, textures and objects can also be added once the game is running. Object indices are 1-based; model and texture index 0 means none.

---

## 🧱 How it works

| System | Files | Does |
|---|---|---|
| Lordi | `Engine/Lordi.*`, `Engine/Game.h` | Starts every system, runs the main loop (with an optional frame cap), shuts down |
| Graphics | `Engine/Graphics/Graphics.*` | Vulkan instance, device, swap chain, render passes, pipelines, buffers, descriptors, display mode |
| Renderer | `Engine/Graphics/Renderer.*` | Records each frame |
| Lighting & shadows | `Engine/Graphics/Lighting.h`, `GraphicsLighting.cpp` | The scene uniform block, shadow render pass, pipeline and map |
| DLSS | `Engine/Graphics/DLSS.*`, `GraphicsUpscaling.cpp` | NVIDIA NGX, and the render targets, pipelines and composite pass for upscaling |
| Models & textures | `Engine/Graphics/Models/*` | Managers, loaders and `MeshBuilder` |
| Objects | `Engine/Objects/*` | Entities, objects, cameras |
| Utility | `Engine/Utility/*` | Input, settings, logging, performance timing, helpers |
| UI | `Engine/Graphics/IMGUI/*` | Dear ImGui in its own subpass |

### A frame

1. Every visible object gets its matrices in a dynamic uniform buffer (with last frame's, for motion vectors), sorted into solid and transparent lists.
2. The scene uniform block is written: cascade matrices, sun, sky, interior, fog, point lights.
3. **Shadow passes**: one depth-only pass per cascade, for lit objects that cast shadows.
4. **Scene**: solid objects, then glass back to front, either straight to the swap chain or, with DLSS, into colour, motion and depth targets at the render size with a jittered projection.
5. **DLSS** (when on): upscales into a window-sized image, which a full-screen pass draws to the swap chain.
6. **UI**: ImGui in the second subpass, at full resolution.

---

## 🗂️ Project layout

```
LV/
├── main.cpp                Sample game
├── Engine/
│   ├── Lordi.*, Game.h     Main loop and the game interface
│   ├── Graphics/           Vulkan, renderer, lighting, shadows, DLSS, models, textures, ImGui
│   │   └── Shaders/        GLSL sources, compile.bat, Compiled/*.spv
│   ├── Objects/            Entities, objects, cameras
│   └── Utility/            Input, settings, logger, performance, helpers
├── Resources/              Sample assets
├── Libraries/              Third-party libraries
└── LV.sln, LV.vcxproj
```

---

## 🧠 Notes

- Only **Debug | x64** has project-relative library paths; the other configurations still point at old absolute paths.
- MSAA is chosen at startup.
- Point lights don't cast shadows.
- Keep GPU drivers up to date; Vulkan (and DLSS) are strict about it.

---

## 🧑‍💻 License

Released under the **MIT License**. The NVIDIA DLSS SDK in `Libraries/DLSS` is under NVIDIA's own license (`Libraries/DLSS/LICENSE.txt`).
