#include "Gui.hpp"

// shader code in SPIR-V binary
static const uint32_t vsSpirv[] = {
#include "../../shaders/vs.vert.spv"
};
static const uint32_t fsSpirv[] = {
#include "../../shaders/ps.frag.spv"
};

namespace ImGui {
bool CheckBoxTristate(const char *label, int *v_tristate) {
    bool ret;
    if (*v_tristate == -1) {
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        bool b = false;
        ret = ImGui::Checkbox(label, &b);
        if (ret)
            *v_tristate = 1;
        ImGui::PopItemFlag();
    } else {
        bool b = (*v_tristate != 0);
        ret = ImGui::Checkbox(label, &b);
        if (ret)
            *v_tristate = (int)b;
    }
    return ret;
}
}; // namespace ImGui

GUI::QueueFamilyIndices GUI::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

GUI::SwapChainSupportDetails GUI::querySwapChainSupport(VkPhysicalDevice device,
                                                        VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                             details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                              &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR GUI::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkExtent2D GUI::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                 GLFWwindow *window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height)};

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void GUI::checkLayerSupport(const std::vector<const char *> &layers) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : layers) {

        bool found = false;
        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error("Error: Layer not available: " +
                                     std::string(layerName));
        }
    }
}

VkShaderModule GUI::createShaderModule(const uint32_t *code,
                                       uint32_t codeSize) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void GUI::createWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("GLFW init error");
    }

    if (!glfwVulkanSupported()) {
        throw std::runtime_error("GLFW error: vulkan is not supported");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        throw std::runtime_error("GLFW error: no monitor");
    }
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode) {
        throw std::runtime_error("GLFW error: glfwGetVideoMode() failed");
    }

    width = mode->width * 3 / 4;
    height = mode->height * 3 / 4;
    glfwWindow =
        glfwCreateWindow(width, height, "Vulkan C++20 generator", NULL, NULL);
    if (!glfwWindow) {
        throw std::runtime_error("GLFW error: window init");
    }
    glfwSetWindowSizeLimits(glfwWindow, mode->width / 8, mode->height / 8,
                            GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowUserPointer(glfwWindow, &window);

#define genericCallback(functionName)                                          \
    [](GLFWwindow *window, auto... args) {                                     \
        auto pointer =                                                         \
            static_cast<GUI::Window *>(glfwGetWindowUserPointer(window));      \
        if (pointer->functionName)                                             \
            pointer->functionName(pointer, args...);                           \
    }

    window.onSize = [&](auto self, int width, int height) {
        this->width = width;
        this->height = height;
        recreateSwapChain();
        drawFrame();
    };

    glfwSetWindowSizeCallback(glfwWindow, genericCallback(onSize));
}

void GUI::createInstance() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                              .pApplicationName = "Hello Triangle",
                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                              .pEngineName = "No Engine",
                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                              .apiVersion = VK_API_VERSION_1_0};

    static std::vector<const char *> layers;

    // layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions};

    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("VK Error: failed to create instance!");
    }
}

