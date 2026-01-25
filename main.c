#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include <../ext/stb_image.h>

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

typedef struct {
    float x, y;
    float u, v;
} vertex_t;

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
    VkPipeline trianglePipeline;
    VkPipelineLayout trianglePipelineLayout;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer triangleVertexBuffer;
    VkDeviceMemory triangleVertexBufferMemory;

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

// Text rendering state
#define MAX_TEXT_VERTICES 10000
vertex_t text_vertices[MAX_TEXT_VERTICES];
uint32_t text_vertex_count = 0;

uint32_t find_memory_type(const uint32_t type_filter, const VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(state.v.physicalDevice, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    ASSERT(0, "failed to find suitable memory type");
    return 0;
}

VkBuffer create_buffer(const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties, VkDeviceMemory* outMemory) {
    VkBuffer buffer;
    const VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_ASSERT(vkCreateBuffer(state.v.device, &buffer_info, NULL, &buffer), "create buffer");

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(state.v.device, buffer, &mem_reqs);

    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties)
    };
    VK_ASSERT(vkAllocateMemory(state.v.device, &alloc_info, NULL, outMemory), "allocate buffer memory");
    vkBindBufferMemory(state.v.device, buffer, *outMemory, 0);
    return buffer;
}

void create_texture_image(const int width, const int height, VkDeviceSize size, const VkBuffer stagingBuffer, VkImage* outImage, VkDeviceMemory* outMemory) {
    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_ASSERT(vkCreateImage(state.v.device, &image_info, NULL, outImage), "create texture image");

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(state.v.device, *outImage, &mem_reqs);
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_ASSERT(vkAllocateMemory(state.v.device, &alloc_info, NULL, outMemory), "allocate texture memory");
    vkBindImageMemory(state.v.device, *outImage, *outMemory, 0);

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(state.v.commandBuffer, &begin_info);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = *outImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(state.v.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    const VkBufferImageCopy region = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {width, height, 1}
    };
    vkCmdCopyBufferToImage(state.v.commandBuffer, stagingBuffer, *outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(state.v.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    vkEndCommandBuffer(state.v.commandBuffer);

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &state.v.commandBuffer
    };
    vkQueueSubmit(state.v.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(state.v.graphicsQueue);
}

void create_texture_image_view(const VkImage image, VkImageView* outView) {
    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_ASSERT(vkCreateImageView(state.v.device, &view_info, NULL, outView), "create texture view");
}

void create_texture_sampler(VkSampler* outSampler) {
    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f
    };
    VK_ASSERT(vkCreateSampler(state.v.device, &sampler_info, NULL, outSampler), "create sampler");
}

void create_descriptor_set_layout(VkDescriptorSetLayout* outLayout) {
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    VK_ASSERT(vkCreateDescriptorSetLayout(state.v.device, &layout_info, NULL, outLayout), "create descriptor layout");
}

void create_descriptor_pool(VkDescriptorPool* outPool) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };
    VK_ASSERT(vkCreateDescriptorPool(state.v.device, &pool_info, NULL, outPool), "create descriptor pool");
}

void allocate_descriptor_set(const VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet* outSet) {
    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };
    VK_ASSERT(vkAllocateDescriptorSets(state.v.device, &alloc_info, outSet), "allocate descriptor set");
}

void update_descriptor_set(const VkDescriptorSet set, const VkImageView view, const VkSampler sampler) {
    VkDescriptorImageInfo image_info = {
        .sampler = sampler,
        .imageView = view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_info
    };
    vkUpdateDescriptorSets(state.v.device, 1, &write, 0, NULL);
}

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

VkShaderModule create_shader_module(const VkDevice device, const char* code, const size_t size) {
    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (const uint32_t*)code;

    VkShaderModule shader_module;
    VK_ASSERT(vkCreateShaderModule(device, &create_info, NULL, &shader_module), "create shader module");
    return shader_module;
}

typedef struct {
    uint8_t x;
    uint8_t y;
} glyph_uv_t;

static glyph_uv_t glyphs[128];

void draw_char(const char c, const float x, const float y, const float char_width, const float char_height) {
    if (text_vertex_count + 6 > MAX_TEXT_VERTICES) return;

    // Character dimensions (16x16 grid in texture)
    const float atlas_char_width  = 1.0f / 16.0f;
    const float atlas_char_height = 1.0f / 16.0f;

    glyph_uv_t g = glyphs[(uint8_t)c];

    const float u0 = g.x * atlas_char_width;
    const float v0 = g.y * atlas_char_height;
    const float u1 = u0 + atlas_char_width;
    const float v1 = v0 + atlas_char_height;

    text_vertices[text_vertex_count++] = (vertex_t){ x, y, u0, v0 };
    text_vertices[text_vertex_count++] = (vertex_t){ x + char_width, y, u1, v0 };
    text_vertices[text_vertex_count++] = (vertex_t){ x + char_width, y + char_height, u1, v1 };

    text_vertices[text_vertex_count++] = (vertex_t){ x, y, u0, v0 };
    text_vertices[text_vertex_count++] = (vertex_t){ x + char_width, y + char_height, u1, v1 };
    text_vertices[text_vertex_count++] = (vertex_t){ x, y + char_height, u0, v1 };
}

