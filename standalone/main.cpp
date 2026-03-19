#include <iostream>
#include <format>
#include <limits>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <functional>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "kgc.h"
#include "kvk.h"

#define VK_ONLY_EXPORTED_PROTOTYPES
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "imnodes/imnodes.h"

#ifdef NDEBUG
#define IS_DEBUG false
#else
#define IS_DEBUG true
#endif

#define ERR(msg_, ...) throw std::runtime_error(std::format(msg_, ##__VA_ARGS__).c_str())
#define VK_ERR(vkresult_, msg_, ...) if (vkresult_ != VK_SUCCESS) { ERR(msg_, ##__VA_ARGS__); }

class Family : public kgc::CustomFamilyID<Family> {
public:
    static inline kgc::TypeID::TypeID_T HalfAdder = 1;
};

class HalfAdder : public kgc::base::AbstractWritableFixedContiguousLinearParentNode<2> {
private:
    static inline uint64_t nextInstance = 0;

    class Terminal : public kgc::base::ITerminal {
    public:
        HalfAdder& _node;
        uint32_t _tid;

        Terminal(HalfAdder& node, uint32_t tid) : _node(node), _tid(tid) {}

        kgc::Value evaluate(uint64_t height) override {
            std::array<kgc::base::ITerminal*, 2> depends;
            for (uint32_t i = 0; i < 2; ++i) {
                depends[i] = _node.getChildAtIndex(i);
            }

            kgc::Value l = kgc::Value::Undefined;
            kgc::Value r = kgc::Value::Undefined;
            if (depends[0] != nullptr) {
                if (height == 0) {
                    l = depends[0]->shallowEvaluate();
                }
                else {
                    l = depends[0]->evaluate(height - 1);
                }
            }

            if (depends[1] != nullptr) {
                if (height == 0) {
                    r = depends[1]->shallowEvaluate();
                }
                else {
                    r = depends[1]->evaluate(height - 1);
                }
            }

            switch (_tid) {
            case 0:
                return l ^ r;
            case 1:
                return l && r;
            default:
                break;
            }

            throw std::runtime_error("HalfAdder._tid is not 0 or 1; should not be possible");
        }

        kgc::Value shallowEvaluate() const override {
            return kgc::Value::Unevaluable;
        }

        uint32_t getTerminalID() const override {
            return _tid;
        }

        HalfAdder* getNode() const override {
            return &_node;
        }

        const char* getName() const override {
            static const char* names[2] = {
                "sum",
                "carry",
            };

            switch (_tid) {
            case 0:
            case 1:
                return names[_tid];
            default:
                break;
            }

            ERR("Invalid terminalID");
        }
    };

    kgc::ID _id;
    Terminal _sumTerminal;
    Terminal _carryTerminal;

public:
    HalfAdder() : _id(Family::get(), Family::HalfAdder, nextInstance++), _sumTerminal(*this, 0), _carryTerminal(*this, 1) {}

    HalfAdder(std::array<kgc::base::ITerminal*, 2> children) : _id(Family::get(), Family::HalfAdder, nextInstance++), _sumTerminal(*this, 0), _carryTerminal(*this, 1) {
        setChildAtIndex(0, children[0]);
        setChildAtIndex(1, children[1]);
    }

    kgc::ID getID() const override {
        return _id;
    }

    const char* getName() const override {
        static const char* quoteName = "Half Adder";
        return quoteName;
    }

    uint32_t getPossibleTerminalCount() const override {
        return 2;
    }

    uint32_t getCurrentTerminalCount() const override {
        return 2;
    }

    Terminal* getTerminal(uint32_t tid) override {
        switch (tid) {
        case 0:
            return &_sumTerminal;
        case 1:
            return &_carryTerminal;
        default:
            break;
        }

        return nullptr;
    }

    const char* getChildSlotNameAtIndex(uint32_t index) const override {
        if (index >= 2) {
            return nullptr;
        }

        static const char* names[2] = { "A", "B" };
        return names[index];
    }
};

struct Queue {
    VkQueue queue = {};
    uint32_t familyIndex;
    uint32_t queueIndex;

    static Queue& invalid() {
        static Queue q = { nullptr };
        return q;
    }
};

class FencePool {
private:
    VkDevice _device = {};

    std::vector<uint32_t> _unusedFences;
    std::vector<VkFence> _fences;

    friend class Renderer;

    void destroy() {
        if (_device == nullptr) {
            return;
        }

        if (!_fences.empty()) {
            vkWaitForFences(_device, static_cast<uint32_t>(_fences.size()), _fences.data(), true, std::numeric_limits<uint64_t>::max());

            for (uint32_t i = 0; i < _fences.size(); ++i) {
                vkDestroyFence(_device, _fences[i], nullptr);
            }
        }
    }

public:
    FencePool(VkDevice device) : _device(device) {}

    VkFence acquireFence(bool signaled) {
        if (_device == nullptr) {
            ERR("Attempted to call acquireFence on invalid reference FencePool");
        }

        VkFenceCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : static_cast<VkFlags>(0),
        };

        VkFence fence;
        VK_ERR(vkCreateFence(_device, &ci, nullptr, &fence), "Failed to create VkFence");

        _fences.push_back(fence);
        return fence;
    }

    void releaseFence(VkFence fence) {
        if (_device == nullptr) {
            ERR("Attempted to call releaseFence on invalid reference FencePool");
        }

        for (uint32_t i = 0; i < _fences.size(); ++i) {
            if (fence == _fences[i]) {
                _unusedFences.push_back(i);
                return;
            }
        }
    }

    static FencePool& invalid() {
        static FencePool fp = FencePool(nullptr);
        return fp;
    }
};