void GUI::createSurface() {

    if (glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void GUI::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void GUI::createDevice() {
    indices = findQueueFamilies(physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void GUI::createSwapChain(VkSwapchainKHR old) {
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode =
        chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent =
        chooseSwapExtent(swapChainSupport.capabilities, glfwWindow);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = old};

    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr,
                             &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount,
                            swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void GUI::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr,
                              &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void GUI::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void GUI::createPipelineCache() {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr,
                              &pipelineCache) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline cache!");
    }
}

void GUI::createShaderModules() {
    vertShaderModule = createShaderModule(vsSpirv, sizeof(vsSpirv));
    fragShaderModule = createShaderModule(fsSpirv, sizeof(fsSpirv));
}

void GUI::createGraphicsPipeline() {

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                               &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    // pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo,
                                  nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

void GUI::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void GUI::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void GUI::createCommandBuffers() {

    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void GUI::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.flags = 0;
    // layoutInfo.bindingCount = 1;
    // layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void GUI::initImgui() {

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo desrciptorPoolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = std::size(poolSizes),
        .pPoolSizes = poolSizes};

    if (vkCreateDescriptorPool(device, &desrciptorPoolInfo, nullptr,
                               &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

    const auto check = [](VkResult result) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error("imgui vulkan init error.");
        }
    };

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .QueueFamily = indices.graphicsFamily.value(),
        .Queue = presentQueue,
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = descriptorPool,
        .MinImageCount = 2,
        .ImageCount = (uint32_t)swapChainImages.size(),
        .Allocator = nullptr,
        .CheckVkResultFn = check};

    ImGui_ImplVulkan_Init(&init_info, renderPass);

    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0};

    if (vkCreateFence(device, &fenceCreateInfo, nullptr, &fence) !=
        VK_SUCCESS) {
        throw std::runtime_error("error: vkCreateFence");
    }

    const auto immediate_submit =
        [&](std::function<void(VkCommandBuffer cmd)> &&function) {
            VkCommandBuffer cmd = *commandBuffers.rbegin();

            VkCommandBufferBeginInfo cmdBeginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr};

            if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS) {
                throw std::runtime_error("error: vkBeginCommandBuffer");
            }

            function(cmd);

            if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
                throw std::runtime_error("error: vkEndCommandBuffer");
            }

            VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .pNext = nullptr,
                                   .waitSemaphoreCount = 0,
                                   .pWaitSemaphores = nullptr,
                                   .pWaitDstStageMask = nullptr,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &cmd,
                                   .signalSemaphoreCount = 0,
                                   .pSignalSemaphores = nullptr};

            if (vkQueueSubmit(graphicsQueue, 1, &submit, fence) != VK_SUCCESS) {
                throw std::runtime_error("error: vkQueueSubmit");
            }

            vkWaitForFences(device, 1, &fence, true, 9999999999);
            vkResetFences(device, 1, &fence);

            vkResetCommandPool(device, commandPool, 0);
        };

    immediate_submit(
        [&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void GUI::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create synchronization objects for a frame!");
        }
    }
}

void GUI::cleanupSwapChain() {
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
}

void GUI::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(glfwWindow, &width, &height);
    while (width == 0 || height == 0) {
        glfwWaitEvents();
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    }

    vkDeviceWaitIdle(device);
    VkSwapchainKHR old = swapChain;
    cleanupSwapChain();

    createSwapChain(old);
    createImageViews();
    createRenderPass();

    createGraphicsPipeline();
    createFramebuffers();

    vkDestroySwapchainKHR(device, old, nullptr);
}

void GUI::cleanup() {

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    cleanupSwapChain();
}

void GUI::initImguiStyle() {
    ImGuiStyle &style = ImGui::GetStyle();

    ImVec4 col_text = ImVec4(1.0, 1.0, 1.0, 1.0);
    ImVec4 col_main = ImVec4(0.18, 0.0, 0, 1.0);
    ImVec4 col_back = ImVec4(0.01, 0.01, 0.01, 1.0);
    ImVec4 col_area = ImVec4(0.15, 0.0, 0, 1.0);

    style.Colors[ImGuiCol_Text] =
        ImVec4(col_text.x, col_text.y, col_text.z, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] =
        ImVec4(col_text.x, col_text.y, col_text.z, 0.58f);
    style.Colors[ImGuiCol_WindowBg] =
        ImVec4(col_back.x, col_back.y, col_back.z, 1.00f);
    style.Colors[ImGuiCol_ChildBg] =
        ImVec4(col_area.x, col_area.y, col_area.z, 0.00f);
    style.Colors[ImGuiCol_Border] =
        ImVec4(col_text.x, col_text.y, col_text.z, 0.30f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] =
        ImVec4(col_area.x, col_area.y, col_area.z, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.68f);
    style.Colors[ImGuiCol_FrameBgActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_TitleBg] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.45f);
    style.Colors[ImGuiCol_TitleBgCollapsed] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.35f);
    style.Colors[ImGuiCol_TitleBgActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_MenuBarBg] =
        ImVec4(col_area.x, col_area.y, col_area.z, 0.57f);
    style.Colors[ImGuiCol_ScrollbarBg] =
        ImVec4(col_back.x, col_back.y, col_back.z, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.31f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_CheckMark] =
        ImVec4(col_text.x, col_text.y, col_text.z, 0.80f);
    style.Colors[ImGuiCol_SliderGrab] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.24f);
    style.Colors[ImGuiCol_SliderGrabActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_Button] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.44f);
    style.Colors[ImGuiCol_ButtonHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.86f);
    style.Colors[ImGuiCol_ButtonActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_Header] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.76f);
    style.Colors[ImGuiCol_HeaderHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.86f);
    style.Colors[ImGuiCol_HeaderActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_ResizeGripActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_PlotLines] =
        ImVec4(col_text.x, col_text.y, col_text.z, 0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] =
        ImVec4(col_text.x, col_text.y, col_text.z, 0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered] =
        ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.43f);
}

