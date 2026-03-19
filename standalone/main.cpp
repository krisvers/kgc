#include <iostream>
#include <format>
#include <limits>
#include <unordered_map>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "kgc.h"
#include "kvk.h"
#include "vulkan/vulkan_core.h"

#define VK_ONLY_EXPORTED_PROTOTYPES
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#ifdef NDEBUG
#define IS_DEBUG false
#else
#define IS_DEBUG true
#endif

#define ERR(msg_, ...) throw std::runtime_error(std::format(msg_, #__VA_ARGS__).c_str())
#define VK_ERR(vkresult_, msg_, ...) if (vkresult_ != VK_SUCCESS) { ERR(msg_, #__VA_ARGS__); }

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
                } else {
                    l = depends[0]->evaluate(height - 1);
                }
            }

            if (depends[1] != nullptr) {
                if (height == 0) {
                    r = depends[1]->shallowEvaluate();
                } else {
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

    uint32_t getPossibleTerminalCount() const override {
        return 1;
    }

    uint32_t getCurrentTerminalCount() const override {
        return 1;
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
};

struct Queue {
    VkQueue queue;
    uint32_t familyIndex;
    uint32_t queueIndex;

    static Queue& invalid() {
        static Queue q = { nullptr };
        return q;
    }
};

class FencePool {
private:
    VkDevice _device;

    std::vector<uint32_t> _unusedFences;
    std::vector<VkFence> _fences;

    friend class Renderer;

    void destroy() {
        if (this == &invalid()) {
            return;
        }

        vkWaitForFences(_device, static_cast<uint32_t>(_fences.size()), _fences.data(), true, std::numeric_limits<uint64_t>::max());
        for (uint32_t i = 0; i < _fences.size(); ++i) {
            vkDestroyFence(_device, _fences[i], nullptr);
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

        _fences.emplace_back(fence);
        return fence;
    }

    void releaseFence(VkFence fence) {
        if (_device == nullptr) {
            ERR("Attempted to call releaseFence on invalid reference FencePool");
        }

        for (uint32_t i = 0; i < _fences.size(); ++i) {
            if (fence == _fences[i]) {
                _unusedFences.emplace_back(i);
                return;
            }
        }
    }

    static FencePool& invalid() {
        static FencePool fp = { nullptr };
        return fp;
    }
};

class SemaphorePool {
private:
    VkDevice _device;

    std::vector<uint32_t> _unusedSemaphores;
    std::vector<VkSemaphore> _semaphores;

    friend class Renderer;

    void destroy() {
        if (this == &invalid()) {
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

        _semaphores.emplace_back(semaphore);
        return semaphore;
    }

    void releaseSemaphore(VkSemaphore semaphore) {
        if (_device == nullptr) {
            ERR("Attempted to call releaseSemaphore on invalid reference SemaphorePool");
        }

        for (uint32_t i = 0; i < _semaphores.size(); ++i) {
            if (semaphore == _semaphores[i]) {
                _unusedSemaphores.emplace_back(i);
                return;
            }
        }
    }

    static SemaphorePool& invalid() {
        static SemaphorePool sp = { nullptr };
        return sp;
    }
};

struct SubmitInfoWait {
    VkSemaphore semaphore;
    VkPipelineStageFlags dstStageMask;
};

class CommandPool {
private:
    VkDevice _device;
    Queue* _queue;
    FencePool* _fencePool;
    VkCommandPool _commandPool;

    std::vector<uint32_t> _unusedCommandBuffers;
    std::vector<VkCommandBuffer> _commandBuffers;

    friend class Renderer;

    void destroy() {
        if (this == &invalid()) {
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
            _commandBuffers.emplace_back(commandBuffer);
        }

        vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        return commandBuffer;
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
            ERR("Provided VkCommandBuffer {} does not exist in this pool", commandBuffer);
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

    static CommandPool& invalid() {
        static CommandPool cp = { nullptr, Queue::invalid(), FencePool::invalid() };
        return cp;
    }
};

struct Backbuffer {
    VkImage image;
    VkImageView imageView;

    VkFence fence;
    VkSemaphore semaphore;
};

class BackbufferPool {
private:
    VkSurfaceKHR _surface;
    bool _preferImmediate;

    VkDevice _device;
    VkSwapchainKHR _swapchain;
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

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(_device, _swapchain, std::numeric_limits<uint64_t>::max(), backbuffer.semaphore, backbuffer.fence, &imageIndex);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire next image for BackbuferPool");
        }

        backbuffer.image = _backbuffers[imageIndex];
        backbuffer.imageView = _backbufferViews[imageIndex];
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

class IRenderPool {
public:
    virtual bool execute(Queue const& queue, VkCommandBuffer commandBuffer) = 0;
};

class ImGuiRenderPool {
public:

};

class Renderer {
private:
    VkInstance _instance;
    VkPhysicalDevice _physicalDevice;
    VkDevice _device;

    FencePool _fencePool;
    SemaphorePool _semaphorePool;

    Queue _graphicsQueue;
    CommandPool _graphicsCommandPool;

    void _initInstance() {
        VK_ERR(
            kvk::create_instance({
                .app_name = "kgc Standalone",
                .app_version = VK_MAKE_API_VERSION(0, 0, 9, 0),
                .vk_version = VK_MAKE_API_VERSION(0, 1, 2, 197),
                .vk_layers = {},
                .vk_extensions = {},
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
        VkPhysicalDeviceVulkan12Features vk12Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
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
            .vk_extensions = {},
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
            }
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

        _backbufferPools[window] = BackbufferPool(_fencePool, _semaphorePool, surface, preferImmediate);

        if (_device != nullptr) {
            _finishInitWSI();
        }
    }

    void stage2() {
        _initDevice();
        _finishInitWSI();

        _fencePool = FencePool(_device);
        _graphicsCommandPool = CommandPool(_device, _graphicsQueue, _fencePool);
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

class Application {
private:
    SDL_Window* _window;
    SDL_DisplayMode const* _display_mode;

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

    Renderer _renderer;

public:
    Application() : _renderer() {
        _initWindowing();
        _renderer.initWSI(_window, false);
        _renderer.stage2();
    }

    bool event(SDL_Event const& event) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        default:
            break;
        }

        return true;
    }

    bool process_events() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!event(e)) {
                return false;
            }
        }

        return true;
    }

    bool run() {
        if (!process_events()) {
            return false;
        }

        return true;
    }

    ~Application() {
        _cleanupWindowing();
    }
};

int main(int argc, char** argv) {
    Application application = {};

    while (application.run()) {}

    return 0;
}
