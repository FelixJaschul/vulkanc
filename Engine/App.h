#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

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
#define WIDTH  800
#define HEIGHT 600
#define SIZE   0.5f

static vec4 tint = {1, 1 ,1 ,1};
#define VK_TINT(r, g, b, a) do { \
    tint[0] = r; tint[1] = g; tint[2] = b; tint[3] = a; \
}while(0)

#define CAM 3.0f
#define ROT 1.2f

#define VK_ASSERT(result, msg) do { \
    if ((result) != VK_SUCCESS) { \
        fprintf(stderr, "ASSERTATION FAILED: Vulkan error: %s (code %d)\n", msg, (int)(result)); \
        exit(1); \
    } \
} while(0)

#define ASSERT(result, msg) do { \
    if (!(result)) { \
        fprintf(stderr, "ASSERTATION FAILED: %s\n", msg); \
        exit(1); \
    } \
} while(0)

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

    VkImage depthImage;
    VkDeviceMemory depthMemory;
    VkImageView depthImageView;

    pipeline_t textured_pipeline;
    pipeline_t colored_pipeline;
    pipeline_t text_pipeline;
    mesh_buffer_t text_buffer;
    mesh_buffer_t cube_buffer;

    const vertex_t *current_vertices;
    uint32_t current_vertex_count;
} vulkan_t;

#define MAX_TEXT_VERTICES 10000
typedef struct
{
    glfw_t glfw;
    vulkan_t v;
    cam_t cam;

    double last_time;
    double last_frame_time;
    int frame_count;
    double fps;
    float delta_time;

    vertex_t text_vertices[MAX_TEXT_VERTICES];
    uint32_t text_vertex_count;
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

static void _draw_char(const char c, const float x, const float y, const float char_width, const float char_height)
{
    if (state.text_vertex_count + 6 > MAX_TEXT_VERTICES) return;

    const float u0 = glyphs[(uint8_t)c].x * 1.0f / 16.0f;
    const float v0 = glyphs[(uint8_t)c].y * 1.0f / 16.0f;
    const float u1 = u0 + 1.0f / 16.0f;
    const float v1 = v0 + 1.0f / 16.0f;
    const float z = 0.0f;

    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x, y, z}, {u0, v0}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x + char_width, y, z}, {u1, v0}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x + char_width, y + char_height, z}, {u1, v1}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x, y, z}, {u0, v0}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x + char_width, y + char_height, z}, {u1, v1}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x, y + char_height, z}, {u0, v1}, {1.0f, 1.0f, 1.0f, 1.0f}};
}

static void _draw_string(const char *str, const float x, const float y)
{
    if (!str) return;

    float current_x = x;
    for (const char *p = str; *p; p++)
    {
        _draw_char(*p, current_x, y, 0.08f * 0.5f, 0.08f);
        current_x += 0.08f * 0.5f * 1.1f;
    }
}
#define VK_BEGINTEXT do { state.text_vertex_count = 0; } while (0)
#define VK_DRAWTEXT(x, y, str) _draw_string((str), (x), (y))

float  VK_GETDELTATIME(void);
double VK_GETFPS(void);

static inline void vk_drawtextf(const float x, const float y, const char *fmt, ...)
{
    char buffer[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    VK_DRAWTEXT(x, y, buffer);
}
#define VK_DRAWTEXTF(x, y, fmt, ...) vk_drawtextf((x), (y), (fmt), __VA_ARGS__)

static inline void _draw_cube(const float x, const float y, const float z, const float rotY, const float scale)
{
    mat4 model, view, proj, mvp;

    glm_mat4_identity(model);
    glm_translate(model, (vec3){x, y, z});
    glm_rotate(model, rotY, (vec3){0.0f, 1.0f, 0.0f});
    glm_scale_uni(model, scale);

    glm_mat4_identity(view);
    glm_rotate(view, state.cam.pitch, (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate(view, state.cam.yaw, (vec3){0.0f, 1.0f, 0.0f});
    glm_translate(view, (vec3){-state.cam.x, -state.cam.y, state.cam.z});

    glm_perspective(glm_rad(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f, proj);
    glm_mat4_mul(proj, view, mvp);
    glm_mat4_mul(mvp, model, mvp);

    push_constants_textured_t pc;

    glm_mat4_copy(mvp, pc.mvp);
    glm_vec4_copy(tint, pc.tint_color);

    vkCmdPushConstants(
        state.v.commandBuffer, state.v.textured_pipeline.layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants_textured_t), &pc
    );

    vkCmdDraw(state.v.commandBuffer, state.v.cube_buffer.vertex_count, 1, 0, 0);
}
#define VK_DRAWCUBE(x, y, z, rotY, scale) _draw_cube((x), (y), (z), (rotY), (scale))