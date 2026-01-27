#include "App.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

state_t state;
glyph_uv_t glyphs[128];

static uint32_t find_memory_type(const uint32_t type_filter, const VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(state.v.physicalDevice, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    ASSERT(0, "failed to find suitable memory type");
    return 0;
}

static void create_buffer(const VkDeviceSize size, const VkBufferUsageFlags usage,
                          const VkMemoryPropertyFlags properties, VkBuffer *outBuffer, VkDeviceMemory *outMemory)
{
    const VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_ASSERT(vkCreateBuffer(state.v.device, &buffer_info, NULL, outBuffer), "create buffer");
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(state.v.device, *outBuffer, &mem_reqs);

    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties)
    };

    VK_ASSERT(vkAllocateMemory(state.v.device, &alloc_info, NULL, outMemory), "allocate buffer memory");
    vkBindBufferMemory(state.v.device, *outBuffer, *outMemory, 0);
}

static void create_image(const uint32_t width, const uint32_t height, const VkFormat format, const VkImageTiling tiling,
                         const VkImageUsageFlags usage, const VkMemoryPropertyFlags properties, VkImage *outImage,
                         VkDeviceMemory *outMemory)
{
    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .tiling = tiling,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_ASSERT(vkCreateImage(state.v.device, &image_info, NULL, outImage), "create image");
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(state.v.device, *outImage, &mem_reqs);

    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties)
    };

    VK_ASSERT(vkAllocateMemory(state.v.device, &alloc_info, NULL, outMemory), "allocate image memory");
    vkBindImageMemory(state.v.device, *outImage, *outMemory, 0);
}

static void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(state.v.loadingCommandBuffer, &begin_info);
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
        old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else
    {
        ASSERT(0, "unsupported layout transition");
    }

    vkCmdPipelineBarrier(state.v.loadingCommandBuffer, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    vkEndCommandBuffer(state.v.loadingCommandBuffer);
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &state.v.loadingCommandBuffer
    };

    vkQueueSubmit(state.v.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(state.v.graphicsQueue);
    vkResetCommandBuffer(state.v.loadingCommandBuffer, 0);
}

static void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(state.v.loadingCommandBuffer, &begin_info);
    const VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(state.v.loadingCommandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(state.v.loadingCommandBuffer);
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &state.v.loadingCommandBuffer
    };

    vkQueueSubmit(state.v.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(state.v.graphicsQueue);
    vkResetCommandBuffer(state.v.loadingCommandBuffer, 0);
}

static void create_texture_from_file(const char *path, texture_t *texture)
{
    int tex_width, tex_height, tex_channels;
    stbi_uc *pixels = stbi_load(path, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    ASSERT(pixels, "failed to load texture");

    texture->width = (uint32_t) tex_width;
    texture->height = (uint32_t) tex_height;

    const VkDeviceSize image_size = (VkDeviceSize) (tex_width * tex_height * 4);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;

    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
                  &staging_memory);

    void *data;
    vkMapMemory(state.v.device, staging_memory, 0, image_size, 0, &data);
    memcpy(data, pixels, (size_t)image_size);
    vkUnmapMemory(state.v.device, staging_memory);
    stbi_image_free(pixels);

    create_image(tex_width, tex_height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 &texture->image, &texture->memory);

    transition_image_layout(texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copy_buffer_to_image(staging_buffer, texture->image, (uint32_t) tex_width, (uint32_t) tex_height);
    transition_image_layout(texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(state.v.device, staging_buffer, NULL);
    vkFreeMemory(state.v.device, staging_memory, NULL);

    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_ASSERT(vkCreateImageView(state.v.device, &view_info, NULL, &texture->view), "create texture view");
    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST
    };

    VK_ASSERT(vkCreateSampler(state.v.device, &sampler_info, NULL, &texture->sampler), "create texture sampler");
}

static void create_descriptor_set_layout(void)
{
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        }
    };

    const VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = bindings
    };

    VK_ASSERT(vkCreateDescriptorSetLayout(state.v.device, &layout_info, NULL, &state.v.textureSetLayout), "create descriptor set layout");
}

