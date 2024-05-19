// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Gui.hpp"

#include "../../fonts/poppins.cpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

using namespace ImGui;

namespace vkgen
{
    Generator *GUI::gen;

    // bool vkgen::GUI::Type::visualizeDisabled;
    // bool vkgen::GUI::Type::drawFiltered;

    bool GUI::menuOpened = false;
}  // namespace vkgen

template <typename V, typename... T>
constexpr auto array_of(T &&...t) -> std::array<V, sizeof...(T)> {
    return { { std::forward<T>(t)... } };
}

template <typename... Args>
auto make_decorator(Args &&...args) {
    return std::make_unique<decltype(vkgen::GUI::RenderPair{ std::forward<Args>(args)... })>(std::forward<Args>(args)...);
}

namespace vkgen
{

    template <typename T>
    auto make_config_option(int x, const T &element, const std::string &text) {
        return std::make_unique<GUI::RenderableArray<3>>(GUI::RenderableArray<3>(
          { std::make_unique<GUI::DummySameLine>(static_cast<float>(x), 0.f), std::make_unique<T>(element), std::make_unique<GUI::HelpMarker>(text) }));
    }

    template <typename T>
    auto make_config_option(GUI::Level level, int x, const T &element, const std::string &text) {
        return std::make_unique<GUI::LevelRenderable<GUI::RenderableArray<3>>>(
          level,
          GUI::RenderableArray<3>(
            { std::make_unique<GUI::DummySameLine>(static_cast<float>(x), 0.f), std::make_unique<T>(element), std::make_unique<GUI::HelpMarker>(text) }));
    }

}  // namespace vkgen

static int xid = 0;

namespace ImGui
{
    bool CheckBoxTristate(const char *label, int *v_tristate) {
        bool ret;
        if (*v_tristate == -1) {
            PushItemFlag(ImGuiItemFlags_MixedValue, true);
            bool b = false;
            ret    = Checkbox(label, &b);
            if (ret)
                *v_tristate = 1;
            PopItemFlag();
        } else {
            bool b = (*v_tristate != 0);
            ret    = Checkbox(label, &b);
            if (ret)
                *v_tristate = (int)b;
        }
        return ret;
    }
}  // namespace ImGui

vkgen::GUI::QueueFamilyIndices vkgen::GUI::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

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

vkgen::GUI::SwapChainSupportDetails vkgen::GUI::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR vkgen::GUI::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkExtent2D vkgen::GUI::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void vkgen::GUI::checkLayerSupport(const std::vector<const char *> &layers) {
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
            throw std::runtime_error("Error: Layer not available: " + std::string(layerName));
        }
    }
}

VkShaderModule vkgen::GUI::createShaderModule(const uint32_t *code, uint32_t codeSize) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = codeSize;
    createInfo.pCode                    = code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void vkgen::GUI::createWindow() {
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

    width      = mode->width * 3 / 4;
    height     = mode->height * 3 / 4;
    glfwWindow = glfwCreateWindow(width, height, "Vulkan C++20 generator", NULL, NULL);
    if (!glfwWindow) {
        throw std::runtime_error("GLFW error: window init");
    }
    glfwSetWindowSizeLimits(glfwWindow, mode->width / 8, mode->height / 8, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowUserPointer(glfwWindow, reinterpret_cast<Window *>(this));

#define genericCallback(functionName)                                                       \
    [](GLFWwindow *window, auto... args) {                                                  \
        auto pointer = static_cast<vkgen::GUI::Window *>(glfwGetWindowUserPointer(window)); \
        if (pointer->functionName)                                                          \
            pointer->functionName(pointer, args...);                                        \
    }

    onSize = [&](auto self, int width, int height) {
        this->width  = width;
        this->height = height;
        recreateSwapChain();
        drawFrame();
    };

    glfwSetWindowSizeCallback(glfwWindow, genericCallback(onSize));

    glfwSetKeyCallback(glfwWindow, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        //        std::cout << "key event: " << key << ", " << scancode << ", " << action << ", " << mods << '\n';
        auto w = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        w->onKey(window, key, scancode, action, mods);
    });
}

void vkgen::GUI::createInstance() {
    uint32_t     glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkApplicationInfo appInfo{ .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                               .pApplicationName   = "Hello Triangle",
                               .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                               .pEngineName        = "No Engine",
                               .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
                               .apiVersion         = VK_API_VERSION_1_0 };

    static std::vector<const char *> layers;

    // layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo instanceInfo{ .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                       .pApplicationInfo        = &appInfo,
                                       .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
                                       .ppEnabledLayerNames     = layers.data(),
                                       .enabledExtensionCount   = glfwExtensionCount,
                                       .ppEnabledExtensionNames = glfwExtensions };

    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("VK Error: failed to create instance!");
    }
}

