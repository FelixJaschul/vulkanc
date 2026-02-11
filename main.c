#include "Engine/App.h"

sector_t* level_find_player_sector(const level_t *level, float px, float pz);
level_t level_load_from_file(const char* filepath);
void level_render(const level_t *level);
bool level_check_collision(const level_t *level, float *px, float *pz, float old_x, float old_z);
void level_cleanup(level_t *level);

void RUN()
{
    VK_START();

    state.current_level = level_load_from_file("Engine/res/level.txt");
    ASSERT(state.current_level.sectors, "Failed to load level file!");

    state.cam.x = 0.0f;
    state.cam.y = 1.5f;
    state.cam.z = 0.0f;
    state.cam.yaw = 0.0f;
    state.current_sector = level_find_player_sector(&state.current_level, state.cam.x, state.cam.z);

    float old_x = 0.0f, old_z = 0.0f;
    while (VK_FRAME())
    {
        old_x = state.cam.x;
        old_z = state.cam.z;
        level_check_collision(&state.current_level, &state.cam.x, &state.cam.z, old_x, old_z);
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

    VK_TEXTURE("Engine/res/checker.png");
    VK_TINT(1.0f, 1.0f, 1.0f, 1.0f);

    // Render level geometry
    {
        level_render(&state.current_level);

        if (state.wall_vertex_count > 0)
        {
            mat4 view, proj, vp;
            glm_mat4_identity(view);
            glm_rotate(view, state.cam.pitch, (vec3){1.0f, 0.0f, 0.0f});
            glm_rotate(view, state.cam.yaw, (vec3){0.0f, 1.0f, 0.0f});
            glm_translate(view, (vec3){-state.cam.x, -state.cam.y, -state.cam.z});

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
        VK_TINT(1.0f, 1.0f, 0.0f, 1.0f);
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

    const vec3 forward = {sinf(state.cam.yaw), 0.0f, -cosf(state.cam.yaw)};
    const vec3 right = {cosf(state.cam.yaw), 0.0f, sinf(state.cam.yaw)};

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

static bool point_in_polygon(const float px, const float pz, const wall_t *walls, const uint32_t wall_count)
{
    int crossings = 0;

    for (uint32_t i = 0; i < wall_count; i++)
    {
        const wall_t *w = &walls[i];
        if (((w->z1 <= pz) && (w->z2 > pz)) || ((w->z2 <= pz) && (w->z1 > pz)))
        {
            const float vt = (pz - w->z1) / (w->z2 - w->z1);
            if (px < w->x1 + vt * (w->x2 - w->x1))
                crossings++;
        }
    }

    return (crossings % 2) == 1;
}

sector_t* level_find_player_sector(const level_t *level, const float px, const float pz)
{
    for (uint32_t i = 0; i < level->sector_count; i++)
    {
        sector_t *sector = &level->sectors[i];
        if (point_in_polygon(px, pz, sector->walls, sector->wall_count))
            return sector;
    }
    return NULL;
}

static void add_wall_quad(const float x1, const float z1,
                          const float x2, const float z2,
                          const float bottom, const float top,
                          const vec4 color, const float u_scale)
{
    if (state.wall_vertex_count + 6 > MAX_WALL_VERTICES)
        return;

    const float dx = x2 - x1;
    const float dz = z2 - z1;
    const float length = sqrtf(dx*dx + dz*dz);
    const float u_max = length * u_scale;
    const float v_max = top - bottom;

    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x1, bottom, z1}, {0.0f, 0.0f}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x2, top, z2}, {u_max, v_max}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x2, bottom, z2}, {u_max, 0.0f}, {color[0], color[1], color[2], color[3]}
    };

    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x1, bottom, z1}, {0.0f, 0.0f}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x1, top, z1}, {0.0f, v_max}, {color[0], color[1], color[2], color[3]}
    };
    state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
            {x2, top, z2}, {u_max, v_max}, {color[0], color[1], color[2], color[3]}
    };
}

