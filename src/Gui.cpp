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

float nestPosX = 0.0;

struct Palette {
    ImVec4 text;
    ImVec4 back;
    ImVec4 button_bg;
    ImVec4 button_select;
    ImVec4 main;
    ImVec4 area;
    ImVec4 highlight;
};

static constexpr Palette PALETTE {
    .text = ImVec4(1.f, 1.f, 1.f, 1.f),
    .back = ImVec4(0.01f, 0.01f, 0.01f, 1.f),
    .button_bg = ImVec4(0.08f, 0.02f, 0.02f, 1.f),
    .button_select = ImVec4(0.1f, 0.02f, 0.02f, 1.f),
    .main = ImVec4(0.3f, 0.f, 0.f, 1.f),
    .area = ImVec4(0.05f, 0.04f, 0.04f, 1.f),
    .highlight = ImVec4(0.3f, .1f, .1f, 1.f),
};

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

    template <typename T>
    auto make_config_option(GUI::Level level, int x, const T &element) {
        return std::make_unique<GUI::LevelRenderable<GUI::RenderableArray<2>>>(
          level,
          GUI::RenderableArray<2>(
            { std::make_unique<GUI::DummySameLine>(static_cast<float>(x), 0.f), std::make_unique<T>(element) }));
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
    glfwWindow = glfwCreateWindow(width, height, "Vulkan C++ API generator", NULL, NULL);
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
    // layers.push_back("VK_LAYER_LUNARG_monitor");
    layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo instanceInfo{ .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                       .pApplicationInfo        = &appInfo,
                                       .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
                                       .ppEnabledLayerNames     = layers.data(),
                                       .enabledExtensionCount   = glfwExtensionCount,
                                       .ppEnabledExtensionNames = glfwExtensions };

    auto result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
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
    font        = io.Fonts->AddFontFromMemoryCompressedBase85TTF(Poppins_compressed_data_base85, 26.0);
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

    ImVec4 col_text = PALETTE.text;
    ImVec4 col_main = PALETTE.main;
    ImVec4 col_back = PALETTE.back;
    ImVec4 col_highlight = PALETTE.highlight;
    ImVec4 col_area = PALETTE.area;

    style.TabRounding = 4.f;
    style.FrameRounding = 4.f;
    style.GrabRounding = 4.f;
    style.WindowRounding = 4.f;
    style.PopupRounding = 4.f;

    style.Colors[ImGuiCol_Text]                 = col_text;
    style.Colors[ImGuiCol_TextDisabled]         = ImVec4(col_text.x, col_text.y, col_text.z, 0.6f);
    style.Colors[ImGuiCol_WindowBg]             = col_back;
    style.Colors[ImGuiCol_ChildBg]              = ImVec4(col_area.x, col_area.y, col_area.z, 0.00f);
    style.Colors[ImGuiCol_Border]               = ImVec4(col_text.x, col_text.y, col_text.z, 0.30f);
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]              = PALETTE.button_bg;
    style.Colors[ImGuiCol_FrameBgHovered]       = col_highlight;
    style.Colors[ImGuiCol_FrameBgActive]        = col_main;
    style.Colors[ImGuiCol_TitleBg]              = col_area;
    style.Colors[ImGuiCol_TitleBgCollapsed]     = col_area;
    style.Colors[ImGuiCol_TitleBgActive]        = col_area;
    style.Colors[ImGuiCol_MenuBarBg]            = col_area;
    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(col_back.x, col_back.y, col_back.z, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(col_main.x, col_main.y, col_main.z, 0.31f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_CheckMark]            = col_text;
    style.Colors[ImGuiCol_SliderGrab]           = ImVec4(col_main.x, col_main.y, col_main.z, 0.24f);
    style.Colors[ImGuiCol_SliderGrabActive]     = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_Button]               = PALETTE.button_bg;
    style.Colors[ImGuiCol_ButtonHovered]        = col_highlight;
    style.Colors[ImGuiCol_ButtonActive]         = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_Header]               = col_area;
    style.Colors[ImGuiCol_HeaderHovered]        = col_highlight;
    style.Colors[ImGuiCol_HeaderActive]         = col_area;
    style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(col_main.x, col_main.y, col_main.z, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(col_main.x, col_main.y, col_main.z, 0.78f);
    style.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_PlotLines]            = ImVec4(col_text.x, col_text.y, col_text.z, 0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(col_text.x, col_text.y, col_text.z, 0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(col_main.x, col_main.y, col_main.z, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(col_main.x, col_main.y, col_main.z, 0.6f);


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

    auto size = CalcTextSize("");
    size.y += 3.0;
    ImGui::Dummy({ 0.0, 0.0 });
    ImGui::SameLine();

    ImVec2 p1 = GetCursorPos();
    p1.x += size.y / 2;

    // nestPosX = p1.x;

    ImGui::Dummy({ size.y, 0 });
    ImGui::SameLine();

    SetWindowFontScale(0.8f);

    content();

    SetWindowFontScale(1.f);
    // nestPosX = 0.0;

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    auto color = ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0));
    auto width = 2.f;

    ImVec2 p2 = { p1.x, p1.y + size.y / 2 };
    ImVec2 p3 = { p2.x + size.y / 2, p2.y };

    window->DrawList->AddLine(p1, p2, color, width);
    window->DrawList->AddLine(p2, p3, color, width);

}


