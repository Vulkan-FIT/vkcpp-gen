#include "Gui.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include "../../fonts/poppins.cpp"

Generator *GUI::gen;
bool GUI::Type::visualizeDisabled;
bool GUI::Type::drawFiltered;
bool GUI::menuOpened = false;
bool GUI::advancedMode = true;

template <typename V, typename... T>
constexpr auto array_of(T&&... t)
    -> std::array < V, sizeof...(T) >
{
    return {{ std::forward<T>(t)... }};
}

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

//    vkDestroyPipeline(device, graphicsPipeline, nullptr);
//    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
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

    //createGraphicsPipeline();
    createFramebuffers();

    vkDestroySwapchainKHR(device, old, nullptr);
}

void GUI::cleanup() {

//    vkDestroyShaderModule(device, fragShaderModule, nullptr);
//    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    cleanupSwapChain();

    ImGui_ImplVulkan_Shutdown();
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
    ImGui::PushID((id++) + 1000);
    if (ImGui::InputText("", &data)) {
    }
    ImGui::PopID();
    ImGui::PopItemWidth();
}

void GUI::guiBoolOption(BoolGUI &data) {
//    size_t y1 = ImGui::CalcTextSize("").y;
    ImGui::SetWindowFontScale(0.9);
//    size_t y2 = ImGui::CalcTextSize("").y;
//    size_t margin = (y1 - y2) / 2;
//    size_t pos = ImGui::GetCursorPosY();
//    ImGui::SetCursorPosY(pos + margin + 30);

//    size_t ypad = GImGui->Style.FramePadding.y;
//    GImGui->Style.FramePadding.y += margin;
    ImGui::Checkbox(data.text.c_str(), data.data);
//    GImGui->Style.FramePadding.y = ypad;

    ImGui::SetWindowFontScale(1.f);
//    ImGui::SameLine();
//    ImGui::Text("%s", data.text.c_str());
}

void GUI::filterTask(std::string filter) noexcept {
    filterTaskRunning = true;
    try {
        filterTaskError.clear();
        std::regex rgx(filter, std::regex_constants::icase);

        auto &enums = collection.enums;
        auto &structs = collection.structs;
        auto &commands = collection.commands;
        auto &handles = collection.handles;
        auto &platforms = collection.platforms;
        auto &extensions = collection.extensions;

        for (auto &c : enums) {
            c.filtered = std::regex_search(c.name, rgx);
            if (filterTaskAbort) {
                break;
            }
        }

        for (auto &c : structs) {
            c.filtered = std::regex_search(c.name, rgx);
            if (filterTaskAbort) {
                break;
            }
        }

        for (auto &c : commands) {
            // std::cout << c.name << std::endl;
            c.filtered = std::regex_search(c.name, rgx);
            if (filterTaskAbort) {
                break;
            }
        }

        for (auto &c : handles) {
            c.filtered = std::regex_search(c.name, rgx);
            if (filterTaskAbort) {
                break;
            }
        }

        for (auto &c : platforms) {
            c.filtered = std::regex_search(c.name, rgx);
            if (filterTaskAbort) {
                break;
            }
        }

        for (auto &c : extensions) {
            c.filtered = std::regex_search(c.name, rgx);
            if (filterTaskAbort) {
                break;
            }
        }

        if (!filterTaskAbort) {
            filterSynced = true;
            filterTaskFinised = true;            
        }
    }
    catch (std::regex_error &e) {
        filterTaskError = e.what();
    }
    catch (std::runtime_error &e) {
        std::cerr << "Task exception: " << e.what() << std::endl;
    }
    queueRedraw();
    filterTaskRunning = false;
}

void Window::queueRedraw() {
    if (glfwWaiting) {
        glfwPostEmptyEvent();
    }
    else {
        redraw = true;
    }
}