void GUI::guiInputText(std::string &data) {
    int s = ImGui::CalcTextSize(data.c_str()).x;
    s = std::clamp(s, 0, width / 3) + 20;
    ImGui::PushItemWidth(s);
    ImGui::PushID(id++);
    if (ImGui::InputText("", &data)) {
    }
    ImGui::PopID();
    ImGui::PopItemWidth();
}

void GUI::guiMacroOption(MacroGUI &m) {
    ImGui::PushID(id++);

    ImGui::Text("%s", m.text.c_str());
    ImGui::SameLine();

    bool disabled = !m.data->usesDefine;

    if (disabled) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
    }

    ImGui::Checkbox("#define ", &m.data->usesDefine);
    if (m.data->usesDefine) {
        ImGui::PushID(id++);
        ImGui::SameLine();
        guiInputText(m.data->define);
        ImGui::PopID();
    }

    if (disabled) {
        ImGui::PopStyleVar();
    }

    ImGui::SameLine();
    guiInputText(m.data->value);

    ImGui::PopID();
}

void GUI::guiBoolOption(BoolGUI &data) {
    ImGui::PushID(id++);

    ImGui::Text("%s", data.text.c_str());
    ImGui::SameLine();

    //    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, false);

    ImGui::Checkbox("", data.data);

    //  ImGui::PopItemFlag();
    // std::cout << data.text << " " << data.data << " : " << (*data.data?
    // "true" : "false") << std::endl;

    ImGui::PopID();
}

void GUI::init() {
    createWindow();
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createDevice();
    createSwapChain(VK_NULL_HANDLE);
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createPipelineCache();
    createShaderModules();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();

    initImgui();
    initImguiStyle();
}

void GUI::setupImguiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    updateImgui();

    ImGui::Render();
    imguiFrameBuilt = true;
}