template <typename T>
void vkgen::GUI::NestedOption<T>::render(GUI &g) {

    auto size = CalcTextSize("");
    size.y += 3.0;
    // ImGui::Dummy({ 0.0, 0.0 });
    // ImGui::SameLine();

    ImVec2 p1 = GetCursorPos();
    p1.x += size.y / 2;
    // p1.x -= 7;

    // nestPosX = p1.x;

    ImGui::Dummy({ size.y, 0 });
    ImGui::SameLine();

    SetWindowFontScale(0.8f);

    T::render(g);

    SetWindowFontScale(1.f);
    // nestPosX = 0.0;

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    auto color = ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0));
    auto width = 2.f;

    ImVec2 p2 = { p1.x, p1.y + size.y / 2 };
    ImVec2 p3 = { p2.x + size.y / 2, p2.y };

    window->DrawList->AddLine(p1, p2, color, width);
    window->DrawList->AddLine(p2, p3, color, width);
}

// void vkgen::GUI::NestedOption::render(GUI &g) {
//     content(g);
// }

vkgen::GUI::BoolDefineGUI::BoolDefineGUI(Define *data, const std::string &text, const std::string &helpText) : data(data), text(text), help(helpText) {
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

    help.render(g);

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

    if (g.guiLevel > GUI::Level::L0 && data->state != Define::DISABLED) {
        //        renderNestedOption([&]() {
        ImGui::SameLine();
        if (!state) {
            PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
        }
        SetWindowFontScale(0.8f);

        if (Checkbox(data->define.c_str(), &stateDefine)) {
            updateState();
            g.queueRedraw();
        }
        if (!state) {
            PopStyleVar();
        }

        SetWindowFontScale(1.f);
//        });
    }

    //    ImVec2 p2 = { p1.x, p1.y + size.y / 2 };
    //    ImVec2 p3 = { p2.x + size.y, p2.y };
    //
    //    window->DrawList->AddLine(p1, p2, color, width);
    //    window->DrawList->AddLine(p2, p3, color, width);
    //
    //    SetWindowFontScale(1.f);
}

void vkgen::GUI::BoolDefineGUI::updateState() {
    if (data->state == Define::ENABLED) {
        data->state = Define::COND_ENABLED;
        stateDefine = true;
    }
    else if (data->state == Define::DISABLED) {
        data->state = Define::ENABLED;
        stateDefine = false;
    }
    else {
        data->state = Define::DISABLED;
        stateDefine = false;
    }

//    if (state) {
//        if (stateDefine) {
//            data->state = Define::COND_ENABLED;
//        } else {
//            data->state = Define::ENABLED;
//        }
//    } else {
//        data->state = Define::DISABLED;
//    }
    // std::cout << "[" << data->define << "] is " << (int) data->state << "\n";
}

void vkgen::GUI::guiBoolOption(BoolGUI &data) {
    // SetWindowFontScale(0.9f);

    Checkbox(data.text.c_str(), data.data);

    // SetWindowFontScale(1.f);
}

template <typename T>
void filterCollection(std::vector<std::reference_wrapper<vkgen::Extension>> &data, const std::regex &rgx, std::atomic_bool &abort) {
    if (abort) {
        return;
    }
    for (vkgen::Extension &c : data) {
        c.filtered = std::regex_search(c.name.original, rgx);

        for (const auto &cmd : c.commands) {
            c.filtered |= std::regex_search(cmd.get().name.original, rgx);
        }
        if (abort) {
            break;
        }
    }
}

