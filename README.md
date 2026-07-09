# 🧩 Vulkan Game Engine

A lightweight, modular **Vulkan-based game engine** designed for learning, experimentation, and performance.  
Built from scratch in C++ with a focus on low-level graphics, extensibility, and clean architecture.

---

## 🚀 Features

- Full Vulkan rendering backend  
- Real-time 3D rendering pipeline  
- Model loading via `.obj` and `.fbx`  
- Custom UI system powered by ImGui  
- Math and utility libraries for clean, modular code  

---

## 🧰 Requirements

Before building, make sure you have the following dependencies installed:

| Dependency | Description | Installation |
|-------------|--------------|---------------|
| **[Vulkan SDK](https://vulkan.lunarg.com/)** | Core graphics API used by the engine. Provides access to GPU hardware and shader compilation tools. | Install via the official Vulkan SDK installer for your platform. Ensure the `VULKAN_SDK` environment variable is set. |
| **[GLFW](https://www.glfw.org/)** | Windowing and input library for creating Vulkan-compatible windows and handling keyboard/mouse events. | **Linux:** `sudo apt install libglfw3-dev`  <br> **macOS:** `brew install glfw` <br> **Windows:** Download binaries or build from source. |
| **[ImGui](https://github.com/ocornut/imgui)** | Immediate-mode GUI for in-engine tools like debug windows, profilers, or inspectors. | Clone or include as a submodule: <br> `git submodule add https://github.com/ocornut/imgui external/imgui` |
| **[glm](https://github.com/g-truc/glm)** | Header-only math library for 3D math (vectors, matrices, quaternions). | `git submodule add https://github.com/g-truc/glm external/glm` |
| **[magic_enum](https://github.com/Neargye/magic_enum)** | Reflection library for enums. Simplifies debugging and serialization. | `git submodule add https://github.com/Neargye/magic_enum external/magic_enum` |
| **[stb_image](https://github.com/nothings/stb)** | Single-header image loading library. Used for loading textures. | Include `stb_image.h` in your `external/` folder. |
| **[termcolor](https://github.com/ikalnytskyi/termcolor)** | Simple terminal color formatting for readable console output. | `git submodule add https://github.com/ikalnytskyi/termcolor external/termcolor` |
| **[tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)** | `.obj` model loader for simple geometry importing. | `git submodule add https://github.com/tinyobjloader/tinyobjloader external/tinyobjloader` |
| **[ufbx](https://github.com/ufbx/ufbx)** | Lightweight `.fbx` model loader with animation and material support. | `git submodule add https://github.com/ufbx/ufbx external/ufbx` |

---

## 🔧 Installing

1. Clone the repository and initialize submodules:
   ```bash
   git clone https://github.com/oddjoey/LV.git
   cd LV
   git submodule update --init --recursive
   ```

---

## 🏗️ Project Structure

```
├── src/                # Core engine source code
├── shaders/            # Vulkan shader files (.vert, .frag)
├── assets/             # Models, textures, and other assets
├── external/           # Third-party dependencies
├── build/              # Compiled output
└── CMakeLists.txt      # Build configuration
```

---

## 🧠 Notes

- The engine is designed for **learning Vulkan** and understanding low-level rendering architecture.  
- Modular enough to extend with physics, ECS, or scripting in the future.  
- Keep your GPU drivers up to date; Vulkan is strict about compliance.

---

## 🧑‍💻 License

This project is released under the **MIT License**.  
Feel free to modify, distribute, and use it in your own projects.

---
