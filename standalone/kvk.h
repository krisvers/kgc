#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <stdexcept>
#include <optional>
#include <unordered_map>

#ifdef KVK_USE_DXC
//#include <dxc/dxcapi.h>
#endif

namespace kvk {

using MessageCallback = void(*)(VkResult vk_result, VkDebugUtilsMessageSeverityFlagsEXT severity, const char* message, const char* function);

using ArrayReferenceResizeCallback = bool(*)(size_t size, void* pdata);
using ArrayReferenceSizeCallback = size_t(*)(void* pdata);

template<typename T>
using ArrayReferenceIndexCallback = T&(*)(uint32_t index, void* pdata, bool& ok);

template<typename T>
class ArrayReference {
private:
    void* pdata;
    ArrayReferenceResizeCallback resize_callback;
    ArrayReferenceSizeCallback size_callback;
    ArrayReferenceIndexCallback<T> index_callback;

public:
    ArrayReference(std::vector<T>& vector) {
        pdata = reinterpret_cast<void*>(&vector);
        resize_callback = [](size_t size, void* pdata) -> bool {
            try {
                reinterpret_cast<std::vector<T>*>(pdata)->resize(size);
                return true;
            } catch (...) {
                return false;
            }
        };

        size_callback = [](void* pdata) -> size_t {
            return reinterpret_cast<std::vector<T>*>(pdata)->size();
        };

        index_callback = [](uint32_t i, void* pdata, bool& ok) -> T& {
            if (i >= reinterpret_cast<std::vector<T>*>(pdata)->size()) {
                ok = false;
                static T dummy {};
                return dummy;
            }

            ok = true;
            return (*reinterpret_cast<std::vector<T>*>(pdata))[i];
        };
    }

    bool resize(size_t size) {
        return resize_callback(size, pdata);
    }

    size_t size() {
        return size_callback(pdata);
    }

    T& operator[](uint32_t i) {
        bool ok = true;
        T& item = index_callback(i, pdata, ok);
        if (!ok) {
            throw std::out_of_range("ArrayReference index out of range");
        }

        return item;
    }
};

struct InstancePresets {
    bool recommended = false;
    bool enable_surfaces = false;
    bool enable_platform_specific_surfaces = false;
    bool enable_validation_layers = false;
    bool enable_debug_utils = false;
    bool create_enumerate_portability_instance = false;

    PFN_vkDebugUtilsMessengerCallbackEXT debug_messenger_callback = nullptr;
    void* debug_messenger_callback_user_data = nullptr;

    PFN_vkDebugReportCallbackEXT debug_report_callback = nullptr;
    void* debug_report_callback_user_data = nullptr;
};

struct InstanceCreateInfo {
    const char* app_name;
    uint32_t app_version;
    uint32_t vk_version;

    void* vk_pnext;
    VkFlags vk_flags;

    std::vector<const char*> const& vk_layers;
    std::vector<const char*> const& vk_extensions;