void vkgen::GUI::createSurface() {
    if (glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void vkgen::GUI::pickPhysicalDevice() {
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

void vkgen::GUI::createDevice() {
    indices = findQueueFamilies(physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount       = 1;

    float queuePriority              = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    const std::vector<const char *> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos    = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount   = (uint32_t)deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void vkgen::GUI::createSwapChain(VkSwapchainKHR old) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities, glfwWindow);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{ .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                  .surface          = surface,
                                                  .minImageCount    = imageCount,
                                                  .imageFormat      = surfaceFormat.format,
                                                  .imageColorSpace  = surfaceFormat.colorSpace,
                                                  .imageExtent      = extent,
                                                  .imageArrayLayers = 1,
                                                  .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                  .preTransform     = swapChainSupport.capabilities.currentTransform,
                                                  .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                  .presentMode      = presentMode,
                                                  .clipped          = VK_TRUE,
                                                  .oldSwapchain     = old };

    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent      = extent;
}

void vkgen::GUI::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image                           = swapChainImages[i];
        createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format                          = swapChainImageFormat;
        createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void vkgen::GUI::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapChainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void vkgen::GUI::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = { swapChainImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = attachments;
        framebufferInfo.width           = swapChainExtent.width;
        framebufferInfo.height          = swapChainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void vkgen::GUI::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void vkgen::GUI::createCommandBuffers() {
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void vkgen::GUI::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.flags = 0;
    // layoutInfo.bindingCount = 1;
    // layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void vkgen::GUI::initImgui() {
    VkDescriptorPoolSize poolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                                         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                                         { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                                         { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                                         { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                                         { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                                         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                                         { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                                         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                                         { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                                         { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo desrciptorPoolInfo{ .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                   .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                   .maxSets       = 1000,
                                                   .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
                                                   .pPoolSizes    = poolSizes };

    if (vkCreateDescriptorPool(device, &desrciptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    CreateContext();

    ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

    const auto check = [](VkResult result) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error("imgui vulkan init error.");
        }
    };

    ImGui_ImplVulkan_InitInfo init_info = { .Instance        = instance,
                                            .PhysicalDevice  = physicalDevice,
                                            .Device          = device,
                                            .QueueFamily     = indices.graphicsFamily.value(),
                                            .Queue           = presentQueue,
                                            .PipelineCache   = VK_NULL_HANDLE,
                                            .DescriptorPool  = descriptorPool,
                                            .MinImageCount   = 2,
                                            .ImageCount      = (uint32_t)swapChainImages.size(),
                                            .Allocator       = nullptr,
                                            .CheckVkResultFn = check };

    ImGui_ImplVulkan_Init(&init_info, renderPass);

    VkFence           fence;
    VkFenceCreateInfo fenceCreateInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0 };

    if (vkCreateFence(device, &fenceCreateInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("error: vkCreateFence");
    }

    const auto immediate_submit = [&](std::function<void(VkCommandBuffer cmd)> &&function) {
        VkCommandBuffer cmd = *commandBuffers.rbegin();

        VkCommandBufferBeginInfo cmdBeginInfo = { .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                  .pNext            = nullptr,
                                                  .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                  .pInheritanceInfo = nullptr };

        if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS) {
            throw std::runtime_error("error: vkBeginCommandBuffer");
        }

        function(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("error: vkEndCommandBuffer");
        }

        VkSubmitInfo submit = { .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .pNext                = nullptr,
                                .waitSemaphoreCount   = 0,
                                .pWaitSemaphores      = nullptr,
                                .pWaitDstStageMask    = nullptr,
                                .commandBufferCount   = 1,
                                .pCommandBuffers      = &cmd,
                                .signalSemaphoreCount = 0,
                                .pSignalSemaphores    = nullptr };

        if (vkQueueSubmit(graphicsQueue, 1, &submit, fence) != VK_SUCCESS) {
            throw std::runtime_error("error: vkQueueSubmit");
        }

        vkWaitForFences(device, 1, &fence, true, 9999999999);
        vkResetFences(device, 1, &fence);

        vkResetCommandPool(device, commandPool, 0);
    };

    ImGuiIO &io = GetIO();
    font        = io.Fonts->AddFontFromMemoryCompressedBase85TTF(Poppins_compressed_data_base85, 24.0);
    if (!font) {
        std::cerr << "font load error" << '\n';
    } else {
        io.Fonts->Build();
    }

    immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void vkgen::GUI::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void vkgen::GUI::cleanupSwapChain() {
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

void vkgen::GUI::recreateSwapChain() {
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

    // createGraphicsPipeline();
    createFramebuffers();

    vkDestroySwapchainKHR(device, old, nullptr);
}

void vkgen::GUI::cleanup() {
    //    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    //    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    cleanupSwapChain();

    ImGui_ImplVulkan_Shutdown();
}

void vkgen::GUI::initImguiStyle() {
    ImGuiStyle &style = GetStyle();

    ImVec4 col_text = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImVec4 col_main = ImVec4(0.18f, 0.f, 0.f, 1.f);
    ImVec4 col_back = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    ImVec4 col_area = ImVec4(0.15f, 0.f, 0.f, 1.f);

    style.Colors[ImGuiCol_Text]                 = ImVec4(col_text.x, col_text.y, col_text.z, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]         = ImVec4(col_text.x, col_text.y, col_text.z, 0.58f);
    style.Colors[ImGuiCol_WindowBg]             = ImVec4(col_back.x, col_back.y, col_back.z, 1.00f);
    style.Colors[ImGuiCol_ChildBg]              = ImVec4(col_area.x, col_area.y, col_area.z, 0.00f);
    style.Colors[ImGuiCol_Border]               = ImVec4(col_text.x, col_text.y, col_text.z, 0.30f);
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]              = ImVec4(col_area.x, col_area.y, col_area.z, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(col_main.x, col_main.y, col_main.z, 0.68f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_TitleBg]              = ImVec4(col_main.x, col_main.y, col_main.z, 0.45f);
    style.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(col_main.x, col_main.y, col_main.z, 0.35f);
    style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(col_main.x, col_main.y, col_main.z, 0.48f);
    style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(col_area.x, col_area.y, col_area.z, 0.57f);
    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(col_back.x, col_back.y, col_back.z, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(col_main.x, col_main.y, col_main.z, 0.31f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_CheckMark]            = ImVec4(col_text.x, col_text.y, col_text.z, 0.80f);
    style.Colors[ImGuiCol_SliderGrab]           = ImVec4(col_main.x, col_main.y, col_main.z, 0.24f);
    style.Colors[ImGuiCol_SliderGrabActive]     = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_Button]               = ImVec4(col_main.x, col_main.y, col_main.z, 0.44f);
    style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(col_main.x, col_main.y, col_main.z, 0.86f);
    style.Colors[ImGuiCol_ButtonActive]         = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_Header]               = ImVec4(col_main.x, col_main.y, col_main.z, 0.76f);
    style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(col_main.x, col_main.y, col_main.z, 0.7f);
    style.Colors[ImGuiCol_HeaderActive]         = ImVec4(col_main.x, col_main.y, col_main.z, 0.90f);
    style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(col_main.x, col_main.y, col_main.z, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_PlotLines]            = ImVec4(col_text.x, col_text.y, col_text.z, 0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(col_text.x, col_text.y, col_text.z, 0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(col_main.x, col_main.y, col_main.z, 0.43f);

    style.Colors[ImGuiCol_Header]        = ImVec4(col_main.x, col_main.y, col_main.z, 0.9f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2f, 0.2f, 0.2f, 0.9f);
}

void vkgen::GUI::guiInputText(std::string &data) {
    float s = CalcTextSize(data.c_str()).x;
    s       = std::clamp(s, 0.f, static_cast<float>(width) / 3.f) + 20;
    PushItemWidth(s);
    PushID((id++) + 1000);
    if (InputText("", &data)) {
    }
    PopID();
    PopItemWidth();
}


static void renderNestedOption(const std::function<void()> content) {
    SetWindowFontScale(0.8f);
    auto size = CalcTextSize("");
    ImGui::Dummy({ size.y * 2, 0 });
    ImVec2 p1 = GetCursorPos();
    p1.x += size.y;
    ImGui::SameLine();

    content();

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    auto color = ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0));
    auto width = 2.f;

    ImVec2 p2 = { p1.x, p1.y + size.y / 2 };
    ImVec2 p3 = { p2.x + size.y, p2.y };

    window->DrawList->AddLine(p1, p2, color, width);
    window->DrawList->AddLine(p2, p3, color, width);

    SetWindowFontScale(1.f);
}


template <typename T>
void vkgen::GUI::NestedOption<T>::render(GUI &g) {
    SetWindowFontScale(0.8f);

    auto space = 60;
    auto size = CalcTextSize("");
    size.x = 20;
    ImVec2 p1 = GetCursorPos();
    p1.x += space;

    ImGui::Dummy({ space + size.x, 0 });

    // p1.x += size.y * ;
    ImGui::SameLine();

    T::render(g);

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    auto color = ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0));
    auto width = 2.f;


    ImVec2 p2 = { p1.x, p1.y + size.y / 2 };
    ImVec2 p3 = { p2.x + size.x, p2.y };

    window->DrawList->AddLine(p1, p2, color, width);
    window->DrawList->AddLine(p2, p3, color, width);

    SetWindowFontScale(1.f);
}

// void vkgen::GUI::NestedOption::render(GUI &g) {
//     content(g);
// }

vkgen::GUI::BoolDefineGUI::BoolDefineGUI(Define *data, const std::string &text) : data(data), text(text) {
    state       = data->state != Define::DISABLED;
    stateDefine = data->state == Define::COND_ENABLED;
}

void vkgen::GUI::BoolDefineGUI::render(GUI &g) {
    SetWindowFontScale(1.f);

    const bool tri = data->state == Define::COND_ENABLED;
    if (tri) {
        PushItemFlag(ImGuiItemFlags_MixedValue, true);
    }
    if (Checkbox(text.c_str(), &state)) {
        updateState();
        g.queueRedraw();
    }
    if (tri) {
        PopItemFlag();
    }

    //    SetWindowFontScale(0.8f);
    //    auto size = CalcTextSize("");
    //    ImGui::Dummy({ size.y * 2, 0 });
    //    ImVec2 p1 = GetCursorPos();
    //    p1.x += size.y;
    //    ImGui::SameLine();
    //    if (!state) {
    //        PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
    //    }
    //    if (Checkbox(data->define.c_str(), &stateDefine)) {
    //        updateState();
    //        g.queueRedraw();
    //    }
    //    if (!state) {
    //        PopStyleVar();
    //    }

    renderNestedOption([&]() {
        if (!state) {
            PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
        }
        if (Checkbox(data->define.c_str(), &stateDefine)) {
            updateState();
            g.queueRedraw();
        }
        if (!state) {
            PopStyleVar();
        }
    });

    //    ImVec2 p2 = { p1.x, p1.y + size.y / 2 };
    //    ImVec2 p3 = { p2.x + size.y, p2.y };
    //
    //    window->DrawList->AddLine(p1, p2, color, width);
    //    window->DrawList->AddLine(p2, p3, color, width);
    //
    //    SetWindowFontScale(1.f);
}

void vkgen::GUI::BoolDefineGUI::updateState() {
    if (state) {
        if (stateDefine) {
            data->state = Define::COND_ENABLED;
        } else {
            data->state = Define::ENABLED;
        }
    } else {
        data->state = Define::DISABLED;
    }
    // std::cout << "[" << data->define << "] is " << (int) data->state << "\n";
}

void vkgen::GUI::guiBoolOption(BoolGUI &data) {
    // SetWindowFontScale(0.9f);

    Checkbox(data.text.c_str(), data.data);

    // SetWindowFontScale(1.f);
}

template <typename T>
void filterCollection(std::vector<std::reference_wrapper<T>> &data, const std::regex &rgx, std::atomic_bool &abort) {
    if (abort) {
        return;
    }
    for (T &c : data) {
        c.filtered = std::regex_search(c.name.original, rgx);
        //        if (std::regex_search(c.name, rgx)) {
        //            std::cout << "  " << c.name << "\n";
        //        }
        if (abort) {
            break;
        }
    }
}

void vkgen::GUI::filterTask(std::string filter) noexcept {
    filterTaskRunning = true;
    try {
        filterTaskError.clear();
        const std::regex rgx(filter, std::regex_constants::icase);

        // std::cout << "filter: " << filter << "\n";

        auto &enums      = *collection.enums.data;
        auto &structs    = *collection.structs.data;
        auto &commands   = *collection.commands.data;
        auto &handles    = *collection.handles.data;
        auto &platforms  = *collection.platforms.data;
        auto &extensions = *collection.extensions.data;

        filterCollection(enums, rgx, filterTaskAbort);
        filterCollection(structs, rgx, filterTaskAbort);
        filterCollection(commands, rgx, filterTaskAbort);
        filterCollection(handles, rgx, filterTaskAbort);
        filterCollection(platforms, rgx, filterTaskAbort);
        filterCollection(extensions, rgx, filterTaskAbort);

        if (!filterTaskAbort) {
            filterSynced      = true;
            filterTaskFinised = true;
        }
    }
    catch (std::regex_error &e) {
        filterTaskError = e.what();
    }
    catch (std::runtime_error &e) {
        std::cerr << "Task exception: " << e.what() << '\n';
    }
    queueRedraw();
    filterTaskRunning = false;
}

void vkgen::Window::queueRedraw() {
    if (glfwWaiting) {
        glfwPostEmptyEvent();
    } else {
        redraw = true;
    }
}

void vkgen::GUI::onLoad() {
    collection.platforms.data  = &gen->getPlatforms().ordered;
    collection.extensions.data = &gen->getExtensions().ordered;
    collection.handles.data    = &gen->getHandles().ordered;
    collection.structs.data    = &gen->getStructs().ordered;
    collection.enums.data      = &gen->getEnums().ordered;
    collection.commands.data   = &gen->getCommands().ordered;
}

vkgen::GUI::GUI(vkgen::Generator &gen) {
    this->gen      = &gen;
    physicalDevice = VK_NULL_HANDLE;

    collection.platforms.name  = "Platforms";
    collection.extensions.name = "Extensions";
    collection.commands.name   = "Commands";
    collection.handles.name    = "Handles";
    collection.structs.name    = "Struct";
    collection.enums.name      = "Enums";

    float size = 1.f;
    // loadRegButton.size    = size;
    unloadRegButton.size  = size;
    generateButton.size   = size;
    loadConfigButton.size = size;
    saveConfigButton.size = size;

    auto standardSelector  = std::make_unique<LevelRenderable<StandardSelector>>(Level::L2, gen);
    this->standardSelector = standardSelector.get();

    auto &cfg    = gen.getConfig();
    tableGeneral = std::make_unique<RenderableTable<3>>(
      "##TableNS",
      "General",
      0,
      std::make_unique<RenderableColumn<12>>(
        0,
        std::make_unique<RenderableText>("Code generation"),
        make_config_option(0, BoolGUI{ &cfg.gen.cppModules.data, "C++ modules" }, "generate c++20 modules"),
        // make_config_option(0, BoolGUI{&cfg.gen.exceptions.data, "exceptions"}, "enable vulkan exceptions"),
        make_config_option(0, BoolGUI{ &cfg.gen.raii.enabled.data, "RAII header" }, "generate vk::raii"),
        make_config_option(0, BoolGUI{ &cfg.gen.expApi.data, "Dynamic PFN linking" }, "PFN pointers will be added to Device and Instance"),
        make_config_option(Level::L2, 0, NestedOption<BoolGUI>{ &cfg.gen.integrateVma.data, "Integrate VMA" }, ""),
        make_config_option(Level::L2, 0, BoolGUI{ &cfg.gen.proxyPassByCopy.data, "Pass ArrayProxy as copy" }, ""),
        make_config_option(Level::L2, 0, BoolGUI{ &cfg.gen.unifiedException.data, "Unified exception" }, ""),
        make_config_option(Level::L2, 0, BoolGUI{ &cfg.gen.branchHint.data, "Branch hints" }, "Add compiler C++20 hints (likely, unlikely)"),
        std::make_unique<RenderableText>("Macros"),
        make_config_option(0, MacroGUI{ &cfg.macro.mNamespace.data }, ""),
        make_config_option(0, MacroGUI{ &cfg.macro.mNamespaceRAII.data }, ""),
        std::move(standardSelector)),
      std::make_unique<RenderableColumn<9>>(
        1,
        std::make_unique<RenderableText>("Handles"),
        make_config_option(0, BoolDefineGUI(&cfg.gen.handleConstructors.data, "constructors"), "TODO description"),
        make_config_option(0, BoolDefineGUI(&cfg.gen.smartHandles.data, "smart handles"), "TODO description"),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.handleTemplates.data, "template helpers"), "TODO description"),
        std::make_unique<RenderableText>("Functions"),
        make_config_option(0, BoolGUI{ &cfg.gen.dispatchParam.data, "Dispatch parameter" }, "TODO description"),
        make_config_option(0, BoolGUI{ &cfg.gen.allocatorParam.data, "Allocator parameter" }, "TODO description"),
        make_config_option(0, BoolGUI{ &cfg.gen.functionsVecAndArray.data, "Array & small vector variant" }, "TODO description"),
        make_config_option(Level::L1, 0, BitSelector{ cfg.gen.classMethods.data, 1, "Methods from subobjects" }, "")),
      std::make_unique<RenderableColumn<8>>(
        2,
        std::make_unique<RenderableText>("Structs & Unions"),
        make_config_option(0, BoolDefineGUI(&cfg.gen.structConstructors.data, "struct constructors"), "TODO description"),
        make_config_option(0, BoolDefineGUI(&cfg.gen.unionConstructors.data, "union constructors"), "TODO description"),
        make_config_option(0, BoolDefineGUI(&cfg.gen.structSetters.data, "struct setters"), "TODO description"),
        make_config_option(0, BoolDefineGUI(&cfg.gen.unionSetters.data, "union setters"), "TODO description"),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.structCompare.data, "compares"), "TODO description"),
        make_config_option(Level::L1, 0, NestedOption<BoolGUI>{ &cfg.gen.spaceshipOperator.data, "spaceship operator" }, "Enable conditional spaceship operator"),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.structReflect.data, "reflect"), "TODO description")));
}

void vkgen::GUI::init() {
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

    unloadRegButton.text = "Load registry";
    unloadRegButton.task = [&] { gen->unload(); };

    generateButton.text = "Generate";
    generateButton.task = [&] { gen->generate(); };

    loadConfigButton.text = "Import config";
    loadConfigButton.task = [&] { gen->loadConfigFile(configPath); };

    saveConfigButton.text = "Export config";
    saveConfigButton.task = [&] { gen->saveConfigFile(configPath); };

    gen->bindGUI([&] { onLoad(); });
}

void vkgen::GUI::setupImguiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    updateImgui();

    Render();
    imguiFrameBuilt = true;
}

void vkgen::GUI::showHelpMarker(const char *desc, const std::string &label) {
    SameLine();
    TextDisabled("[%s]", label.c_str());
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35.0f);
        TextUnformatted(desc);
        PopTextWrapPos();
        EndTooltip();
    }
}

