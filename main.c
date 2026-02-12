#include "Engine/App.h"

#define LEVEL_RENDERING
#include "level.h"
#include "Engine/util.h"

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

    // Render level geometry
    {
        VK_TEXTURE("Engine/res/checker.png");
        VK_TINT(1.0f, 1.0f, 1.0f, 1.0f);
        VK_TILETEXTURE(3.0f);
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
            pc.tiling = texture_tiling;

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

ENGINE_ENTRY_POINT