void GUI::HelpMarker(const char *desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void GUI::updateImgui() {
    Generator::Config &cfg = gen.getConfig();

    id = 0;
    ImGui::NewFrame();

    bool mainw;

    ImGui::SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGui::Begin("Gen", &mainw,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (gen.isLoaded()) {

        static std::string output{gen.getOutputFilePath()};
        static bool outputPathBad = !gen.isOuputFilepathValid();
        bool outputBadFlag = outputPathBad;

        if (outputBadFlag) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                                ImGui::GetStyle().Alpha * 0.5f);
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (ImGui::Button("Generate") && !outputBadFlag) {
            gen.generate();
        }
        if (outputBadFlag) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Text("Output: ");
        ImGui::SameLine();
        if (outputBadFlag) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
        }
        ImGui::PushID(0);
        if (ImGui::InputText("", &output)) {
            outputPathBad = !gen.isOuputFilepathValid();
        }
        ImGui::PopID();
        if (outputBadFlag) {
            ImGui::PopStyleColor();
        }

        static auto &platforms = gen.getPlatforms();
        static auto &extensions = gen.getExtensions();
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        if (ImGui::CollapsingHeader("Platforms")) {

            int i = 0;
            for (auto &p : platforms) {

                ImGui::PushID(i);
                int *v = &p.second.guiState;

                if (ImGui::CheckBoxTristate("", v)) {
                    if (*v == 0) {
                        for (auto &e : extensions) {
                            if (e.second.platform &&
                                e.second.platform->first == p.first &&
                                e.second.enabled) {
                                e.second.whitelisted = false;
                            }
                        }
                    } else {
                        for (auto &e : extensions) {
                            if (e.second.platform &&
                                e.second.platform->first == p.first &&
                                e.second.enabled) {
                                e.second.whitelisted = true;
                            }
                        }
                    }
                }
                ImGui::SameLine();

                bool recalc = false;

                if (ImGui::TreeNodeEx(p.first.c_str(),
                                      ImGuiTreeNodeFlags_OpenOnArrow)) {

                    int j = 0;
                    for (auto &e : extensions) {
                        if (e.second.platform &&
                            e.second.platform->first == p.first) {
                            std::string suf =
                                e.second.enabled ? "" : " (disabled)";

                            bool *v = &e.second.whitelisted;
                            bool disabled = !*v;
                            if (disabled) {
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                                                    ImGui::GetStyle().Alpha *
                                                        0.4f);
                            }
                            if (!e.second.enabled) {
                                ImGui::PushItemFlag(ImGuiItemFlags_Disabled,
                                                    true);
                                ImGui::PushStyleColor(
                                    ImGuiCol_Text, IM_COL32(180, 10, 10, 255));
                            }
                            ImGui::PushID(j);
                            if (ImGui::Checkbox("", v)) {
                                recalc = true;
                            }
                            ImGui::SameLine();
                            ImGui::Text("%s%s", e.first.c_str(), suf.c_str());
                            ImGui::PopID();
                            if (disabled) {
                                ImGui::PopStyleVar();
                            }
                            if (!e.second.enabled) {
                                ImGui::PopItemFlag();
                                ImGui::PopStyleColor();
                            }
                        }
                        j++;
                    }

                    ImGui::TreePop();
                }

                if (recalc && !extensions.empty()) {
                    for (auto &e : extensions) {
                        if (e.second.platform &&
                            e.second.platform->first == p.first &&
                            e.second.enabled) {
                            *v = e.second.whitelisted ? 1 : 0;
                            break;
                        }
                    }
                    for (auto &e : extensions) {
                        if (e.second.platform &&
                            e.second.platform->first == p.first &&
                            e.second.enabled) {
                            if (e.second.whitelisted) {
                                if (*v == 0) {
                                    *v = -1;
                                    break;
                                }
                            } else {
                                if (*v == 1) {
                                    *v = -1;
                                    break;
                                }
                            }
                        }
                    }
                }

                ImGui::PopID();
                i++;
            }

        }   

        if (ImGui::CollapsingHeader("Other extensions")) {            
            int i = 0;
            for (auto &e : extensions) {
                if (!e.second.platform) {
                    std::string suf = e.second.enabled ? "" : " (disabled)";

                    if (!e.second.enabled) {
                        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                    }
                    ImGui::PushID(i);
                    ImGui::Checkbox("", &e.second.whitelisted);
                    ImGui::SameLine();
                    ImGui::PopID();

                    if (!e.second.enabled) {
                        ImGui::PopItemFlag();
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              IM_COL32(180, 10, 10, 255));
                    }
                    ImGui::Text("%s%s", e.first.c_str(), suf.c_str());
                    if (!e.second.enabled) {
                        ImGui::PopStyleColor();
                    }
                    i++;
                }
            }            
        }

        if (ImGui::CollapsingHeader("Advanced")) {

            ImGui::Text("Fileprotect");
            ImGui::SameLine();
            guiInputText(cfg.fileProtect);

            static std::array<BoolGUI, 3> bools = {
                BoolGUI{&cfg.gen.objectParents, "Parent object pointers"},
                BoolGUI{&cfg.gen.structNoinit, "Struct noinit"},
                BoolGUI{&cfg.gen.structProxy, "Struct proxy"}
            };
            for (auto &b : bools) {
                guiBoolOption(b);
            }

            if (ImGui::TreeNode("C++ macros")) {
                static std::array<MacroGUI, 5> macros = {
                    MacroGUI{&cfg.macro.mNamespace, "Namespace"},
                    MacroGUI{&cfg.macro.mConstexpr, "Constexpr"},
                    MacroGUI{&cfg.macro.mNoexcept, "Noexcept"},
                    MacroGUI{&cfg.macro.mInline, "Inline"},
                    MacroGUI{&cfg.macro.mExplicit, "Explicit"}
                };
                for (auto &m : macros) {
                    guiMacroOption(m);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Debug")) {
                static std::array<BoolGUI, 1> bools = {
                    BoolGUI{&cfg.dbg.methodTags, "Show functions categories"}
                };
                for (auto &b : bools) {
                    guiBoolOption(b);
                }
                ImGui::TreePop();
            }
        }
    } else {

        static std::string regInputText;
        static bool regButtonDisabled = true;
        if (ImGui::InputText("Path", &regInputText)) {
            regButtonDisabled = !std::filesystem::is_regular_file(regInputText);
        }

        if (regButtonDisabled) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                                ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Load registry")) {
            gen.load(regInputText);
        }

        if (regButtonDisabled) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

void GUI::setupCommandBuffer(VkCommandBuffer cmd) {

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    if (imguiFrameBuilt) {
        ImDrawData *data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(data, cmd);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void GUI::drawFrame() {

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                    UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE,
                        UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    setupImguiFrame();
    setupCommandBuffer(commandBuffers[currentFrame]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                      inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void GUI::run() {

    while (!glfwWindowShouldClose(glfwWindow)) {
        glfwWaitEvents();
        drawFrame();
    }
}