static void render_sector(const sector_t *sector)
{
    const vec4 floor_color = {
        0.3f * sector->light_intensity,
        0.3f * sector->light_intensity,
        0.3f * sector->light_intensity,
        1.0f
    };

    const vec4 ceil_color = {
        0.7f * sector->light_intensity,
        0.7f * sector->light_intensity,
        0.7f * sector->light_intensity,
        1.0f
    };

    for (uint32_t i = 0; i < sector->wall_count; i++)
    {
        const wall_t *wall = &sector->walls[i];

        if (!wall->is_invisible)
        {
            add_wall_quad(
                wall->x1, wall->z1,
                wall->x2, wall->z2,
                sector->floor_height,
                sector->ceil_height,
                (vec4) {
                    wall->color[0] * sector->light_intensity,
                    wall->color[1] * sector->light_intensity,
                    wall->color[2] * sector->light_intensity,
                    1.0f
                },
                1.0f
            );
        }

        if (i > 0)
        {
            if (state.wall_vertex_count + 6 <= MAX_WALL_VERTICES)
            {
                // Floor
                state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
                            {sector->walls[0].x1, sector->floor_height, sector->walls[0].z1},
                            {sector->walls[0].x1, sector->walls[0].z1},
                            {floor_color[0], floor_color[1], floor_color[2], floor_color[3]}
                };
                state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
                            {wall->x1, sector->floor_height, wall->z1},
                            {wall->x1, wall->z1},
                            {floor_color[0], floor_color[1], floor_color[2], floor_color[3]}
                };
                state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
                            {wall->x2, sector->floor_height, wall->z2},
                            {wall->x2, wall->z2},
                            {floor_color[0], floor_color[1], floor_color[2], floor_color[3]}
                };

                // Ceil
                state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
                            {sector->walls[0].x1, sector->ceil_height, sector->walls[0].z1},
                            {sector->walls[0].x1, sector->walls[0].z1},
                            {ceil_color[0], ceil_color[1], ceil_color[2], ceil_color[3]}
                };
                state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
                            {wall->x2, sector->ceil_height, wall->z2},
                            {wall->x2, wall->z2},
                            {ceil_color[0], ceil_color[1], ceil_color[2], ceil_color[3]}
                };
                state.wall_vertices[state.wall_vertex_count++] = (vertex_t){
                            {wall->x1, sector->ceil_height, wall->z1},
                            {wall->x1, wall->z1},
                            {ceil_color[0], ceil_color[1], ceil_color[2], ceil_color[3]}
                };
            }
        }
    }
}

void level_render(const level_t *level)
{
    state.wall_vertex_count = 0;

    for (uint32_t i = 0; i < level->sector_count; i++)
    {
        render_sector(&level->sectors[i]);
    }
}

static bool line_segment_intersect(const float x1, const float z1, const float x2, const float z2,
                                   const float x3, const float z3, const float x4, const float z4)
{
    const float denom = (x1 - x2) * (z3 - z4) - (z1 - z2) * (x3 - x4);
    if (fabsf(denom) < 0.0001f) return false;

    const float t = ((x1 - x3) * (z3 - z4) - (z1 - z3) * (x3 - x4)) / denom;
    const float u = -((x1 - x2) * (z1 - z3) - (z1 - z2) * (x1 - x3)) / denom;

    return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
}

static void handle_collision(const wall_t *wall, float *px, float *pz, float old_x, float old_z)
{
    if (!wall->is_solid) return;

    *px = old_x;
    *pz = old_z;
}

