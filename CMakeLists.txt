cmake_minimum_required(VERSION 3.10)
project(skytri)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External dependencies
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)
add_subdirectory(external/glm)

# Glad configuration - we use pre-built glad.c in src/
# Create a minimal glad library target with headers
add_library(glad INTERFACE)

# Main executable
add_executable(skytri
    src/Source.cpp
    src/glad.c
    src/stb_image.cpp
)

# Include directories
target_include_directories(skytri PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad_headers
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)

# Link libraries
target_link_libraries(skytri PRIVATE
    glfw
    glm::glm
    glad
)

# Copy shaders to build directory
add_custom_command(TARGET skytri POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ${CMAKE_CURRENT_BINARY_DIR}/shaders
)
