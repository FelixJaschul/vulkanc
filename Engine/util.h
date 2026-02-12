#pragma once
#include "App.h"

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

static vec4 tint = {1, 1 ,1 ,1};
#define VK_TINT(r, g, b, a) do { \
    tint[0] = r; tint[1] = g; tint[2] = b; tint[3] = a; \
} while(0)

static float texture_tiling = 1.0f;
#define VK_TILETEXTURE(scale) do { \
    texture_tiling = scale; \
} while(0)

VkDescriptorSet* vk_get_texture(const char* path);
static VkDescriptorSet *current_texture = NULL;
#define VK_TEXTURE(path) do { \
    current_texture = vk_get_texture(path); \
} while(0)

static void _draw_char(const char c, const float x, const float y, const float char_width, const float char_height)
{
    if (state.text_vertex_count + 6 > MAX_TEXT_VERTICES) return;

    const float u0 = glyphs[(uint8_t)c].x * 1.0f / 16.0f;
    const float v0 = glyphs[(uint8_t)c].y * 1.0f / 16.0f;
    const float u1 = u0 + 1.0f / 16.0f;
    const float v1 = v0 + 1.0f / 16.0f;
    const float z = 0.0f;

    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x, y, z}, {u0, v0}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x + char_width, y + char_height, z}, {u1, v1}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x + char_width, y, z}, {u1, v0}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x, y, z}, {u0, v0}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x, y + char_height, z}, {u0, v1}, {1.0f, 1.0f, 1.0f, 1.0f}};
    state.text_vertices[state.text_vertex_count++] = (vertex_t){{x + char_width, y + char_height, z}, {u1, v1}, {1.0f, 1.0f, 1.0f, 1.0f}};
}

static void _draw_string(const char *str, const float x, const float y)
{
    if (!str) return;

    float current_x = x;
    for (const char *p = str; *p; p++)
    {
        _draw_char(*p, current_x, y, CHAR_WIDTH, CHAR_HEIGHT);
        current_x += CHAR_WIDTH * CHAR_SPACING;
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
    glm_translate(view, (vec3){-state.cam.x, -state.cam.y, -state.cam.z});

    glm_perspective(glm_rad(FOV_DEGREES), (float)WIDTH / (float)HEIGHT, NEAR_PLANE, FAR_PLANE, proj);
    glm_mat4_mul(proj, view, mvp);
    glm_mat4_mul(mvp, model, mvp);

    push_constants_textured_t pc;

    glm_mat4_copy(mvp, pc.mvp);
    glm_vec4_copy(tint, pc.tint_color);
    pc.tiling = texture_tiling;

    const VkDescriptorSet *tex_to_use = current_texture ? current_texture : &board_descriptor_set;
    vkCmdBindDescriptorSets(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.textured_pipeline.layout, 0, 1, tex_to_use, 0, NULL);

    vkCmdPushConstants(
        state.v.commandBuffer, state.v.textured_pipeline.layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants_textured_t), &pc
    );

    vkCmdDraw(state.v.commandBuffer, state.v.cube_buffer.vertex_count, 1, 0, 0);
}
#define VK_DRAWCUBE(x, y, z, rotY, scale) _draw_cube((x), (y), (z), (rotY), (scale))
