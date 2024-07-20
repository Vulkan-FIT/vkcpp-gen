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

#ifndef GUI_HPP
#define GUI_HPP

#include <fstream>
#include <future>
#include <optional>
#include <thread>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "Generator.hpp"
#include "imgui.h"

namespace vkgen
{

    class Window
    {
      protected:
        std::atomic_bool redraw      = false;
        std::atomic_bool glfwWaiting = false;
        void             queueRedraw();

      public:
        std::function<void(Window *)>           onClose;
        std::function<void(Window *, int, int)> onSize;

        void onKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
            queueRedraw();
        }
    };

    class GUI : public Window
    {
      public:
        enum Level
        {
            L0,
            L1,
            L2,
        };

        struct Renderable
        {
            virtual ~Renderable() {}

            virtual void render(GUI &g) = 0;
        };

        template <size_t N>
        struct RenderableArray : public Renderable
        {
            std::array<std::unique_ptr<Renderable>, N> items;

            template <typename... T>
            RenderableArray(T &&...t) : items({ std::forward<T>(t)... }) {}

            virtual void render(GUI &g) override {
                for (const auto &i : items) {
                    i->render(g);
                }
            }
        };

        struct RenderableVector : public Renderable
        {
            std::vector<std::unique_ptr<Renderable>> items;

            template <typename... T>
            RenderableVector(T &&...t) : items({ std::forward<T>(t)... }) {}

            virtual void render(GUI &g) override {
                for (const auto &i : items) {
                    i->render(g);
                }
            }
        };

        struct RenderableGeneric : public Renderable
        {
            std::function<void(GUI &g)> lambda;

            RenderableGeneric(std::function<void(GUI &g)> lambda) : lambda(lambda) {}

            virtual void render(GUI &g) override {
                lambda(g);
            }
        };

        struct RenderableText : public Renderable
        {
            std::string text;

            RenderableText(const std::string &text) : text(text) {}

            void render(GUI &g) override {
                ImGui::Text("%s", text.c_str());
            }
        };

        template <size_t N>
        struct RenderableColumn : public Renderable
        {
            int                column;
            RenderableArray<N> data;

            //            template <typename... T>
            //            RenderableColumn(int column, const std::string &text, T &&... t)
            //              : column(column), data({ std::make_unique<RenderableText>(text), std::forward<T>(t)... })
            //            {}

            template <typename... T>
            RenderableColumn(int column, T &&...t) : column(column), data({ std::forward<T>(t)... }) {}

            void render(GUI &g) override {
                ImGui::TableSetColumnIndex(column);
                data.render(g);
            }
        };

        template <size_t N>
        struct RenderableTable : public Renderable
        {
            std::string str_id;
            std::string text;
            // int             columns;
            ImGuiTableFlags                            flags;
            bool                                       advancedOnly;
            std::array<std::unique_ptr<Renderable>, N> data;

            template <typename... T>
            RenderableTable(const std::string &str_id,
                            const std::string &text,
                            // int                          columns,
                            ImGuiTableFlags flags,
                            // std::function<void(GUI & g)> lambda,
                            // bool                         advancedOnly = false
                            T &&...t)
              :  // RenderableGeneric(lambda),
              str_id(str_id)
              , text(text)
              ,
              // columns(columns),
              flags(flags)
              ,
              // advancedOnly(advancedOnly)
              data({ std::forward<T>(t)... }) {}

            void render(GUI &g) override {
                //                if (advancedOnly && !GUI::advancedMode) {
                //                    return;
                //                }
                if (ImGui::BeginTable(str_id.c_str(), 1, ImGuiTableFlags_BordersOuter)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    if (ImGui::TreeNode(text.c_str())) {
                        if (ImGui::BeginTable((str_id).c_str(), N, flags)) {
                            ImGui::TableNextRow();

                            for (auto &i : data) {
                                assert(i && "column not initialized");
                                i->render(g);
                            }
                            // lambda(g);

                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::EndTable();
                }
            }
        };

        template <class A, class B>
        struct RenderPair : public Renderable
        {
            RenderPair(A a, B b) : a(std::move(a)), b(std::move(b)) {}

            virtual void render(GUI &g) override {
                a.render(g);
                b.render(g);
            }

            A a;
            B b;
        };

        struct SameLineEmpty : public Renderable
        {
            virtual void render(GUI &g) override {
                ImGui::SameLine();
            }
        };

        struct Dummy : public Renderable
        {
            ImVec2 size;

            Dummy(const ImVec2 &size) : size(size) {}

            Dummy(float w, float h) : size(w, h) {}

            virtual void render(GUI &g) override {
                ImGui::Dummy(size);
            }
        };

        struct DummySameLine : public Dummy
        {
            DummySameLine(const ImVec2 &size) : Dummy(size) {}

            DummySameLine(float w, float h) : Dummy(w, h) {}

            virtual void render(GUI &g) override {
                Dummy::render(g);
                ImGui::SameLine();
            }
        };

        struct HelpMarker : public Renderable
        {
            std::string text;

            HelpMarker(const std::string &text) : text(text) {}

            virtual void render(GUI &g) override {
                if (!text.empty()) {
                    GUI::showHelpMarker(text.c_str());
                }
            }
        };

        struct BoolGUI : public Renderable
        {
            bool       *data;
            std::string text;

            BoolGUI(bool *data, const std::string &text) : data(data), text(text) {}

            void render(GUI &g) override {
                GUI::guiBoolOption(*this);
            }
        };


        template <typename T>
        struct NestedOption : public T
        {

            template <typename... Args>
            NestedOption(Args &&...args) : T(std::forward<Args>(args)...) {}

            void render(GUI &g) override;
        };

        struct BoolDefineGUI : public Renderable
        {
            Define     *data;
            std::string text;

            bool state;
            bool stateDefine;
            // NestedOption define;

            BoolDefineGUI(Define *data, const std::string &text);

            void render(GUI &g) override;

          private:
            void updateState();
        };

        struct MacroGUI : public Renderable
        {
            Macro *data;

            MacroGUI(Macro *data) : data(data) {}

            void render(GUI &g) override {
                ImGui::PushID(g.id++);
                ImGui::Checkbox("", &data->usesDefine);
                ImGui::SameLine();
                ImGui::Text("%s", data->define.c_str());
                ImGui::PopID();
            }
        };

        //        struct AdvancedBoolGUI : public BoolGUI
        //        {
        //            AdvancedBoolGUI(bool *data, std::string text) : BoolGUI(data, text) {}
        //
        //            void render(GUI &g) override {
        //                if (GUI::advancedMode) {
        //                    GUI::guiBoolOption(*this);
        //                } else {
        //                    ImGui::Text("%s", text.c_str());
        //                }
        //            }
        //        };

        //        struct AdvancedOnlyGUI : public BoolGUI
        //        {
        //            AdvancedOnlyGUI(bool *data, std::string text) : BoolGUI(data, text) {}
        //
        //            virtual void render(GUI &g) override {
        //                if (GUI::advancedMode) {
        //                    GUI::guiBoolOption(*this);
        //                    ImGui::Text("%s", text.c_str());
        //                }
        //            }
        //        };

        struct EditableMacroGUI : public Renderable
        {
            Macro      *data;
            std::string text;

            EditableMacroGUI(Macro *data, std::string text) : data(data), text(text) {}

            virtual void render(GUI &g) override {
                r(g, *data);
            }

          private:
            void r(GUI &g, Macro &m) {
                ImGui::Text("%s", text.c_str());
                ImGui::SameLine();

                bool disabled = !m.usesDefine;

                if (disabled) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                }

                ImGui::Checkbox("#define ", &m.usesDefine);
                if (m.usesDefine) {
                    ImGui::SameLine();
                    g.guiInputText(m.define);
                }

                if (disabled) {
                    ImGui::PopStyleVar();
                }

                ImGui::SameLine();
                g.guiInputText(m.value);
            }
        };

        struct BitSelector : public Renderable
        {
            int        &data;
            int         mask;
            bool        state;
            std::string text;

            BitSelector(int &data, int bit, const std::string &text) : data(data), text(text) {
                mask  = 1 << bit;
                state = data & mask;
            }

            void render(GUI &g) override {
                if (ImGui::Checkbox(text.c_str(), &state)) {
                    if (state) {
                        data |= mask;
                    } else {
                        data &= ~mask;
                    }
                }
            }
        };

        struct StandardSelector : public Renderable
        {
            Generator &gen;
            bool       c11 = true;
            bool       c20 = false;

            StandardSelector(Generator &gen) : gen(gen) {}

            void render(GUI &g) override {
                ImGui::Text("Minimum standard");
                if (ImGui::Checkbox("c++11", &c11)) {
                    c11 = true;
                    c20 = false;
                    g.queueRedraw();
                    gen.cfg.gen.cppStd.data = 11;
                }
                if (ImGui::Checkbox("c++20", &c20)) {
                    c20 = true;
                    c11 = false;
                    g.queueRedraw();
                    gen.cfg.gen.cppStd.data = 20;
                }
            }
        };

        template <typename T>
        struct LevelRenderable : public T
        {
            GUI::Level level;

            template <typename... Args>
            LevelRenderable(GUI::Level level, Args &&...args) : T(std::forward<Args>(args)...), level(level) {}

            void render(GUI &g) override {
                if (g.guiLevel >= level) {
                    T::render(g);
                }
            }
        };

        /*
            template <typename T>
            class Container : public std::vector<T>, public Selectable {
              public:
                std::string name;

                void draw(int id);

                virtual void setEnabledChildren(bool value,
                                                bool ifSelected = false) override {
                    for (auto &e : *this) {
                        if constexpr (std::is_pointer_v<T>) {
                            e->setEnabled(value, ifSelected);
                        } else {
                            e.setEnabled(value, ifSelected);
                        }
                    }
                };

                virtual void setSelected(bool value) override {
                    for (auto &e : *this) {
                        if constexpr (std::is_pointer_v<T>) {
                            e->selected = value;
                        } else {
                            e.selected = value;
                        }
                    }
                }
            };

            struct Type : public SelectableData<Generator::BaseType> {
                static bool visualizeDisabled;
                static bool drawFiltered;

                const char *name = {};

                Type(Generator::BaseType &data)
                    : SelectableData<Generator::BaseType>(data),
                      name(data.name.original.c_str()) {}

                Generator::BaseType *operator->() { return data; }

                void draw(int id);
            };

            struct Extension : public SelectableData<Generator::ExtensionData> {
                const char *name;

                Container<Type *> commands;

                Extension(Generator::ExtensionData &data)
                    : SelectableData<Generator::ExtensionData>(data),
                      name(data.name.original.c_str()) {
                    commands.name = "Commands";
                }

                Generator::ExtensionData *operator->() { return data; }

                void draw(int id);

                virtual void setEnabledChildren(bool enabled,
                                                bool ifSelected = false) override {
                    for (auto &c : commands) {
                        c->setEnabled(enabled, ifSelected);
                    }
                }
            };

            struct Platform : public SelectableData<Generator::PlatformData> {
                const char *name;
                Container<Extension *> extensions;

                Platform(Generator::PlatformData &data)
                    : SelectableData<Generator::PlatformData>(data),
                      name(data.name.original.c_str()) {}

                Generator::PlatformData *operator->() { return data; }

                void draw(int id);

                virtual void setEnabledChildren(bool enabled,
                                                bool ifSelected = false) override {
                    for (auto &e : extensions) {
                        e->setEnabled(enabled, ifSelected);
                    }
                }
            };
        */

      private:
        static Generator *gen;
        static bool       menuOpened;

        // using Platforms = Container<Platform>;
        // using Extensions = Container<Extension>;
        // using Types = Container<Type>;

        template <typename T>
        class Container : public SelectableGUI
        {
          public:
            using V = std::vector<std::reference_wrapper<T>>;
            V *data = {};

            std::string name;

            virtual void setEnabledChildren(bool value, bool ifSelected = false) {
                for (T &e : *data) {
                    if (ifSelected && !e.isSelected()) {
                        continue;
                    }
                    e.setEnabled(value);
                }
            }

            void draw(int id, bool filterNested);
        };

        struct Collection
        {
            Container<vkr::Platform>  platforms;
            Container<vkr::Feature>   features;
            Container<vkr::Extension> extensions;
            Container<vkr::Handle>    handles;
            Container<vkr::Struct>    structs;
            Container<vkr::Enum>      enums;
            Container<vkr::Command>   commands;

            void draw(int &id, bool filtered = false);
        };

        Collection collection;

        std::unique_ptr<RenderableTable<3>> tableGeneral;

        struct AsyncButton
        {
            std::string               text;
            std::function<void(void)> task;
            std::future<void>         future;
            std::atomic_bool          running = false;
            float                     size    = 1.f;

            void draw();
            void run();
        };

        void onLoad();

        static bool drawContainerHeader(const char *name, SelectableGUI *s = nullptr, bool empty = false, const std::string &info = "");

        //    template <typename T>
        //    static bool drawContainerHeader(const char *name,
        //                                    SelectableData<T> *s = nullptr,
        //                                    bool empty = false);

        // static void drawSelectable(const char *name, Selectable *s);

        std::string configPath;

        std::string filter;

        ImFont *font = {};

        // ImFont *font2;

        struct QueueFamilyIndices
        {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;

            bool isComplete() {
                return graphicsFamily.has_value() && presentFamily.has_value();
            }
        };

        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR        capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR>   presentModes;
        };

        const int MAX_FRAMES_IN_FLIGHT = 3;

        Level                    guiLevel{};
        int                      id;
        int                      width, height;
        GLFWwindow              *glfwWindow;
        VkInstance               instance;
        VkSurfaceKHR             surface;
        VkPhysicalDevice         physicalDevice;
        QueueFamilyIndices       indices;
        VkDevice                 device;
        VkQueue                  graphicsQueue;
        VkQueue                  presentQueue;
        VkSwapchainKHR           swapChain;
        std::vector<VkImage>     swapChainImages;
        VkFormat                 swapChainImageFormat;
        VkExtent2D               swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;
        VkRenderPass             renderPass;
        //    VkPipelineLayout pipelineLayout;
        //    VkPipelineCache pipelineCache;
        //    VkPipeline graphicsPipeline;
        //    VkShaderModule vertShaderModule;
        //    VkShaderModule fragShaderModule;
        std::vector<VkFramebuffer>   swapChainFramebuffers;
        VkCommandPool                commandPool;
        std::vector<VkCommandBuffer> commandBuffers;
        VkDescriptorSetLayout        descriptorSetLayout;

        std::vector<VkDescriptorSet> descriptorSets;
        std::vector<VkDescriptorSet> videoDescriptorSets;

        VkDescriptorPool descriptorPool;

        VkViewport viewport;
        VkRect2D   scissor;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence>     inFlightFences;
        std::vector<VkFence>     imagesInFlight;
        size_t                   currentFrame = 0;
        uint32_t                 imageIndex   = 0;

        // AsyncButton loadRegButton;
        AsyncButton unloadRegButton;
        AsyncButton generateButton;
        AsyncButton loadConfigButton;
        AsyncButton saveConfigButton;

        StandardSelector *standardSelector = {};

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        bool isDeviceSuitable(VkPhysicalDevice device) {
            QueueFamilyIndices indices = findQueueFamilies(device);
            return indices.isComplete();
        };

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
            for (const auto &availablePresentMode : availablePresentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return availablePresentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window);

        static void checkLayerSupport(const std::vector<const char *> &layers);

        static std::vector<char> readFile(const std::string &filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }

            size_t            fileSize = (size_t)file.tellg();
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

        // void createPipelineCache();

        // void createShaderModules();

        // void createGraphicsPipeline();

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

        static void guiBoolOption(BoolGUI &data);

        std::atomic_bool  filterTaskRunning = false;
        std::atomic_bool  filterTaskAbort   = false;
        std::atomic_bool  filterTaskFinised = false;
        std::atomic_bool  filterSynced      = true;
        std::string       filterTaskError;
        std::future<void> filterFuture;

        void filterTask(std::string filter) noexcept;

        static void showHelpMarker(const char *desc, const std::string &label = "?");

      public:
        GUI(Generator &gen);

        void init();

        bool imguiFrameBuilt = false;

        void setupImguiFrame();

        void updateImgui();

        void mainScreen();

        void loadScreen();

        void setupCommandBuffer(VkCommandBuffer cmd);

        void drawFrame();

        void run();

        void setConfigPath(const std::string &path);

        bool showFps = {};
    };

}  // namespace vkgen

#endif  // GUI_HPP
