#include "Engine/App.h"

sector_t* level_find_player_sector(level_t *level, float px, float pz);
void level_render(level_t *level);
bool level_check_collision(level_t *level, float *px, float *pz, float old_x, float old_z, float radius);
void level_cleanup(level_t *level);

void RUN()
{
    VK_START();

    {
        const level_t level = {
            .name = "TEST",
            .path = NULL,
            .sectors = NULL,
            .sector_count = 0,
        };
        state.current_level = level;

        const sector_t room = {
            .id = 0,
            .light_intensity = 0.2f,
            .floor_height = 0.0f,
            .ceil_height = 3.0f,
            .wall_count = 4,
            .walls = malloc(sizeof(wall_t) * 4)
        };

        room.walls[0] = (wall_t){0, -5.0f, -5.0f,  5.0f, -5.0f, true, NULL};
        room.walls[1] = (wall_t){1,  5.0f, -5.0f,  5.0f,  5.0f, true, NULL};
        room.walls[2] = (wall_t){2,  5.0f,  5.0f, -5.0f,  5.0f, true, NULL};
        room.walls[3] = (wall_t){3, -5.0f,  5.0f, -5.0f, -5.0f, true, NULL};

        state.current_level.sectors = realloc(state.current_level.sectors, sizeof(sector_t) * (state.current_level.sector_count + 1));
        state.current_level.sectors[state.current_level.sector_count] = room;
        state.current_level.sector_count++;

        state.cam.x = 0.0f;
        state.cam.y = 1.5f;
        state.cam.z = 0.0f;

        state.current_sector = level_find_player_sector(&state.current_level, state.cam.x, state.cam.z);
    }

    float old_x = 0.0f, old_z = 0.0f;

    while (VK_FRAME())
    {
        old_x = state.cam.x;
        old_z = state.cam.z;

        level_check_collision(&state.current_level, &state.cam.x, &state.cam.z, old_x, old_z, 0.5f);
        state.current_sector = level_find_player_sector(&state.current_level, state.cam.x, state.cam.z);

        VK_BEGINTEXT;
        VK_DRAWTEXT(-0.9f, -0.9f, "Doom Demo");
        VK_DRAWTEXTF(-0.9f, 0.9f, "FPS:%.0f", state.fps);
        VK_DRAWTEXTF(-0.9f, 0.8f, "Pos: X:%.2f Z:%.2f Y:%.2f", state.cam.x, state.cam.z, state.cam.y);

        if (state.current_sector) VK_DRAWTEXTF(-0.9f, 0.7f, "Sector:%i Light:%.2f", state.current_sector->id, state.current_sector->light_intensity);
        else VK_DRAWTEXT(-0.9f, 0.7f, "Sector:OUTSIDE");
    }

    VK_END();
    level_cleanup(&state.current_level);
}

