#include <vulkan/vulkan.h>

#include <format>

#ifndef KVK_WINDOWS
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#define KVK_WINDOWS
#endif
#endif

#ifndef KVK_APPLE
#if defined(__APPLE__)
#define KVK_APPLE
#endif
#endif

#ifndef KVK_FUNCTION
#ifdef _MSC_VER
#define KVK_FUNCTION __FUNCSIG__
#else
#define KVK_FUNCTION __PRETTY_FUNCTION__
#endif
#endif

#define KVK_ERR(res_, sev_, msg_, ...) if (g_error_callback != nullptr) { g_error_callback(res_, sev_, std::format(msg_, ##__VA_ARGS__).c_str(), KVK_FUNCTION); }

template<>
struct std::formatter<VkFormat> {
    template<class ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FormatContext>
    auto format(VkFormat const& vk_format, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", static_cast<int32_t>(vk_format));
    }
};

template<>
struct std::formatter<VkColorSpaceKHR> {
    template<class ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FormatContext>
    auto format(VkColorSpaceKHR const& vk_color_space, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", static_cast<int32_t>(vk_color_space));
    }
};

template<>
struct std::formatter<VkPresentModeKHR> {
    template<class ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FormatContext>
    auto format(VkPresentModeKHR const& vk_present_mode, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", static_cast<int32_t>(vk_present_mode));
    }
};

inline bool operator==(VkSurfaceFormatKHR const& a, VkSurfaceFormatKHR const& b) {
    return (a.format == b.format) && (a.colorSpace == b.colorSpace);
}

