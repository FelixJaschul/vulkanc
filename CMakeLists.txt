cmake_minimum_required(VERSION 3.22)
project(vulkan LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64")

set(VULKAN_SDK_ROOT "/Users/felixjaschul/VulkanSDK/1.4.335.1/macOS")
set(Vulkan_INCLUDE_DIR ${VULKAN_SDK_ROOT}/include)
set(Vulkan_LIBRARY ${VULKAN_SDK_ROOT}/lib/libvulkan.dylib)

find_package(glfw3 REQUIRED)

add_executable(${PROJECT_NAME} main.c)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${Vulkan_INCLUDE_DIR}
        /opt/homebrew/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${Vulkan_LIBRARY}
        glfw
        /opt/homebrew/lib/libcglm.dylib
)