static void create_descriptor_pool(void)
{
    const VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_TEXTURES
        }
    };
    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_TEXTURES,
        .poolSizeCount = 1,
        .pPoolSizes = pool_sizes
    };
    VK_ASSERT(vkCreateDescriptorPool(state.v.device, &pool_info, NULL, &state.v.descriptorPool), "create descriptor pool");
}

static void create_descriptor_set(texture_t *texture, VkDescriptorSet *descriptor_set)
{
    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = state.v.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &state.v.textureSetLayout
    };
    VK_ASSERT(vkAllocateDescriptorSets(state.v.device, &alloc_info, descriptor_set), "allocate descriptor set");

    const VkDescriptorImageInfo image_info = {
        .sampler = texture->sampler,
        .imageView = texture->view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    const VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = *descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_info
    };

    vkUpdateDescriptorSets(state.v.device, 1, &write, 0, NULL);
}

static void read_file(const char *path, char **data, size_t *size)
{
    FILE *file = fopen(path, "rb");
    ASSERT(file, "Failed to open file");
    fseek(file, 0, SEEK_END);
    *size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    *data = (char *)malloc(*size);
    ASSERT(data, "Failed to allocate memory for file");
    fread(*data, 1, *size, file);
    fclose(file);
}

VkDescriptorSet* vk_get_texture(const char* path)
{
    if (!path) return NULL;

    for (uint32_t i = 0; i < state.v.texture_count; i++) {
        if (strcmp(state.v.texture_cache[i].path, path) == 0) {
            return &state.v.texture_cache[i].descriptor_set;
        }
    }

    if (state.v.texture_count >= MAX_TEXTURES) {
        fprintf(stderr, "Warning: Texture cache full, cannot load %s\n", path);
        return NULL;
    }

    texture_cache_entry_t *entry = &state.v.texture_cache[state.v.texture_count];
    strncpy(entry->path, path, sizeof(entry->path) - 1);

    create_texture_from_file(path, &entry->texture);

    create_descriptor_set(&entry->texture, &entry->descriptor_set);

    entry->loaded = true;
    state.v.texture_count++;

    return &entry->descriptor_set;
}

static VkFormat _find_depth_format(void)
{
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    VkFormatProperties properties;
    for (uint32_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        vkGetPhysicalDeviceFormatProperties(state.v.physicalDevice, candidates[i], &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return candidates[i];
    }

    ASSERT(0, "failed to find supported depth format");
    return VK_FORMAT_UNDEFINED;
}

static void create_depth_resources(void)
{
    const VkFormat depth_format = _find_depth_format();

    // Create depth image
    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = (VkExtent3D){
            .width = state.v.swapChainExtent.width,
            .height = state.v.swapChainExtent.height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = depth_format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_ASSERT(vkCreateImage(state.v.device, &image_info, NULL, &state.v.depthImage), "create depth image");

    // Allocate memory
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(state.v.device, state.v.depthImage, &mem_reqs);
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_ASSERT(vkAllocateMemory(state.v.device, &alloc_info, NULL, &state.v.depthMemory), "allocate depth memory");
    vkBindImageMemory(state.v.device, state.v.depthImage, state.v.depthMemory, 0);

    // Create image view
    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = state.v.depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VK_ASSERT(vkCreateImageView(state.v.device, &view_info, NULL, &state.v.depthImageView), "create depth image view");

    // Transition layout
    transition_image_layout(state.v.depthImage, depth_format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

static VkShaderModule create_shader_module(const char *code, const size_t size)
{
    const VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const uint32_t *) code
    };

    VkShaderModule shader_module;
    VK_ASSERT(vkCreateShaderModule(state.v.device, &create_info, NULL, &shader_module), "create shader module");
    return shader_module;
}

static void create_render_pass(void)
{
    const VkAttachmentDescription attachments[] = {
        // Color attachment (existing)
        {
            .format = state.v.swapChainImageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        {
            .format = _find_depth_format(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    const VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref
    };

    const VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    const VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_ASSERT(vkCreateRenderPass(state.v.device, &render_pass_info, NULL, &state.v.renderPass), "create render pass");
}

static void create_pipeline(const char *vert_path, const char *frag_path, bool textured, pipeline_t *pipeline)
{
    char *vert_code; size_t vert_size;
    char *frag_code; size_t frag_size;

    read_file(vert_path, &vert_code, &vert_size);
    read_file(frag_path, &frag_code, &frag_size);

    const VkShaderModule vert_shader = create_shader_module(vert_code, vert_size);
    const VkShaderModule frag_shader = create_shader_module(frag_code, frag_size);

    const VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader, .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader, .pName = "main"
        }
    };

    const VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = sizeof(vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkVertexInputAttributeDescription attribute_descriptions[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(vertex_t, position)
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vertex_t, tex_coord)
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(vertex_t, color)
        }
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = attribute_descriptions
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float) state.v.swapChainExtent.width,
        .height = (float) state.v.swapChainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    const VkRect2D scissor = {
        .offset = {0, 0},
        .extent = state.v.swapChainExtent
    };

    const VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    const VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = textured ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };

    const VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = textured ? 1 : 0,
        .pSetLayouts = textured ? &state.v.textureSetLayout : NULL,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(push_constants_textured_t)
        }
    };

    const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    VK_ASSERT(vkCreatePipelineLayout(state.v.device, &pipeline_layout_info, NULL, &pipeline->layout), "create pipeline layout");

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = NULL,
        .layout = pipeline->layout,
        .renderPass = state.v.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VK_ASSERT(vkCreateGraphicsPipelines(state.v.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline->pipeline), "create graphics pipeline");

    vkDestroyShaderModule(state.v.device, vert_shader, NULL);
    vkDestroyShaderModule(state.v.device, frag_shader, NULL);
    free(vert_code);
    free(frag_code);
}

