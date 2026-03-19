#define KVK_IMPLEMENTATION
#include "kvk.h"

#include <vector>
#include <format>
#include <limits>
#include <algorithm>

#include "kvk_util.inl"

namespace kvk {

static MessageCallback g_error_callback = nullptr;

void set_error_callback(MessageCallback callback) {
    g_error_callback = callback;
}

VkResult create_instance(InstanceCreateInfo const& create_info, VkInstance& vk_instance) {
    VkResult vk_result;
    VkApplicationInfo vk_application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = create_info.app_name,
        .applicationVersion = create_info.app_version,
        .pEngineName = "kvk",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = create_info.vk_version,
    };

    VkDebugUtilsMessengerCreateInfoEXT vk_debug_utils_messenger_create_info_ext = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = create_info.presets.debug_messenger_callback,
        .pUserData = create_info.presets.debug_messenger_callback_user_data,
    };

    VkDebugReportCallbackCreateInfoEXT vk_debug_report_callback_create_info_ext = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
        .pfnCallback = create_info.presets.debug_report_callback,
        .pUserData = create_info.presets.debug_report_callback_user_data,
    };

    uint32_t vk_available_instance_layer_properties_count;
    vk_result = vkEnumerateInstanceLayerProperties(&vk_available_instance_layer_properties_count, nullptr);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to enumerate instance layer properties");
        return vk_result;
    }

    std::vector<VkLayerProperties> vk_available_instance_layer_properties(vk_available_instance_layer_properties_count);
    vk_result = vkEnumerateInstanceLayerProperties(&vk_available_instance_layer_properties_count, vk_available_instance_layer_properties.data());
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to enumerate instance layer properties");
        return vk_result;
    }

    void* vk_pnext = create_info.vk_pnext;
    VkFlags vk_flags = create_info.vk_flags;

    std::vector<const char*> enabled_layers(create_info.vk_layers);
    std::vector<const char*> enabled_extensions(create_info.vk_extensions);

    /* specific optional presets */
    if (create_info.presets.enable_validation_layers) {
        if (std::find_if(vk_available_instance_layer_properties.begin(), vk_available_instance_layer_properties.end(), [](VkLayerProperties const& layer_properties) { return std::strcmp(layer_properties.layerName, "VK_LAYER_KHRONOS_validation") == 0; }) == vk_available_instance_layer_properties.end()) {
            KVK_ERR(VK_ERROR_LAYER_NOT_PRESENT, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "Validation layers requested but VK_LAYER_KHRONOS_validation not available");
        } else {
            enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
        }
    }

    if (create_info.presets.enable_debug_utils) {
        enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (create_info.presets.debug_messenger_callback) {
        vk_debug_utils_messenger_create_info_ext.pNext = vk_pnext;
        vk_pnext = &vk_debug_utils_messenger_create_info_ext;
        if (!create_info.presets.enable_debug_utils) {
            enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    if (create_info.presets.debug_report_callback) {
        vk_debug_report_callback_create_info_ext.pNext = vk_pnext;
        vk_pnext = &vk_debug_report_callback_create_info_ext;
        enabled_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    /* recommended & optional presets */
    if (create_info.presets.recommended || create_info.presets.create_enumerate_portability_instance) {
#ifndef KVK_APPLE
        if (!create_info.presets.recommended) {
#endif
            enabled_extensions.push_back("VK_KHR_portability_enumeration");
            vk_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#ifndef KVK_APPLE
        }
#endif
    }

    if (create_info.presets.recommended || create_info.presets.enable_surfaces) {
        enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    if (create_info.presets.recommended || create_info.presets.enable_platform_specific_surfaces) {
        /* TODO: refine platform-specific surface extension selection */
#ifdef KVK_WINDOWS
        enabled_extensions.push_back("VK_KHR_win32_surface");
#elif defined(KVK_APPLE)
        enabled_extensions.push_back("VK_EXT_metal_surface");
#else
        enabled_extensions.push_back("VK_KHR_xlib_surface");
#endif
    }

    VkInstanceCreateInfo vk_instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = vk_pnext,
        .flags = vk_flags,
        .pApplicationInfo = &vk_application_info,
        .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
        .ppEnabledLayerNames = enabled_layers.size() == 0 ? nullptr : enabled_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.size() == 0 ? nullptr : enabled_extensions.data(),
    };

    vk_result = vkCreateInstance(&vk_instance_create_info, nullptr, &vk_instance);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to create Vulkan instance");
        return vk_result;
    }

    return vk_result;
}

struct PhysicalDeviceFeaturesNext {
    VkBool32 a;
    VkBool32 b;
};

VkPhysicalDevice select_physical_device(VkInstance vk_instance, PhysicalDeviceQuery const& query) {
    VkResult vk_result;
    uint32_t physical_device_count = 0;
    vk_result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
    if (vk_result != VK_SUCCESS || physical_device_count == 0) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to enumerate Vulkan physical devices");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vk_result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices.data());
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to enumerate Vulkan physical devices");
        return VK_NULL_HANDLE;
    }

    for (VkPhysicalDevice physical_device : physical_devices) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
        if (query.device_name_substring != nullptr) {
            if (std::strstr(&physical_device_properties.deviceName[0], query.device_name_substring) == nullptr) {
                continue;
            }
        }

        if (((static_cast<uint32_t>(query.excluded_device_types)) & (1 << (static_cast<uint32_t>(physical_device_properties.deviceType)))) != 0) {
            continue;
        }

        if (physical_device_properties.apiVersion < query.minimum_vk_version) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" has insufficient api version (found {}.{}.{}, required {}.{}.{})", &physical_device_properties.deviceName[0], VK_API_VERSION_MAJOR(physical_device_properties.apiVersion), VK_API_VERSION_MINOR(physical_device_properties.apiVersion), VK_API_VERSION_PATCH(physical_device_properties.apiVersion), VK_API_VERSION_MAJOR(query.minimum_vk_version), VK_API_VERSION_MINOR(query.minimum_vk_version), VK_API_VERSION_PATCH(query.minimum_vk_version));
            continue;
        }

        VkPhysicalDeviceFeatures physical_device_features;
        vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

        bool satisfied = true;
        PhysicalDeviceFeaturesNext* features_next = reinterpret_cast<PhysicalDeviceFeaturesNext*>(&physical_device_features.robustBufferAccess);
        PhysicalDeviceFeaturesNext* features_final = reinterpret_cast<PhysicalDeviceFeaturesNext*>(&physical_device_features.inheritedQueries);

        uint32_t feature_index = 0;
        PhysicalDeviceFeaturesNext const* queried_features_next = reinterpret_cast<PhysicalDeviceFeaturesNext const*>(&query.minimum_features.robustBufferAccess);
        while (features_next <= features_final) {
            if (queried_features_next->a && !features_next->a) {
                KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum feature at index {}", &physical_device_properties.deviceName[0], feature_index * 2);
                satisfied = false;
                break;
            }

            features_next = reinterpret_cast<PhysicalDeviceFeaturesNext*>(&features_next->b);
            queried_features_next = reinterpret_cast<PhysicalDeviceFeaturesNext const*>(&queried_features_next->b);
            ++feature_index;
        }

        if (!satisfied) {
            continue;
        }

        if (physical_device_properties.limits < query.minimum_limits) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum limits", &physical_device_properties.deviceName[0]);
            continue;
        }

        uint32_t available_extension_count;
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(available_extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, available_extensions.data());

        for (uint32_t i = 0; i < query.required_extensions.size(); ++i) {
            bool found = false;
            for (uint32_t j = 0; j < available_extension_count; ++j) {
                if (std::strcmp(query.required_extensions[i].extensionName, available_extensions[j].extensionName) == 0) {
                    if (available_extensions[j].specVersion < query.required_extensions[i].specVersion) {
                        KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" has extension \"{}\" but with insufficient spec version (found {}, required {})", &physical_device_properties.deviceName[0], query.required_extensions[i].extensionName, available_extensions[j].specVersion, query.required_extensions[i].specVersion);
                        satisfied = false;
                        break;
                    }

                    found = true;
                    break;
                }
            }

            if (!found && satisfied) {
                KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" is missing required extension \"{}\"", &physical_device_properties.deviceName[0], query.required_extensions[i].extensionName);
                satisfied = false;
                break;
            }
        }

        if (!satisfied) {
            continue;
        }

        for (uint32_t i = 0; i < query.minimum_format_properties.size(); ++i) {
            VkFormatProperties format_properties;
            vkGetPhysicalDeviceFormatProperties(physical_device, query.minimum_format_properties[i].format, &format_properties);
            if ((format_properties.linearTilingFeatures & query.minimum_format_properties[i].minimum_properties.linearTilingFeatures) != query.minimum_format_properties[i].minimum_properties.linearTilingFeatures ||
                (format_properties.optimalTilingFeatures & query.minimum_format_properties[i].minimum_properties.optimalTilingFeatures) != query.minimum_format_properties[i].minimum_properties.optimalTilingFeatures ||
                (format_properties.bufferFeatures & query.minimum_format_properties[i].minimum_properties.bufferFeatures) != query.minimum_format_properties[i].minimum_properties.bufferFeatures) {
                KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum format properties for format {}", &physical_device_properties.deviceName[0], query.minimum_format_properties[i].format);
                satisfied = false;
                break;
            }
        }

        if (!satisfied) {
            continue;
        }

        for (uint32_t i = 0; i < query.minimum_image_format_properties.size(); ++i) {
            VkImageFormatProperties image_format_properties;
            vk_result = vkGetPhysicalDeviceImageFormatProperties(physical_device,
                query.minimum_image_format_properties[i].format,
                query.minimum_image_format_properties[i].image_type,
                query.minimum_image_format_properties[i].tiling,
                query.minimum_image_format_properties[i].usage,
                query.minimum_image_format_properties[i].flags,
                &image_format_properties);

            if (vk_result != VK_SUCCESS) {
                KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not support image format {} with the specified type, tiling, usage, and flags", &physical_device_properties.deviceName[0], query.minimum_image_format_properties[i].format);
                satisfied = false;
                break;
            }

            if (image_format_properties.maxExtent.width < query.minimum_image_format_properties[i].minimum_properties.maxExtent.width ||
                image_format_properties.maxExtent.height < query.minimum_image_format_properties[i].minimum_properties.maxExtent.height ||
                image_format_properties.maxExtent.depth < query.minimum_image_format_properties[i].minimum_properties.maxExtent.depth ||
                image_format_properties.maxMipLevels < query.minimum_image_format_properties[i].minimum_properties.maxMipLevels ||
                image_format_properties.maxArrayLayers < query.minimum_image_format_properties[i].minimum_properties.maxArrayLayers ||
                image_format_properties.sampleCounts < query.minimum_image_format_properties[i].minimum_properties.sampleCounts ||
                image_format_properties.maxResourceSize < query.minimum_image_format_properties[i].minimum_properties.maxResourceSize) {
                KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum image format properties for format {}", &physical_device_properties.deviceName[0], query.minimum_image_format_properties[i].format);
                satisfied = false;
                break;
            }
        }

        if (!satisfied) {
            continue;
        }

        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
        if (memory_properties.memoryTypeCount < query.minimum_memory_properties.memoryTypeCount) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum memory type count", &physical_device_properties.deviceName[0]);
            continue;
        }

        if (memory_properties.memoryHeapCount < query.minimum_memory_properties.memoryHeapCount) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum memory heap count", &physical_device_properties.deviceName[0]);
            continue;
        }

        uint32_t physical_device_queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &physical_device_queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_family_properties_list(physical_device_queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &physical_device_queue_family_count, queue_family_properties_list.data());

        for (uint32_t i = 0; i < query.required_queues.size(); ++i) {
            bool found = false;
            for (uint32_t j = 0; j < physical_device_queue_family_count; ++j) {
                if (query.required_queues[i].surface_support != nullptr) {
                    VkBool32 vk_surface_support;
                    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, j, query.required_queues[i].surface_support, &vk_surface_support);

                    if (!vk_surface_support) {
                        continue;
                    }
                }

                if (queue_family_properties_list[j].queueCount >= query.required_queues[i].properties.queueCount &&
                    (queue_family_properties_list[j].queueFlags & query.required_queues[i].properties.queueFlags) == query.required_queues[i].properties.queueFlags &&
                    queue_family_properties_list[j].minImageTransferGranularity.width >= query.required_queues[i].properties.minImageTransferGranularity.width &&
                    queue_family_properties_list[j].minImageTransferGranularity.height >= query.required_queues[i].properties.minImageTransferGranularity.height &&
                    queue_family_properties_list[j].minImageTransferGranularity.depth >= query.required_queues[i].properties.minImageTransferGranularity.depth) {

                    queue_family_properties_list[j].queueCount = 0; // prevent re-use

                    found = true;
                    break;
                }
            }

            if (!found) {
                KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum queue family properties at index {}", &physical_device_properties.deviceName[0], i);
                satisfied = false;
                break;
            }
        }

        if (!satisfied) {
            continue;
        }

        return physical_device;
    }

    return VK_NULL_HANDLE;
}