void GUI::onLoad() {

    auto &gEnums = gen->getEnums();
    auto &gStructs = gen->getStructs();
    auto &gHandles = gen->getHandles();
    auto &gCommands = gen->getCommands();
    auto &gPlatforms = gen->getPlatforms();
    auto &gExtensions = gen->getExtensions();

    auto &enums = collection.enums;
    auto &structs = collection.structs;
    auto &handles = collection.handles;
    auto &commands = collection.commands;
    auto &platforms = collection.platforms;
    auto &extensions = collection.extensions;
    extensions.clear();
    platforms.clear();
    enums.clear();
    structs.clear();
    handles.clear();
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
        platforms.emplace_back(p);
    }

    enums.reserve(gEnums.size());
    for (auto &e : gEnums) {
        enums.emplace_back(e);
    }

    structs.reserve(gStructs.size());
    for (auto &e : gStructs) {
        structs.emplace_back(e);
    }

    handles.reserve(gHandles.size());
    for (auto &h : gHandles) {
        handles.emplace_back(h);
    }

    std::map<std::string, Type *> cmdmap;
    commands.reserve(gCommands.size());
    for (auto &e : gCommands) {
        const auto &name = e.name.original;
        auto &p = commands.emplace_back(e);
        cmdmap.emplace(name, &p);
    }

    extensions.reserve(gExtensions.size());
    for (auto &e : gExtensions) {
        auto &ext = extensions.emplace_back(e);
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
            auto p = findPlatform(e->platform->name);
            if (p) {
                p->extensions.push_back(&e);
            }
        }
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
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();

    initImgui();
    initImguiStyle();

    unloadRegButton.text = "Unload reg";
    unloadRegButton.task = [&]{
        gen->unload();
    };

    generateButton.text = "Generate";
    generateButton.task = [&]{
        gen->generate();
    };

    loadConfigButton.text = "Import config";
    loadConfigButton.task = [&]{
        gen->loadConfigFile(configPath);
    };

    saveConfigButton.text = "Export config";
    saveConfigButton.task = [&]{
        gen->saveConfigFile(configPath);
    };

    gen->bindGUI([&]{onLoad();});
}

void GUI::setupImguiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    updateImgui();

    ImGui::Render();
    imguiFrameBuilt = true;
}

void GUI::showHelpMarker(const char *desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("[?]");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

template <typename... Args>
auto make_decorator(Args&&... args)
{
    return std::make_unique<decltype(GUI::RenderPair{std::forward<Args>(args)...})>(std::forward<Args>(args)...);
}

template <typename T>
auto make_config_option(int x, const T &element, const std::string &text)
{
    return std::make_unique<GUI::RenderableArray<3>>(
        GUI::RenderableArray<3>({
            std::make_unique<GUI::DummySameLine>(x, 0),
            std::make_unique<T>(element),
            std::make_unique<GUI::HelpMarker>(text)
        }));
}

void GUI::updateImgui() {    
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    bool mainw;
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
        mainScreen();
    } else {
        loadScreen();
    }    

    ImGuiContext& g = *GImGui;
    // ImGuiIO& io = ImGui::GetIO();
    if (menuOpened) {
        // queueRedraw();
        menuOpened = false;
    }
    else if (g.WantTextInputNextFrame == 1) {
        queueRedraw();
    }

    ImGui::End();
}