class SemaphorePool {
private:
    VkDevice _device = {};

    std::vector<uint32_t> _unusedSemaphores;
    std::vector<VkSemaphore> _semaphores;

    friend class Renderer;

    void destroy() {
        if (_device == nullptr) {
            return;
        }

        vkDeviceWaitIdle(_device);
        for (uint32_t i = 0; i < _semaphores.size(); ++i) {
            vkDestroySemaphore(_device, _semaphores[i], nullptr);
        }
    }

public:
    SemaphorePool(VkDevice device) : _device(device) {}

    VkSemaphore acquireSemaphore() {
        if (_device == nullptr) {
            ERR("Attempted to call acquireSemaphore on invalid reference SemaphorePool");
        }

        VkSemaphoreCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VkSemaphore semaphore;
        VK_ERR(vkCreateSemaphore(_device, &ci, nullptr, &semaphore), "Failed to create VkSemaphore");

        _semaphores.push_back(semaphore);
        return semaphore;
    }

    void releaseSemaphore(VkSemaphore semaphore) {
        if (_device == nullptr) {
            ERR("Attempted to call releaseSemaphore on invalid reference SemaphorePool");
        }

        for (uint32_t i = 0; i < _semaphores.size(); ++i) {
            if (semaphore == _semaphores[i]) {
                _unusedSemaphores.push_back(i);
                return;
            }
        }
    }

    static SemaphorePool& invalid() {
        static SemaphorePool sp = SemaphorePool(nullptr);
        return sp;
    }
};

struct SubmitInfoWait {
    VkSemaphore semaphore = {};
    VkPipelineStageFlags dstStageMask;
};

class CommandPool {
private:
    VkDevice _device = {};
    VkCommandPool _commandPool = {};

    Queue* _queue;
    FencePool* _fencePool;

    std::vector<uint32_t> _unusedCommandBuffers;
    std::vector<VkCommandBuffer> _commandBuffers;

    friend class Renderer;

    void destroy() {
        if (_device == nullptr) {
            return;
        }

        vkQueueWaitIdle(_queue->queue);
        vkDestroyCommandPool(_device, _commandPool, nullptr);
    }

public:
    CommandPool(VkDevice device, Queue& queue, FencePool& fencePool) : _device(device), _queue(&queue), _fencePool(&fencePool) {
        if (_device == nullptr) {
            return;
        }

        VkCommandPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queue.familyIndex,
        };

        VK_ERR(vkCreateCommandPool(_device, &ci, nullptr, &_commandPool), "Failed to create VkCommandPool for command grouping");
    }

    VkCommandBuffer acquireCommandBuffer() {
        if (_device == nullptr) {
            ERR("Attempted to call acquireCommandBuffer on invalid reference CommandPool");
        }

        VkCommandBuffer commandBuffer;
        if (!_unusedCommandBuffers.empty()) {
            uint32_t cb = _unusedCommandBuffers[_unusedCommandBuffers.size() - 1];
            _unusedCommandBuffers.resize(_unusedCommandBuffers.size() - 1);
            commandBuffer = _commandBuffers[cb];
        } else {
            VkCommandBufferAllocateInfo ai = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = _commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VK_ERR(vkAllocateCommandBuffers(_device, &ai, &commandBuffer), "Failed to allocate new VkCommandBuffer");
            _commandBuffers.push_back(commandBuffer);
        }

        return commandBuffer;
    }

    void releaseCommandBuffer(VkCommandBuffer commandBuffer) {
        if (_device == nullptr) {
            ERR("Attempted to call acquireCommandBuffer on invalid reference CommandPool");
        }

        uint32_t index;
        for (index = 0; index < _commandBuffers.size(); ++index) {
            if (_commandBuffers[index] == commandBuffer) {
                break;
            }
        }

        if (index == _commandBuffers.size()) {
            ERR("Provided command buffer is not a child of this CommandPool");
        }

        vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        _unusedCommandBuffers.push_back(index);
    }

    VkFence submitCommandBuffer(VkCommandBuffer commandBuffer, std::vector<SubmitInfoWait> const& waits, std::vector<VkSemaphore> const& signals) {
        if (_device == nullptr) {
            ERR("Attempted to call submitCommandBuffer on invalid reference CommandPool");
        }

        uint32_t cb;
        for (cb = 0; cb < _commandBuffers.size(); ++cb) {
            if (_commandBuffers[cb] == commandBuffer) {
                break;
            }
        }

        if (cb == _commandBuffers.size()) {
            ERR("Provided VkCommandBuffer does not exist in this pool");
        }

        std::vector<VkSemaphore> waitSemaphores(waits.size());
        std::vector<VkPipelineStageFlags> waitDstStageMasks(waits.size());
        for (uint32_t i = 0; i < waits.size(); ++i) {
            waitSemaphores[i] = waits[i].semaphore;
            waitDstStageMasks[i] = waits[i].dstStageMask;
        }

        VkFence fence = _fencePool->acquireFence(false);
        
        VkSubmitInfo si = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = static_cast<uint32_t>(waits.size()),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitDstStageMasks.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = static_cast<uint32_t>(signals.size()),
            .pSignalSemaphores = signals.data(),
        };

        VK_ERR(vkQueueSubmit(_queue->queue, 1, &si, fence), "Failed to submit VkQueue");
        return fence;
    }

    void releaseSubmitFence(VkFence fence) {
        if (_device == nullptr) {
            ERR("Attempted to call releaseSubmitFence on invalid reference CommandPool");
        }

        _fencePool->releaseFence(fence);
    }

    static CommandPool& invalid() {
        static CommandPool cp = CommandPool(nullptr, Queue::invalid(), FencePool::invalid());
        return cp;
    }
};

