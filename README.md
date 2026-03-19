# Skytri - OpenGL Learning Project

A graphics learning project built with OpenGL 3.3, GLFW, GLM, and Glad. Features basic lighting with materials and a simple camera system.

## Features

- **Core Graphics**: OpenGL 3.3 with modern shader-based rendering
- **Lighting**: Material-based lighting with ambient, diffuse, and specular components
- **Camera**: Free-look camera with mouse and keyboard controls
- **Dependencies**: All dependencies managed via git submodules

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

## Controls

- **W/A/S/D** - Move forward/left/backward/right
- **Mouse** - Look around (free-look camera)
- **Scroll** - Zoom in/out
- **ESC** - Exit

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
├── CMakeLists.txt    # Build configuration
└── LICENSE           # MIT License
```

## Dependencies

All managed via git submodules:
- **GLFW** - Window creation and input handling
- **GLM** - Mathematics and vectors/matrices
- **Glad** - OpenGL function loader
- **STB Image** - Image loading (single-header library)

## License

MIT License - see [LICENSE](LICENSE) file for details

## Credits

Based on learnopengl.com tutorials for learning OpenGL fundamentals.