static void create_mesh_buffer(const vertex_t *vertices, const uint32_t vertex_count, mesh_buffer_t *buffer)
{
    const VkDeviceSize buffer_size = sizeof(vertex_t) * vertex_count;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
                  &staging_memory);

    void *data;
    vkMapMemory(state.v.device, staging_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices, (size_t)buffer_size);
    vkUnmapMemory(state.v.device, staging_memory);
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer->buffer, &buffer->memory);

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(state.v.loadingCommandBuffer, &begin_info);
    const VkBufferCopy copy_region = {
        .size = buffer_size
    };

    vkCmdCopyBuffer(state.v.loadingCommandBuffer, staging_buffer, buffer->buffer, 1, &copy_region);
    vkEndCommandBuffer(state.v.loadingCommandBuffer);
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &state.v.loadingCommandBuffer
    };

    vkQueueSubmit(state.v.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(state.v.graphicsQueue);
    vkDestroyBuffer(state.v.device, staging_buffer, NULL);
    vkFreeMemory(state.v.device, staging_memory, NULL);
    buffer->vertex_count = vertex_count;
}

static void create_cube_mesh(void)
{
    const float half_size = SIZE;
    const float tile_scale = 4.0f;

    const vertex_t cube_vertices[] = {
        {{-half_size, -half_size,  half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size, -half_size,  half_size}, {tile_scale, 0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size,  half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size, -half_size,  half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size,  half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size,  half_size,  half_size}, {0.0f,       tile_scale},  {1, 1 ,1, 1}},

        {{ half_size, -half_size, -half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{-half_size, -half_size, -half_size}, {tile_scale, 0.0f},        {1, 1 ,1, 1}},
        {{-half_size,  half_size, -half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{ half_size, -half_size, -half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{-half_size,  half_size, -half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{ half_size,  half_size, -half_size}, {0.0f,       tile_scale},  {1, 1 ,1, 1}},

        {{-half_size, -half_size, -half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{-half_size, -half_size,  half_size}, {tile_scale, 0.0f},        {1, 1 ,1, 1}},
        {{-half_size,  half_size,  half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size, -half_size, -half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{-half_size,  half_size,  half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size,  half_size, -half_size}, {0.0f,       tile_scale},  {1, 1 ,1, 1}},

        {{ half_size, -half_size,  half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size, -half_size, -half_size}, {tile_scale, 0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size, -half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{ half_size, -half_size,  half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size, -half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{ half_size,  half_size,  half_size}, {0.0f,       tile_scale},  {1, 1 ,1, 1}},

        {{-half_size,  half_size,  half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size,  half_size}, {tile_scale, 0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size, -half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size,  half_size,  half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size,  half_size, -half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size,  half_size, -half_size}, {0.0f,       tile_scale},  {1, 1 ,1, 1}},

        {{-half_size, -half_size, -half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size, -half_size, -half_size}, {tile_scale, 0.0f},        {1, 1 ,1, 1}},
        {{ half_size, -half_size,  half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size, -half_size, -half_size}, {0.0f,       0.0f},        {1, 1 ,1, 1}},
        {{ half_size, -half_size,  half_size}, {tile_scale, tile_scale},  {1, 1 ,1, 1}},
        {{-half_size, -half_size,  half_size}, {0.0f,       tile_scale},  {1, 1 ,1, 1}},
    };
    state.v.cube_buffer.vertex_count = 36;
    create_mesh_buffer(cube_vertices, state.v.cube_buffer.vertex_count, &state.v.cube_buffer);
}

VkDescriptorSet font_descriptor_set;
VkDescriptorSet board_descriptor_set;

void VK_START()
{
    state = (state_t){0};
    state.v.texture_count = 0;
    for (uint32_t i = 0; i < MAX_TEXTURES; i++)
        state.v.texture_cache[i].loaded = false;

    {
        glyphs[':'] = (glyph_uv_t){10, 3};
        glyphs[';'] = (glyph_uv_t){11, 3};
        glyphs['<'] = (glyph_uv_t){12, 3};
        glyphs['='] = (glyph_uv_t){13, 3};
        glyphs['>'] = (glyph_uv_t){14, 3};
        glyphs['?'] = (glyph_uv_t){15, 3};
        glyphs['0'] = (glyph_uv_t){0, 3};
        glyphs['1'] = (glyph_uv_t){1, 3};
        glyphs['2'] = (glyph_uv_t){2, 3};
        glyphs['3'] = (glyph_uv_t){3, 3};
        glyphs['4'] = (glyph_uv_t){4, 3};
        glyphs['5'] = (glyph_uv_t){5, 3};
        glyphs['6'] = (glyph_uv_t){6, 3};
        glyphs['7'] = (glyph_uv_t){7, 3};
        glyphs['8'] = (glyph_uv_t){8, 3};
        glyphs['9'] = (glyph_uv_t){9, 3};
        glyphs[' '] = (glyph_uv_t){0, 2};
        glyphs['.'] = (glyph_uv_t){14, 2};
        glyphs['A'] = (glyph_uv_t){1, 4};
        glyphs['B'] = (glyph_uv_t){2, 4};
        glyphs['C'] = (glyph_uv_t){3, 4};
        glyphs['D'] = (glyph_uv_t){4, 4};
        glyphs['E'] = (glyph_uv_t){5, 4};
        glyphs['F'] = (glyph_uv_t){6, 4};
        glyphs['G'] = (glyph_uv_t){7, 4};
        glyphs['H'] = (glyph_uv_t){8, 4};
        glyphs['I'] = (glyph_uv_t){9, 4};
        glyphs['J'] = (glyph_uv_t){10, 4};
        glyphs['K'] = (glyph_uv_t){11, 4};
        glyphs['L'] = (glyph_uv_t){12, 4};
        glyphs['M'] = (glyph_uv_t){13, 4};
        glyphs['N'] = (glyph_uv_t){14, 4};
        glyphs['O'] = (glyph_uv_t){15, 4};
        glyphs['P'] = (glyph_uv_t){0, 5};
        glyphs['Q'] = (glyph_uv_t){1, 5};
        glyphs['R'] = (glyph_uv_t){2, 5};
        glyphs['S'] = (glyph_uv_t){3, 5};
        glyphs['T'] = (glyph_uv_t){4, 5};
        glyphs['U'] = (glyph_uv_t){5, 5};
        glyphs['V'] = (glyph_uv_t){6, 5};
        glyphs['W'] = (glyph_uv_t){7, 5};
        glyphs['X'] = (glyph_uv_t){8, 5};
        glyphs['Y'] = (glyph_uv_t){9, 5};
        glyphs['Z'] = (glyph_uv_t){10, 5};
        glyphs['_'] = (glyph_uv_t){15, 5};
        glyphs['a'] = (glyph_uv_t){1, 6};
        glyphs['b'] = (glyph_uv_t){2, 6};
        glyphs['c'] = (glyph_uv_t){3, 6};
        glyphs['d'] = (glyph_uv_t){4, 6};
        glyphs['e'] = (glyph_uv_t){5, 6};
        glyphs['f'] = (glyph_uv_t){6, 6};
        glyphs['g'] = (glyph_uv_t){7, 6};
        glyphs['h'] = (glyph_uv_t){8, 6};
        glyphs['i'] = (glyph_uv_t){9, 6};
        glyphs['j'] = (glyph_uv_t){10, 6};
        glyphs['k'] = (glyph_uv_t){11, 6};
        glyphs['l'] = (glyph_uv_t){12, 6};
        glyphs['m'] = (glyph_uv_t){13, 6};
        glyphs['n'] = (glyph_uv_t){14, 6};
        glyphs['o'] = (glyph_uv_t){15, 6};
        glyphs['p'] = (glyph_uv_t){0, 7};
        glyphs['q'] = (glyph_uv_t){1, 7};
        glyphs['r'] = (glyph_uv_t){2, 7};
        glyphs['s'] = (glyph_uv_t){3, 7};
        glyphs['t'] = (glyph_uv_t){4, 7};
        glyphs['u'] = (glyph_uv_t){5, 7};
        glyphs['v'] = (glyph_uv_t){6, 7};
        glyphs['w'] = (glyph_uv_t){7, 7};
        glyphs['x'] = (glyph_uv_t){8, 7};
        glyphs['y'] = (glyph_uv_t){9, 7};
        glyphs['z'] = (glyph_uv_t){10, 7};

        state.cam.x = 0.0f; state.cam.y = 0.0f; state.cam.z = -3.0f;
        state.cam.yaw = 0.0f; state.cam.pitch = 0.0f;
    }

    ASSERT(glfwInit(), "Window initialization failed");
    ASSERT(glfwVulkanSupported(), "GLFW: Vulkan not supported!\n");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    state.glfw.win = glfwCreateWindow(WIDTH, HEIGHT, TITLE, NULL, NULL);
    ASSERT(state.glfw.win, "Window creation failed");

    {
        uint32_t glfw_ext_count = 0;
        const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
        const char **enabled_extensions = (const char **) malloc(sizeof(char *) * (glfw_ext_count + 1));
        uint32_t enabled_ext_count = glfw_ext_count;

        for (uint32_t i = 0; i < glfw_ext_count; ++i) enabled_extensions[i] = glfw_extensions[i];

        enabled_extensions[enabled_ext_count++] = "VK_KHR_portability_enumeration";
        const VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
        };

        const VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .enabledExtensionCount = enabled_ext_count,
            .ppEnabledExtensionNames = enabled_extensions,
            .enabledLayerCount = 0,
            .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        };

        VK_ASSERT(vkCreateInstance(&create_info, NULL, &state.v.instance), "create instance");
        free((void *) enabled_extensions);
    }

    VK_ASSERT(glfwCreateWindowSurface(state.v.instance, state.glfw.win, NULL, &state.v.surface), "create surface");

    // Select physical device
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(state.v.instance, &device_count, NULL);
        ASSERT(device_count > 0, "failed to enumerate physical devices");
        VkPhysicalDevice *devices = (VkPhysicalDevice *) malloc(sizeof(VkPhysicalDevice) * device_count);
        vkEnumeratePhysicalDevices(state.v.instance, &device_count, devices);
        state.v.physicalDevice = devices[0];
        free(devices);
    }

    // Find queue families
    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(state.v.physicalDevice, &queue_family_count, NULL);
        VkQueueFamilyProperties *queue_families = (VkQueueFamilyProperties *) malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);

        vkGetPhysicalDeviceQueueFamilyProperties(state.v.physicalDevice, &queue_family_count, queue_families);
        bool graphics_found = false;
        bool present_found = false;

        for (uint32_t i = 0; i < queue_family_count; ++i)
        {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                state.v.graphicsFamilyIndex = i;
                graphics_found = true;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(state.v.physicalDevice, i, state.v.surface, &present_support);
            if (present_support)
            {
                state.v.presentFamilyIndex = i;
                present_found = true;
            }
        }

        free(queue_families);
        ASSERT((graphics_found && present_found), "failed to find queue families");
    }

    // Create logical device
    {
        const float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_create_infos[2];
        uint32_t queue_info_count = 0;
        queue_create_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = state.v.graphicsFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };

        if (state.v.graphicsFamilyIndex != state.v.presentFamilyIndex)
        {
            queue_create_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = state.v.presentFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority
            };
        }

        const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const VkDeviceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = queue_info_count,
            .pQueueCreateInfos = queue_create_infos,
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = device_extensions
        };

        VK_ASSERT(vkCreateDevice(state.v.physicalDevice, &create_info, NULL, &state.v.device), "create device");

        vkGetDeviceQueue(state.v.device, state.v.graphicsFamilyIndex, 0, &state.v.graphicsQueue);
        vkGetDeviceQueue(state.v.device, state.v.presentFamilyIndex, 0, &state.v.presentQueue);
    }

    // Create swapchain
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.v.physicalDevice, state.v.surface, &capabilities);
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(state.v.physicalDevice, state.v.surface, &format_count, NULL);
        VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *) malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(state.v.physicalDevice, state.v.surface, &format_count, formats);
        VkSurfaceFormatKHR surface_format = formats[0];
        for (uint32_t i = 0; i < format_count; ++i)
        {
            if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB)
            {
                surface_format = formats[i];
                break;
            }
        }

        free(formats);

        state.v.swapChainImageFormat = surface_format.format;
        state.v.swapChainExtent = capabilities.currentExtent;
        uint32_t image_count = capabilities.minImageCount + 1;

        if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) image_count = capabilities.maxImageCount;

        const uint32_t queue_family_indices[] = {state.v.graphicsFamilyIndex, state.v.presentFamilyIndex};
        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = state.v.surface,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = state.v.swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = (state.v.graphicsFamilyIndex != state.v.presentFamilyIndex)
                                    ? VK_SHARING_MODE_CONCURRENT
                                    : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = (state.v.graphicsFamilyIndex != state.v.presentFamilyIndex) ? 2 : 0,
            .pQueueFamilyIndices = (state.v.graphicsFamilyIndex != state.v.presentFamilyIndex)
                                       ? queue_family_indices
                                       : NULL,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
            .clipped = VK_TRUE
        };

        VK_ASSERT(vkCreateSwapchainKHR(state.v.device, &swapchain_info, NULL, &state.v.swapchain), "create swapchain");

        vkGetSwapchainImagesKHR(state.v.device, state.v.swapchain, &state.v.imageCount, NULL);
        state.v.images = (VkImage *) malloc(sizeof(VkImage) * state.v.imageCount);
        state.v.imageViews = (VkImageView *) malloc(sizeof(VkImageView) * state.v.imageCount);
        state.v.framebuffers = (VkFramebuffer *) malloc(sizeof(VkFramebuffer) * state.v.imageCount);
        vkGetSwapchainImagesKHR(state.v.device, state.v.swapchain, &state.v.imageCount, state.v.images);
    }

    for (uint32_t i = 0; i < state.v.imageCount; ++i)
    {
        const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = state.v.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = state.v.swapChainImageFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VK_ASSERT(vkCreateImageView(state.v.device, &view_info, NULL, &state.v.imageViews[i]), "create image view");
    }

    {
        const VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = state.v.graphicsFamilyIndex,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };

        VK_ASSERT(vkCreateCommandPool(state.v.device, &pool_info, NULL, &state.v.commandPool), "create command pool");
    }

    {
        const VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = state.v.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_ASSERT(vkAllocateCommandBuffers(state.v.device, &alloc_info, &state.v.commandBuffer), "allocate command buffer");
        VK_ASSERT(vkAllocateCommandBuffers(state.v.device, &alloc_info, &state.v.loadingCommandBuffer), "allocate loading command buffer");
    }

    create_depth_resources();
    create_render_pass();
    for (uint32_t i = 0; i < state.v.imageCount; ++i)
    {
        const VkImageView attachments[] = {
            state.v.imageViews[i],
            state.v.depthImageView
        };

        const VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = state.v.renderPass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = state.v.swapChainExtent.width,
            .height = state.v.swapChainExtent.height,
            .layers = 1
        };
        VK_ASSERT(vkCreateFramebuffer(state.v.device, &framebuffer_info, NULL, &state.v.framebuffers[i]), "create framebuffer");
    }

    create_descriptor_set_layout();
    create_descriptor_pool();

    create_texture_from_file("Engine/res/font.png", &state.v.font_texture);
    create_texture_from_file("Engine/res/checker.png", &state.v.board_texture);

    create_descriptor_set(&state.v.font_texture, &font_descriptor_set);
    create_descriptor_set(&state.v.board_texture, &board_descriptor_set);

    create_pipeline("Engine/shad/col.vert.spv", "Engine/shad/col.frag.spv", false, &state.v.colored_pipeline);
    create_pipeline("Engine/shad/tex.vert.spv", "Engine/shad/tex.frag.spv", true, &state.v.textured_pipeline);
    create_pipeline("Engine/shad/text.vert.spv", "Engine/shad/text.frag.spv", true, &state.v.text_pipeline);

    {
        const VkDeviceSize buffer_size = sizeof(vertex_t) * MAX_TEXT_VERTICES;
        create_buffer(buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &state.v.text_buffer.buffer, &state.v.text_buffer.memory);
        state.v.text_buffer.vertex_count = 0;
    }

    create_cube_mesh();

    {
        const VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        VK_ASSERT(vkCreateSemaphore(state.v.device, &semaphore_info, NULL, &state.v.imageAvailableSemaphore), "create semaphore");
        VK_ASSERT(vkCreateSemaphore(state.v.device, &semaphore_info, NULL, &state.v.renderFinishedSemaphore), "create semaphore");

        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        VK_ASSERT(vkCreateFence(state.v.device, &fence_info, NULL, &state.v.inFlightFence), "create fence");
    }

    state.last_time = glfwGetTime();
    state.last_frame_time = state.last_time;
}