void vkgen::GUI::updateImgui() {
    NewFrame();

    SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)), ImGuiCond_Always);

    bool mainw;
    Begin("Gen", &mainw, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (showFps) {
        int   fps   = 0;
        float delta = GetIO().DeltaTime;
        if (delta != 0) {
            fps = static_cast<int>(1.0 / delta);
        }
        Text("FPS %d, avg: %d", fps, static_cast<int>(GetIO().Framerate));
    }

    if (gen->isLoaded()) {
        mainScreen();
    } else {
        loadScreen();
    }

    ImGuiContext &g = *GImGui;
    // ImGuiIO& io = GetIO();
    if (menuOpened) {
        // queueRedraw();
        menuOpened = false;
    } else if (g.WantTextInputNextFrame == 1) {
        queueRedraw();
    }

    End();
}

void vkgen::GUI::mainScreen() {
    auto              &cfg = gen->getConfig();
    static std::string output{ gen->getOutputFilePath() };
    // static bool outputPathBad = !gen->isOuputFilepathValid();
    id = 0;
    Dummy(ImVec2(0.0f, 4.0f));
    unloadRegButton.draw();
    SameLine();
    std::string loadedText = "Current registry: " + gen->getRegistryPath();
    showHelpMarker(loadedText.c_str(), "i");
    SameLine();

    Dummy(ImVec2(0.0f, 4.0f));
    SameLine();
    generateButton.draw();
    SameLine();

    // Dummy(ImVec2(0.0f, 4.0f));
    Text("Output directory:");
    SameLine();
    //        if (outputBadFlag) {
    //            PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
    //        }
    PushID(id++);
    if (InputText("", &output)) {
        gen->setOutputFilePath(output);
        // outputPathBad = !gen->isOuputFilepathValid();
    }
    PopID();
    //        if (outputBadFlag) {
    //            PopStyleColor();
    //        }

    Dummy(ImVec2(0.0f, 10.0f));
    loadConfigButton.draw();
    SameLine();
    saveConfigButton.draw();
    SameLine();
    Text("Config path:");
    SameLine();

    PushID(id++);
    if (InputText("", &configPath)) {
    }
    PopID();

    Dummy(ImVec2(0.0f, 10.0f));

    SetWindowFontScale(1.f);
    if (CollapsingHeader("Configuration")) {
        if (Button("Simple")) {
            guiLevel = Level::L0;
            queueRedraw();
        }
        SameLine();
        if (Button("Advanced")) {
            guiLevel = Level::L1;
            queueRedraw();
        }
        SameLine();
        if (Button("Experimental")) {
            guiLevel = Level::L2;
            queueRedraw();
        }
        //        SameLine();
        //        Dummy(ImVec2(40.0f, 0.0f));

        PushID(id++);

        tableGeneral->render(*this);

        /*
        ImGuiTableFlags containerTableFlags = 0;

        static auto t3 = RenderableTable(
          "##TableG",
          "Generator debug",
          1,
          containerTableFlags,
          [&](GUI & g) {
              TableSetColumnIndex(0);
              static auto content = RenderableArray<1>({ std::make_unique<BoolGUI>(&cfg.dbg.methodTags.data, "Show function categories") });
              content.render(g);
          },
          true);
        t3.render(*this);
        */
        PopID();

        if (guiLevel >= Level::L1) {
            ImGui::Dummy({ 0, 16 });
            InputText("vk::Context class name", &cfg.gen.contextClassName.data);
        }

        if (Button("Load VulkanHPP preset")) {
            gen->loadConfigPreset();
        }

        ImGui::Dummy({ 0, 4 });
    }

    ImGuiTableFlags containerTableFlags = 0;
    containerTableFlags |= ImGuiTableFlags_Resizable;
    containerTableFlags |= ImGuiTableFlags_BordersOuter;
    containerTableFlags |= ImGuiTableFlags_ScrollY;

    if (CollapsingHeader("Detailed selection")) {
        if (BeginTable("##Table", 1, containerTableFlags)) {
            TableNextRow();
            TableSetColumnIndex(0);

            Dummy(ImVec2(0, 10));
            PushID(id++);
            Text("Filter");
            SameLine();
            bool filterChanged = InputText("", &filter);
            if (filterChanged) {
                // std::cout << "filter text: " << filter << '\n';
                if (filterTaskRunning) {
                    filterTaskAbort = true;
                } else {
                    filterSynced = false;
                    filterFuture = std::async(std::launch::async, &vkgen::GUI::filterTask, this, filter);
                }
            } else if (filterTaskAbort && !filterTaskRunning) {
                filterTaskAbort = false;
                filterSynced    = false;
                filterFuture    = std::async(std::launch::async, &vkgen::GUI::filterTask, this, filter);
            }

            bool error = !filterSynced && !filterTaskRunning && !filterTaskError.empty();
            if (error) {
                SameLine();
                PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
                Text("Bad regex: %s", filterTaskError.c_str());
                PopStyleColor();
            }

            PopID();

            if (filterSynced) {
                collection.draw(id, true);
            }

            EndTable();
        }
    }
}

