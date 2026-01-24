#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>

#define TITLE  "vulkan"
#define WIDTH  800
#define HEIGHT 600

#define VK !glfwWindowShouldClose(state.glfw.win)

typedef struct
{
    GLFWwindow *win;
} glfw_t;

typedef struct
{

} vulkan_t;

typedef struct
{
    glfw_t glfw;
    vulkan_t v;
} state_t;

state_t state;

void VK_START()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    state.glfw.win = glfwCreateWindow(WIDTH, HEIGHT, TITLE, NULL, NULL);
}

void initVulkan()
{
}

void VK_FRAME()
{
    glfwPollEvents();
}

void VK_END()
{
    glfwDestroyWindow(state.glfw.win);
    glfwTerminate();
}

void VK_RESIZE()
{

}

int main()
{
    VK_START();
    while (VK) VK_FRAME();
    VK_END();

    return 0;
}