struct Backbuffer {
    uint32_t index;

    VkImage image = {};
    VkImageView imageView = {};

    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
    uint32_t layerCount;

    VkFence fence = {};
    VkSemaphore semaphore = {};
};

class BackbufferPool {
private:
    VkSurfaceKHR _surface = {};
    bool _preferImmediate;

    VkDevice _device = {};
    VkSwapchainKHR _swapchain = {};
    kvk::SwapchainPreference _swapchainPreference;
    VkExtent2D _lastKnownExtent;

    std::vector<VkImage> _backbuffers;
    std::vector<VkImageView> _backbufferViews;

    FencePool* _fencePool;
    SemaphorePool* _semaphorePool;

    friend class Renderer;

public:
    BackbufferPool() {}

    BackbufferPool(FencePool& fencePool, SemaphorePool& semaphorePool, VkSurfaceKHR surface, bool preferImmediate) : _surface(surface), _preferImmediate(preferImmediate), _fencePool(&fencePool), _semaphorePool(&semaphorePool) {}

    Backbuffer acquireBackbuffer() {
        Backbuffer backbuffer = {};
        if (_fencePool != &FencePool::invalid()) {
            backbuffer.fence = _fencePool->acquireFence(false);
        }
        
        if (_semaphorePool != &SemaphorePool::invalid()) {
            backbuffer.semaphore = _semaphorePool->acquireSemaphore();
        }

        VkResult result = vkAcquireNextImageKHR(_device, _swapchain, std::numeric_limits<uint64_t>::max(), backbuffer.semaphore, backbuffer.fence, &backbuffer.index);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire next image for BackbuferPool");
        }

        backbuffer.image = _backbuffers[backbuffer.index];
        backbuffer.imageView = _backbufferViews[backbuffer.index];
        backbuffer.surfaceFormat = _swapchainPreference.vk_surface_format;
        backbuffer.extent = _lastKnownExtent;
        backbuffer.layerCount = _swapchainPreference.layer_count;
        return backbuffer;
    }

    void releaseBackbuffer(Backbuffer& backbuffer) {
        if (backbuffer.fence != nullptr) {
            _fencePool->releaseFence(backbuffer.fence);
        }

        if (backbuffer.semaphore != nullptr) {
            _semaphorePool->releaseSemaphore(backbuffer.semaphore);
        }
    }
};

struct RenderPoolPacket {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkCommandBuffer commandBuffer;

    Queue const& queue;
};

struct RenderPoolBackbufferPacket {
    Backbuffer backbuffer;
    VkImageLayout lastLayout;
    VkPipelineStageFlags lastStage;
};

class IRenderPool {
public:
    virtual bool requiresBackbuffer() = 0;
    virtual bool execute(RenderPoolPacket const& packet, RenderPoolBackbufferPacket* backbufferPacket, std::vector<SubmitInfoWait>& waits, std::vector<VkSemaphore>& signals) = 0;
};

class Renderer {
private:
    VkInstance _instance = {};
    VkPhysicalDevice _physicalDevice = {};
    VkDevice _device = {};

    FencePool _fencePool;
    SemaphorePool _semaphorePool;

    Queue _graphicsQueue;
    CommandPool _graphicsCommandPool;

    std::vector<IRenderPool*> _renderPools;

    ImGuiContext* _imguiContext = {};