VkResult create_device(VkInstance vk_instance, DeviceCreateInfo const& create_info, VkPhysicalDevice& vk_physical_device, VkDevice& vk_device, ArrayReference<DeviceQueueReturn> queue_returns) {
    vk_physical_device = create_info.vk_physical_device;

    VkResult vk_result;
    uint32_t physical_device_count = 0;
    vk_result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
    if (vk_result != VK_SUCCESS || physical_device_count == 0) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to enumerate Vulkan physical devices");
        return vk_result;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vk_result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices.data());
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to enumerate Vulkan physical devices");
        return vk_result;
    }

    bool satisfied = true;
    VkPhysicalDeviceProperties physical_device_properties;
    std::vector<VkExtensionProperties> available_extensions;
    VkPhysicalDeviceMemoryProperties memory_properties;
    std::vector<VkQueueFamilyProperties> queue_family_properties_list;

    std::vector<VkDeviceQueueCreateInfo> vk_device_queue_create_infos;
    if (vk_physical_device == nullptr) {
        for (VkPhysicalDevice physical_device : physical_devices) {
            satisfied = true;

            vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
            if (create_info.physical_device_query.device_name_substring != nullptr) {
                if (std::strstr(&physical_device_properties.deviceName[0], create_info.physical_device_query.device_name_substring) == nullptr) {
                    continue;
                }
            }

            if ((static_cast<uint32_t>(create_info.physical_device_query.excluded_device_types) & (1 << static_cast<uint32_t>(physical_device_properties.deviceType))) != 0) {
                continue;
            }

            if (physical_device_properties.apiVersion < create_info.physical_device_query.minimum_vk_version) {
                KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" has insufficient api version (found {}.{}.{}, required {}.{}.{})", &physical_device_properties.deviceName[0], VK_API_VERSION_MAJOR(physical_device_properties.apiVersion), VK_API_VERSION_MINOR(physical_device_properties.apiVersion), VK_API_VERSION_PATCH(physical_device_properties.apiVersion), VK_API_VERSION_MAJOR(create_info.physical_device_query.minimum_vk_version), VK_API_VERSION_MINOR(create_info.physical_device_query.minimum_vk_version), VK_API_VERSION_PATCH(create_info.physical_device_query.minimum_vk_version));
                continue;
            }

            VkPhysicalDeviceFeatures available_physical_device_features;
            vkGetPhysicalDeviceFeatures(physical_device, &available_physical_device_features);

            PhysicalDeviceFeaturesNext* features_next = reinterpret_cast<PhysicalDeviceFeaturesNext*>(&available_physical_device_features.robustBufferAccess);
            PhysicalDeviceFeaturesNext* features_final = reinterpret_cast<PhysicalDeviceFeaturesNext*>(&available_physical_device_features.inheritedQueries);

            uint32_t feature_index = 0;
            PhysicalDeviceFeaturesNext const* queried_features_next = reinterpret_cast<PhysicalDeviceFeaturesNext const*>(&create_info.physical_device_query.minimum_features.robustBufferAccess);
            while (features_next <= features_final) {
                if (queried_features_next->a && !features_next->a) {
                    KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum feature at index {}", &physical_device_properties.deviceName[0], feature_index * 2);
                    satisfied = false;
                    break;
                }

                features_next = reinterpret_cast<PhysicalDeviceFeaturesNext*>(&features_next->b);
                queried_features_next = reinterpret_cast<PhysicalDeviceFeaturesNext const*>(&queried_features_next->b);
                ++feature_index;
            }

            if (!satisfied) {
                continue;
            }

            if (physical_device_properties.limits < create_info.physical_device_query.minimum_limits) {
                KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum limits", &physical_device_properties.deviceName[0]);
                continue;
            }

            uint32_t available_extension_count;
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, nullptr);

            available_extensions.resize(available_extension_count);
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, available_extensions.data());

            for (uint32_t i = 0; i < create_info.physical_device_query.required_extensions.size(); ++i) {
                bool found = false;
                for (uint32_t j = 0; j < available_extension_count; ++j) {
                    if (std::strcmp(create_info.physical_device_query.required_extensions[i].extensionName, available_extensions[j].extensionName) == 0) {
                        if (available_extensions[j].specVersion < create_info.physical_device_query.required_extensions[i].specVersion) {
                            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" has extension \"{}\" but with insufficient spec version (found {}, required {})", &physical_device_properties.deviceName[0], create_info.physical_device_query.required_extensions[i].extensionName, available_extensions[j].specVersion, create_info.physical_device_query.required_extensions[i].specVersion);
                            satisfied = false;
                            break;
                        }

                        found = true;
                        break;
                    }
                }

                if (!found && satisfied) {
                    KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" is missing required extension \"{}\"", &physical_device_properties.deviceName[0], create_info.physical_device_query.required_extensions[i].extensionName);
                    satisfied = false;
                    break;
                }
            }

            if (!satisfied) {
                continue;
            }

            for (uint32_t i = 0; i < create_info.physical_device_query.minimum_format_properties.size(); ++i) {
                VkFormatProperties format_properties;
                vkGetPhysicalDeviceFormatProperties(physical_device, create_info.physical_device_query.minimum_format_properties[i].format, &format_properties);
                if ((format_properties.linearTilingFeatures & create_info.physical_device_query.minimum_format_properties[i].minimum_properties.linearTilingFeatures) != create_info.physical_device_query.minimum_format_properties[i].minimum_properties.linearTilingFeatures ||
                    (format_properties.optimalTilingFeatures & create_info.physical_device_query.minimum_format_properties[i].minimum_properties.optimalTilingFeatures) != create_info.physical_device_query.minimum_format_properties[i].minimum_properties.optimalTilingFeatures ||
                    (format_properties.bufferFeatures & create_info.physical_device_query.minimum_format_properties[i].minimum_properties.bufferFeatures) != create_info.physical_device_query.minimum_format_properties[i].minimum_properties.bufferFeatures) {
                    KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum format properties for format {}", &physical_device_properties.deviceName[0], create_info.physical_device_query.minimum_format_properties[i].format);
                    satisfied = false;
                    break;
                }
            }

            if (!satisfied) {
                continue;
            }

            for (uint32_t i = 0; i < create_info.physical_device_query.minimum_image_format_properties.size(); ++i) {
                VkImageFormatProperties image_format_properties;
                vk_result = vkGetPhysicalDeviceImageFormatProperties(physical_device,
                    create_info.physical_device_query.minimum_image_format_properties[i].format,
                    create_info.physical_device_query.minimum_image_format_properties[i].image_type,
                    create_info.physical_device_query.minimum_image_format_properties[i].tiling,
                    create_info.physical_device_query.minimum_image_format_properties[i].usage,
                    create_info.physical_device_query.minimum_image_format_properties[i].flags,
                    &image_format_properties);

                if (vk_result != VK_SUCCESS) {
                    KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not support image format {} with the specified type, tiling, usage, and flags", &physical_device_properties.deviceName[0], create_info.physical_device_query.minimum_image_format_properties[i].format);
                    satisfied = false;
                    break;
                }

                if (image_format_properties.maxExtent.width < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.maxExtent.width ||
                    image_format_properties.maxExtent.height < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.maxExtent.height ||
                    image_format_properties.maxExtent.depth < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.maxExtent.depth ||
                    image_format_properties.maxMipLevels < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.maxMipLevels ||
                    image_format_properties.maxArrayLayers < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.maxArrayLayers ||
                    image_format_properties.sampleCounts < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.sampleCounts ||
                    image_format_properties.maxResourceSize < create_info.physical_device_query.minimum_image_format_properties[i].minimum_properties.maxResourceSize) {
                    KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum image format properties for format {}", &physical_device_properties.deviceName[0], create_info.physical_device_query.minimum_image_format_properties[i].format);
                    satisfied = false;
                    break;
                }
            }

            if (!satisfied) {
                continue;
            }

            vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
            if (memory_properties.memoryTypeCount < create_info.physical_device_query.minimum_memory_properties.memoryTypeCount) {
                KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum memory type count", &physical_device_properties.deviceName[0]);
                continue;
            }

            if (memory_properties.memoryHeapCount < create_info.physical_device_query.minimum_memory_properties.memoryHeapCount) {
                KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum memory heap count", &physical_device_properties.deviceName[0]);
                continue;
            }

            uint32_t physical_device_queue_family_count;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &physical_device_queue_family_count, nullptr);

            queue_family_properties_list.resize(physical_device_queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &physical_device_queue_family_count, queue_family_properties_list.data());

            for (uint32_t i = 0; i < create_info.physical_device_query.required_queues.size(); ++i) {
                bool found = false;
                for (uint32_t j = 0; j < physical_device_queue_family_count; ++j) {
                    if (queue_family_properties_list[j].queueCount >= create_info.physical_device_query.required_queues[i].properties.queueCount &&
                        (queue_family_properties_list[j].queueFlags & create_info.physical_device_query.required_queues[i].properties.queueFlags) == create_info.physical_device_query.required_queues[i].properties.queueFlags &&
                        queue_family_properties_list[j].minImageTransferGranularity.width >= create_info.physical_device_query.required_queues[i].properties.minImageTransferGranularity.width &&
                        queue_family_properties_list[j].minImageTransferGranularity.height >= create_info.physical_device_query.required_queues[i].properties.minImageTransferGranularity.height &&
                        queue_family_properties_list[j].minImageTransferGranularity.depth >= create_info.physical_device_query.required_queues[i].properties.minImageTransferGranularity.depth) {

                        queue_family_properties_list[j].queueCount = 0; // prevent re-use

                        if (create_info.vk_physical_device == nullptr) {
                            if (create_info.physical_device_query.required_queues[i].priorities.size() != create_info.physical_device_query.required_queues[i].properties.queueCount) {
                                KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "{} device queues requested with only {} priority values; these must match", create_info.physical_device_query.required_queues[i].properties.queueCount, create_info.physical_device_query.required_queues[i].priorities.size());
                                return VK_ERROR_INITIALIZATION_FAILED;
                            }

                            VkDeviceQueueCreateInfo vk_device_queue_create_info = {
                                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                .pNext = nullptr,
                                .flags = create_info.physical_device_query.required_queues[i].create_flags,
                                .queueFamilyIndex = j,
                                .queueCount = create_info.physical_device_query.required_queues[i].properties.queueCount,
                                .pQueuePriorities = create_info.physical_device_query.required_queues[i].priorities.data(),
                            };

                            vk_device_queue_create_infos.push_back(vk_device_queue_create_info);
                        }

                        found = true;
                        break;
                    }
                }

                if (!found) {
                    KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Physical device \"{}\" does not satisfy minimum queue family properties at index {}", &physical_device_properties.deviceName[0], i);
                    satisfied = false;
                    break;
                }
            }

            if (!satisfied) {
                continue;
            }

            vk_physical_device = physical_device;
            break;
        }

        if (!satisfied) {
            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to find a suitable Vulkan physical device for logical device creation");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    uint32_t queue_count = 0;
    for (VkDeviceQueueCreateInfo const& ci : vk_device_queue_create_infos) {
        queue_count += ci.queueCount;
    }

    if (queue_returns.size() < queue_count) {
        if (!queue_returns.resize(queue_count)) {
            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to resize guest return array for device queues to size {}", vk_device_queue_create_infos.size());
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    VkFlags vk_flags = create_info.vk_flags;
    void* vk_pnext = create_info.vk_pnext;
    std::vector<const char*> enabled_extensions(create_info.vk_extensions);
    VkPhysicalDeviceFeatures enabled_features = {};
    if (create_info.vk_enabled_features.has_value()) {
        enabled_features = create_info.vk_enabled_features.value();
    } else if (create_info.vk_physical_device == nullptr) {
        enabled_features = create_info.physical_device_query.minimum_features;
    }

    if (create_info.presets.enable_portability_subset) {
        enabled_extensions.push_back("VK_KHR_portability_subset");
    } else if (create_info.presets.recommended) {
#ifdef KVK_APPLE
        enabled_extensions.push_back("VK_KHR_portability_subset");
#endif
    }

    if (create_info.presets.recommended || create_info.presets.enable_swapchain) {
        enabled_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT vk_pageable_device_local_memory_features_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,
        .pNext = nullptr,
        .pageableDeviceLocalMemory = VK_TRUE,
    };

    VkPhysicalDeviceMemoryPriorityFeaturesEXT vk_memory_priority_features_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,
        .pNext = nullptr,
        .memoryPriority = VK_TRUE,
    };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR vk_dynamic_rendering_features_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = nullptr,
        .dynamicRendering = VK_TRUE,
    };

    #define KVK_TMP_HAS_EXT(list_, name_) (std::find_if(list_.begin(), list_.end(), [](VkExtensionProperties const& p) -> bool { return std::strcmp(p.extensionName, name_) == 0; }) != list_.end())

    if (create_info.presets.recommended && KVK_TMP_HAS_EXT(available_extensions, VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME)) {
        enabled_extensions.push_back(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
        vk_pageable_device_local_memory_features_ext.pNext = vk_pnext;
        vk_pnext = &vk_pageable_device_local_memory_features_ext;
    }

    if (create_info.presets.recommended && KVK_TMP_HAS_EXT(available_extensions, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
        enabled_extensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
        vk_memory_priority_features_ext.pNext = vk_pnext;
        vk_pnext = &vk_memory_priority_features_ext;
    }

    if (create_info.presets.enable_dynamic_rendering) {
        enabled_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        vk_dynamic_rendering_features_ext.pNext = vk_pnext;
        vk_pnext = &vk_dynamic_rendering_features_ext;
    }

    #undef KVK_TMP_HAS_EXT

    VkDeviceCreateInfo vk_device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = vk_pnext,
        .flags = vk_flags,
        .queueCreateInfoCount = static_cast<uint32_t>(vk_device_queue_create_infos.size()),
        .pQueueCreateInfos = vk_device_queue_create_infos.size() == 0 ? nullptr : vk_device_queue_create_infos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.size() == 0 ? nullptr : enabled_extensions.data(),
        .pEnabledFeatures = &enabled_features,
    };

    if (create_info.vk_physical_device != nullptr) {
        vk_device_create_info.queueCreateInfoCount = static_cast<uint32_t>(create_info.manual_selection.vk_queue_create_infos.size());
        vk_device_create_info.pQueueCreateInfos = create_info.manual_selection.vk_queue_create_infos.size() == 0 ? nullptr : create_info.manual_selection.vk_queue_create_infos.data();
    }

    vk_result = vkCreateDevice(vk_physical_device, &vk_device_create_info, nullptr, &vk_device);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to create Vulkan device");
        return vk_result;
    }

    uint32_t queue_index = 0;
    for (uint32_t i = 0; i < vk_device_queue_create_infos.size(); ++i) {
        for (uint32_t j = 0; j < vk_device_queue_create_infos[i].queueCount; ++j) {
            VkQueue vk_queue;
            vkGetDeviceQueue(vk_device, vk_device_queue_create_infos[i].queueFamilyIndex, j, &vk_queue);

            queue_returns[queue_index] = {
                .vk_queue = vk_queue,
                .family_index = vk_device_queue_create_infos[i].queueFamilyIndex,
                .request_index = i,
                .queue_index = j,
            };

            ++queue_index;
        }
    }

    return vk_result;
}