void RENDER()
{
    const VkDeviceSize offsets[] = {0};

    // Render level geometry
    {
        VK_TEXTURE("Engine/res/test.png");
        if (state.current_sector)
            VK_TINT(
                state.current_sector->light_intensity,
                state.current_sector->light_intensity,
                state.current_sector->light_intensity, 1.0f);

        level_render(&state.current_level);

        if (state.wall_vertex_count > 0)
        {
            mat4 view, proj, vp;
            glm_mat4_identity(view);
            glm_rotate(view, state.cam.pitch, (vec3){1.0f, 0.0f, 0.0f});
            glm_rotate(view, state.cam.yaw, (vec3){0.0f, 1.0f, 0.0f});
            glm_translate(view, (vec3){-state.cam.x, -state.cam.y, state.cam.z});

            glm_perspective(glm_rad(FOV_DEGREES), (float)WIDTH / (float)HEIGHT, NEAR_PLANE, FAR_PLANE, proj);
            glm_mat4_mul(proj, view, vp);

            push_constants_textured_t pc;
            glm_mat4_copy(vp, pc.mvp);
            glm_vec4_copy(tint, pc.tint_color);

            vkCmdBindPipeline(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.textured_pipeline.pipeline);

            const VkDescriptorSet *tex = current_texture ? current_texture : &board_descriptor_set;

            vkCmdBindDescriptorSets(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.textured_pipeline.layout, 0, 1, tex, 0, NULL);
            vkCmdPushConstants(state.v.commandBuffer, state.v.textured_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants_textured_t), &pc);
            vkCmdBindVertexBuffers(state.v.commandBuffer, 0, 1, &state.v.wall_buffer.buffer, offsets);
            vkCmdDraw(state.v.commandBuffer, state.wall_vertex_count, 1, 0, 0);
        }
    }

    // Render text overlay
    {
        VK_TEXTURE("Engine/res/font.png");
        VK_TINT(1.0f, 1.0f, 1.0f, 1.0f);
        mat4 proj;
        glm_ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, proj);

        push_constants_textured_t pc;
        glm_mat4_copy(proj, pc.mvp);
        glm_vec4_copy(tint, pc.tint_color);

        vkCmdBindPipeline(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.text_pipeline.pipeline);
        vkCmdBindDescriptorSets(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.text_pipeline.layout, 0, 1, &font_descriptor_set, 0, NULL);
        vkCmdPushConstants(state.v.commandBuffer, state.v.text_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants_textured_t), &pc);
        vkCmdBindVertexBuffers(state.v.commandBuffer, 0, 1, &state.v.text_buffer.buffer, offsets);

        if (state.v.text_buffer.vertex_count > 0) vkCmdDraw(state.v.commandBuffer, state.v.text_buffer.vertex_count, 1, 0, 0);
    }
}

void INPUT()
{
    const float cam_speed = CAM * state.delta_time;
    const float rot_speed = ROT * state.delta_time;

    if (glfwGetKey(state.glfw.win, GLFW_KEY_LEFT) == GLFW_PRESS) state.cam.yaw -= rot_speed;
    if (glfwGetKey(state.glfw.win, GLFW_KEY_RIGHT) == GLFW_PRESS) state.cam.yaw += rot_speed;
    if (glfwGetKey(state.glfw.win, GLFW_KEY_UP) == GLFW_PRESS) state.cam.pitch += rot_speed;
    if (glfwGetKey(state.glfw.win, GLFW_KEY_DOWN) == GLFW_PRESS) state.cam.pitch -= rot_speed;

    const vec3 forward = {sinf(state.cam.yaw), 0.0f, cosf(state.cam.yaw)};
    const vec3 right = {cosf(state.cam.yaw), 0.0f, -sinf(state.cam.yaw)};

    if (glfwGetKey(state.glfw.win, GLFW_KEY_W) == GLFW_PRESS)
    {
        state.cam.x += forward[0] * cam_speed;
        state.cam.z += forward[2] * cam_speed;
    }
    if (glfwGetKey(state.glfw.win, GLFW_KEY_S) == GLFW_PRESS)
    {
        state.cam.x -= forward[0] * cam_speed;
        state.cam.z -= forward[2] * cam_speed;
    }
    if (glfwGetKey(state.glfw.win, GLFW_KEY_D) == GLFW_PRESS)
    {
        state.cam.x += right[0] * cam_speed;
        state.cam.z += right[2] * cam_speed;
    }
    if (glfwGetKey(state.glfw.win, GLFW_KEY_A) == GLFW_PRESS)
    {
        state.cam.x -= right[0] * cam_speed;
        state.cam.z -= right[2] * cam_speed;
    }
}

// LEVEL RENDERING IMPLEMENTATION

void level_cleanup(level_t *level)
{
    if (level->sectors)
    {
        for (uint32_t i = 0; i < level->sector_count; i++)
        {
            if (level->sectors[i].walls)
            {
                free(level->sectors[i].walls);
                level->sectors[i].walls = NULL;
            }
        }
        free(level->sectors);
        level->sectors = NULL;
    }
    level->sector_count = 0;
}

static bool point_in_polygon(float px, float pz, const wall_t *walls, uint32_t wall_count)
{
    int crossings = 0;

    for (uint32_t i = 0; i < wall_count; i++)
    {
        float x1 = walls[i].x1;
        float z1 = walls[i].z1;
        float x2 = walls[i].x2;
        float z2 = walls[i].z2;

        if (((z1 <= pz) && (z2 > pz)) || ((z1 > pz) && (z2 <= pz)))
        {
            float vt = (pz - z1) / (z2 - z1);
            if (px < x1 + vt * (x2 - x1))
                crossings++;
        }
    }

    return (crossings % 2) == 1;
}

