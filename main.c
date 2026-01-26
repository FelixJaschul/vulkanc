#include "Engine/App.h"

state_t state;

void RUN()
{
    VK_START();

    while (VK_FRAME())
    {
        VK_BEGINTEXT;
        VK_DRAWTEXTF(-0.9f, 0.9f, "FPS:%.0f", VK_GETFPS());
        VK_DRAWTEXTF(-0.9f, 0.8f, "X:%.2f Y:%.2f Z:%.2f", state.cam.x, state.cam.y, state.cam.z);
        VK_DRAWTEXT("Vulkan Demo", -0.9f, -0.9f);
    }

    VK_END();
}

void INPUT()
{
    const float cam_speed = 3.0f * state.delta_time;
    const float rot_speed = 1.2f * state.delta_time;

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