    void _initInstance() {
        VK_ERR(
            kvk::create_instance({
                .app_name = "kgc Standalone",
                .app_version = VK_MAKE_API_VERSION(0, 0, 9, 0),
                .vk_version = VK_MAKE_API_VERSION(0, 1, 2, 197),
                .vk_layers = {},
                .vk_extensions = {
                    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                    VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME
                },
                .presets = {
                    .recommended = true,
                    .enable_surfaces = true,
                    .enable_platform_specific_surfaces = true,
                    .enable_validation_layers = IS_DEBUG,
                    .enable_debug_utils = IS_DEBUG,

                    .debug_messenger_callback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
                        const char* severity_strings[4] = {
                            "VERBOSE",
                            "INFO",
                            "WARNING",
                            "ERROR",
                        };
                    
                        const char* type_strings[4] = {
                            "GENERAL",
                            "VALIDATION",
                            "PERFORMANCE",
                            "DEVICE_ADDRESS_BINDING_EXT",
                        };
                    
                        uint32_t severity_index = 0;
                        uint32_t severity_shifted = static_cast<uint32_t>(severity);
                        for (severity_index = 0; severity_index < 4; ++severity_index) {
                            if (severity_shifted == 1) {
                                break;
                            }
                        
                            severity_shifted >>= 4;
                        }
                    
                        std::cout << "[vk] (" << severity_strings[severity_index];
                        for (uint32_t i = 0; i < 4; ++i) {
                            if ((types & (1 << i)) != 0) {
                                std::cout << ", " << type_strings[i];
                            }
                        }

                        std::cout << "): " << pCallbackData->pMessage << std::endl;
                        return false;
                    },
                }
            }, _instance),
            "Failed to create VkInstance using kvk"
        );
    }

    void _cleanupInstance() {
        if (_instance != nullptr) {
            vkDestroyInstance(_instance, nullptr);
        }
    }

    void _initDevice() {
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenance1Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,
            .swapchainMaintenance1 = true,
        };

        VkPhysicalDeviceVulkan12Features vk12Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &swapchainMaintenance1Features,
            .shaderStorageBufferArrayNonUniformIndexing = true,
            .shaderStorageImageArrayNonUniformIndexing = true,
        };

        VkSurfaceKHR surfaceToBeCompatibleWith = nullptr;
        if (!_backbufferPools.empty()) {
            surfaceToBeCompatibleWith = _backbufferPools.begin()->second._surface;
        }

        std::vector<kvk::DeviceQueueReturn> deviceQueueReturns;
        VK_ERR(kvk::create_device(_instance, {
            .vk_pnext = &vk12Features,
            .vk_extensions = { VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME },
            .physical_device_query = {
                .minimum_vk_version = VK_MAKE_API_VERSION(0, 1, 2, 197),
                .excluded_device_types = kvk::PhysicalDeviceTypeFlags::CPU | kvk::PhysicalDeviceTypeFlags::VIRTUAL_GPU | kvk::PhysicalDeviceTypeFlags::OTHER,
                .minimum_features = {
                    .shaderStorageBufferArrayDynamicIndexing = true,
                    .shaderStorageImageArrayDynamicIndexing = true,
                },
                .minimum_limits = {},
                .required_extensions = {},
                .minimum_format_properties = {
                    {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .minimum_properties = {
                            .optimalTilingFeatures = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
                        },
                    },
                    {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .minimum_properties = {
                            .optimalTilingFeatures = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
                        },
                    },
                },
                .minimum_image_format_properties = {},
                .minimum_memory_properties = {
                    .memoryTypeCount = 1,
                    .memoryTypes = {
                        {
                            .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        },
                        {
                            .propertyFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                        },
                    },
                },
                .required_queues = {
                    {
                        .properties = {
                            .queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT,
                            .queueCount = 1,
                        },
                        .surface_support = surfaceToBeCompatibleWith,
                        .priorities = {
                            1.0f,
                        },
                    }
                }
            },
            .presets = {
                .recommended = true,
                .enable_swapchain = true,
                .enable_dynamic_rendering = true,
                .enable_maintenance1 = true,
            },
        }, _physicalDevice, _device, deviceQueueReturns), "Failed to create VkDevice using kvk");

        _graphicsQueue = {
            .queue = deviceQueueReturns[0].vk_queue,
            .familyIndex = deviceQueueReturns[0].family_index,
            .queueIndex = deviceQueueReturns[0].queue_index,
        };
    }

    void _cleanupDevice() {
        if (_device != nullptr) {
            vkDestroyDevice(_device, nullptr);
        }
    }

    std::unordered_map<SDL_Window*, BackbufferPool> _backbufferPools;

    void _cleanupWSI() {
        for (auto const& p : _backbufferPools) {
            for (uint32_t i = 0; i < p.second._swapchainPreference.image_count; ++i) {
                if (p.second._backbufferViews[i] != nullptr) {
                    vkDestroyImageView(_device, p.second._backbufferViews[i], nullptr);
                }
            }

            if (p.second._swapchain != nullptr) {
                vkDestroySwapchainKHR(_device, p.second._swapchain, nullptr);
            }

            vkDestroySurfaceKHR(_instance, p.second._surface, nullptr);
        }
    }

    void _finishInitWSI() {
        for (auto& p : _backbufferPools) {
            BackbufferPool& backbufferPool = p.second;
            if (backbufferPool._swapchain != nullptr) {
                continue;
            }

            backbufferPool._fencePool = &_fencePool;
            backbufferPool._semaphorePool = &_semaphorePool;

            std::vector<kvk::SwapchainPreference> swapchainPreferences = {
                {
                    .image_count = 3,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = backbufferPool._preferImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR,
                },
                {
                    .image_count = 3,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = backbufferPool._preferImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR,
                },
                {
                    .image_count = 2,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = backbufferPool._preferImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR,
                },
                {
                    .image_count = 2,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = backbufferPool._preferImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR,
                },
                {
                    .image_count = 3,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                },
                {
                    .image_count = 3,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                },
                {
                    .image_count = 2,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                },
                {
                    .image_count = 2,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                },
                {
                    .image_count = 3,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_KHR,
                },
                {
                    .image_count = 3,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_KHR,
                },
                {
                    .image_count = 2,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_KHR,
                },
                {
                    .image_count = 2,
                    .layer_count = 1,

                    .vk_surface_format = {
                        .format = VK_FORMAT_B8G8R8A8_SRGB,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    .vk_present_mode = VK_PRESENT_MODE_FIFO_KHR,
                },
            };

            kvk::SwapchainReturns swapchainReturns = {
                .vk_backbuffers = backbufferPool._backbuffers,
            };

            VK_ERR(kvk::create_swapchain(_device, 
                {
                    .vk_physical_device = _physicalDevice,
                    .vk_surface = backbufferPool._surface,

                    .vk_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,

                    .preferences = swapchainPreferences,

                    .vk_image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
                    .vk_queue_family_indices = {},

                    .vk_pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                    .vk_composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                    .vk_clipped = false,
                },
                swapchainReturns
            ), "Failed to create VkSwapchainKHR using kvk");

            backbufferPool._device = _device;
            backbufferPool._swapchain = swapchainReturns.vk_swapchain;
            backbufferPool._swapchainPreference = swapchainPreferences[swapchainReturns.chosen_preference];
            backbufferPool._lastKnownExtent = swapchainReturns.vk_current_extent;
            backbufferPool._backbufferViews.resize(backbufferPool._swapchainPreference.image_count);

            for (uint32_t i = 0; i < backbufferPool._swapchainPreference.image_count; ++i) {
                VkImageViewCreateInfo viewCI = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = backbufferPool._backbuffers[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = backbufferPool._swapchainPreference.vk_surface_format.format,
                    .components = {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    }
                };

                VK_ERR(vkCreateImageView(_device, &viewCI, nullptr, &backbufferPool._backbufferViews[i]), "Failed to create image view for backbuffer {}", i);
            }
        }
    }

public:
    Renderer() : _fencePool(FencePool::invalid()), _semaphorePool(SemaphorePool::invalid()), _graphicsCommandPool(CommandPool::invalid()) {
        _initInstance();
    }

    void initWSI(SDL_Window* window, bool preferImmediate) {
        if (_backbufferPools.contains(window)) {
            return;
        }

        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(window, _instance, nullptr, &surface)) {
            ERR("Failed to create VkSurfaceKHR using SDL3: {}", SDL_GetError());
        }

        _backbufferPools[window] = BackbufferPool(FencePool::invalid(), SemaphorePool::invalid(), surface, preferImmediate);

        if (_device != nullptr) {
            _finishInitWSI();
        }
    }

    void stage2() {
        _initDevice();

        _fencePool = FencePool(_device);
        _semaphorePool = SemaphorePool(_device);
        _graphicsCommandPool = CommandPool(_device, _graphicsQueue, _fencePool);

        _finishInitWSI();
    }

    void initImGuiVulkan(ImGuiContext* imguiContext) {
        _imguiContext = imguiContext;

        if (_backbufferPools.empty()) {
            ERR("Must have at least one BackbufferPool in order to perform UI init");
        }

        BackbufferPool& chosenPool = _backbufferPools.begin()->second;

        ImGui_ImplVulkan_InitInfo ii = {
            .ApiVersion = VK_MAKE_API_VERSION(0, 1, 2, 197),
            .Instance = _instance,
            .PhysicalDevice = _physicalDevice,
            .Device = _device,
            .QueueFamily = _graphicsQueue.familyIndex,
            .Queue = _graphicsQueue.queue,
            .DescriptorPool = {},
            .DescriptorPoolSize = 1024,
            .MinImageCount = chosenPool._swapchainPreference.image_count,
            .ImageCount = chosenPool._swapchainPreference.image_count,
            .PipelineInfoMain = {
                .RenderPass = {},
                .Subpass = 0,
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &chosenPool._swapchainPreference.vk_surface_format.format,
                    .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
                    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
                },
            },
            .UseDynamicRendering = true,
            .Allocator = nullptr,
            .MinAllocationSize = 1024 * 1024,
        };

        if (!ImGui_ImplVulkan_Init(&ii)) {
            ERR("Failed to init ImGui Vulkan backend");
        }
    }

    void cleanupImGuiVulkan() {
        ImGui_ImplVulkan_Shutdown();
    }

    void beginUIFrame() {
        ImGui_ImplVulkan_NewFrame();
    }

    void executeRenderPools(std::vector<IRenderPool*>& renderPools, SDL_Window* window) {
        VkCommandBuffer commandBuffer = _graphicsCommandPool.acquireCommandBuffer();
        if (window != nullptr && !_backbufferPools.contains(window)) {
            ERR("No such window has been registered for renderer");
        }

        VkCommandBufferBeginInfo commandBufferBI = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        VK_ERR(vkBeginCommandBuffer(commandBuffer, &commandBufferBI), "Failed to begin command buffer");

        BackbufferPool* backbufferPool = {};
        Backbuffer backbuffer = {};

        VkPipelineStageFlags furthestBackbufferStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        RenderPoolBackbufferPacket backbufferPacket = {};
        backbufferPacket.lastLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        backbufferPacket.lastStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        std::vector<SubmitInfoWait> waits;
        std::vector<VkSemaphore> signals;
        for (IRenderPool* pool : renderPools) {
            RenderPoolPacket packet = {
                .instance = _instance,
                .device = _device,
                .physicalDevice = _physicalDevice,

                .commandBuffer = commandBuffer,

                .queue = _graphicsQueue,
            };

            backbufferPacket.backbuffer = backbuffer;

            if (pool->requiresBackbuffer() && window != nullptr) {
                if (backbufferPool == nullptr) {
                    backbufferPool = &_backbufferPools[window];
                    backbuffer = backbufferPool->acquireBackbuffer();
                }

                backbufferPacket.backbuffer = backbuffer;
                if (!pool->execute(packet, &backbufferPacket, waits, signals)) {
                    ERR("Failed to execute render pool");
                }

                if (backbufferPacket.lastStage > furthestBackbufferStage) {
                    furthestBackbufferStage = backbufferPacket.lastStage;
                }
            } else {
                if (!pool->execute(packet, nullptr, waits, signals)) {
                    ERR("Failed to execute render pool");
                }
            }
        }

        /* transition backbuffer for presentation */
        if (backbufferPool != nullptr) {
            VkImageMemoryBarrier backbufferBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = {},
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = backbufferPacket.lastLayout,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = _graphicsQueue.familyIndex,
                .dstQueueFamilyIndex = _graphicsQueue.familyIndex,
                .image = backbuffer.image,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = backbuffer.layerCount,
                },
            };

            vkCmdPipelineBarrier(commandBuffer,
                backbufferPacket.lastStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &backbufferBarrier
            );
        }

        VK_ERR(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer");

        VkSemaphore backbufferFinishedSemaphore = {};
        if (backbufferPool != nullptr) {
            waits.push_back({
                .semaphore = backbuffer.semaphore,
                .dstStageMask = furthestBackbufferStage,
            });

            backbufferFinishedSemaphore = _semaphorePool.acquireSemaphore();
            signals.push_back(backbufferFinishedSemaphore);
        }

        VkFence submissionFence = _graphicsCommandPool.submitCommandBuffer(commandBuffer, waits, signals);

        VkFence presentationFinishedFence = {};
        std::vector<VkFence> waitFences = { submissionFence };
        if (backbufferPool != nullptr) {
            presentationFinishedFence = _fencePool.acquireFence(false);

            VkSwapchainPresentFenceInfoEXT spfi = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
                .swapchainCount = 1,
                .pFences = &presentationFinishedFence,
            };

            VkPresentInfoKHR pi = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = &spfi,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &backbufferFinishedSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &backbufferPool->_swapchain,
                .pImageIndices = &backbuffer.index,
                .pResults = nullptr,
            };

            VK_ERR(vkQueuePresentKHR(_graphicsQueue.queue, &pi), "Failed to present backbuffer");
            waitFences.push_back(presentationFinishedFence);
        }

        vkWaitForFences(_device, static_cast<uint32_t>(waitFences.size()), waitFences.data(), true, std::numeric_limits<uint64_t>::max());
        vkResetFences(_device, static_cast<uint32_t>(waitFences.size()), &waitFences[0]);

        _graphicsCommandPool.releaseCommandBuffer(commandBuffer);
        _graphicsCommandPool.releaseSubmitFence(submissionFence);
        if (backbufferPool != nullptr) {
            _fencePool.releaseFence(presentationFinishedFence);
            _semaphorePool.releaseSemaphore(backbufferFinishedSemaphore);
            backbufferPool->releaseBackbuffer(backbuffer);
        }
    }

    ~Renderer() {
        if (_device != nullptr) {
            vkDeviceWaitIdle(_device);
        }

        _graphicsCommandPool.destroy();

        _cleanupWSI();

        _semaphorePool.destroy();
        _fencePool.destroy();

        _cleanupDevice();
        _cleanupInstance();
    }
};