void GUI::mainScreen() {
    Generator::Config &cfg = gen->getConfig();
    static std::string output{gen->getOutputFilePath()};
    // static bool outputPathBad = !gen->isOuputFilepathValid();
    id = 0;
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    unloadRegButton.draw();
    ImGui::SameLine();
    std::string loadedText = "Current registry: " + gen->getRegistryPath();
    showHelpMarker(loadedText.c_str());
    ImGui::SameLine();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::SameLine();
    generateButton.draw();
    ImGui::SameLine();

    // ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::Text("Output directory:");
    ImGui::SameLine();
//        if (outputBadFlag) {
//            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
//        }
    ImGui::PushID(id++);
    if (ImGui::InputText("", &output)) {
        gen->setOutputFilePath(output);
       // outputPathBad = !gen->isOuputFilepathValid();
    }
    ImGui::PopID();
//        if (outputBadFlag) {
//            ImGui::PopStyleColor();
//        }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    loadConfigButton.draw();
    ImGui::SameLine();
    saveConfigButton.draw();
    ImGui::SameLine();

    ImGui::PushID(id++);
    if (ImGui::InputText("", &configPath)) {
    }
    ImGui::PopID();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::Text("Settings");
    ImGui::SameLine();
    if (ImGui::Button("Simple")) {
        advancedMode = false;
        queueRedraw();
    }
    ImGui::SameLine();
    if (ImGui::Button("Advanced")) {
        advancedMode = true;
        queueRedraw();
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(40.0f, 0.0f));
    ImGui::SameLine();
    if (ImGui::Button("Load VulkanHPP preset")) {
        gen->loadConfigPreset();
    }

    if (ImGui::CollapsingHeader("Configuration")) {
        ImGui::PushID(id++);

        ImGuiTableFlags containerTableFlags = 0;
        static auto t1 = RenderableTable("##TableNS", "General", 4, containerTableFlags, [&](GUI &g){
            static bool tempbool = false;
            static int indent = 25;

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Code generation");

            static auto content = RenderableArray<3>({
                make_config_option(0, BoolGUI{&cfg.gen.cppModules.data, "C++ modules"}, "generate api in c++20 modules"),
                make_config_option(0, BoolGUI{&cfg.gen.exceptions.data, "exceptions"}, "enable vulkan exceptions"),
                make_config_option(0, BoolGUI{&cfg.gen.nodiscard.data, "nodiscard"}, ""),
                //make_config_option(0, AdvancedOnlyGUI{&cfg.gen.orderCommands.data, "order PFN pointers"}, "description"),
                //make_decorator(DummySameLine(0, 0), BoolGUI{&cfg.gen.inlineCommands.data, "inline commands"}),
            });
            content.render(g);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Vulkan namespace");

            static auto contentNamespace1 = RenderableArray<5>({
                // make_config_option(0, AdvancedBoolGUI(&cfg.gen.vulkanEnums.data, "enums"), "description"),
                make_config_option(0, AdvancedBoolGUI(&cfg.gen.vulkanStructs.data, "structures"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.structConstructor.data, "struct constructors"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.structSetters.data, "setters"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.structSetters.data, "proxy setters"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.structReflect.data, "reflect"), "description"),                
                }
            );
            contentNamespace1.render(g);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("");

            static auto contentNamespace2 = RenderableArray<5>({
                make_config_option(0, AdvancedBoolGUI(&cfg.gen.vulkanHandles.data, "handles"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.smartHandles.data, "smart handles"), "description"),
                make_config_option(0, BoolGUI(&cfg.gen.vulkanCommands.data, "commands"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.dispatchParam.data, "dispatch paramenter"), "description"),
                //make_config_option(indent * 2, AdvancedOnlyGUI(&cfg.gen.dispatchTemplate.data, "dispatch as template"), "description"),
                make_config_option(indent, BoolGUI(&cfg.gen.allocatorParam.data, "allocator parameter"), "description")
                }
            );
            contentNamespace2.render(g);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("Vulkan RAII namespace");
            static auto contentRAII = RenderableArray<1>({
                make_config_option(0, BoolGUI(&cfg.gen.vulkanCommandsRAII.data, "commands"), "description"),
                }
            );
            contentRAII.render(g);

        });
        t1.render(*this);

        static auto t2 = RenderableTable("##TableP", "C++ preprocessor", 1, containerTableFlags, [&](GUI &g){
            ImGui::TableSetColumnIndex(0);
            static auto content = RenderableArray<6>({
                std::make_unique<MacroGUI>(&cfg.macro.mNamespace.data, "Namespace"),
                std::make_unique<MacroGUI>(&cfg.macro.mConstexpr.data, "Constexpr"),
                std::make_unique<MacroGUI>(&cfg.macro.mConstexpr14.data, "Constexpr 14"),
                std::make_unique<MacroGUI>(&cfg.macro.mNoexcept.data,  "Noexcept"),
                std::make_unique<MacroGUI>(&cfg.macro.mInline.data,    "Inline"),
                std::make_unique<MacroGUI>(&cfg.macro.mExplicit.data,  "Explicit")
            });
            content.render(g);
        }, true);
        t2.render(*this);

        static auto t3 = RenderableTable("##TableG", "Generator debug", 1, containerTableFlags, [&](GUI &g){
            ImGui::TableSetColumnIndex(0);
            static auto content = RenderableArray<1>({
                std::make_unique<BoolGUI>(&cfg.dbg.methodTags.data, "Show function categories")
            });
            content.render(g);
        }, true);
        t3.render(*this);

        ImGui::PopID();
    }

    ImGuiTableFlags containerTableFlags = 0;
    containerTableFlags |= ImGuiTableFlags_Resizable;
    containerTableFlags |= ImGuiTableFlags_BordersOuter;
    containerTableFlags |= ImGuiTableFlags_ScrollY;

    if (ImGui::CollapsingHeader("Detailed selection")) {
        if (ImGui::BeginTable("##Table", 1, containerTableFlags)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Dummy(ImVec2(0, 10));
            ImGui::PushID(id++);
            ImGui::Text("Filter");
            ImGui::SameLine();
            bool filterChanged = ImGui::InputText("", &filter);
            if (filterChanged) {
                // std::cout << "filter text: " << filter << std::endl;
                if (filterTaskRunning) {
                    filterTaskAbort = true;
                }
                else {
                    filterSynced = false;
                    filterFuture = std::async(std::launch::async, &GUI::filterTask, this, filter);
                }
            }
            else if (filterTaskAbort && !filterTaskRunning) {
                filterTaskAbort = false;
                filterSynced = false;
                filterFuture = std::async(std::launch::async, &GUI::filterTask, this, filter);
            }

            bool error = !filterSynced && !filterTaskRunning && !filterTaskError.empty();
            if (error) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
                ImGui::Text("Bad regex: %s", filterTaskError.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PopID();

            if (filterSynced) {
                collection.draw(id, true);
            }

            ImGui::EndTable();
        }
    }
}

void GUI::loadScreen() {
    static std::string regInputText;
    static bool regButtonDisabled = true;
    if (ImGui::Button("Load registry")) {
        gen->load(regInputText);
    }
    ImGui::SameLine();
    if (ImGui::InputText("", &regInputText)) {
        regButtonDisabled = !std::filesystem::is_regular_file(regInputText);
    }

    if (regButtonDisabled) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
    }

    if (regButtonDisabled) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }

    std::string path = Generator::findDefaultRegistryPath();
    if (!path.empty()) {
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Text("Found: %s", path.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            gen->load(path);
        }
    }
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
        if (redraw) {
            redraw = false;
            glfwPollEvents();
        }
        else {
            glfwWaiting = true;
            glfwWaitEvents();
            glfwWaiting = false;
        }
        auto t = glfwGetTime();
        if (t >= next) {
            drawFrame();
            next = t + target;
        }
    }
}