void draw_string(const char* str, const float x, const float y) {
    if (!str) return;

    const float char_aspect = 0.5f;
    const float char_height = 0.08f;
    const float char_width = char_height * char_aspect;
    const float spacing = char_width * 1.1f;

    float current_x = x;
    for (const char* p = str; *p; p++) {
        draw_char(*p, current_x, y, char_width, char_height);
        current_x += spacing;
    }
}

#define DISPLAY(str, x, y) draw_string(str, x, y)

void begin_text_rendering() {
    text_vertex_count = 0;
}

void VK_START()
{
    {
        glyphs[':'] = (glyph_uv_t){ 10, 3 };
        glyphs[';'] = (glyph_uv_t){ 11, 3 };
        glyphs['<'] = (glyph_uv_t){ 12, 3 };
        glyphs['='] = (glyph_uv_t){ 13, 3 };
        glyphs['>'] = (glyph_uv_t){ 14, 3 };
        glyphs['?'] = (glyph_uv_t){ 15, 3 };

        glyphs['A'] = (glyph_uv_t){  1, 4 };
        glyphs['B'] = (glyph_uv_t){  2, 4 };
        glyphs['C'] = (glyph_uv_t){  3, 4 };
        glyphs['D'] = (glyph_uv_t){  4, 4 };
        glyphs['E'] = (glyph_uv_t){  5, 4 };
        glyphs['F'] = (glyph_uv_t){  6, 4 };
        glyphs['G'] = (glyph_uv_t){  7, 4 };
        glyphs['H'] = (glyph_uv_t){  8, 4 };
        glyphs['I'] = (glyph_uv_t){  9, 4 };
        glyphs['J'] = (glyph_uv_t){ 10, 4 };
        glyphs['K'] = (glyph_uv_t){ 11, 4 };
        glyphs['L'] = (glyph_uv_t){ 12, 4 };
        glyphs['M'] = (glyph_uv_t){ 13, 4 };
        glyphs['N'] = (glyph_uv_t){ 14, 4 };
        glyphs['O'] = (glyph_uv_t){ 15, 4 };

        glyphs['P'] = (glyph_uv_t){  0, 5 };
        glyphs['Q'] = (glyph_uv_t){  1, 5 };
        glyphs['R'] = (glyph_uv_t){  2, 5 };
        glyphs['S'] = (glyph_uv_t){  3, 5 };
        glyphs['T'] = (glyph_uv_t){  4, 5 };
        glyphs['U'] = (glyph_uv_t){  5, 5 };
        glyphs['V'] = (glyph_uv_t){  6, 5 };
        glyphs['W'] = (glyph_uv_t){  7, 5 };
        glyphs['X'] = (glyph_uv_t){  8, 5 };
        glyphs['Y'] = (glyph_uv_t){  9, 5 };
        glyphs['Z'] = (glyph_uv_t){ 10, 5 };
        glyphs['_'] = (glyph_uv_t){ 15, 5 };

        glyphs['a'] = (glyph_uv_t){  1, 6 };
        glyphs['b'] = (glyph_uv_t){  2, 6 };
        glyphs['c'] = (glyph_uv_t){  3, 6 };
        glyphs['d'] = (glyph_uv_t){  4, 6 };
        glyphs['e'] = (glyph_uv_t){  5, 6 };
        glyphs['f'] = (glyph_uv_t){  6, 6 };
        glyphs['g'] = (glyph_uv_t){  7, 6 };
        glyphs['h'] = (glyph_uv_t){  8, 6 };
        glyphs['i'] = (glyph_uv_t){  9, 6 };
        glyphs['j'] = (glyph_uv_t){ 10, 6 };
        glyphs['k'] = (glyph_uv_t){ 11, 6 };
        glyphs['l'] = (glyph_uv_t){ 12, 6 };
        glyphs['m'] = (glyph_uv_t){ 13, 6 };
        glyphs['n'] = (glyph_uv_t){ 14, 6 };
        glyphs['o'] = (glyph_uv_t){ 15, 6 };

        glyphs['p'] = (glyph_uv_t){  0, 7 };
        glyphs['q'] = (glyph_uv_t){  1, 7 };
        glyphs['r'] = (glyph_uv_t){  2, 7 };
        glyphs['s'] = (glyph_uv_t){  3, 7 };
        glyphs['t'] = (glyph_uv_t){  4, 7 };
        glyphs['u'] = (glyph_uv_t){  5, 7 };
        glyphs['v'] = (glyph_uv_t){  6, 7 };
        glyphs['w'] = (glyph_uv_t){  7, 7 };
        glyphs['x'] = (glyph_uv_t){  8, 7 };
        glyphs['y'] = (glyph_uv_t){  9, 7 };
        glyphs['z'] = (glyph_uv_t){ 10, 7 };

    }

    ASSERT(glfwInit(), "Window initialization failed");
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
        swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
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

    // Load font texture
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("res/font.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    ASSERT(pixels, "Failed to load font.png");
    printf("Loaded font texture: %dx%d (%d channels)\n", texWidth, texHeight, texChannels);

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    stagingBuffer = create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBufferMemory);

    void* data;
    vkMapMemory(state.v.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(state.v.device, stagingBufferMemory);
    stbi_image_free(pixels);

    create_texture_image(texWidth, texHeight, imageSize, stagingBuffer, &state.v.textureImage, &state.v.textureImageMemory);
    vkDestroyBuffer(state.v.device, stagingBuffer, NULL);
    vkFreeMemory(state.v.device, stagingBufferMemory, NULL);

    create_texture_image_view(state.v.textureImage, &state.v.textureImageView);
    create_texture_sampler(&state.v.textureSampler);
    create_descriptor_set_layout(&state.v.descriptorSetLayout);
    create_descriptor_pool(&state.v.descriptorPool);
    allocate_descriptor_set(state.v.descriptorPool, state.v.descriptorSetLayout, &state.v.descriptorSet);
    update_descriptor_set(state.v.descriptorSet, state.v.textureImageView, state.v.textureSampler);

    VkDeviceSize vertexBufferSize = sizeof(vertex_t) * MAX_TEXT_VERTICES;
    state.v.vertexBuffer = create_buffer(vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &state.v.vertexBufferMemory);

    // Create triangle vertex buffer
    const float triangle_vertices[] = {
        -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // Bottom-left - Red
         1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // Bottom-right - Green
         0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 1.0f   // Top - Blue
    };
    VkDeviceSize triangleSize = sizeof(triangle_vertices);
    state.v.triangleVertexBuffer = create_buffer(triangleSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &state.v.triangleVertexBufferMemory);

    void* tri_data;
    vkMapMemory(state.v.device, state.v.triangleVertexBufferMemory, 0, triangleSize, 0, &tri_data);
    memcpy(tri_data, triangle_vertices, triangleSize);
    vkUnmapMemory(state.v.device, state.v.triangleVertexBufferMemory);

    char* vert_code; size_t vert_size;
    char* frag_code; size_t frag_size;
    read_file("shad/quad.vert.spv", &vert_code, &vert_size);
    read_file("shad/quad.frag.spv", &frag_code, &frag_size);

    VkShaderModule vert_shader = create_shader_module(state.v.device, vert_code, vert_size);
    VkShaderModule frag_shader = create_shader_module(state.v.device, frag_code, frag_size);

    VkPipelineShaderStageCreateInfo shader_stages[2] = { {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader, .pName = "main" }, {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader, .pName = "main" }
    };

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attr_descs[2] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex_t, x) },
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex_t, u) }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attr_descs
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &state.v.descriptorSetLayout
    };
    VK_ASSERT(vkCreatePipelineLayout(state.v.device, &layout_info, NULL, &state.v.pipelineLayout), "create pipeline layout");

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {
        .x = 0.0f, .y = 0.0f,
        .width = (float)state.v.swapChainExtent.width,
        .height = (float)state.v.swapChainExtent.height,
        .minDepth = 0.0f, .maxDepth = 1.0f
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
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending = {0};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
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

    // Create triangle pipeline (simple colored triangle, no textures)
    char* tri_vert_code; size_t tri_vert_size;
    char* tri_frag_code; size_t tri_frag_size;
    read_file("shad/tri.vert.spv", &tri_vert_code, &tri_vert_size);
    read_file("shad/tri.frag.spv", &tri_frag_code, &tri_frag_size);

    VkShaderModule tri_vert_shader = create_shader_module(state.v.device, tri_vert_code, tri_vert_size);
    VkShaderModule tri_frag_shader = create_shader_module(state.v.device, tri_frag_code, tri_frag_size);

    VkPipelineShaderStageCreateInfo tri_shader_stages[2] = { {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = tri_vert_shader, .pName = "main"  }, {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = tri_frag_shader, .pName = "main"  }
    };

    VkVertexInputBindingDescription tri_binding = {
        .binding = 0,
        .stride = 6 * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription tri_attrs[2] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
        { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 2 * sizeof(float) }
    };
    VkPipelineVertexInputStateCreateInfo tri_vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &tri_binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = tri_attrs
    };

    VkPipelineLayoutCreateInfo tri_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = NULL
    };
    VK_ASSERT(vkCreatePipelineLayout(state.v.device, &tri_layout_info, NULL, &state.v.trianglePipelineLayout), "create triangle pipeline layout");

    VkPipelineColorBlendAttachmentState tri_blend = {0};
    tri_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    tri_blend.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo tri_blending = {0};
    tri_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    tri_blending.logicOpEnable = VK_FALSE;
    tri_blending.attachmentCount = 1;
    tri_blending.pAttachments = &tri_blend;

    VkGraphicsPipelineCreateInfo tri_pipeline_info = {0};
    tri_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    tri_pipeline_info.stageCount = 2;
    tri_pipeline_info.pStages = tri_shader_stages;
    tri_pipeline_info.pVertexInputState = &tri_vertex_input;
    tri_pipeline_info.pInputAssemblyState = &input_assembly;
    tri_pipeline_info.pViewportState = &viewport_state;
    tri_pipeline_info.pRasterizationState = &rasterizer;
    tri_pipeline_info.pMultisampleState = &multisampling;
    tri_pipeline_info.pColorBlendState = &tri_blending;
    tri_pipeline_info.layout = state.v.trianglePipelineLayout;
    tri_pipeline_info.renderPass = state.v.renderPass;
    tri_pipeline_info.subpass = 0;

    VK_ASSERT(vkCreateGraphicsPipelines(state.v.device, VK_NULL_HANDLE, 1, &tri_pipeline_info, NULL, &state.v.trianglePipeline), "create triangle pipeline");

    vkDestroyShaderModule(state.v.device, tri_vert_shader, NULL);
    vkDestroyShaderModule(state.v.device, tri_frag_shader, NULL);
    free(tri_vert_code);
    free(tri_frag_code);

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

    if (text_vertex_count > 0) {
        void* data;
        vkMapMemory(state.v.device, state.v.vertexBufferMemory, 0, sizeof(vertex_t) * text_vertex_count, 0, &data);
        memcpy(data, text_vertices, sizeof(vertex_t) * text_vertex_count);
        vkUnmapMemory(state.v.device, state.v.vertexBufferMemory);
    }

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
    const VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(state.v.commandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.trianglePipeline);
    const VkDeviceSize tri_offsets[] = {0};
    vkCmdBindVertexBuffers(state.v.commandBuffer, 0, 1, &state.v.triangleVertexBuffer, tri_offsets);
    vkCmdDraw(state.v.commandBuffer, 3, 1, 0, 0);

    vkCmdBindPipeline(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v.pipeline);

    vkCmdBindDescriptorSets(state.v.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            state.v.pipelineLayout, 0, 1, &state.v.descriptorSet, 0, NULL);

    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(state.v.commandBuffer, 0, 1, &state.v.vertexBuffer, offsets);

    if (text_vertex_count > 0) {
        vkCmdDraw(state.v.commandBuffer, text_vertex_count, 1, 0, 0);
    }

    vkCmdEndRenderPass(state.v.commandBuffer);
    vkEndCommandBuffer(state.v.commandBuffer);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkSemaphore wait_semaphores[] = {state.v.imageAvailableSemaphore};
    const VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &state.v.commandBuffer;

    const VkSemaphore signal_semaphores[] = {state.v.renderFinishedSemaphore};
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

    vkDestroyBuffer(state.v.device, state.v.vertexBuffer, NULL);
    vkFreeMemory(state.v.device, state.v.vertexBufferMemory, NULL);

    vkDestroyImageView(state.v.device, state.v.textureImageView, NULL);
    vkDestroySampler(state.v.device, state.v.textureSampler, NULL);
    vkDestroyImage(state.v.device, state.v.textureImage, NULL);
    vkFreeMemory(state.v.device, state.v.textureImageMemory, NULL);

    vkDestroyDescriptorPool(state.v.device, state.v.descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(state.v.device, state.v.descriptorSetLayout, NULL);

    vkDestroyPipeline(state.v.device, state.v.pipeline, NULL);
    vkDestroyPipelineLayout(state.v.device, state.v.pipelineLayout, NULL);
    vkDestroyRenderPass(state.v.device, state.v.renderPass, NULL);

    vkDestroyCommandPool(state.v.device, state.v.commandPool, NULL);

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

int main()
{
    VK_START();
    while (VK)
    {
        VK_FRAME();
        begin_text_rendering();
        DISPLAY("ABCDEFGHIJKLMNOPQRSTUVWXYZ_;<=>?", -0.9f, -0.9f);
        DISPLAY("abcdefghijklmnopqrstuvwxyz:", -0.9f, -0.8f);
    }
    VK_END();

    return 0;
}