VkResult create_swapchain(VkDevice vk_device, SwapchainCreateInfo const& create_info, SwapchainReturns& returns) {
    VkSurfaceCapabilitiesKHR vk_surface_capabilities;
    VkResult vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(create_info.vk_physical_device, create_info.vk_surface, &vk_surface_capabilities);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan physical device surface capabilities for swapchain creation");
        return vk_result;
    }

    VkExtent2D vk_swapchain_extent = vk_surface_capabilities.currentExtent;
    if (create_info.vk_extent.has_value()) {
        if (create_info.vk_extent->width < vk_surface_capabilities.minImageExtent.width) {
            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Requested swapchain extent width {} is less than the surface's minimum supported width {}", create_info.vk_extent->width, vk_surface_capabilities.minImageExtent.width);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (create_info.vk_extent->width > vk_surface_capabilities.maxImageExtent.width) {
            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Requested swapchain extent width {} is greater than the surface's maximum supported width {}", create_info.vk_extent->width, vk_surface_capabilities.maxImageExtent.width);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (create_info.vk_extent->height < vk_surface_capabilities.minImageExtent.height) {
            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Requested swapchain extent height {} is less than the surface's minimum supported height {}", create_info.vk_extent->height, vk_surface_capabilities.minImageExtent.height);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (create_info.vk_extent->height > vk_surface_capabilities.maxImageExtent.height) {
            KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Requested swapchain extent height {} is greater than the surface's maximum supported height {}", create_info.vk_extent->height, vk_surface_capabilities.maxImageExtent.height);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        vk_swapchain_extent = create_info.vk_extent.value();
    }

    uint32_t vk_surface_format_count;
    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(create_info.vk_physical_device, create_info.vk_surface, &vk_surface_format_count, nullptr);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan list of compatible surface formats for swapchain creation");
        return vk_result;
    }

    std::vector<VkSurfaceFormatKHR> vk_surface_formats(vk_surface_format_count);
    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(create_info.vk_physical_device, create_info.vk_surface, &vk_surface_format_count, vk_surface_formats.data());
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan list of compatible surface formats for swapchain creation");
        return vk_result;
    }

    uint32_t vk_present_mode_count;
    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(create_info.vk_physical_device, create_info.vk_surface, &vk_present_mode_count, nullptr);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan list of compatible present modes for swapchain creation");
        return vk_result;
    }

    std::vector<VkPresentModeKHR> vk_present_modes(vk_present_mode_count);
    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(create_info.vk_physical_device, create_info.vk_surface, &vk_present_mode_count, vk_present_modes.data());
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan list of compatible present modes for swapchain creation");
        return vk_result;
    }

    uint32_t chosen_preference = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < create_info.preferences.size(); ++i) {
        SwapchainPreference const& p = create_info.preferences[i];
        if (p.image_count < vk_surface_capabilities.minImageCount) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Swapchain preference at index {} requests image count {} which is less than the surface's minimum supported image count {}", i, p.image_count, vk_surface_capabilities.minImageCount);
            continue;
        }

        if (p.layer_count > vk_surface_capabilities.maxImageArrayLayers) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Swapchain preference at index {} requests layer count {} which is greater than the surface's maximum supported layer count {}", i, p.layer_count, vk_surface_capabilities.maxImageArrayLayers);
            continue;
        }

        if (std::find(vk_surface_formats.begin(), vk_surface_formats.end(), p.vk_surface_format) == vk_surface_formats.end()) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Swapchain preference at index {} requests unsupported surface format (format {}, color space {})", i, p.vk_surface_format.format, p.vk_surface_format.colorSpace);
            continue;
        }

        if (std::find(vk_present_modes.begin(), vk_present_modes.end(), p.vk_present_mode) == vk_present_modes.end()) {
            KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Swapchain preference at index {} requests unsupported present mode {}", i, p.vk_present_mode);
            continue;
        }

        KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Swapchain preference at index {} selected for swapchain creation (image count {}, layer count {}, format {}, color space {}, present mode {})", i, p.image_count, p.layer_count, p.vk_surface_format.format, p.vk_surface_format.colorSpace, p.vk_present_mode);
        chosen_preference = i;
        break;
    }

    if (chosen_preference == std::numeric_limits<uint32_t>::max()) {
        KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to find a suitable swapchain preference for swapchain creation");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    SwapchainPreference const& preference = create_info.preferences[chosen_preference];

    VkSwapchainCreateInfoKHR vk_swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = create_info.vk_pnext,
        .flags = create_info.vk_flags,
        .surface = create_info.vk_surface,
        .minImageCount = preference.image_count,
        .imageFormat = preference.vk_surface_format.format,
        .imageColorSpace = preference.vk_surface_format.colorSpace,
        .imageExtent = vk_swapchain_extent,
        .imageArrayLayers = preference.layer_count,
        .imageUsage = create_info.vk_image_usage,
        .imageSharingMode = create_info.vk_image_sharing_mode,
        .queueFamilyIndexCount = static_cast<uint32_t>(create_info.vk_queue_family_indices.size()),
        .pQueueFamilyIndices = create_info.vk_queue_family_indices.size() == 0 ? nullptr : create_info.vk_queue_family_indices.data(),
        .preTransform = create_info.vk_pre_transform,
        .compositeAlpha = create_info.vk_composite_alpha,
        .presentMode = preference.vk_present_mode,
        .clipped = create_info.vk_clipped,
        .oldSwapchain = create_info.vk_old_swapchain,
    };

    VkSwapchainKHR vk_swapchain;
    vk_result = vkCreateSwapchainKHR(vk_device, &vk_swapchain_create_info, nullptr, &vk_swapchain);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to create Vulkan swapchain");
        return vk_result;
    }

    if (returns.vk_backbuffers.has_value()) {
        uint32_t vk_image_count;
        vk_result = vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_image_count, nullptr);
        if (vk_result != VK_SUCCESS) {
            KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan swapchain backbuffer image count");
            return vk_result;
        }

        if (returns.vk_backbuffers->size() != vk_image_count) {
            if (!returns.vk_backbuffers->resize(vk_image_count)) {
                vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
                KVK_ERR(VK_ERROR_INITIALIZATION_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to resize guest return array for swapchain backbuffer images to size {}", vk_image_count);
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }

        vk_result = vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_image_count, &returns.vk_backbuffers.value()[0]);
        if (vk_result != VK_SUCCESS) {
            KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to get Vulkan swapchain backbuffer images");
            return vk_result;
        }
    }

    returns.vk_swapchain = vk_swapchain;
    returns.chosen_preference = chosen_preference;
    returns.vk_current_extent = vk_swapchain_extent;

    return VK_SUCCESS;
}

