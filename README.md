# Skytri - OpenGL Learning Project

A graphics learning project using OpenGL, GLFW, GLM, and Glad.

## Setup

### Clone with Submodules
```bash
git clone --recursive https://github.com/yourusername/skytri.git
cd skytri
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
```bash
./skytri
```

## Project Structure
- `src/` - Source code files
- `include/skytri/` - Header files
- `shaders/` - GLSL shader files
- `assets/` - Textures and other resources
- `external/` - Git submodules (GLFW, GLM, Glad)

## Dependencies
All dependencies are managed via git submodules:
- **GLFW** - Window and input handling
- **GLM** - Mathematics library
- **Glad** - OpenGL loader
- **STB Image** - Image loading (single header included in src)
