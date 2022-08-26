#include "Gui.hpp"

// shader code in SPIR-V binary
static const uint32_t vsSpirv[] = {
#include "../../shaders/vs.vert.spv"
};
static const uint32_t fsSpirv[] = {
#include "../../shaders/ps.frag.spv"
};

#include "../../fonts/poppins.cpp"

Generator *GUI::gen;
GUI::Platforms GUI::platforms;
GUI::Extensions GUI::extensions;
//std::vector<GUI::Extension *> GUI::otherExtensions;
GUI::Types GUI::commands;
GUI::Types GUI::structs;
GUI::Types GUI::enums;
bool GUI::Type::visualizeDisabled;

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


//void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {}

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
    glfwSetWindowUserPointer(glfwWindow, reinterpret_cast<Window*>(this));

#define genericCallback(functionName)                                          \
    [](GLFWwindow *window, auto... args) {                                     \
        auto pointer =                                                         \
            static_cast<GUI::Window *>(glfwGetWindowUserPointer(window));      \
        if (pointer->functionName)                                             \
            pointer->functionName(pointer, args...);                           \
    }

    onSize = [&](auto self, int width, int height) {
        this->width = width;
        this->height = height;
        recreateSwapChain();
        drawFrame();
    };

    glfwSetWindowSizeCallback(glfwWindow, genericCallback(onSize));

    glfwSetKeyCallback(glfwWindow, [](GLFWwindow *window, int key, int scancode, int action, int mods){
//        std::cout << "key event: " << key << ", " << scancode << ", " << action << ", " << mods << std::endl;
        auto w = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        w->onKey(window, key, scancode, action, mods);
    });
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


    ImGuiIO& io = ImGui::GetIO();
    font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(Poppins_compressed_data_base85, 20.0);
    if (!font) {
        std::cerr << "font load error" << std::endl;
    }
    else {
        io.Fonts->Build();
    }

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
        ImVec4(col_main.x, col_main.y, col_main.z, 0.48f);
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
        ImVec4(col_main.x, col_main.y, col_main.z, 0.7f);
    style.Colors[ImGuiCol_HeaderActive] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.90f);
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

    style.Colors[ImGuiCol_Header] =
        ImVec4(col_main.x, col_main.y, col_main.z, 0.9f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2f, 0.2f, 0.2f, 0.9f);
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

void GUI::guiMacroOption(Macro &m) {   

    ImGui::Text("%s", m.text.c_str());
    ImGui::SameLine();

    bool disabled = !m.data->usesDefine;

    if (disabled) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
    }

    ImGui::Checkbox("#define ", &m.data->usesDefine);
    if (m.data->usesDefine) {
        ImGui::PushID(0);
        ImGui::SameLine();
        guiInputText(m.data->define);
        ImGui::PopID();
    }

    if (disabled) {
        ImGui::PopStyleVar();
    }

    ImGui::SameLine();
    guiInputText(m.data->value);

}

void GUI::guiBoolOption(BoolGUI &data) {
    //ImGui::PushID(0);

    ImGui::Text("%s", data.text.c_str());
    ImGui::SameLine();

    //    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, false);

    ImGui::Checkbox("", data.data);

    //  ImGui::PopItemFlag();
    // std::cout << data.text << " " << data.data << " : " << (*data.data?
    // "true" : "false") << std::endl;

    //ImGui::PopID();
}

