#include "Engine/App.h"

#define CUBE 4

void RUN()
{
    VK_START();
    while (VK_FRAME())
    {
        VK_BEGINTEXT;
        VK_DRAWTEXTF(-0.9f, 0.9f, "FPS:%.0f", state.fps);
        VK_DRAWTEXTF(-0.9f, 0.8f, "X:%.2f Y:%.2f Z:%.2f", state.cam.x, state.cam.y, state.cam.z);
        VK_DRAWTEXT(-0.9f, -0.9f, "Vulkan Demo");
        VK_DRAWTEXTF(-0.9f, 0.7f, "SIZE:%i", CUBE*CUBE*CUBE);
    }

    VK_END();
}

void RENDER()
{
    const VkDeviceSize offsets[] = {0};

    {
        vkCmdBindDescriptorSets(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.textured_pipeline.layout, 0, 1, &board_descriptor_set, 0, NULL);
        vkCmdBindPipeline(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.textured_pipeline.pipeline);
        vkCmdBindVertexBuffers(state.v.commandBuffer, 0, 1, &state.v.cube_buffer.buffer, offsets);
    }

    {
        VK_TEXTURE("Engine/res/checker.png");
        VK_TINT(0.67f, 0.70f, 0.67f, 1.0f);
        VK_DRAWCUBE(1, 0, 0, 0, 1);
        for (int i = 0; i < CUBE; i++)
        for (int j = 0; j < CUBE; j++)
        for (int k = 0; k < CUBE; k++)
        {
            VK_TEXTURE("Engine/res/test.png");
            VK_TINT(0.37f, 0.10f, 0.17f, 1.0f);
            VK_DRAWCUBE(k-4, j-2, i-4, 0, 1);
        }
    }

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

ENGINE_ENTRY_POINT