template <typename T>
void filterCollection(std::vector<std::reference_wrapper<T>> &data, const std::regex &rgx, std::atomic_bool &abort) {
    if (abort) {
        return;
    }
    for (T &c : data) {
        c.filtered = std::regex_search(c.name.original, rgx);
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
    collection.features.data   = &gen->getFeatures().ordered;
    collection.handles.data    = &gen->getHandles().ordered;
    collection.structs.data    = &gen->getStructs().ordered;
    collection.enums.data      = &gen->getEnums().ordered;
    collection.commands.data   = &gen->getCommands().ordered;
#ifdef GENERATOR_TOOL
    vkgen::tools::init(*gen);
#endif
}

vkgen::GUI::GUI(vkgen::Generator &gen) {
    this->gen      = &gen;
    physicalDevice = VK_NULL_HANDLE;

    collection.platforms.name  = "Platforms";
    collection.extensions.name = "Extensions";
    collection.features.name   = "Features";
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

    standardSelector = ButtonGroup("Minimum standard", {"c++11", "c++17", "c++20"});
    switch (gen.cfg.gen.cppStd.data) {
        case 11:
            standardSelector.index = 0;
            break;
        case 17:
            standardSelector.index = 1;
            break;
        case 20:
            standardSelector.index = 2;
            break;
        default:
            break;
    }
    standardSelector.onChange = [&](int index) {
        switch (index) {
            case 0:
                gen.cfg.gen.cppStd.data = 11;
                break;
            case 1:
                gen.cfg.gen.cppStd.data = 17;
                break;
            case 2:
                gen.cfg.gen.cppStd.data = 20;
                break;
            default:
                break;
        }
    };


    guiLevelSelector = ButtonGroup("", {"Simple", "Advanced", "Experimental"});
    guiLevelSelector.onChange = [&](int index) {
        if (index >= Level::L0 && index <= Level::L2) {
            guiLevel = static_cast<GUI::Level>(index);
        }
    };

    auto &cfg    = gen.getConfig();
    tableGeneral = std::make_unique<RenderableTable<3>>(
      "##TableNS",
      "General",
      0,
      std::make_unique<RenderableColumn<11>>(
        0,
        std::make_unique<RenderableText>("Variant"),
        make_config_option(0, BoolGUI{ &cfg.gen.globalMode.data, "vkg mode" }, "Vulkan with global functions"),
        std::make_unique<RenderableText>("Code generation"),
        make_config_option(0, BoolGUI{ &cfg.gen.cppModules.data, "C++ module" }, "Generate C++20 module (vulkan.cppm)"),
        make_config_option(0, BoolGUI{ &cfg.gen.functionsVecAndArray.data, "Small vector" }, "Functions returning vk::Vector instead of std::vector"),
        make_config_option(Level::L2, BoolGUI{ &cfg.gen.raii.enabled.data, "RAII header" }, "Generate vk::raii header (vulkan_raii.hpp)"),
        make_config_option(0, BoolGUI{ &cfg.gen.expandMacros.data, "Expand macros" }, "Expand preprocessor macros whenever possible"),
        // make_config_option(0, BoolGUI{&cfg.gen.exceptions.data, "exceptions"}, "enable vulkan exceptions"),
        // make_config_option(0, BoolGUI{ &cfg.gen.expApi.data, "Dynamic PFN linking" }, "PFN dispatcher will be embedded to Device and Instance"),
        make_config_option(Level::L2, 0, NestedOption<BoolGUI>{ &cfg.gen.integrateVma.data, "Integrate VMA" }, "PFN dispatcher can be used with VMA"),
        make_config_option(Level::L2, 0, BoolGUI{ &cfg.gen.proxyPassByCopy.data, "Pass ArrayProxy as copy" }, "Pass ArrayProxy parameter as copy instead of reference"),
        make_config_option(Level::L2, 0, BoolGUI{ &cfg.gen.unifiedException.data, "Unified exception" }, "Generates only vk::Error exeption"),
        make_config_option(Level::L2, 0, BoolGUI{ &cfg.gen.branchHint.data, "Branch hints" }, "Add compiler C++20 hints (likely, unlikely)")),
      std::make_unique<RenderableColumn<9>>(
        1,
        std::make_unique<RenderableText>("Handles"),
        make_config_option(Level::L0, 0, BoolDefineGUI(&cfg.gen.handleConstructors.data, "constructors", "Generates handle constructors")),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.smartHandles.data, "smart handles", "Generates smart (unique) handles")),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.handleTemplates.data, "type traits", "Generates CppType and isVulkanHandleType")),
        std::make_unique<RenderableText>("Functions"),
        make_config_option(0, BoolGUI{ &cfg.gen.dispatchParam.data, "Dispatch parameter" }, "Generates dispatch template parameter"),
        make_config_option(0, BoolGUI{ &cfg.gen.allocatorParam.data, "Allocator parameter" }, "Generates allocationCallbacks parameter"),
        make_config_option(0, BoolGUI{ &cfg.gen.extendedFunctions.data, "Extended functions" }, "Generates throwing and nothrowing variants where applicable"),
        make_config_option(Level::L2, 0, BitSelector{ cfg.gen.classMethods.data, 1, "Methods from subobjects" }, "Methods from subobjects will be added to top level handle")),
      std::make_unique<RenderableColumn<9>>(
        2,
        Level::L1,
        std::make_unique<RenderableText>("C++ Structs"),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.structConstructors.data, "struct constructors", "Generates struct constructors")),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.structSetters.data, "struct setters", "Generates struct setter functions")),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.structCompare.data, "compares", "Generates struct compare operator")),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.structReflect.data, "reflect", "Generates struct reflection")),
        make_config_option(Level::L1, 0, NestedOption<BoolGUI>{ &cfg.gen.spaceshipOperator.data, "spaceship operator" }, "Generates spaceship operator from API"),
        std::make_unique<RenderableText>("C++ Unions"),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.unionConstructors.data, "union constructors","Generates union constructors")),
        make_config_option(Level::L1, 0, BoolDefineGUI(&cfg.gen.unionSetters.data, "union setters", "Generates union setter functions")))
      );

    tableMacros = std::make_unique<RenderableTable<1>>(
      "##TableMacros",
      "Macros",
      0,
      std::make_unique<RenderableColumn<3>>(
        0,
        // std::make_unique<RenderableText>("Macros"),
        make_config_option(0, BoolGUI{ &cfg.gen.expandMacros.data, "Expand macros" }, "Expand preprocessor macros whenever possible"),
        make_config_option(0, MacroGUI{ &cfg.macro.mNamespace.data }, ""),
        make_config_option(0, MacroGUI{ &cfg.macro.mNamespaceRAII.data }, ""))
    );
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
#ifdef GENERATOR_TOOL
        if (showToolScreen) {
            ImGui::Dummy({0, 1});
            unloadRegButton.render(*this);
            SameLine();
            std::string loadedText = "Current registry: " + gen->getRegistryPath();
            showHelpMarker(loadedText.c_str(), "i");

            toolScreen();
        }
        else {
            mainScreen();
        }