template<typename T>
bool GUI::drawContainerHeader(const char *name, SelectableData<T> *s, bool empty) {
    bool check = s->data->isEnabled();
    if (ImGui::Checkbox("", &check)) {
        s->data->setEnabled(check);
    }
    ImGui::SameLine();
    return drawContainerHeader(name, reinterpret_cast<Selectable*>(s), empty);
}

bool GUI::drawContainerHeader(const char *name, Selectable *s, bool empty) {
    bool open = false;
    if (!empty) {
        ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_OpenOnArrow;
        if (s->selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        open = ImGui::TreeNodeEx(name, flags);
        if (ImGui::IsItemClicked()) {
            if (ImGui::GetIO().KeyCtrl) {
                s->selected = !s->selected;
            }
        }

        if (!GUI::menuOpened && ImGui::BeginPopupContextItem()) {
            ImGui::Text("-- %s --", name);
            if (ImGui::MenuItem("Select all")) {
                s->setSelected(true);
            }
            if (ImGui::MenuItem("Deselect all")) {
                s->setSelected(false);
            }
            if (ImGui::MenuItem("Enable selected")) {
                s->setEnabledChildren(true, true);
            }
            if (ImGui::MenuItem("Disable selected")) {
                s->setEnabledChildren(false, true);
            }
            if (ImGui::MenuItem("Enable all")) {
                s->setEnabledChildren(true);
            }
            if (ImGui::MenuItem("Disable all")) {
                s->setEnabledChildren(false);
            }
            ImGui::EndPopup();
            GUI::menuOpened = true;
        }
    }
    else {
        int x = ImGui::GetTreeNodeToLabelSpacing();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
        drawSelectable(name, s);
    }
    return open;
}

void GUI::drawSelectable(const char *text, Selectable *s) {
    if (s->hovered || s->selected) {
        auto size = ImGui::CalcTextSize(text);
        ImVec2 p = ImGui::GetCursorScreenPos();
        auto clr = ImGui::ColorConvertFloat4ToU32(
            ImGui::GetStyle()
                .Colors[s->hovered ? ImGuiCol_HeaderHovered
                                   : ImGuiCol_Header]);
        auto pad = ImGui::GetStyle().FramePadding;
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x, p.y),
            ImVec2(p.x + size.x + pad.x * 2, p.y + size.y + pad.y * 2),
            clr);
    }
    ImGui::Text("%s", text);
    s->hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl) {
            s->selected = !s->selected;
        }
    }
}

