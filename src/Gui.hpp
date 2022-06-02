#ifndef GUI_HPP
#define GUI_HPP

#include <fstream>
#include <optional>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include "Generator.hpp"

class GUI {

    struct MacroGUI {
        Generator::Macro *data;
        std::string text;

        MacroGUI(Generator::Macro *data, std::string text) : data(data), text(text) {
        }
    };

    struct BoolGUI {
        bool *data;
        std::string text;

        BoolGUI(bool *data, std::string text) : data(data), text(text) {
        }
    };

    class Window {
        public:
        std::function<void(Window*)> onClose;
        std::function<void(Window*, int, int)> onSize;
    };

    Generator &gen;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    const int MAX_FRAMES_IN_FLIGHT = 3;

    int id;
    int width, height;
    GLFWwindow *glfwWindow;
    Window window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    QueueFamilyIndices indices;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipelineCache pipelineCache;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkDescriptorSetLayout descriptorSetLayout;

    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<VkDescriptorSet> videoDescriptorSets;

    VkDescriptorPool descriptorPool;

    VkViewport viewport;
    VkRect2D scissor;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
    uint32_t imageIndex = 0;

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);
        return indices.isComplete();
    };    

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device,
                                                  VkSurfaceKHR surface);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                GLFWwindow *window);

    static void checkLayerSupport(const std::vector<const char *> &layers);

    static std::vector<char> readFile(const std::string &filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    VkShaderModule createShaderModule(const uint32_t *code, uint32_t codeSize);

    void createWindow();

    void createInstance();

    void createSurface();

    void pickPhysicalDevice();

    void createDevice();

    void createSwapChain(VkSwapchainKHR old);

    void createImageViews();

    void createRenderPass();

    void createPipelineCache();

    void createShaderModules();

    void createGraphicsPipeline();

    void createFramebuffers();

    void createCommandPool();

    void createCommandBuffers();

    void createDescriptorSetLayout();

    void initImgui();

    void createSyncObjects();

    void cleanupSwapChain();

    void recreateSwapChain();

    void cleanup();

    void initImguiStyle();

    void guiInputText(std::string &data);

    void guiMacroOption(MacroGUI &data);

    void guiBoolOption(BoolGUI &data);

  public:
    GUI(Generator &gen) : gen(gen) { physicalDevice = VK_NULL_HANDLE; }

    void init();

    bool imguiFrameBuilt = false;

    void setupImguiFrame();

    static void HelpMarker(const char *desc);

    void updateImgui();

    void setupCommandBuffer(VkCommandBuffer cmd);

    void drawFrame();

    void run();
};

#endif // GUI_HPP