int VK_FRAME()
{
    const double current_time = glfwGetTime();
    state.delta_time = (float) (current_time - state.last_frame_time);
    state.last_frame_time = current_time;

    state.frame_count++;
    if (current_time - state.last_time >= 1.0)
    {
        state.fps = state.frame_count / (current_time - state.last_time);
        state.frame_count = 0;
        state.last_time = current_time;
    }

    glfwPollEvents();
    fflush(stdout);
    ASSERT(state.glfw.win != NULL, "GLFW window is NULL during input");
    INPUT();

    vkWaitForFences(state.v.device, 1, &state.v.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(state.v.device, 1, &state.v.inFlightFence);
    uint32_t image_index;
    vkAcquireNextImageKHR(state.v.device, state.v.swapchain, UINT64_MAX, state.v.imageAvailableSemaphore,
                          VK_NULL_HANDLE, &image_index);

    if (state.text_vertex_count > 0)
    {
        void *data;
        vkMapMemory(state.v.device, state.v.text_buffer.memory, 0, sizeof(vertex_t) * state.text_vertex_count, 0, &data);
        memcpy(data, state.text_vertices, sizeof(vertex_t) * state.text_vertex_count);
        vkUnmapMemory(state.v.device, state.v.text_buffer.memory);
        state.v.text_buffer.vertex_count = state.text_vertex_count;
    }

    vkResetCommandBuffer(state.v.commandBuffer, 0);
    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    const VkClearValue clear_values[2] = {
        {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
        {.depthStencil = {1.0f, 0}}
    };

    vkBeginCommandBuffer(state.v.commandBuffer, &begin_info);
    const VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state.v.renderPass,
        .framebuffer = state.v.framebuffers[image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = state.v.swapChainExtent
        },
        .clearValueCount = 2,
        .pClearValues = clear_values
    };

    vkCmdBeginRenderPass(state.v.commandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    RENDER();

    vkCmdEndRenderPass(state.v.commandBuffer);
    vkEndCommandBuffer(state.v.commandBuffer);
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state.v.imageAvailableSemaphore,
        .pWaitDstStageMask = &(VkPipelineStageFlags){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount = 1,
        .pCommandBuffers = &state.v.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &state.v.renderFinishedSemaphore
    };

    VK_ASSERT(vkQueueSubmit(state.v.graphicsQueue, 1, &submit_info, state.v.inFlightFence), "submit draw");

    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state.v.renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &state.v.swapchain,
        .pImageIndices = &image_index
    };

    vkQueuePresentKHR(state.v.presentQueue, &present_info);
    return !glfwWindowShouldClose(state.glfw.win);
}

void VK_END(void)
{
    vkDeviceWaitIdle(state.v.device);
    vkDestroySemaphore(state.v.device, state.v.imageAvailableSemaphore, NULL);
    vkDestroySemaphore(state.v.device, state.v.renderFinishedSemaphore, NULL);
    vkDestroyFence(state.v.device, state.v.inFlightFence, NULL);
    vkDestroyBuffer(state.v.device, state.v.text_buffer.buffer, NULL);
    vkFreeMemory(state.v.device, state.v.text_buffer.memory, NULL);
    vkDestroyBuffer(state.v.device, state.v.cube_buffer.buffer, NULL);
    vkFreeMemory(state.v.device, state.v.cube_buffer.memory, NULL);
    vkDestroyImageView(state.v.device, state.v.font_texture.view, NULL);
    vkDestroySampler(state.v.device, state.v.font_texture.sampler, NULL);
    vkDestroyImage(state.v.device, state.v.font_texture.image, NULL);
    vkFreeMemory(state.v.device, state.v.font_texture.memory, NULL);
    vkDestroyImageView(state.v.device, state.v.board_texture.view, NULL);
    vkDestroySampler(state.v.device, state.v.board_texture.sampler, NULL);
    vkDestroyImage(state.v.device, state.v.board_texture.image, NULL);
    vkFreeMemory(state.v.device, state.v.board_texture.memory, NULL);
    vkDestroyPipeline(state.v.device, state.v.textured_pipeline.pipeline, NULL);
    vkDestroyPipelineLayout(state.v.device, state.v.textured_pipeline.layout, NULL);
    vkDestroyPipeline(state.v.device, state.v.colored_pipeline.pipeline, NULL);
    vkDestroyPipelineLayout(state.v.device, state.v.colored_pipeline.layout, NULL);
    vkDestroyDescriptorSetLayout(state.v.device, state.v.textureSetLayout, NULL);
    vkDestroyDescriptorPool(state.v.device, state.v.descriptorPool, NULL);
    vkDestroyRenderPass(state.v.device, state.v.renderPass, NULL);
    vkDestroyCommandPool(state.v.device, state.v.commandPool, NULL);

    for (uint32_t i = 0; i < state.v.imageCount; ++i)
    {
        vkDestroyFramebuffer(state.v.device, state.v.framebuffers[i], NULL);
        vkDestroyImageView(state.v.device, state.v.imageViews[i], NULL);
    }

    free(state.v.images);
    free(state.v.imageViews);
    free(state.v.framebuffers);

    vkDestroyImageView(state.v.device, state.v.depthImageView, NULL);
    vkDestroyImage(state.v.device, state.v.depthImage, NULL);
    vkFreeMemory(state.v.device, state.v.depthMemory, NULL);

    vkDestroySwapchainKHR(state.v.device, state.v.swapchain, NULL);
    vkDestroyDevice(state.v.device, NULL);
    vkDestroySurfaceKHR(state.v.instance, state.v.surface, NULL);
    vkDestroyInstance(state.v.instance, NULL);
    glfwDestroyWindow(state.glfw.win);
    glfwTerminate();
}

void VK_GET_CAMPOS(float *x, float *y, float *z)
{
    if (x) *x = state.cam.x;
    if (y) *y = state.cam.y;
    if (z) *z = state.cam.z;
}

void VK_SET_CAMPOS(const float x, const float y, const float z)
{
    state.cam.x = x;
    state.cam.y = y;
    state.cam.z = z;
}

void VK_GET_CAMROT(float *yaw, float *pitch)
{
    if (yaw) *yaw = state.cam.yaw;
    if (pitch) *pitch = state.cam.pitch;
}

void VK_SET_CAMROT(const float yaw, const float pitch)
{
    state.cam.yaw = yaw;
    state.cam.pitch = pitch;
}

float VK_GETDELTATIME()
{
    return state.delta_time;
}

double VK_GETFPS()
{
    return state.fps;
}