void GUI::setConfigPath(const std::string &path) {
    configPath = path;
}

template<typename T>
void GUI::Container<T>::draw(int id) {
    auto &elements = *this;
    if (Type::drawFiltered) {
        bool hasNone = true;
        for (size_t i = 0; i < elements.size(); i++) {
            bool f = false;
            if constexpr (std::is_pointer_v<T>) {
                f = elements[i]->filtered;
            } else {
                f = elements[i].filtered;
            }
            if (f) {
                hasNone = false;
                break;
            }
         }
         if (hasNone) {
            return;
         }
    }


    ImGui::PushID(id);
    bool open = true;
    if (!name.empty()) {
        open = drawContainerHeader(name.c_str(), this);
    }
    if (open) {
        for (size_t i = 0; i < elements.size(); i++) {
            if constexpr (std::is_pointer_v<T>) {
                elements[i]->draw(i);
            } else {
                elements[i].draw(i);
            }
        }        
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void GUI::Type::draw(int id) {
    if (Type::drawFiltered && !filtered) {
        return;
    }
    std::string disabler;
    if (visualizeDisabled) {
        if (data->ext) {
            if (!data->ext->isEnabled()) {
                disabler = data->ext->name;
            }
            else if (data->ext->platform) {
                if (!data->ext->platform->isEnabled()) {
                    disabler = data->ext->platform->name;
                }
            }
        }
    }

    if (!disabler.empty()) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.6f);
    }

    std::string n = data->name.original;
    ImGui::PushID(id);
    ImGui::Dummy(ImVec2(5.0f, 0.0f));
    ImGui::SameLine();
    bool check = data->isEnabled();
    if (ImGui::Checkbox("", &check)) {
        std::cout << "set enabled: " << check << ": " << data->name << std::endl;
        data->setEnabled(check);
    }
    ImGui::SameLine();
    drawSelectable(n.c_str(), this);

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

    // if ( !data->dependencies.empty()) {

    if (!disabler.empty()) {
        ImGui::PopStyleVar();
    }
    ImGui::PopID();
}

void GUI::Extension::draw(int id) {
    if (Type::drawFiltered && !filtered) {
        return;
    }
    if (!data->isSuppored()) {
        return;
    }
    bool hasNone = true;
    for (const auto &c : commands) {
        if (!Type::drawFiltered || c->filtered) {
            hasNone = false;
            break;
        }
    }

    ImGui::PushID(id);
    if (drawContainerHeader(name, this, hasNone)) {
        this->commands.draw(0);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void GUI::Platform::draw(int id) {
    if (Type::drawFiltered && !filtered) {
        return;
    }
    bool hasNone = true;
    for (const auto &c : extensions) {
        if (!Type::drawFiltered || c->filtered) {
            hasNone = false;
            break;
        }
    }
    ImGui::PushID(id);
    if (drawContainerHeader(name, this, hasNone)) {
        this->extensions.draw(0);
        ImGui::TreePop();
    }
    ImGui::PopID();
}


void GUI::Collection::draw(int &id, bool filtered) {
    Type::drawFiltered = filtered;
    Type::visualizeDisabled = false;
    platforms.draw(id++);
    extensions.draw(id++);
    Type::visualizeDisabled = true;
    enums.draw(id++);
    structs.draw(id++);
    handles.draw(id++);
    commands.draw(id++);
}

void GUI::AsyncButton::draw() {
    bool locked = running;
    if (locked) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.7f);
    }
    if (ImGui::Button(text.c_str())) {
        run();
    }
    if (locked) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
}

void GUI::AsyncButton::run() {
    if (running) {
        return;
    }
    running = true;

    future = std::async(std::launch::async, [&]{
        try {
            task();
        }
        catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
        running = false;
    });
}

