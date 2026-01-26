cmake_minimum_required(VERSION 3.10)
project(vulkan LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64")

# Add the Engine subdirectory
add_subdirectory(Engine)

# Create the main executable
add_executable(vulkan main.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

# Link against the Engine library
target_link_libraries(vulkan PRIVATE Engine)