inline bool operator<(VkPhysicalDeviceLimits const& a, VkPhysicalDeviceLimits const& b) {
    return (a.maxImageDimension1D < b.maxImageDimension1D) ||
        (a.maxImageDimension2D < b.maxImageDimension2D) ||
        (a.maxImageDimension3D < b.maxImageDimension3D) ||
        (a.maxImageDimensionCube < b.maxImageDimensionCube) ||
        (a.maxImageArrayLayers < b.maxImageArrayLayers) ||
        (a.maxTexelBufferElements < b.maxTexelBufferElements) ||
        (a.maxUniformBufferRange < b.maxUniformBufferRange) ||
        (a.maxStorageBufferRange < b.maxStorageBufferRange) ||
        (a.maxPushConstantsSize < b.maxPushConstantsSize) ||
        (a.maxMemoryAllocationCount < b.maxMemoryAllocationCount) ||
        (a.maxSamplerAllocationCount < b.maxSamplerAllocationCount) ||
        (a.bufferImageGranularity < b.bufferImageGranularity) ||
        (a.sparseAddressSpaceSize < b.sparseAddressSpaceSize) ||
        (a.maxBoundDescriptorSets < b.maxBoundDescriptorSets) ||
        (a.maxPerStageDescriptorSamplers < b.maxPerStageDescriptorSamplers) ||
        (a.maxPerStageDescriptorUniformBuffers < b.maxPerStageDescriptorUniformBuffers) ||
        (a.maxPerStageDescriptorStorageBuffers < b.maxPerStageDescriptorStorageBuffers) ||
        (a.maxPerStageDescriptorSampledImages < b.maxPerStageDescriptorSampledImages) ||
        (a.maxPerStageDescriptorStorageImages < b.maxPerStageDescriptorStorageImages) ||
        (a.maxPerStageDescriptorInputAttachments < b.maxPerStageDescriptorInputAttachments) ||
        (a.maxPerStageResources < b.maxPerStageResources) ||
        (a.maxDescriptorSetSamplers < b.maxDescriptorSetSamplers) ||
        (a.maxDescriptorSetUniformBuffers < b.maxDescriptorSetUniformBuffers) ||
        (a.maxDescriptorSetUniformBuffersDynamic < b.maxDescriptorSetUniformBuffersDynamic) ||
        (a.maxDescriptorSetStorageBuffers < b.maxDescriptorSetStorageBuffers) ||
        (a.maxDescriptorSetStorageBuffersDynamic < b.maxDescriptorSetStorageBuffersDynamic) ||
        (a.maxDescriptorSetSampledImages < b.maxDescriptorSetSampledImages) ||
        (a.maxDescriptorSetStorageImages < b.maxDescriptorSetStorageImages) ||
        (a.maxDescriptorSetInputAttachments < b.maxDescriptorSetInputAttachments) ||
        (a.maxVertexInputAttributes < b.maxVertexInputAttributes) ||
        (a.maxVertexInputBindings < b.maxVertexInputBindings) ||
        (a.maxVertexInputAttributeOffset < b.maxVertexInputAttributeOffset) ||
        (a.maxVertexInputBindingStride < b.maxVertexInputBindingStride) ||
        (a.maxVertexOutputComponents < b.maxVertexOutputComponents) ||
        (a.maxTessellationGenerationLevel < b.maxTessellationGenerationLevel) ||
        (a.maxTessellationPatchSize < b.maxTessellationPatchSize) ||
        (a.maxTessellationControlPerVertexInputComponents < b.maxTessellationControlPerVertexInputComponents) ||
        (a.maxTessellationControlPerVertexOutputComponents < b.maxTessellationControlPerVertexOutputComponents) ||
        (a.maxTessellationControlPerPatchOutputComponents < b.maxTessellationControlPerPatchOutputComponents) ||
        (a.maxTessellationControlTotalOutputComponents < b.maxTessellationControlTotalOutputComponents) ||
        (a.maxTessellationEvaluationInputComponents < b.maxTessellationEvaluationInputComponents) ||
        (a.maxTessellationEvaluationOutputComponents < b.maxTessellationEvaluationOutputComponents) ||
        (a.maxGeometryShaderInvocations < b.maxGeometryShaderInvocations) ||
        (a.maxGeometryInputComponents < b.maxGeometryInputComponents) ||
        (a.maxGeometryOutputComponents < b.maxGeometryOutputComponents) ||
        (a.maxGeometryOutputVertices < b.maxGeometryOutputVertices) ||
        (a.maxGeometryTotalOutputComponents < b.maxGeometryTotalOutputComponents) ||
        (a.maxFragmentInputComponents < b.maxFragmentInputComponents) ||
        (a.maxFragmentOutputAttachments < b.maxFragmentOutputAttachments) ||
        (a.maxFragmentDualSrcAttachments < b.maxFragmentDualSrcAttachments) ||
        (a.maxFragmentCombinedOutputResources < b.maxFragmentCombinedOutputResources) ||
        (a.maxComputeSharedMemorySize < b.maxComputeSharedMemorySize) ||
        (a.maxComputeWorkGroupCount[0] < b.maxComputeWorkGroupCount[0]) ||
        (a.maxComputeWorkGroupCount[1] < b.maxComputeWorkGroupCount[1]) ||
        (a.maxComputeWorkGroupCount[2] < b.maxComputeWorkGroupCount[2]) ||
        (a.maxComputeWorkGroupInvocations < b.maxComputeWorkGroupInvocations) ||
        (a.maxComputeWorkGroupSize[0] < b.maxComputeWorkGroupSize[0]) ||
        (a.maxComputeWorkGroupSize[1] < b.maxComputeWorkGroupSize[1]) ||
        (a.maxComputeWorkGroupSize[2] < b.maxComputeWorkGroupSize[2]) ||
        (a.subPixelPrecisionBits < b.subPixelPrecisionBits) ||
        (a.subTexelPrecisionBits < b.subTexelPrecisionBits) ||
        (a.mipmapPrecisionBits < b.mipmapPrecisionBits) ||
        (a.maxDrawIndexedIndexValue < b.maxDrawIndexedIndexValue) ||
        (a.maxDrawIndirectCount < b.maxDrawIndirectCount) ||
        (a.maxSamplerLodBias < b.maxSamplerLodBias) ||
        (a.maxSamplerAnisotropy < b.maxSamplerAnisotropy) ||
        (a.maxViewports < b.maxViewports) ||
        (a.maxViewportDimensions[0] < b.maxViewportDimensions[0]) ||
        (a.maxViewportDimensions[1] < b.maxViewportDimensions[1]) ||
        (a.viewportBoundsRange[0] > b.viewportBoundsRange[0]) ||
        (a.viewportBoundsRange[1] < b.viewportBoundsRange[1]) ||
        (a.viewportSubPixelBits < b.viewportSubPixelBits) ||
        (a.minMemoryMapAlignment < b.minMemoryMapAlignment) ||
        (a.minTexelBufferOffsetAlignment < b.minTexelBufferOffsetAlignment) ||
        (a.minUniformBufferOffsetAlignment < b.minUniformBufferOffsetAlignment) ||
        (a.minStorageBufferOffsetAlignment < b.minStorageBufferOffsetAlignment) ||
        (a.minTexelOffset > b.minTexelOffset) ||
        (a.maxTexelOffset < b.maxTexelOffset) ||
        (a.minTexelGatherOffset > b.minTexelGatherOffset) ||
        (a.maxTexelGatherOffset < b.maxTexelGatherOffset) ||
        (a.minInterpolationOffset > b.minInterpolationOffset) ||
        (a.maxInterpolationOffset < b.maxInterpolationOffset) ||
        (a.subPixelInterpolationOffsetBits < b.subPixelInterpolationOffsetBits) ||
        (a.maxFramebufferWidth < b.maxFramebufferWidth) ||
        (a.maxFramebufferHeight < b.maxFramebufferHeight) ||
        (a.maxFramebufferLayers < b.maxFramebufferLayers) ||
        (a.framebufferColorSampleCounts < b.framebufferColorSampleCounts) ||
        (a.framebufferDepthSampleCounts < b.framebufferDepthSampleCounts) ||
        (a.framebufferStencilSampleCounts < b.framebufferStencilSampleCounts) ||
        (a.framebufferNoAttachmentsSampleCounts < b.framebufferNoAttachmentsSampleCounts) ||
        (a.maxColorAttachments < b.maxColorAttachments) ||
        (a.sampledImageColorSampleCounts < b.sampledImageColorSampleCounts) ||
        (a.sampledImageIntegerSampleCounts < b.sampledImageIntegerSampleCounts) ||
        (a.sampledImageDepthSampleCounts < b.sampledImageDepthSampleCounts) ||
        (a.sampledImageStencilSampleCounts < b.sampledImageStencilSampleCounts) ||
        (a.storageImageSampleCounts < b.storageImageSampleCounts) ||
        (a.maxSampleMaskWords < b.maxSampleMaskWords) ||
        (a.timestampComputeAndGraphics < b.timestampComputeAndGraphics) ||
        (a.timestampPeriod < b.timestampPeriod) ||
        (a.maxClipDistances < b.maxClipDistances) ||
        (a.maxCullDistances < b.maxCullDistances) ||
        (a.maxCombinedClipAndCullDistances < b.maxCombinedClipAndCullDistances) ||
        (a.discreteQueuePriorities < b.discreteQueuePriorities) ||
        (a.pointSizeRange[0] < b.pointSizeRange[0]) ||
        (a.pointSizeRange[1] < b.pointSizeRange[1]) ||
        (a.lineWidthRange[0] < b.lineWidthRange[0]) ||
        (a.lineWidthRange[1] < b.lineWidthRange[1]) ||
        (a.pointSizeGranularity < b.pointSizeGranularity) ||
        (a.lineWidthGranularity < b.lineWidthGranularity) ||
        (a.strictLines < b.strictLines) ||
        (a.standardSampleLocations < b.standardSampleLocations) ||
        (a.optimalBufferCopyOffsetAlignment < b.optimalBufferCopyOffsetAlignment) ||
        (a.optimalBufferCopyRowPitchAlignment < b.optimalBufferCopyRowPitchAlignment) ||
        (a.nonCoherentAtomSize < b.nonCoherentAtomSize);
}