class ImGuiRenderPool : public IRenderPool {
public:
    bool requiresBackbuffer() override {
        return true;
    }

    bool execute(RenderPoolPacket const& packet, RenderPoolBackbufferPacket* backbufferPacket, std::vector<SubmitInfoWait>& waits, std::vector<VkSemaphore>& signals) override {
        VkImageMemoryBarrier backbufferToColorAttachmentBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = {},
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = backbufferPacket->lastLayout,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = packet.queue.familyIndex,
            .dstQueueFamilyIndex = packet.queue.familyIndex,
            .image = backbufferPacket->backbuffer.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = backbufferPacket->backbuffer.layerCount,
            },
        };

        vkCmdPipelineBarrier(packet.commandBuffer,
            backbufferPacket->lastStage,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {},
            0, nullptr,
            0, nullptr,
            1, &backbufferToColorAttachmentBarrier
        );

        backbufferPacket->lastStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(packet.instance, "vkCmdBeginRenderingKHR"));
        PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(packet.instance, "vkCmdEndRenderingKHR"));

        VkRenderingAttachmentInfoKHR backbufferCAI = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = backbufferPacket->backbuffer.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {
                .color = {
                    .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
                },
            },
        };

        VkRenderingInfoKHR ri = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .renderArea = {
                .offset = {
                    .x = 0,
                    .y = 0,
                },
                .extent = backbufferPacket->backbuffer.extent,
            },
            .layerCount = backbufferPacket->backbuffer.layerCount,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = &backbufferCAI,
        };

        vkCmdBeginRenderingKHR(packet.commandBuffer, &ri);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), packet.commandBuffer, nullptr);
        vkCmdEndRenderingKHR(packet.commandBuffer);
        return true;
    }
};

