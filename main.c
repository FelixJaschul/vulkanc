#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE  "vulkan"
#define WIDTH  800
#define HEIGHT 600

#define VK !glfwWindowShouldClose(state.glfw.win)

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

typedef struct
{
    GLFWwindow *win;
} glfw_t;

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
    VkImage* images;
    VkImageView* imageViews;
    VkFramebuffer* framebuffers;

    VkRenderPass renderPass;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    uint32_t graphicsFamilyIndex;
    uint32_t presentFamilyIndex;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
} vulkan_t;

typedef struct
{
    glfw_t glfw;
    vulkan_t v;
} state_t;

state_t state;

void read_file(const char* path, char** data, size_t* size) {
    FILE* file = fopen(path, "rb");
    ASSERT(file, "Failed to open file");
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *data = malloc(*size);
    fread(*data, 1, *size, file);
    fclose(file);
}

VkShaderModule create_shader_module(VkDevice device, const char* code, size_t size) {
    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (const uint32_t*)code;

    VkShaderModule shader_module;
    VK_ASSERT(vkCreateShaderModule(device, &create_info, NULL, &shader_module), "create shader module");
    return shader_module;
}

void VK_START()
{
    ASSERT(glfwInit(), "Window initialization failed");

    // Check if Vulkan is supported
    ASSERT(glfwVulkanSupported(), "GLFW: Vulkan not supported!\n");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    state.glfw.win = glfwCreateWindow(WIDTH, HEIGHT, TITLE, NULL, NULL);
    ASSERT(state.glfw.win, "Window creation failed");

    {

        uint32_t glfw_ext_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        const char** enabled_extensions = malloc(sizeof(char*) * (glfw_ext_count + 1));
        uint32_t enabled_ext_count = glfw_ext_count;

        for (uint32_t i = 0; i < glfw_ext_count; ++i) enabled_extensions[i] = glfw_extensions[i];

        enabled_extensions[enabled_ext_count++] = "VK_KHR_portability_enumeration";

        VkApplicationInfo app_info = {0};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

        VkInstanceCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = enabled_ext_count;
        create_info.ppEnabledExtensionNames = enabled_extensions;
        create_info.enabledLayerCount = 0;

        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        VK_ASSERT(vkCreateInstance(&create_info, NULL, &state.v.instance), "create instance");
        free(enabled_extensions);
    }

    VK_ASSERT(glfwCreateWindowSurface(state.v.instance, state.glfw.win, NULL, &state.v.surface), "create surface");

    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(state.v.instance, &device_count, NULL);
        ASSERT(device_count > 0, "failed to enumerate physical devices");

        VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * device_count);
        vkEnumeratePhysicalDevices(state.v.instance, &device_count, devices);

        // Just pick first for simplicity (you can score later)
        state.v.physicalDevice = devices[0];
        free(devices);
    }

    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(state.v.physicalDevice, &queue_family_count, NULL);
        VkQueueFamilyProperties* queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(state.v.physicalDevice, &queue_family_count, queue_families);

        int graphics_found = 0, present_found = 0;
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                state.v.graphicsFamilyIndex = i;
                graphics_found = 1;
            }

            VkBool32 present_support = 0;
            vkGetPhysicalDeviceSurfaceSupportKHR(state.v.physicalDevice, i, state.v.surface, &present_support);
            if (present_support) {
                state.v.presentFamilyIndex = i;
                present_found = 1;
            }
        }
        free(queue_families);
        ASSERT((graphics_found && present_found), "failed to find graphics queue family");
    }

    {
        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_create_infos[2];
        uint32_t queue_info_count = 0;

        queue_create_infos[queue_info_count++] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = state.v.graphicsFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };

        if (state.v.graphicsFamilyIndex != state.v.presentFamilyIndex) {
            queue_create_infos[queue_info_count++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = state.v.presentFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority
            };
        }

        const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = queue_info_count;
        create_info.pQueueCreateInfos = queue_create_infos;
        create_info.enabledExtensionCount = 1;
        create_info.ppEnabledExtensionNames = device_extensions;

        VK_ASSERT(vkCreateDevice(state.v.physicalDevice, &create_info, NULL, &state.v.device), "create device");

        vkGetDeviceQueue(state.v.device, state.v.graphicsFamilyIndex, 0, &state.v.graphicsQueue);
        vkGetDeviceQueue(state.v.device, state.v.presentFamilyIndex, 0, &state.v.presentQueue);
    }

    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.v.physicalDevice, state.v.surface, &capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(state.v.physicalDevice, state.v.surface, &format_count, NULL);
        VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(state.v.physicalDevice, state.v.surface, &format_count, formats);

        VkSurfaceFormatKHR surface_format = formats[0];
        for (uint32_t i = 0; i < format_count; ++i) {
            if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB) {
                surface_format = formats[i];
                break;
            }
        }
        free(formats);

        state.v.swapChainImageFormat = surface_format.format;
        state.v.swapChainExtent = capabilities.currentExtent;

        uint32_t image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
            image_count = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR swapchain_info = {0};
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.surface = state.v.surface;
        swapchain_info.minImageCount = image_count;
        swapchain_info.imageFormat = surface_format.format;
        swapchain_info.imageColorSpace = surface_format.colorSpace;
        swapchain_info.imageExtent = state.v.swapChainExtent;
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queue_family_indices[] = {state.v.graphicsFamilyIndex, state.v.presentFamilyIndex};
        if (state.v.graphicsFamilyIndex != state.v.presentFamilyIndex) {
            swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_info.queueFamilyIndexCount = 2;
            swapchain_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        swapchain_info.preTransform = capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync
        swapchain_info.clipped = VK_TRUE;
        swapchain_info.oldSwapchain = VK_NULL_HANDLE;

        VK_ASSERT(vkCreateSwapchainKHR(state.v.device, &swapchain_info, NULL, &state.v.swapchain), "create swapchain");

        vkGetSwapchainImagesKHR(state.v.device, state.v.swapchain, &state.v.imageCount, NULL);
        state.v.images = malloc(sizeof(VkImage) * state.v.imageCount);
        state.v.imageViews = malloc(sizeof(VkImageView) * state.v.imageCount);
        state.v.framebuffers = malloc(sizeof(VkFramebuffer) * state.v.imageCount);
        vkGetSwapchainImagesKHR(state.v.device, state.v.swapchain, &state.v.imageCount, state.v.images);
    }

    for (uint32_t i = 0; i < state.v.imageCount; ++i)
    {
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = state.v.images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = state.v.swapChainImageFormat;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VK_ASSERT(vkCreateImageView(state.v.device, &view_info, NULL, &state.v.imageViews[i]), "create image view");
    }

    {
        VkAttachmentDescription color_attachment = {0};
        color_attachment.format = state.v.swapChainImageFormat;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref = {0};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkSubpassDependency dependency = {0};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info = {0};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        VK_ASSERT(vkCreateRenderPass(state.v.device, &render_pass_info, NULL, &state.v.renderPass), "create render pass");
    }

    for (uint32_t i = 0; i < state.v.imageCount; ++i)
    {
        VkFramebufferCreateInfo framebuffer_info = {0};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = state.v.renderPass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &state.v.imageViews[i];
        framebuffer_info.width = state.v.swapChainExtent.width;
        framebuffer_info.height = state.v.swapChainExtent.height;
        framebuffer_info.layers = 1;

        VK_ASSERT(vkCreateFramebuffer(state.v.device, &framebuffer_info, NULL, &state.v.framebuffers[i]), "create framebuffer");
    }

    {
        VkCommandPoolCreateInfo pool_info = {0};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = state.v.graphicsFamilyIndex;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_ASSERT(vkCreateCommandPool(state.v.device, &pool_info, NULL, &state.v.commandPool), "create command pool");
    }

    {
        VkCommandBufferAllocateInfo alloc_info = {0};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = state.v.commandPool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VK_ASSERT(vkAllocateCommandBuffers(state.v.device, &alloc_info, &state.v.commandBuffer), "allocate command buffer");
    }

    char* vert_code; size_t vert_size;
    char* frag_code; size_t frag_size;
    read_file("shad/shad.vert.spv", &vert_code, &vert_size);
    read_file("shad/shad.frag.spv", &frag_code, &frag_size);

    VkShaderModule vert_shader = create_shader_module(state.v.device, vert_code, vert_size);
    VkShaderModule frag_shader = create_shader_module(state.v.device, frag_code, frag_size);

    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader,
            .pName = "main"
        }
    };

    // Fixed-function stages
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)state.v.swapChainExtent.width,
        .height = (float)state.v.swapChainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = state.v.swapChainExtent
    };

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {0};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VK_ASSERT(vkCreatePipelineLayout(state.v.device, &pipeline_layout_info, NULL, &state.v.pipelineLayout), "create pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = state.v.pipelineLayout;
    pipeline_info.renderPass = state.v.renderPass;
    pipeline_info.subpass = 0;

    VK_ASSERT(vkCreateGraphicsPipelines(state.v.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &state.v.pipeline), "create graphics pipeline");

    vkDestroyShaderModule(state.v.device, vert_shader, NULL);
    vkDestroyShaderModule(state.v.device, frag_shader, NULL);
    free(vert_code);
    free(frag_code);

    {
        VkSemaphoreCreateInfo semaphore_info = {0};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_ASSERT(vkCreateSemaphore(state.v.device, &semaphore_info, NULL, &state.v.imageAvailableSemaphore), "create image available semaphore");
        VK_ASSERT(vkCreateSemaphore(state.v.device, &semaphore_info, NULL, &state.v.renderFinishedSemaphore), "create render finished semaphore");

        VkFenceCreateInfo fence_info = {0};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_ASSERT(vkCreateFence(state.v.device, &fence_info, NULL, &state.v.inFlightFence), "create fence");
    }
}

