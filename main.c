#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>

#define WIDTH  800
#define HEIGHT 600

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

void initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    state.glfw.win = glfwCreateWindow(WIDTH, HEIGHT, "vulkan", NULL, NULL);
}

void initVulkan()
{
}

void mainLoop()
{
    while (!glfwWindowShouldClose(state.glfw.win))
    {
        glfwPollEvents();
    }
}

void cleanup()
{
    glfwDestroyWindow(state.glfw.win);
    glfwTerminate();
}

int main()
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();

    return 0;
}