struct NodeFactory {
    std::function<const char*()> getName;
    std::function<kgc::base::INode&()> instantiate;
};

class Application {
private:
    SDL_Window* _window = {};
    SDL_DisplayMode const* _display_mode = {};

    ImGuiContext* _imguiContext = {};

    void _initWindowing() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            ERR("Failed to init SDL3: {}", SDL_GetError());
        }

        if (!SDL_Vulkan_LoadLibrary(nullptr)) {
            ERR("Failed to load Vulkan with SDL3: {}", SDL_GetError());
        }

        _window = SDL_CreateWindow("kgc Standalone", 1200, 800, SDL_WINDOW_VULKAN);
        if (_window == nullptr) {
            ERR("Failed create SDL3 window: {}", SDL_GetError());
        }

        _display_mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(_window));
    }

    void _cleanupWindowing() {
        SDL_Vulkan_UnloadLibrary();
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }

    void _initUI() {
        _imguiContext = ImGui::CreateContext();
        if (_imguiContext == nullptr) {
            ERR("Failed to create ImGuiContext");
        }

        if (ImNodes::CreateContext() == nullptr) {
            ERR("Failed to create ImNodesContext");
        }

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        if (!ImGui_ImplSDL3_InitForVulkan(_window)) {
            ERR("Failed to init ImGui for Vulkan using SDL window");
        }

        _renderer.initImGuiVulkan(_imguiContext);
    }

    void _cleanupUI() {
        _renderer.cleanupImGuiVulkan();
        ImGui_ImplSDL3_Shutdown();
        ImNodes::DestroyContext();
        ImGui::DestroyContext();
    }

    bool _event(SDL_Event const& event) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        default:
            break;
        }

        return true;
    }

    bool _process_events() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!_event(e)) {
                return false;
            }
        }

        return true;
    }

    Renderer _renderer;
    ImGuiRenderPool _imguiRenderPool;

    void _beginUIFrame() {
        _renderer.beginUIFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void _renderUIFrame() {
        ImGui::Render();
    }

    int _imnodeLinkID = 0;

    int _hashNodeIDToImNodesID(kgc::base::INode const& node) {
        return _hashes[reinterpret_cast<void const*>(&node)];
    }

    int _hashTerminalToImNodesID(kgc::base::ITerminal const& terminal) {
        return _hashes[reinterpret_cast<void const*>(&terminal)];
    }

    int _hashChildSlotToImNodesID(kgc::base::INode const& node, uint32_t child) {
        return _hashes[reinterpret_cast<void const*>(&node)] ^ (child + 127);
    }

    void _executeNodeUI(kgc::base::INode& node) {
        ImNodes::BeginNode(_hashNodeIDToImNodesID(node));

        std::stringstream ss;
        ss << node.getName() << " (" << node.getID().instance << ")";

        ImNodes::BeginNodeTitleBar();
        ImGui::Text(ss.str().c_str());
        ImNodes::EndNodeTitleBar();

        if (node.getID() <= kgc::ID(kgc::builtin::Family::get(), kgc::builtin::Family::Misc::Uniform, 0)) {
            kgc::builtin::misc::UniformNode& uniformNode = dynamic_cast<kgc::builtin::misc::UniformNode&>(node);
            bool v = static_cast<std::underlying_type_t<kgc::Value>>(uniformNode.getValue()) == static_cast<std::underlying_type_t<kgc::Value>>(kgc::Value::High);
            ImGui::Checkbox("Value", &v);

            uniformNode.setValue(v ? kgc::Value::High : kgc::Value::Low);
        }
        else if (node.getID() <= kgc::ID(kgc::builtin::Family::get(), kgc::builtin::Family::Misc::Buffer, 0)) {
            kgc::base::ITerminal* terminal = node.getTerminal(0);

            kgc::Value value = kgc::Value::Undefined;
            if (_cachedEvaluations.contains(terminal)) {
                value = _cachedEvaluations[terminal];
            }

            switch (value) {
            case kgc::Value::High:
                ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(87, 46, 45, 255));
                break;
            case kgc::Value::Low:
                ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(18, 14, 14, 255));
                break;
            case kgc::Value::Undefined:
                ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(71, 29, 70, 255));
                break;
            case kgc::Value::Unevaluable:
                ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(34, 66, 32, 255));
                break;
            }
        }

        if (node.isLinearParentNode()) {
            kgc::base::ILinearParentNode& lpnode = dynamic_cast<kgc::base::ILinearParentNode&>(node);
            for (uint32_t ci = 0; ci < lpnode.getPossibleChildCount(); ++ci) {
                ImNodes::BeginInputAttribute(_hashChildSlotToImNodesID(node, ci));
                ImGui::Text(lpnode.getChildSlotNameAtIndex(ci));
                ImNodes::EndInputAttribute();
            }
        }

        for (uint32_t tid = 0; tid < node.getPossibleTerminalCount(); ++tid) {
            kgc::base::ITerminal* terminal = node.getTerminal(tid);
            if (terminal == nullptr) {
                continue;
            }

            kgc::Value value = kgc::Value::Undefined;
            if (_cachedEvaluations.contains(terminal)) {
                value = _cachedEvaluations[terminal];
            }

            switch (value) {
            case kgc::Value::High:
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(235, 108, 106, 240));
                break;
            case kgc::Value::Low:
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(18, 14, 14, 200));
                break;
            case kgc::Value::Undefined:
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(255, 66, 249, 220));
                break;
            case kgc::Value::Unevaluable:
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(96, 191, 90, 200));
                break;
            }

            ImNodes::BeginOutputAttribute(_hashTerminalToImNodesID(*terminal));
            ImGui::Text(terminal->getName());
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    int _currentHash = 0;
    std::unordered_map<void const*, int> _hashes;
    std::vector<NodeFactory> _nodeFactories;
    std::vector<kgc::base::INode*> _nodes;
    std::unordered_map<kgc::base::ITerminal*, kgc::Value> _cachedEvaluations;

    void _executeNodeLinkUI(kgc::base::INode& node) {
        if (node.isLinearParentNode()) {
            kgc::base::ILinearParentNode& lpnode = dynamic_cast<kgc::base::ILinearParentNode&>(node);
            for (uint32_t ci = 0; ci < lpnode.getPossibleChildCount(); ++ci) {
                kgc::base::ITerminal* terminal = lpnode.getChildAtIndex(ci);
                if (terminal == nullptr) {
                    continue;
                }

                kgc::Value value = kgc::Value::Undefined;
                if (_cachedEvaluations.contains(terminal)) {
                    value = _cachedEvaluations[terminal];
                }

                switch (value) {
                case kgc::Value::High:
                    ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(235, 108, 106, 240));
                    break;
                case kgc::Value::Low:
                    ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(18, 14, 14, 200));
                    break;
                case kgc::Value::Undefined:
                    ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(255, 66, 249, 220));
                    break;
                case kgc::Value::Unevaluable:
                    ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(96, 191, 90, 200));
                    break;
                }

                ImNodes::Link(_imnodeLinkID++, _hashTerminalToImNodesID(*terminal), _hashChildSlotToImNodesID(lpnode, ci));
            }
        }
    }

    void _executeNodeSandboxUI() {
        int w, h;
        SDL_GetWindowSizeInPixels(_window, &w, &h);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x = w;
        io.DisplaySize.y = h;
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowPos({ 0, 0 });

        ImGui::Begin("Sandbox");
        ImNodes::BeginNodeEditor();

        for (kgc::base::INode* node : _nodes) {
            _executeNodeUI(*node);
        }

        for (kgc::base::INode* node : _nodes) {
            _executeNodeLinkUI(*node);
        }

        ImNodes::EndNodeEditor();
        ImGui::End();
    }

    void _executeNodeFactoryUI() {
        ImGui::Begin("Factories");
        for (NodeFactory const& factory : _nodeFactories) {
            if (ImGui::Button(factory.getName())) {
                kgc::base::INode& node = factory.instantiate();
                _hashes[&node] = _currentHash++;
                if (node.isLinearParentNode()) {
                    kgc::base::ILinearParentNode& lpnode = dynamic_cast<kgc::base::ILinearParentNode&>(node);
                    for (uint32_t ci = 0; ci < lpnode.getPossibleChildCount(); ++ci) {
                        lpnode.setChildAtIndex(ci, nullptr);
                    }
                }

                for (uint32_t tid = 0; tid < node.getPossibleTerminalCount(); ++tid) {
                    _hashes[node.getTerminal(tid)] = _currentHash++;
                }

                _nodes.push_back(&node);
            }
        }
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::End();
    }

    static inline uint64_t _lastLinkCheck = 0;

    kgc::builtin::misc::UniformNode _uniformNode10 = { kgc::Value::High };
    kgc::builtin::misc::UniformNode _uniformNode01 = { kgc::Value::Low };

    HalfAdder _halfAdder = { { _uniformNode01.getTerminal(0), _uniformNode10.getTerminal(0) }};

    void _evaluateNodes(uint64_t height) {
        if (_lastLinkCheck - SDL_GetTicks() > 100) {
            _lastLinkCheck = SDL_GetTicks();
            for (kgc::base::INode* node1 : _nodes) {
                for (kgc::base::INode* node2 : _nodes) {
                    if (node1 == node2) {
                        continue;
                    }

                    for (uint32_t tid = 0; tid < node2->getPossibleTerminalCount(); ++tid) {
                        kgc::base::ITerminal* terminal = node2->getTerminal(tid);
                        if (terminal == nullptr) {
                            continue;
                        }

                        int imnodeTerminalID = _hashTerminalToImNodesID(*terminal);
                        if (node1->isLinearParentNode()) {
                            kgc::base::ILinearParentNode* lpnode = dynamic_cast<kgc::base::ILinearParentNode*>(node1);
                            for (uint32_t ci = 0; ci < lpnode->getPossibleChildCount(); ++ci) {
                                int imnodeChildID = _hashChildSlotToImNodesID(*lpnode, ci);
                                if (ImNodes::IsLinkCreated(&imnodeTerminalID, &imnodeChildID)) {
                                    std::cout << "joining " << node2->getID() << "." << tid << " and " << node1->getID() << "," << ci << std::endl;
                                    std::cout << "------- " << imnodeTerminalID << " : " << imnodeChildID << std::endl;
                                    lpnode->setChildAtIndex(ci, terminal);
                                }
                            }
                        }
                    }
                }
            }
        }

        _cachedEvaluations.clear();
        for (kgc::base::INode* node : _nodes) {
            for (uint32_t tid = 0; tid < node->getPossibleTerminalCount(); ++tid) {
                kgc::base::ITerminal* terminal = node->getTerminal(tid);
                if (terminal == nullptr) {
                    continue;
                }

                _cachedEvaluations[terminal] = terminal->evaluate(height);
            }
        }
    }