#else
        mainScreen();
#endif
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
    float spacing = 1;
    ImGui::Dummy({0, spacing});
    unloadRegButton.render(*this);
    SameLine();
    std::string loadedText = "Current registry: " + gen->getRegistryPath();
    showHelpMarker(loadedText.c_str(), "i");
    SameLine();

    ImGui::Dummy({0, spacing});
    SameLine();
    generateButton.render(*this);
    SameLine();

    // Dummy(ImVec2(0.0f, 4.0f));
    Text("Output directory:");
    SameLine();
    //        if (outputBadFlag) {
    //            PushStyleColor(ImGuiCol_Text, IM_COL32(255, 134, 0, 255));
    //        }
    PushID(id++);
    PushItemWidth(450);
    if (InputText("", &output)) {
        gen->setOutputFilePath(output);
        // outputPathBad = !gen->isOuputFilepathValid();
    }
    PopItemWidth();
    PopID();
    //        if (outputBadFlag) {
    //            PopStyleColor();
    //        }
    ImGui::Dummy({0, spacing});
    loadConfigButton.render(*this);
    SameLine();
    saveConfigButton.render(*this);
    SameLine();
    Text("Config path:");
    SameLine();

    PushID(id++);
    PushItemWidth(450);
    if (InputText("", &configPath)) {
    }
    PopItemWidth();
    PopID();
    ImGui::Dummy({ 0, spacing });

    SetWindowFontScale(1.f);
    if (CollapsingHeader("Configuration")) {
        ImGui::Dummy({ 0, 4 });
        guiLevelSelector.render(*this);
        ImGui::SameLine(0, 100);
        standardSelector.render(*this);

        PushID(id++);

        tableGeneral->render(*this);
        tableMacros->render(*this);

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
            ImGui::Dummy({ 0, 4 });
            PushItemWidth(200);
            InputText("Namespace", &cfg.macro.mNamespace.data.value);
            InputText("vk::Context name", &cfg.gen.contextClassName.data);
            InputText("Module name", &cfg.gen.moduleName.data);
            PopItemWidth();
        }

        if (Button("Load VulkanHPP preset")) {
            gen->loadConfigPreset();
        }

        ImGui::Dummy({ 0, 4 });
    }

    ImGui::Dummy({ 0, spacing });

    ImGuiTableFlags containerTableFlags = 0;
    containerTableFlags |= ImGuiTableFlags_Resizable;
    containerTableFlags |= ImGuiTableFlags_BordersOuter;
    containerTableFlags |= ImGuiTableFlags_ScrollY;

    if (CollapsingHeader("Subset selection")) {
        if (BeginTable("##Table", 1, containerTableFlags)) {
            TableNextRow();
            TableSetColumnIndex(0);

            ImGui::Dummy({0, 4});
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

static int showLoadEntry(vkgen::Generator *gen, const std::string &path) {
    if (!path.empty()) {
        Dummy(ImVec2(0, 10));
        Text("Found: %s", path.c_str());
        SameLine();
        if (Button("Load")) {
            gen->load(path);
        }
        return 1;
    }
    return 0;
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
        // DummySameLine(100, 0).render(*this);
        // ImGui::Text("File does not exist.");
    }

    int entries = 0;
    entries += showLoadEntry(gen, Generator::getSystemRegistryPath());
    entries += showLoadEntry(gen, Generator::getLocalRegistryPath());
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
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
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
    double target = 1.0 / 30.0;
    double next   = 0.0;

    while (!glfwWindowShouldClose(glfwWindow)) {
        auto t = glfwGetTime();
        if (redraw) {
            glfwPollEvents();
            if (t >= next) {
                redraw = false;
                drawFrame();
                next = t + target;
            }
        } else {
            glfwWaiting = true;
            glfwWaitEvents();
            glfwWaiting = false;
        }
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

        if (!vkgen::GUI::menuOpened && BeginPopupContextItem("options")) {
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


static void draw(vkgen::Feature *data, bool filterNested) {

    if (!data->filtered) {
        return;
    }
    bool supported = data->isSupported(); // && data->version != nullptr;
    // std::cout << data->name << " " << supported << " " << (data->version? data->version : "NULL") << "\n";
    if (!supported) {
        return;
    }

    const auto &name = data->name;
    bool open = false;

    bool check = data->isEnabled();
    if (Checkbox("", &check)) {
        // std::cout << "set enabled: " << check << ": " << data->name << '\n';
        data->setEnabled(check);
    }

    if (!name.empty()) {
        // open = drawContainerHeader(name.c_str(), this, false, info);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
        SameLine();
        open = TreeNodeEx(name.c_str(), flags);
        std::string info;

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


    }
    if (open) {
        int i = 0;
        for (const vkgen::Command &c : data->commands) {
            PushID(i++);
            // ::draw(&e, filterNested);
            // + " (" + c.metaTypeString() + "), " + (c.version? c.version : "-")
            std::string text = c.name.original + "\n";
            ImGui::Text("%s", text.c_str());
            PopID();
        }

        for (vkgen::GenericType &t : data->promotedTypes) {
            PushID(i++);
            // ::draw(&e, filterNested);
            // + " (" + c.metaTypeString() + "), " + (c.version? c.version : "-")
            std::string text = t.name.original + " (p)\n";
            ImGui::Text("%s", text.c_str());
            PopID();
        }
//        for (vkgen::GenericType &t : data->types) {
//            PushID(i++);
//            // ::draw(&e, filterNested);
//            std::string text = t.name + " (" + t.metaTypeString() + "), " + (t.version? t.version : "-") +"\n";
//            ImGui::Text("%s", text.c_str());
//            PopID();
//        }
        TreePop();
    }

}

static void draw(vkgen::Extension *data, bool filterNested) {

    if (!data->filtered) {
        return;
    }
    bool supported = data->isSupported(); //&& data->version != nullptr;
    // std::cout << data->name << " " << supported << " " << (data->version? data->version : "NULL") << "\n";
    if (!supported) {
        return;
    }

    const auto &name = data->name;
    bool open = false;

    bool check = data->isEnabled();
    if (Checkbox("", &check)) {
        // std::cout << "set enabled: " << check << ": " << data->name << '\n';
        data->setEnabled(check);
    }

    if (!name.empty()) {
        // open = drawContainerHeader(name.c_str(), this, false, info);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
        SameLine();
        open = TreeNodeEx(name.c_str(), flags);
        std::string info;
        if (data->platform) {
            info += "platform: ";
            info += data->platform->name;
        }

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


    }
    if (open) {
        int i = 0;
        for (const vkgen::Command &c : data->commands) {
            PushID(i++);
            // ::draw(&e, filterNested);
            std::string text = c.name.original + "\n";
            ImGui::Text("%s", text.c_str());
            PopID();
        }
//        for (vkgen::GenericType &t : data->types) {
//            PushID(i++);
//            // ::draw(&e, filterNested);
//            std::string text = t.name + " (" + t.metaTypeString() + "), " + (t.version? t.version : "-") +"\n";
//            ImGui::Text("%s", text.c_str());
//            PopID();
//        }
        TreePop();
    }

}

static void draw(vkgen::GenericType *data, bool filterNested) {
    if (!data->filtered) {
        return;
    }
    bool supported = data->isSupported(); // && data->version != nullptr;
    // std::cout << data->name << " " << supported << " " << (data->version? data->version : "NULL") << "\n";
//    if (!supported) {
//        return;
//    }
    std::string disabler;
    if (!supported) {
        disabler += "is not supported";
    }
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
    if (data->version) {
        n += ", ";
        n += data->version;
    }
    if (!data->tempversion.empty()) {
        n += ", ";
        n += data->tempversion;
    }
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

    std::string text;
    auto* feat = data->getFeature();
    if (feat) {
        text += feat->name.original;
        text += "\n";
    }

    auto* ext = data->getExtension();
    if (ext) {
        text += "Extension: ";
        text += ext->name.original;
        text += "\n";
    }

    if (!disabler.empty()) {
        text += disabler + " is disabled\n\n";
    }
    text += "Requires:\n";
    for (const auto &d : data->dependencies) {
        text += "  " + d->name.original;
        // text += " (" + d->metaTypeString() + ")";
        text += "\n";
    }
    if (!data->subscribers.empty()) {
        text += "\nRequired by:\n";
        for (const auto &d : data->subscribers) {
            text += "  " + d->name.original;
            // text += " (" + d->metaTypeString() + ")";
            text += "\n";
        }
    }
    text += "[" + data->metaTypeString() + "]\n";

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
    auto &elements = *this->data;
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
    for (size_t i = 0; i < elements.size(); i++) {
         if constexpr (std::is_pointer_v<T>) {
            if (elements[i]->isSuppored()) {
                total++;
                if (elements[i]->isEnabled()) {
                    cnt++;
                }
            }
         } else {
            if (elements[i].get().isSupported()) {
                total++;
                if (elements[i].get().isEnabled()) {
                    cnt++;
                }
            }
         }
    }
    std::string info;
    if (total > 0) {
        info = std::to_string(cnt) + " / " + std::to_string(total);
    }

    PushID(id);
    if (!name.empty()) {
        std::string n = name;
        n += "  ";
        n += std::to_string(cnt);
        n += " / ";
        n += std::to_string(total);
        open = drawContainerHeader(n.c_str(), this, false, info);
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

void vkgen::GUI::Collection::draw(int &id, bool filtered) {
    //    Type::visualizeDisabled = true;
    commands.draw(id++, true);
    handles.draw(id++, true);
    structs.draw(id++, true);
    enums.draw(id++, true);
    // Type::drawFiltered = filtered;
    // Type::visualizeDisabled = false;
    platforms.draw(id++, false);
    features.draw(id++, false);
    extensions.draw(id++, false);
}

void vkgen::GUI::ButtonGroup::render(GUI &g) {
    ImGui::Text("%s", label.c_str());
    // ImGui::BeginGroup();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
    for (int i = 0; i < buttons.size(); ++i) {
        if (i == index) {
            ImGui::PushStyleColor(ImGuiCol_Button, PALETTE.button_select);
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, PALETTE.area);
        }
        if (i == 0) {
            ImGui::SameLine();
        }
        else {
            ImGui::SameLine(0, 0);
        }
        buttons[i].render(g);
        ImGui::PopStyleColor();
        if (buttons[i]) {
            index = i;
            if (onChange) {
                onChange(i);
            }
            g.queueRedraw();
        }
    }
    ImGui::PopStyleVar();

    // ImGui::EndGroup();
}

void vkgen::GUI::AsyncButton::render(GUI &g) {
    bool locked = running;
    if (locked) {
        PushItemFlag(ImGuiItemFlags_Disabled, true);
        PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.7f);
    }
    SetWindowFontScale(size);
    Button::render(g);
    if (Button::state) {
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
