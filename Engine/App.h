#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "level.h"

// Engine Application API
// Simple interface for creating Vulkan applications

// Main macro
// and the engine handles the rest
void INPUT();
void RENDER();
void RUN();
#define ENGINE_ENTRY_POINT \
    int main() { \
        RUN(); \
        return 0; \
    }

#define TITLE  "vulkan"
#define WIDTH  1270
#define HEIGHT 850
#define SIZE   0.5f

#define CAM 3.0f
#define ROT 1.2f

// Rendering configuration
#define NEAR_PLANE 0.1f
#define FAR_PLANE 100.0f
#define FOV_DEGREES 45.0f

// Text rendering
#define CHAR_WIDTH 0.04f
#define CHAR_HEIGHT 0.08f
#define CHAR_SPACING 1.1f

// Graphics
#define TILE_SCALE 4.0f
#define CLEAR_COLOR_R 0.0f
#define CLEAR_COLOR_G 0.0f
#define CLEAR_COLOR_B 0.0f
#define CLEAR_COLOR_A 1.0f


extern VkDescriptorSet font_descriptor_set;
extern VkDescriptorSet board_descriptor_set;

typedef struct
{
    GLFWwindow *win;
} glfw_t;

typedef struct
{
    float position[3];
    float tex_coord[2];
    float color[4];
} vertex_t;

typedef struct
{
    float x, y, z;
    float yaw, pitch;
} cam_t;

typedef struct
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t vertex_count;
    uint32_t index_count;
    bool is_text;
} mesh_buffer_t;

typedef struct
{
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    uint32_t width;
    uint32_t height;
} texture_t;

typedef struct
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
} pipeline_t;

#define MAX_TEXTURES 64
typedef struct
{
    char path[256];
    texture_t texture;
    VkDescriptorSet descriptor_set;
    bool loaded;
} texture_cache_entry_t;

typedef struct
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkExtent2D swapChainExtent;
    VkFormat swapChainImageFormat;

    uint32_t imageCount;
    VkImage *images;
    VkImageView *imageViews;
    VkFramebuffer *framebuffers;

    VkRenderPass renderPass;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkCommandBuffer loadingCommandBuffer;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsFamilyIndex;
    uint32_t presentFamilyIndex;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout textureSetLayout;
    texture_t font_texture;
    texture_t board_texture;

    texture_cache_entry_t texture_cache[MAX_TEXTURES];
    uint32_t texture_count;

    VkImage depthImage;
    VkDeviceMemory depthMemory;
    VkImageView depthImageView;

    pipeline_t textured_pipeline;
    pipeline_t colored_pipeline;
    pipeline_t text_pipeline;
    mesh_buffer_t text_buffer;
    mesh_buffer_t cube_buffer;
    mesh_buffer_t wall_buffer;

    const vertex_t *current_vertices;
    uint32_t current_vertex_count;
} vulkan_t;

#define MAX_TEXT_VERTICES 10000
#define MAX_WALL_VERTICES 10000
typedef struct
{
    glfw_t glfw;
    vulkan_t v;
    cam_t cam;
    level_t current_level;

    double last_time;
    double last_frame_time;
    int frame_count;
    double fps;
    float delta_time;

    vertex_t text_vertices[MAX_TEXT_VERTICES];
    uint32_t text_vertex_count;

    vertex_t wall_vertices[MAX_WALL_VERTICES];
    uint32_t wall_vertex_count;
    sector_t *current_sector;
} state_t;

extern state_t state;

typedef struct
{
    uint8_t x;
    uint8_t y;
} glyph_uv_t;

extern glyph_uv_t glyphs[128];

typedef struct {
    mat4 mvp;
    vec4 tint_color;
} push_constants_textured_t;

void VK_START(void);
int VK_FRAME(void);
void VK_END(void);

typedef struct {
    vec3 position;
    vec3 rotation;
    vec3 scale;
    vec4 color;
} cube_instance_t;

#include "util.h"
