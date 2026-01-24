cmake_minimum_required(VERSION 3.22)
project(vulkan LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(HOMEBREW_PREFIX "/opt/homebrew")

add_executable(${PROJECT_NAME} main.c)

target_include_directories(${PROJECT_NAME} PRIVATE ${HOMEBREW_PREFIX}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw ${HOMEBREW_PREFIX}/lib/libcglm.dylib)

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_OSX_ARCHITECTURES "arm64" MACOSX_RPATH TRUE)
endif()