static void showLoadEntry(vkgen::Generator *gen, const std::string &path) {
    if (!path.empty()) {
        Dummy(ImVec2(0, 10));
        Text("Found: %s", path.c_str());
        SameLine();
        if (Button("Load")) {
            gen->load(path);
        }
    }
}

void vkgen::GUI::loadScreen() {
    static std::string regInputText;
    static bool        regButtonDisabled = true;
    if (regButtonDisabled) {
        PushItemFlag(ImGuiItemFlags_Disabled, true);
        PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
    }
    if (Button("Load registry") && !regButtonDisabled) {
        gen->load(regInputText);
    }
    if (regButtonDisabled) {
        PopItemFlag();
        PopStyleVar();
    }

    SameLine();
    // PushID(100);
    if (InputText("", &regInputText)) {
        regButtonDisabled = regInputText.empty() || !std::filesystem::is_regular_file(regInputText);
        queueRedraw();
    }
    // PopID();
    if (regButtonDisabled) {
        DummySameLine(100, 0).render(*this);
        ImGui::Text("File does not exist.");
    }

    showLoadEntry(gen, Generator::findSystemRegistryPath());
    showLoadEntry(gen, Generator::findLocalRegistryPath());
}

void vkgen::GUI::setupCommandBuffer(VkCommandBuffer cmd) {
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass            = renderPass;
    renderPassInfo.framebuffer           = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset     = { 0, 0 };
    renderPassInfo.renderArea.extent     = swapChainExtent;

    VkClearValue clearColor        = { 0.0f, 0.0f, 0.0f, 1.0f };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (imguiFrameBuilt) {
        ImDrawData *data = GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(data, cmd);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void vkgen::GUI::drawFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    setupImguiFrame();
    setupCommandBuffer(commandBuffers[currentFrame]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore          waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[]     = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount         = 1;
    submitInfo.pWaitSemaphores            = waitSemaphores;
    submitInfo.pWaitDstStageMask          = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[]  = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vkgen::GUI::run() {
    double target = 1.0 / 60.0;
    double next   = 0.0;

    while (!glfwWindowShouldClose(glfwWindow)) {
        if (redraw) {
            redraw = false;
            glfwPollEvents();
            drawFrame();
        } else {
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

bool vkgen::GUI::drawContainerHeader(const char *name, SelectableGUI *s, bool empty, const std::string &info) {
    bool open = false;
    if (!empty) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
        if (s->selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        open = TreeNodeEx(name, flags);
        if (!info.empty()) {
            SameLine();
            TextDisabled("[i]");
            if (IsItemHovered()) {
                BeginTooltip();
                PushTextWrapPos(GetFontSize() * 100.0f);
                TextUnformatted(info.c_str());
                PopTextWrapPos();
                EndTooltip();
            }
        }

        if (IsItemClicked()) {
            if (GetIO().KeyCtrl) {
                s->selected = !s->selected;
            }
        }

        if (!vkgen::GUI::menuOpened && BeginPopupContextItem()) {
            Text("-- %s --", name);
            if (MenuItem("Select all")) {
                s->setSelected(true);
            }
            if (MenuItem("Deselect all")) {
                s->setSelected(false);
            }
            if (MenuItem("Enable selected")) {
                s->setEnabledChildren(true, true);
            }
            if (MenuItem("Disable selected")) {
                s->setEnabledChildren(false, true);
            }
            if (MenuItem("Enable all")) {
                s->setEnabledChildren(true);
            }
            if (MenuItem("Disable all")) {
                s->setEnabledChildren(false);
            }
            EndPopup();
            vkgen::GUI::menuOpened = true;
        }
    } else {
        float x = GetTreeNodeToLabelSpacing();
        SetCursorPosX(GetCursorPosX() + x);
        // drawSelectable(name, s);
    }
    return open;
}

void vkgen::GUI::setConfigPath(const std::string &path) {
    configPath = path;
}

static void drawSelectable(const char *text, vkgen::SelectableGUI *s) {
    if (s->hovered || s->selected) {
        auto   size = CalcTextSize(text);
        ImVec2 p    = GetCursorScreenPos();
        auto   clr  = ColorConvertFloat4ToU32(GetStyle().Colors[s->hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header]);
        auto   pad  = GetStyle().FramePadding;
        GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + size.x + pad.x * 2, p.y + size.y + pad.y * 2), clr);
    }
    Text("%s", text);
    s->hovered = IsItemHovered();
    if (IsItemClicked()) {
        if (GetIO().KeyCtrl) {
            s->selected = !s->selected;
        }
    }
}

static void draw(vkgen::BaseType *data, bool filterNested) {
    if (!data->filtered) {
        return;
    }
    bool supported = data->isSuppored() && data->version != nullptr;
    // std::cout << data->name << " " << supported << " " << (data->version? data->version : "NULL") << "\n";
    if (!supported) {
        return;
    }
    std::string disabler;
    //    if (data->vulkanSpec > 0) {
    //        disabler += "vk: " + std::to_string(data->vulkanSpec);
    //    }

    //    if (visualizeDisabled) {
    //        if (data->ext) {
    //            if (!data->ext->isEnabled()) {
    //                disabler = data->ext->name;
    //            }
    //            else if (data->ext->platform) {
    //                if (!data->ext->platform->isEnabled()) {
    //                    disabler = data->ext->platform->name;
    //                }
    //            }
    //        }
    //    }

    if (!disabler.empty()) {
        PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.6f);
    }

    std::string n = data->name.original;
    // PushID(id);
    Dummy(ImVec2(5.0f, 0.0f));
    SameLine();
    bool check = supported && data->isEnabled();

    // PushID(xid++);

    if (Checkbox("", &check)) {
        // std::cout << "set enabled: " << check << ": " << data->name << '\n';
        if (supported) {
            data->setEnabled(check);
        }
    }
    // PopID();

    SameLine();
    drawSelectable(n.c_str(), data);

    std::string text = "(" + data->metaTypeString() + ")\n";

    if (!disabler.empty()) {
        text += disabler + " is disabled\n\n";
    }
    text += "Requires:\n";
    for (const auto &d : data->dependencies) {
        text += "  " + d->name;
        text += " (" + d->metaTypeString() + ")";
        text += "\n";
    }
    if (!data->subscribers.empty()) {
        text += "\nRequired by:\n";
        for (const auto &d : data->subscribers) {
            text += "  " + d->name;
            text += " (" + d->metaTypeString() + ")";
            text += "\n";
        }
    }

    if (!text.empty()) {
        SameLine();
        TextDisabled("[?]");
        if (IsItemHovered()) {
            BeginTooltip();
            PushTextWrapPos(GetFontSize() * 100.0f);
            TextUnformatted(text.c_str());
            PopTextWrapPos();
            EndTooltip();
        }
    }

    // if ( !data->dependencies.empty()) {

    if (!disabler.empty()) {
        PopStyleVar();
    }
    // PopID();
}

template <typename T>
void vkgen::GUI::Container<T>::draw(int id, bool filterNested) {
    auto &elements = *this;
    //    if (Type::drawFiltered) {
    //        bool hasNone = true;
    //        for (size_t i = 0; i < elements.size(); i++) {
    //            bool f = false;
    //            if constexpr (std::is_pointer_v<T>) {
    //                f = elements[i]->filtered;
    //            } else {
    //                f = elements[i].filtered;
    //            }
    //            if (f) {
    //                hasNone = false;
    //                break;
    //            }
    //         }
    //         if (hasNone) {
    //            return;
    //         }
    //    }

    size_t total = 0;
    size_t cnt   = 0;
    //    for (size_t i = 0; i < elements.size(); i++) {
    //         if constexpr (std::is_pointer_v<T>) {
    //            if (elements[i]->data->isSuppored() && elements[i]->data->vulkanSpec > 0) {
    //                total++;
    //                if (elements[i]->data->isEnabled()) {
    //                    cnt++;
    //                }
    //            }
    //         } else {
    //            if (elements[i].data->isSuppored() && elements[i].data->vulkanSpec > 0) {
    //                total++;
    //                if (elements[i].data->isEnabled()) {
    //                    cnt++;
    //                }
    //            }
    //         }
    //    }
    std::string info;
    if (total > 0) {
        info = std::to_string(cnt) + " / " + std::to_string(total);
    }

    PushID(id);
    bool open = true;
    if (!name.empty()) {
        open = drawContainerHeader(name.c_str(), this, false, info);
    }
    if (open) {
        int i = 0;
        for (T &e : *this->data) {
            PushID(i++);
            ::draw(&e, filterNested);
            PopID();
        }
        TreePop();
    }
    PopID();
}

/*
void vkgen::GUI::Type::draw(int id) {
    if (Type::drawFiltered && !filtered) {
        return;
    }
    bool supported = data->isSuppored() && data->vulkanSpec != 0;
    if (!supported) {
        return;
    }
    std::string disabler;
//    if (data->vulkanSpec > 0) {
//        disabler += "vk: " + std::to_string(data->vulkanSpec);
//    }

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
//    if (!supported) {
//        disabler += "unsupported\n";
//    }

    if (!disabler.empty()) {
        PushStyleVar(ImGuiStyleVar_Alpha,
                            GetStyle().Alpha * 0.6f);
    }

    std::string n = data->name.original;
    PushID(id);
    Dummy(ImVec2(5.0f, 0.0f));
    SameLine();
    bool check = supported && data->isEnabled();

    PushID(xid++);

    if (Checkbox("", &check)) {
        // std::cout << "set enabled: " << check << ": " << data->name << '\n';
        if (supported) {
            data->setEnabled(check);
        }
    }
    PopID();

    SameLine();
    drawSelectable(n.c_str(), this);

    std::string text = "(" + data->metaTypeString() + ")\n";

    if (!disabler.empty()) {
        text += disabler + " is disabled\n\n";
    }
    text += "Requires:\n";
    for (const auto &d : data->dependencies) {
        text += "  " + d->name;
        text += " (" + d->metaTypeString() + ")";
        text += "\n";
    }
    if (!data->subscribers.empty()) {
        text += "\nRequired by:\n";
        for (const auto &d : data->subscribers) {
            text += "  " + d->name;
            text += " (" + d->metaTypeString() + ")";
            text += "\n";
        }
    }

    if (!text.empty()) {
        SameLine();
        TextDisabled("[?]");
        if (IsItemHovered()) {
            BeginTooltip();
            PushTextWrapPos(GetFontSize() * 100.0f);
            TextUnformatted(text.c_str());
            PopTextWrapPos();
            EndTooltip();
        }
    }

    // if ( !data->dependencies.empty()) {

    if (!disabler.empty()) {
        PopStyleVar();
    }
    PopID();
}

void vkgen::GUI::Extension::draw(int id) {
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

    PushID(id);
    if (drawContainerHeader(name, this, hasNone)) {
        this->commands.draw(0);
        TreePop();
    }
    PopID();
}

void vkgen::GUI::Platform::draw(int id) {
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
    PushID(id);
    if (drawContainerHeader(name, this, hasNone)) {
        this->extensions.draw(0);
        TreePop();
    }
    PopID();
}
*/

void vkgen::GUI::Collection::draw(int &id, bool filtered) {
    //    Type::visualizeDisabled = true;
    commands.draw(id++, true);
    handles.draw(id++, true);
    structs.draw(id++, true);
    enums.draw(id++, true);
    // Type::drawFiltered = filtered;
    // Type::visualizeDisabled = false;
    platforms.draw(id++, false);
    extensions.draw(id++, false);
}

void vkgen::GUI::AsyncButton::draw() {
    bool locked = running;
    if (locked) {
        PushItemFlag(ImGuiItemFlags_Disabled, true);
        PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.7f);
    }
    SetWindowFontScale(size);
    if (Button(text.c_str())) {
        run();
    }
    SetWindowFontScale(1.f);
    if (locked) {
        PopItemFlag();
        PopStyleVar();
    }
}

void vkgen::GUI::AsyncButton::run() {
    if (running) {
        return;
    }
    running = true;

    future = std::async(std::launch::async, [&] {
        try {
            task();
        }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
        }
        running = false;
    });
}