void VK_FRAME()
{
    glfwPollEvents();

    vkWaitForFences(state.v.device, 1, &state.v.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(state.v.device, 1, &state.v.inFlightFence);

    uint32_t image_index;
    vkAcquireNextImageKHR(state.v.device, state.v.swapchain, UINT64_MAX,
                          state.v.imageAvailableSemaphore, VK_NULL_HANDLE, &image_index);

    vkResetCommandBuffer(state.v.commandBuffer, 0);

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(state.v.commandBuffer, &begin_info);

    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = state.v.renderPass;
    render_pass_info.framebuffer = state.v.framebuffers[image_index];
    render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
    render_pass_info.renderArea.extent = state.v.swapChainExtent;
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(state.v.commandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.pipeline);
    vkCmdDraw(state.v.commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(state.v.commandBuffer);

    vkEndCommandBuffer(state.v.commandBuffer);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {state.v.imageAvailableSemaphore};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &state.v.commandBuffer;

    VkSemaphore signal_semaphores[] = {state.v.renderFinishedSemaphore};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VK_ASSERT(vkQueueSubmit(state.v.graphicsQueue, 1, &submit_info, state.v.inFlightFence), "submit draw");

    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &state.v.swapchain;
    present_info.pImageIndices = &image_index;

    vkQueuePresentKHR(state.v.presentQueue, &present_info);
}

void VK_END()
{
    vkDeviceWaitIdle(state.v.device);

    vkDestroySemaphore(state.v.device, state.v.imageAvailableSemaphore, NULL);
    vkDestroySemaphore(state.v.device, state.v.renderFinishedSemaphore, NULL);
    vkDestroyFence(state.v.device, state.v.inFlightFence, NULL);

    vkDestroyPipeline(state.v.device, state.v.pipeline, NULL);
    vkDestroyPipelineLayout(state.v.device, state.v.pipelineLayout, NULL);
    vkDestroyRenderPass(state.v.device, state.v.renderPass, NULL);

    for (uint32_t i = 0; i < state.v.imageCount; ++i) {
        vkDestroyFramebuffer(state.v.device, state.v.framebuffers[i], NULL);
        vkDestroyImageView(state.v.device, state.v.imageViews[i], NULL);
    }
    free(state.v.images);
    free(state.v.imageViews);
    free(state.v.framebuffers);

    vkDestroySwapchainKHR(state.v.device, state.v.swapchain, NULL);
    vkDestroyDevice(state.v.device, NULL);
    vkDestroySurfaceKHR(state.v.instance, state.v.surface, NULL);
    vkDestroyInstance(state.v.instance, NULL);

    glfwDestroyWindow(state.glfw.win);
    glfwTerminate();
}

void VK_RESIZE()
{
    // TODO: handle resize (recreate swapchain)
}

int main()
{
    VK_START();
    while (VK) VK_FRAME();
    VK_END();

    return 0;
}