void GUI::onLoad() {
    auto &gEnums = gen->getEnums();
    auto &gStructs = gen->getStructs();
    auto &gCommands = gen->getCommands();
    auto &gPlatforms = gen->getPlatforms();
    auto &gExtensions = gen->getExtensions();

    // otherExtensions.clear();
    extensions.clear();
    platforms.clear();
    enums.clear();
    structs.clear();
    commands.clear();

    const auto findPlatform = [&](const std::string &name) -> Platform * {
        for (auto &p : platforms) {
            if (p.name == name) {
                return &p;
            }
        }
        return nullptr;
    };

    platforms.reserve(gPlatforms.size());
    for (auto &p : gPlatforms) {
        platforms.emplace_back(p.first, p.second);
    }

    enums.reserve(gEnums.size());
    for (auto &e : gEnums) {
        enums.emplace_back(e.first, e.second);
    }

    structs.reserve(gStructs.size());
    for (auto &e : gStructs) {
        structs.emplace_back(e.first, e.second);
    }

    std::map<std::string, Type *> cmdmap;
    commands.reserve(gCommands.size());
    for (auto &e : gCommands) {
        auto p = &commands.emplace_back(e.first, e.second);
        cmdmap.emplace(e.first, p);
    }

    extensions.reserve(gExtensions.size());
    for (auto &e : gExtensions) {
        auto &ext = extensions.emplace_back(e.first, e.second);
        ext.commands.reserve(ext.data->commands.size());
        for (const auto &c : ext.data->commands) {
            auto cmd = cmdmap.find(c->name.original);
            if (cmd == cmdmap.end()) {
                std::cerr << "Gui: can't find command: " << c->name.original
                          << std::endl;
            } else {
                ext.commands.push_back(cmd->second);
            }
        }
    }

    for (auto &e : extensions) {
        if (e->platform) {
            auto p = findPlatform(e->platform->first);
            if (p) {
                p->extensions.push_back(&e);
            }
        }
//        else {
//            otherExtensions.push_back(&e);
//        }
    }

    std::cout << "GUI loaded data." << std::endl;
    //std::cout << "  " << commands.size() << " commands" << std::endl;
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

    gen->bindGUI(onLoad);
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
    Generator::Config &cfg = gen->getConfig();

    ImGui::NewFrame();

    bool mainw;

    ImGui::SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGui::Begin("Gen", &mainw,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    if (showFps) {
        int fps = 0;
        float delta = ImGui::GetIO().DeltaTime;
        if (delta != 0) {
            fps = 1.0 / delta;
        }
        ImGui::Text("FPS %d, avg: %d", fps, (int)ImGui::GetIO().Framerate);
    }


    if (gen->isLoaded()) {

        static std::string output{gen->getOutputFilePath()};
        static bool outputPathBad = !gen->isOuputFilepathValid();        

        int id = 0;
        bool outputBadFlag = outputPathBad;
        if (outputBadFlag) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                                ImGui::GetStyle().Alpha * 0.5f);
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (ImGui::Button("Generate") && !outputBadFlag) {
            gen->generate();
        }
        ImGui::SameLine();
        if (outputBadFlag) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        // ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Text("Output directory:");
        ImGui::SameLine();
        if (outputBadFlag) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
        }
        ImGui::PushID(id++);
        if (ImGui::InputText("", &output)) {
            gen->setOutputFilePath(output);
            outputPathBad = !gen->isOuputFilepathValid();
        }
        ImGui::PopID();
        if (outputBadFlag) {
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        if (ImGui::Button("Load config")) {
            gen->loadConfigFile(configPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save config")) {
            gen->saveConfigFile(configPath);
        }
        ImGui::SameLine();
        ImGui::PushID(id++);
        if (ImGui::InputText("", &configPath)) {

        }
        ImGui::PopID();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        if (ImGui::CollapsingHeader("Configuration")) {
            ImGui::PushID(id++);

//            ImGui::Text("Fileprotect");
//            ImGui::SameLine();
//            guiInputText(cfg.fileProtect);

            ImGui::Text("Code generation");
            static std::array<BoolGUI, 2> bools = {
                BoolGUI{&cfg.gen.cppModules.data, "C++ Modules"},
                BoolGUI{&cfg.gen.exceptions.data, "exeptions"}
            };
            int i = 0;
            for (auto &b : bools) {
                ImGui::PushID(i++);
                guiBoolOption(b);
                ImGui::PopID();
            }
            ImGui::Text("  Vulkan namespace");
            static std::array<BoolGUI, 4> vknsbools = {
                BoolGUI{&cfg.gen.vulkanCommands.data, "commands"},
                BoolGUI{&cfg.gen.dispatchParam.data, "dispatch paramenter"},
                BoolGUI{&cfg.gen.allocatorParam.data, "allocator parameter"},
                BoolGUI{&cfg.gen.smartHandles.data, "smart handles"}
            };
            for (auto &b : vknsbools) {
                ImGui::PushID(i++);
                guiBoolOption(b);
                ImGui::PopID();
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            if (ImGui::TreeNode("C++ macros")) {
                static std::array<Macro, 5> macros = {
                    Macro{&cfg.macro.mNamespace.data, "Namespace"},
                    Macro{&cfg.macro.mConstexpr.data, "Constexpr"},
                    Macro{&cfg.macro.mNoexcept.data, "Noexcept"},
                    Macro{&cfg.macro.mInline.data, "Inline"},
                    Macro{&cfg.macro.mExplicit.data, "Explicit"}};
                for (auto &m : macros) {
                    ImGui::PushID(i++);
                    guiMacroOption(m);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Debug")) {
                static std::array<BoolGUI, 1> bools = {
                    BoolGUI{&cfg.dbg.methodTags.data, "Show function categories"}};
                for (auto &b : bools) {
                    ImGui::PushID(i++);
                    guiBoolOption(b);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

/*
        ImGui::PushID(id++);
        ImGui::Text("Filter");
        ImGui::SameLine();
        if (ImGui::InputText("", &filter)) {
            // std::cout << "filter text: " << filter << std::endl;
        }
        ImGui::PopID();
*/
        Type::visualizeDisabled = false;
        platforms.draw(id++);
        extensions.draw(id++);
        Type::visualizeDisabled = true;
        enums.draw(id++);
        structs.draw(id++);
        commands.draw(id++);

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
            gen->load(regInputText);
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
    double target = 1.0 / 60.0;
    double next = 0.0;

    while (!glfwWindowShouldClose(glfwWindow)) {
        ImGuiIO& io = ImGui::GetIO();
        std::cout << "keys: " << io.InputQueueCharacters.size() << std::endl;
        glfwWaitEvents();
        if (glfwGetTime() >= next) {
            std::cout << "draw " << glfwGetTime()  << std::endl;
            drawFrame();
            next += target;
        }
    }
}

template<typename T>
bool GUI::drawContainerHeader(const char *name, bool node, SelectableData<T> *s) {
    bool check = s->data->isEnabled();
    if (ImGui::Checkbox("", &check)) {
        s->data->setEnabled(check);
    }
    ImGui::SameLine();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_OpenOnArrow;
    if (s->selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool open = drawContainerHeader(name, node, reinterpret_cast<Selectable*>(s));

    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl) {
            s->selected = !s->selected;
        }
    }
    return open;
}

bool GUI::drawContainerHeader(const char *name, bool node, Selectable *s) {
//    static const auto setSelect = [](std::vector<T> &elements, bool enabled) {
//       for (auto &p : elements) {
//            if constexpr (std::is_pointer_v<T>) {
//                p->selected = enabled;
//            } else {
//                p.selected = enabled;
//            }
//        }
//    };

//    static const auto setEnabled = [](std::vector<T> &elements, bool enabled, bool all) {
//       for (auto &p : elements) {
//            if constexpr (std::is_pointer_v<T>) {
//                if (p->selected || all) {
//                    p->data->enabled = enabled;
//                }
//            } else {
//                if (p.selected || all) {
//                    p.data->enabled = enabled;
//                }
//            }
//        }
//    };

    bool open;
    if (node) {
        ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_OpenOnArrow;
        if (s->selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        open = ImGui::TreeNodeEx(name, flags);
    }
    else {
        open = ImGui::CollapsingHeader(name);
    }


    if (ImGui::BeginPopupContextItem()) {
        ImGui::Text("-- %s --", name);
        if (ImGui::MenuItem("Select all")) {
            //setSelect(elements, true);
            // s->setEnabled(true);
        }
        if (ImGui::MenuItem("Deselect all")) {
            //setSelect(elements, false);
        }
        if (ImGui::MenuItem("Enable selected")) {
            //setEnabled(elements, true, false);
        }
        if (ImGui::MenuItem("Disable selected")) {
            //setEnabled(elements, false, false);
        }
        if (ImGui::MenuItem("Enable all")) {
            s->setEnabled(true);
            //setEnabled(elements, true, true);
        }
        if (ImGui::MenuItem("Disable all")) {
            s->setEnabled(false);
            //setEnabled(elements, false, true);
        }
        ImGui::EndPopup();
    }
    return open;
}

void GUI::setConfigPath(const std::string &path) {
    configPath = path;
}

template<typename T>
void GUI::Container<T>::draw(int id) {
    ImGui::PushID(id);

    auto &elements = *this;
    bool open = true;
    if (!name.empty()) {
        open = drawContainerHeader(name.c_str(), isNode, this);
    }
    if (open) {
        for (size_t i = 0; i < elements.size(); i++) {
            if constexpr (std::is_pointer_v<T>) {
                elements[i]->draw(i);
            } else {
                elements[i].draw(i);
            }
        }
        if (isNode) {
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}

void GUI::Type::draw(int id) {
    bool alpha = false;
    std::string disabler;
    if (visualizeDisabled) {
        if (data->ext) {
            if (!data->ext->isEnabled()) {
                alpha = true;
                disabler = data->ext->name;
            }
            else if (data->ext->platform) {
                if (!data->ext->platform->second.isEnabled()) {
                    alpha = true;
                    disabler = data->ext->platform->second.name;
                }
            }
        }
    }

    if (!disabler.empty()) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
    }

    std::string n = data->name.original;
    ImGui::PushID(id);
    ImGui::Dummy(ImVec2(5.0f, 0.0f));
    ImGui::SameLine();
    bool check = data->isEnabled();
    if (ImGui::Checkbox("", &check)) {
        data->setEnabled(check);
    }
    ImGui::SameLine();
    if (hovered || selected) {
        auto size = ImGui::CalcTextSize(n.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos();
        auto clr = ImGui::ColorConvertFloat4ToU32(
            ImGui::GetStyle()
                .Colors[selected ? ImGuiCol_Header
                                   : ImGuiCol_HeaderHovered]);
        auto pad = ImGui::GetStyle().FramePadding;
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x, p.y),
            ImVec2(p.x + size.x + pad.x * 2, p.y + size.y + pad.y * 2),
            clr);
    }
    ImGui::Text("%s", n.c_str());
    hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl) {
            selected = !selected;
        }
    }

    if (!data->dependencies.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 100.0f);
            std::string text;
            if (!disabler.empty()) {
                text += disabler + " is disabled\n\n";
            }
            text += "Requires:\n\n";
            for (const auto &d : data->dependencies) {
                text += d->name + "\n";
            }
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    if (!disabler.empty()) {
        ImGui::PopStyleVar();
    }
    ImGui::PopID();
}

void GUI::Extension::draw(int id) {
    if (!data->supported) {
        return;
    }
    ImGui::PushID(id);
    if (drawContainerHeader(name, true, this)) {
        this->commands.draw(0);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void GUI::Platform::draw(int id) {
    ImGui::PushID(id);
    if (drawContainerHeader(name, true, this)) {
        this->extensions.draw(0);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