public:
    Application() : _renderer() {
        _initWindowing();
        _renderer.initWSI(_window, false);
        _renderer.stage2();

        _initUI();

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "Half Adder";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                HalfAdder& node = *new HalfAdder();
                return node;
            },
        });

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "Uniform";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                kgc::builtin::misc::UniformNode &node = *new kgc::builtin::misc::UniformNode();
                return node;
            },
        });

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "Buffer";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                kgc::builtin::misc::BufferNode &node = *new kgc::builtin::misc::BufferNode();
                return node;
            },
        });

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "Nand";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                kgc::builtin::gate::Nand &node = *new kgc::builtin::gate::Nand();
                return node;
            },
        });

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "Nor";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                kgc::builtin::gate::Nor &node = *new kgc::builtin::gate::Nor();
                return node;
            },
        });

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "And";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                kgc::builtin::gate::And &node = *new kgc::builtin::gate::And();
                return node;
            },
        });

        _nodeFactories.push_back(NodeFactory {
            .getName = []() -> const char* {
                return "Or";
            },
            .instantiate = [&]() -> kgc::base::INode& {
                kgc::builtin::gate::Or &node = *new kgc::builtin::gate::Or();
                return node;
            },
        });
    }

    bool run() {
        if (!_process_events()) {
            return false;
        }

        _beginUIFrame();
        _evaluateNodes(100);

        _executeNodeSandboxUI();
        _executeNodeFactoryUI();

        _renderUIFrame();

        std::vector<IRenderPool*> renderPools = { &_imguiRenderPool };
        _renderer.executeRenderPools(renderPools, _window);
        return true;
    }

    ~Application() {
        _cleanupUI();
        _cleanupWindowing();
    }
};

int main(int argc, char** argv) {
    Application application = {};
    while (application.run()) {}

    return 0;
}