namespace resource {

/* adapted from my previous Odin code (https://github.com/krisvers/vulkan-sandbox/blob/e3a6738e790bcab9647da5218fe49cd728bf0ade/main.odin#L155) */
uint32_t find_memory_type_index(VkPhysicalDevice vk_physical_device, std::vector<VkMemoryRequirements> const& vk_memory_requirementses, VkMemoryPropertyFlags vk_memory_properties) {
    VkPhysicalDeviceMemoryProperties vk_physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(vk_physical_device, &vk_physical_device_memory_properties);

    for (uint32_t i = 0; i < vk_physical_device_memory_properties.memoryTypeCount; ++i) {
        if ((vk_physical_device_memory_properties.memoryTypes[i].propertyFlags & vk_memory_properties) != vk_memory_properties) {
            continue;
        }

        bool incompatible = false;
        for (VkMemoryRequirements const& r : vk_memory_requirementses) {
            if ((r.memoryTypeBits & (1 << i)) == 0) {
                incompatible = true;
                break;
            }
        }

        if (!incompatible) {
            return i;
        }
    }

    KVK_ERR(VK_SUCCESS, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "Failed to find compatible memory type index");
    return VK_MAX_MEMORY_TYPES;
}

/* adapted from my previous Odin code (https://github.com/krisvers/vulkan-sandbox/blob/e3a6738e790bcab9647da5218fe49cd728bf0ade/main.odin#L200) */
VkResult mono_alloc_for_residents(VkDevice vk_device, MonoAllocationCreateInfo const& create_info, MonoAllocationHeap& heap) {
    uint32_t memory_type_index;
    std::vector<VkMemoryRequirements> vk_memory_requirementses(create_info.residents.size());
    std::vector<VkDeviceSize> vk_sizes(create_info.residents.size());
    std::vector<VkDeviceSize> vk_offsets(create_info.residents.size());

    VkDeviceSize total_size = 0;
    for (size_t i = 0; i < create_info.residents.size(); ++i) {
        if (create_info.residents[i].is_image) {
            vkGetImageMemoryRequirements(vk_device, create_info.residents[i].vk_image, &vk_memory_requirementses[i]);
        } else {
            vkGetBufferMemoryRequirements(vk_device, create_info.residents[i].vk_buffer, &vk_memory_requirementses[i]);
        }

        VkDeviceSize r = total_size % vk_memory_requirementses[i].alignment;
        total_size = total_size + (r == 0 ? 0 : vk_memory_requirementses[i].alignment - r);
        vk_offsets[i] = total_size;
        vk_sizes[i] = vk_memory_requirementses[i].size;
        total_size += vk_sizes[i];
    }

    total_size = std::max(total_size, create_info.vk_minimum_heap_size);
    memory_type_index = find_memory_type_index(create_info.vk_physical_device, vk_memory_requirementses, create_info.vk_memory_properties);
    if (memory_type_index == VK_MAX_MEMORY_TYPES) {
        KVK_ERR(VK_ERROR_MEMORY_MAP_FAILED, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to find suitable memory type for mono allocation");
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    VkMemoryAllocateInfo vk_memory_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = total_size,
        .memoryTypeIndex = memory_type_index,
    };

    VkDeviceMemory vk_heap_memory;
    VkResult vk_result = vkAllocateMemory(vk_device, &vk_memory_allocate_info, nullptr, &vk_heap_memory);
    if (vk_result != VK_SUCCESS) {
        KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to allocate memory for mono allocation");
        return vk_result;
    }

    heap.vk_heap_memory = vk_heap_memory;
    heap.vk_heap_size = total_size;
    heap.residents = {};

    for (size_t i = 0; i < create_info.residents.size(); ++i) {
        heap.residents[create_info.residents[i]] = {
            .id = create_info.residents[i],
            .vk_heap_offset = vk_offsets[i],
            .vk_alignment = vk_memory_requirementses[i].alignment,
            .vk_size = vk_sizes[i],
        };
    }

    return VK_SUCCESS;
}

VkResult mono_bind_residents(VkDevice vk_device, MonoAllocationHeap& heap) {
    for (auto& p : heap.residents) {
        if (p.second.bound) {
            continue;
        }

        VkResult vk_result;
        if (p.second.id.is_image) {
            vk_result = vkBindImageMemory(vk_device, p.first.vk_image, heap.vk_heap_memory, p.second.vk_heap_offset);
            if (vk_result != VK_SUCCESS) {
                KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to bind image to heap memory");
                return vk_result;
            }
        } else {
            vk_result = vkBindBufferMemory(vk_device, p.first.vk_buffer, heap.vk_heap_memory, p.second.vk_heap_offset);
            if (vk_result != VK_SUCCESS) {
                KVK_ERR(vk_result, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "Failed to bind buffer to heap memory");
                return vk_result;
            }
        }

        p.second.bound = true;
    }

    return VK_SUCCESS;
}

void mono_free_heap(VkDevice vk_device, MonoAllocationHeap& heap) {
    if (heap.vk_heap_memory != VK_NULL_HANDLE) {
        for (auto& p : heap.residents) {
            if (!p.second.bound) {
                continue;
            }

            if (p.first.is_image) {
                
            }
        }

        vkFreeMemory(vk_device, heap.vk_heap_memory, nullptr);
        heap.vk_heap_memory = VK_NULL_HANDLE;
        heap.vk_heap_size = 0;
        heap.residents.clear();
    }
}

}

namespace shader {

#ifdef KVK_USE_DXC

namespace hlsl {

/* adapted from https://github.com/krisvers/vulkan-sandbox/blob/e3a6738e790bcab9647da5218fe49cd728bf0ade/main.odin#L342 */
VkResult compile(VkDevice vk_device, std::string const& source, std::string const& entry, VkShaderStageFlags vk_shader_stage_flags, VkShaderModule& vk_shader_module) {
    /* TODO: implement */
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

}

#endif

}

}
