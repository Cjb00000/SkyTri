# Skytri - Graphics Experiments

Built with OpenGL 3.3, GLFW, GLM, and Glad.

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
│   ├── glfw/         # Submodule: Window and input library
│   ├── glm/          # Submodule: Math library
│   ├── glad/         # Submodule: OpenGL loader
│   └── includes/     # Local headers (glad.h, stb_image.h, etc)
└── CMakeLists.txt    # Build configuration
```

## Dependencies

All managed via git submodules:
- **GLFW** - Window creation and input handling
- **GLM** - Mathematics and vectors/matrices
- **Glad** - OpenGL function loader
- **STB Image** - Image loading (single-header library)

<video src="assets/recordings/cape.mp4" width="100%" controls autoplay loop muted>
  Your browser does not support the video tag.
</video>