bool level_check_collision(const level_t *level, float *px, float *pz, const float old_x, const float old_z)
{
    bool collided = false;

    const sector_t *old_sector = level_find_player_sector(level, old_x, old_z);
    const sector_t *new_sector = level_find_player_sector(level, *px, *pz);

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

level_t level_load_from_file(const char* filepath)
{
    level_t level = {
        .name = "LOADED",
        .path = filepath,
        .sectors = NULL,
        .sector_count = 0
    };

    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Could not open level file: %s\n", filepath);
        return level;
    }

    char line[512];
    enum { NONE, WALLS, SECTORS } section = NONE;

    // Temporary storage for walls (max 256 walls)
    #define MAX_TEMP_WALLS 256
    wall_t temp_walls[MAX_TEMP_WALLS];
    int wall_count = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '#' || line[0] == '\0') continue;

        if (strncmp(line, "[WALLS]", 7) == 0) {
            section = WALLS;
            continue;
        }
        if (strncmp(line, "[SECTORS]", 9) == 0) {
            section = SECTORS;
            continue;
        }

        // Parse based on current section
        if (section == WALLS)
        {
            int id, is_solid, is_inv = 0;
            float x1, z1, x2, z2;
            float r = 1.0f, g = 1.0f, b = 1.0f;

            const int read = sscanf(line, "%d %f %f %f %f %d %d %f %f %f", &id, &x1, &z1, &x2, &z2, &is_solid, &is_inv, &r, &g, &b);
            if (read >= 6) {
                if (id >= 0 && id < MAX_TEMP_WALLS) {
                    temp_walls[id] = (wall_t){
                        .id = id,
                        .x1 = x1, .z1 = z1,
                        .x2 = x2, .z2 = z2,
                        .color = {r, g, b},
                        .is_solid = (is_solid != 0),
                        .is_invisible = (read >= 7 && is_inv != 0),
                        .texture_path = NULL
                    };
                    if (id >= wall_count) wall_count = id + 1;
                }
            }
        }
        else if (section == SECTORS)
        {
            int sector_id;
            float light, floor_h, ceil_h;

            // Parse sector header
            const char* ptr = line;
            if (sscanf(ptr, "%d %f %f %f", &sector_id, &light, &floor_h, &ceil_h) == 4)
            {
                // Skip past the first 4 values
                int values_read = 0;
                while (*ptr && values_read < 4) {
                    while (*ptr && isspace(*ptr)) ptr++;
                    while (*ptr && !isspace(*ptr)) ptr++;
                    values_read++;
                }

                // Count wall IDs
                int wall_ids[MAX_TEMP_WALLS];
                int wall_id_count = 0;
                int wall_id;
                int n;

                while (sscanf(ptr, "%d%n", &wall_id, &n) == 1) {
                    wall_ids[wall_id_count++] = wall_id;
                    ptr += n;
                }

                // Create sector
                const sector_t new_sector = {
                    .id = sector_id,
                    .light_intensity = light,
                    .floor_height = floor_h,
                    .ceil_height = ceil_h,
                    .wall_count = wall_id_count,
                    .walls = malloc(sizeof(wall_t) * wall_id_count)
                };

                // Copy walls from temp storage
                for (int i = 0; i < wall_id_count; i++)
                    if (wall_ids[i] >= 0 && wall_ids[i] < wall_count)
                        new_sector.walls[i] = temp_walls[wall_ids[i]];

                // Enclosure check
                bool enclosed = true;
                for (int i = 0; i < wall_id_count; i++) {
                    const int next = (i + 1) % wall_id_count;
                    if (fabsf(new_sector.walls[i].x2 - new_sector.walls[next].x1) > 0.001f ||
                        fabsf(new_sector.walls[i].z2 - new_sector.walls[next].z1) > 0.001f) {
                        enclosed = false;
                        break;
                    }
                }

                if (!enclosed) {
                    fprintf(stderr, "ERROR: Sector %d walls not done (not enclosed)\n", sector_id);
                    for (int i = 0; i < wall_id_count; i++) {
                        fprintf(stderr, "  Wall %d: (%f, %f) -> (%f, %f)\n",
                               new_sector.walls[i].id,
                               new_sector.walls[i].x1, new_sector.walls[i].z1,
                               new_sector.walls[i].x2, new_sector.walls[i].z2);
                    }
                    exit(1);
                }

                // Add sector to level
                level.sectors = realloc(level.sectors, sizeof(sector_t) * (level.sector_count + 1));
                level.sectors[level.sector_count] = new_sector;
                level.sector_count++;
            }
        }
    }

    fclose(file);
    printf("Loaded level: %d sectors, %d walls\n", level.sector_count, wall_count);
    return level;
}

ENGINE_ENTRY_POINT