    InstancePresets presets;
};

void set_error_callback(MessageCallback callback);
VkResult create_instance(InstanceCreateInfo const& create_info, VkInstance& vk_instance);

enum class PhysicalDeviceTypeFlags : uint32_t {
    OTHER = (1 << VK_PHYSICAL_DEVICE_TYPE_OTHER),
    INTEGRATED_GPU = (1 << VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
    DISCRETE_GPU = (1 << VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU),
    VIRTUAL_GPU = (1 << VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU),
    CPU = (1 << VK_PHYSICAL_DEVICE_TYPE_CPU),
};

inline PhysicalDeviceTypeFlags operator|(PhysicalDeviceTypeFlags const& a, PhysicalDeviceTypeFlags const& b) {
    return static_cast<PhysicalDeviceTypeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct PhysicalDeviceFormatPropertyRequirement {
    VkFormat format;
    VkFormatProperties minimum_properties;
};

struct PhysicalDeviceImageFormatPropertyRequirement {
    VkFormat format;
    VkImageType image_type;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkImageCreateFlags flags;
    VkImageFormatProperties minimum_properties;
};

struct PhysicalDeviceQueueRequirements {
    VkQueueFamilyProperties properties;
    VkSurfaceKHR surface_support;

    /* only used for create_device() */
    VkDeviceQueueCreateFlags create_flags;
    std::vector<float> const& priorities;
};

struct PhysicalDeviceQuery {
    uint32_t minimum_vk_version;
    const char* device_name_substring;
    PhysicalDeviceTypeFlags excluded_device_types;

    VkPhysicalDeviceFeatures minimum_features;
    VkPhysicalDeviceLimits minimum_limits;

    std::vector<VkExtensionProperties> const& required_extensions;

    std::vector<PhysicalDeviceFormatPropertyRequirement> const& minimum_format_properties;
    std::vector<PhysicalDeviceImageFormatPropertyRequirement> const& minimum_image_format_properties;

    VkPhysicalDeviceMemoryProperties minimum_memory_properties;

    //std::vector<VkQueueFamilyProperties> const& minimum_queue_family_properties;
    std::vector<PhysicalDeviceQueueRequirements> const& required_queues;
};

VkPhysicalDevice select_physical_device(VkInstance vk_instance, PhysicalDeviceQuery const& query);

struct ManualPhysicalDeviceSelection {
    std::vector<VkDeviceQueueCreateInfo> const& vk_queue_create_infos;
};

struct DevicePresets {
    bool recommended;
    bool enable_portability_subset;
    bool enable_swapchain;
    bool enable_dynamic_rendering;
    bool enable_maintenance1;
};

struct DeviceCreateInfo {
    VkPhysicalDevice vk_physical_device;
    VkFlags vk_flags;
    void* vk_pnext;
    std::vector<const char*> const& vk_extensions;
    std::optional<VkPhysicalDeviceFeatures> vk_enabled_features;

    union {
        ManualPhysicalDeviceSelection manual_selection;
        PhysicalDeviceQuery physical_device_query;
    };

    DevicePresets presets;
};

struct DeviceQueueReturn {
    VkQueue vk_queue;
    uint32_t family_index;
    uint32_t request_index;
    uint32_t queue_index;
};

VkResult create_device(VkInstance vk_instance, DeviceCreateInfo const& create_info, VkPhysicalDevice& vk_physical_device, VkDevice& vk_device, ArrayReference<DeviceQueueReturn> queue_returns);

struct SwapchainPreference {
    uint32_t image_count;
    uint32_t layer_count;

    VkSurfaceFormatKHR vk_surface_format;
    VkPresentModeKHR vk_present_mode;
};

struct SwapchainCreateInfo {
    VkPhysicalDevice vk_physical_device;
    VkSurfaceKHR vk_surface;
    VkSwapchainKHR vk_old_swapchain;

    void* vk_pnext;
    VkFlags vk_flags;
    std::optional<VkExtent2D> vk_extent;
    VkImageUsageFlags vk_image_usage;

    std::vector<SwapchainPreference> const& preferences;

    VkSharingMode vk_image_sharing_mode;
    std::vector<uint32_t> const& vk_queue_family_indices;

    VkSurfaceTransformFlagBitsKHR vk_pre_transform;
    VkCompositeAlphaFlagBitsKHR vk_composite_alpha;
    VkBool32 vk_clipped;
};

struct SwapchainReturns {
    VkSwapchainKHR vk_swapchain;

    uint32_t chosen_preference;
    VkExtent2D vk_current_extent;
    std::optional<ArrayReference<VkImage>> vk_backbuffers;
};

VkResult create_swapchain(VkDevice vk_device, SwapchainCreateInfo const& create_info, SwapchainReturns& returns);

namespace resource {

uint32_t find_memory_type_index(VkPhysicalDevice vk_physical_device, std::vector<VkMemoryRequirements> const& vk_memory_requirementses, VkMemoryPropertyFlags vk_memory_properties);

struct MonoAllocationResidentID {
    union {
        VkBuffer vk_buffer;
        VkImage vk_image;
    };

    bool is_image;

    bool operator==(MonoAllocationResidentID const& other) const {
        return (is_image == other.is_image) && (is_image ? (vk_image == other.vk_image) : (vk_buffer == other.vk_buffer));
    }
};

}

}

namespace std {

template<>
struct hash<kvk::resource::MonoAllocationResidentID> {
    size_t operator()(kvk::resource::MonoAllocationResidentID const& id) const noexcept {
        return reinterpret_cast<size_t>(id.vk_buffer);
    }
};

}

namespace kvk {

namespace resource {

struct MonoAllocationResident {
    MonoAllocationResidentID id;
    VkDeviceSize vk_heap_offset;
    VkDeviceSize vk_alignment;
    VkDeviceSize vk_size;
    bool bound;
};

struct MonoAllocationHeap {
    VkDeviceMemory vk_heap_memory;
    VkDeviceSize vk_heap_size;

    std::unordered_map<MonoAllocationResidentID, MonoAllocationResident> residents;
};

struct MonoAllocationCreateInfo {
    VkPhysicalDevice vk_physical_device;
    VkDeviceSize vk_minimum_heap_size;
    VkMemoryPropertyFlags vk_memory_properties;

    std::vector<MonoAllocationResidentID> const& residents;
};

VkResult mono_alloc_for_residents(VkDevice vk_device, MonoAllocationCreateInfo const& create_info, MonoAllocationHeap& heap);
VkResult mono_bind_residents(VkDevice vk_device, MonoAllocationHeap& heap);

/* NOTE: destroy all resident resources before freeing */
void mono_free_heap(VkDevice vk_device, MonoAllocationHeap& heap);

}

namespace shader {

#ifdef KVK_USE_DXC

namespace hlsl {

VkResult compile(VkDevice vk_device, std::string const& source, std::string const& entry, VkShaderStageFlags vk_shader_stage_flags, VkShaderModule& vk_shader_module);

}

#endif

}

}
