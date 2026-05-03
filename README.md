# Skytri - Graphics Experiments

A real-time OpenGL-based 3D rendering engine built with OpenGL 3.3, Assimp, GLFW, GLM, Glad and ImGui.

References used:
-https://learnopengl.com/

-https://www.cs.cmu.edu/afs/cs/academic/class/15462-s13/www/lec_slides/Jakobsen.pdf

## Current Highlights 
- Verlet integrated cloth physics with Position Based Dynamics constraint projection (Jakobsen method) for spring and collision constraints
- Depth-based post-processing effects via a custom framebuffer

https://github.com/user-attachments/assets/6385bc9d-271f-4986-b6b8-a146b582368d



## How it works

### Cloth simulation
Each vertex of the imported cape mesh becomes a simulated particle. 
Particles are connected by springs matching the mesh edges.

**Verlet integration** moves each particle every frame:
newPos = pos + (pos - prevPos) + accel * dt²
Velocity is never stored - it falls out of the position history as 
`(pos - prevPos)`. This means spring and collision corrections to 
position automatically carry into the next frame's movement.

**Jakobsen constraint projection** enforces spring lengths by directly 
correcting positions rather than computing spring forces (F = kx). 
This avoids the instability of force-based springs at high stiffness. 

**PBD collision resolution** treats capsule collision the same way - 
detect a violated constraint (particle inside capsule) and directly 
project the position to satisfy it (push to surface). prevPosition 
is adjusted to cancel inward velocity so the particle doesn't 
re-enter next frame.

### Future Improvements
- Implement cloth physics using a compute shader (OpenGL 4.3 or higher)
- Switch development to Vulkan
- Endless features! (SSAO, Deferred Rendering, etc.)

## Setup

### Prerequisites

- CMake 3.10 or higher
- Visual Studio 2022 (or another C++ compiler)
- Git

### Clone with Submodules

```bash
git clone --recursive https://github.com/Cjb00000/SkyTri.git
cd SkyTri
```

Or if you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

### Build

**Windows (Visual Studio):**
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

**Linux/Mac:**
```bash
mkdir build
cd build
cmake ..
make
```

### Run

**Windows:**
```bash
./build/Debug/skytri.exe
```

**Linux/Mac:**
```bash
./build/skytri
```

## Project Structure

```
.
├── src/              # Source code (.cpp files)
├── include/skytri/   # Project headers
├── shaders/          # GLSL shader files
├── assets/           # Textures and resources
├── external/         # Dependencies
│   ├── assimp/       # Submodule: Model loading library
│   ├── glfw/         # Submodule: Window and input library
│   ├── glm/          # Submodule: Math library
│   ├── glad/         # Submodule: OpenGL loader
│   └── includes/     # Local headers (glad.h, stb_image.h, etc)
└── CMakeLists.txt    # Build configuration
```

## Dependencies

All managed via git submodules:
- **Assimp** - Model importing into a uniform/normalized data structure
- **GLFW** - Window creation and input handling
- **GLM** - Mathematics and vectors/matrices
- **Glad** - OpenGL function loader
- **STB Image** - Image loading (single-header library)
 




