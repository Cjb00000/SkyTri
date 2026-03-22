# Skytri - Graphics Experiments

A real-time OpenGL-based 3D rendering engine I develop in to deepen my understanding of graphics concepts and explore topics of interest.
Built with OpenGL 3.3, Assimp, GLFW, GLM, and Glad. LearnOpenGL.com has been great a great resource.

## Current Highlights 
- Cloth Physics, Post-processing effects

https://github.com/user-attachments/assets/63a5d178-48e4-4884-abaa-076c2cba2836

### Future Improvements
- Implement cloth physics using a compute shader (OpenGL 4.3 or higher)
- Add bounding boxes/spheres and collision detection for cloth physics. These bounding spheres will be beneficial to future collision testing as well.
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
 