sector_t* level_find_player_sector(level_t *level, float px, float pz)
{
    for (uint32_t i = 0; i < level->sector_count; i++)
    {
        sector_t *sector = &level->sectors[i];
        if (point_in_polygon(px, pz, sector->walls, sector->wall_count))
            return sector;
    }
    return NULL;
}

static void add_wall_quad(float x1, float z1,
                          float x2, float z2,
                          float bottom, float top,
                          vec4 color, float u_scale)
{
    if (state.wall_vertex_count + 6 > MAX_WALL_VERTICES)
        return;

    float dx = x2 - x1;
    float dz = z2 - z1;
    float length = sqrtf(dx*dx + dz*dz);
    float u_max = length * u_scale;
    float v_max = top - bottom;

    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x1, bottom, z1}, {0.0f, 0.0f}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x2, bottom, z2}, {u_max, 0.0f}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x2, top, z2}, {u_max, v_max}, {color[0], color[1], color[2], color[3]}
    };

    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x1, bottom, z1}, {0.0f, 0.0f}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x2, top, z2}, {u_max, v_max}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x1, top, z1}, {0.0f, v_max}, {color[0], color[1], color[2], color[3]}
    };
}

static void render_wall(const wall_t *wall, const sector_t *sector)
{
    vec4 light_color = {
        sector->light_intensity,
        sector->light_intensity,
        sector->light_intensity,
        1.0f
    };

    add_wall_quad(
        wall->x1, wall->z1,
        wall->x2, wall->z2,
        sector->floor_height,
        sector->ceil_height,
        light_color,
        1.0f
    );
}

static void render_sector(const sector_t *sector)
{
    for (uint32_t i = 0; i < sector->wall_count; i++)
    {
        render_wall(&sector->walls[i], sector);
    }
}

void level_render(level_t *level)
{
    state.wall_vertex_count = 0;

    for (uint32_t i = 0; i < level->sector_count; i++)
    {
        render_sector(&level->sectors[i]);
    }
}

static bool line_segment_intersect(float x1, float z1, float x2, float z2,
                                   float x3, float z3, float x4, float z4)
{
    float denom = (x1 - x2) * (z3 - z4) - (z1 - z2) * (x3 - x4);
    if (fabsf(denom) < 0.0001f) return false;

    float t = ((x1 - x3) * (z3 - z4) - (z1 - z3) * (x3 - x4)) / denom;
    float u = -((x1 - x2) * (z1 - z3) - (z1 - z2) * (x1 - x3)) / denom;

    return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
}

static void handle_collision(const wall_t *wall, float *px, float *pz, float old_x, float old_z)
{
    if (!wall->is_solid) return;

    *px = old_x;
    *pz = old_z;
}

bool level_check_collision(level_t *level, float *px, float *pz, float old_x, float old_z, float radius)
{
    bool collided = false;

    sector_t *old_sector = level_find_player_sector(level, old_x, old_z);
    sector_t *new_sector = level_find_player_sector(level, *px, *pz);

    if (old_sector)
    {
        for (uint32_t i = 0; i < old_sector->wall_count; i++)
        {
            const wall_t *wall = &old_sector->walls[i];
            if (!wall->is_solid) continue;

            if (line_segment_intersect(old_x, old_z, *px, *pz,
                                       wall->x1, wall->z1, wall->x2, wall->z2))
            {
                handle_collision(wall, px, pz, old_x, old_z);
                collided = true;
            }
        }
    }

    if (new_sector && new_sector != old_sector)
    {
        for (uint32_t i = 0; i < new_sector->wall_count; i++)
        {
            const wall_t *wall = &new_sector->walls[i];
            if (!wall->is_solid) continue;

            if (line_segment_intersect(old_x, old_z, *px, *pz,
                                       wall->x1, wall->z1, wall->x2, wall->z2))
            {
                handle_collision(wall, px, pz, old_x, old_z);
                collided = true;
            }
        }
    }

    return collided;
}

ENGINE_ENTRY_POINT