/**
 * @file cgfx_internal.h
 * @brief Internal helpers for cgfx — not part of the public API.
 *
 * Contains synchronous wrappers around WebGPU's asynchronous adapter and
 * device request callbacks. These are used by cgfx_ctx_init() internally.
 *
 * Functions prefixed with cgfx__ (double underscore) are internal.
 */
#ifndef CGFX_INTERNAL_H
#define CGFX_INTERNAL_H

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <webgpu/webgpu.h>


/* ── Adapter request (synchronous wrapper) ────────────────────────── */

typedef struct {
    WGPUAdapter adapter;
    bool request_ended;
} CgfxAdapterRequestData;

static void cgfx__on_adapter_request_ended(WGPURequestAdapterStatus status,
                                            WGPUAdapter adapter,
                                            char const *message,
                                            void *user_data) {
    CgfxAdapterRequestData *data = user_data;
    if (status == WGPURequestAdapterStatus_Success) {
        data->adapter = adapter;
    } else {
        fprintf(stderr, "[cgfx] Could not get WebGPU adapter: %s\n", message);
    }
    data->request_ended = true;
}

/**
 * Request a WebGPU adapter synchronously.
 *
 * Internally calls wgpuInstanceRequestAdapter with a callback that captures
 * the result. On Emscripten, polls with emscripten_sleep until the async
 * operation completes. On native backends the callback fires synchronously.
 *
 * @param instance  The WebGPU instance to request from.
 * @param options   Adapter request options (e.g., compatible surface).
 * @return          The adapter handle, or NULL on failure.
 */
static WGPUAdapter cgfx__request_adapter_sync(WGPUInstance instance,
                                               const WGPURequestAdapterOptions *options) {
    CgfxAdapterRequestData data = { .adapter = nullptr, .request_ended = false };

    wgpuInstanceRequestAdapter(instance, options,
                               &cgfx__on_adapter_request_ended,
                               (void *)&data);

#ifdef __EMSCRIPTEN__
    while (!data.request_ended) {
        emscripten_sleep(100);
    }
#endif

    assert(data.request_ended);
    return data.adapter;
}


/* ── Device request (synchronous wrapper) ─────────────────────────── */

typedef struct {
    WGPUDevice device;
    bool request_ended;
} CgfxDeviceRequestData;

static void cgfx__on_device_request_ended(WGPURequestDeviceStatus status,
                                           WGPUDevice device,
                                           char const *message,
                                           void *user_data) {
    CgfxDeviceRequestData *data = user_data;
    if (status == WGPURequestDeviceStatus_Success) {
        data->device = device;
    } else {
        fprintf(stderr, "[cgfx] Could not get WebGPU device: %s\n", message);
    }
    data->request_ended = true;
}

/**
 * Request a WebGPU device synchronously.
 *
 * Same pattern as cgfx__request_adapter_sync: wraps the async callback
 * into a blocking call. On Emscripten, polls with emscripten_sleep.
 *
 * @param adapter     The adapter to request the device from.
 * @param descriptor  Device descriptor (features, limits, queue label, etc.).
 * @return            The device handle, or NULL on failure.
 */
static WGPUDevice cgfx__request_device_sync(WGPUAdapter adapter,
                                             const WGPUDeviceDescriptor *descriptor) {
    CgfxDeviceRequestData data = { .device = nullptr, .request_ended = false };

    wgpuAdapterRequestDevice(adapter, descriptor,
                             &cgfx__on_device_request_ended,
                             (void *)&data);

#ifdef __EMSCRIPTEN__
    while (!data.request_ended) {
        emscripten_sleep(100);
    }
#endif

    assert(data.request_ended);
    return data.device;
}

/* ── Debug inspection (compiled out in release / NDEBUG) ──────────── */

#ifndef NDEBUG

#include <string.h>

static const char *cgfx__adapter_type_str(WGPUAdapterType type) {
    switch (type) {
    case WGPUAdapterType_DiscreteGPU:   return "Discrete GPU";
    case WGPUAdapterType_IntegratedGPU: return "Integrated GPU";
    case WGPUAdapterType_CPU:           return "CPU";
    default:                            return "Unknown";
    }
}

static const char *cgfx__backend_type_str(WGPUBackendType type) {
    switch (type) {
    case WGPUBackendType_Null:     return "Null";
    case WGPUBackendType_WebGPU:   return "WebGPU";
    case WGPUBackendType_D3D11:    return "D3D11";
    case WGPUBackendType_D3D12:    return "D3D12";
    case WGPUBackendType_Metal:    return "Metal";
    case WGPUBackendType_Vulkan:   return "Vulkan";
    case WGPUBackendType_OpenGL:   return "OpenGL";
    case WGPUBackendType_OpenGLES: return "OpenGL ES";
    default:                       return "Undefined";
    }
}

static void cgfx__inspect_adapter(WGPUAdapter adapter) {
    WGPUAdapterProperties props = {0};
    props.nextInChain = nullptr;
    wgpuAdapterGetProperties(adapter, &props);

    fprintf(stderr, "[cgfx] Adapter:\n");
    fprintf(stderr, "         name:    %s\n", props.name ? props.name : "(null)");
    fprintf(stderr, "         vendor:  %s (0x%04X)\n", props.vendorName ? props.vendorName : "(null)", props.vendorID);
    fprintf(stderr, "         device:  0x%04X\n", props.deviceID);
    fprintf(stderr, "         driver:  %s\n", props.driverDescription ? props.driverDescription : "(null)");
    fprintf(stderr, "         type:    %s\n", cgfx__adapter_type_str(props.adapterType));
    fprintf(stderr, "         backend: %s\n", cgfx__backend_type_str(props.backendType));

    size_t count = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    fprintf(stderr, "         features: %zu\n", count);
}

static void cgfx__inspect_limits(const char *label, const WGPULimits *l) {
    fprintf(stderr, "[cgfx] %s limits:\n", label);
    fprintf(stderr, "         maxTextureDimension1D          = %u\n",  l->maxTextureDimension1D);
    fprintf(stderr, "         maxTextureDimension2D          = %u\n",  l->maxTextureDimension2D);
    fprintf(stderr, "         maxTextureDimension3D          = %u\n",  l->maxTextureDimension3D);
    fprintf(stderr, "         maxTextureArrayLayers           = %u\n",  l->maxTextureArrayLayers);
    fprintf(stderr, "         maxBindGroups                   = %u\n",  l->maxBindGroups);
    fprintf(stderr, "         maxBindingsPerBindGroup         = %u\n",  l->maxBindingsPerBindGroup);
    fprintf(stderr, "         maxDynamicUniformBuffers        = %u\n",  l->maxDynamicUniformBuffersPerPipelineLayout);
    fprintf(stderr, "         maxDynamicStorageBuffers        = %u\n",  l->maxDynamicStorageBuffersPerPipelineLayout);
    fprintf(stderr, "         maxSampledTextures/stage        = %u\n",  l->maxSampledTexturesPerShaderStage);
    fprintf(stderr, "         maxSamplers/stage               = %u\n",  l->maxSamplersPerShaderStage);
    fprintf(stderr, "         maxStorageBuffers/stage         = %u\n",  l->maxStorageBuffersPerShaderStage);
    fprintf(stderr, "         maxStorageTextures/stage        = %u\n",  l->maxStorageTexturesPerShaderStage);
    fprintf(stderr, "         maxUniformBuffers/stage         = %u\n",  l->maxUniformBuffersPerShaderStage);
    fprintf(stderr, "         maxUniformBufferBindingSize     = %lu\n", (unsigned long)l->maxUniformBufferBindingSize);
    fprintf(stderr, "         maxStorageBufferBindingSize     = %lu\n", (unsigned long)l->maxStorageBufferBindingSize);
    fprintf(stderr, "         minUniformBufferOffsetAlignment = %u\n",  l->minUniformBufferOffsetAlignment);
    fprintf(stderr, "         minStorageBufferOffsetAlignment = %u\n",  l->minStorageBufferOffsetAlignment);
    fprintf(stderr, "         maxVertexBuffers                = %u\n",  l->maxVertexBuffers);
    fprintf(stderr, "         maxBufferSize                   = %lu\n", (unsigned long)l->maxBufferSize);
    fprintf(stderr, "         maxVertexAttributes             = %u\n",  l->maxVertexAttributes);
    fprintf(stderr, "         maxVertexBufferArrayStride      = %u\n",  l->maxVertexBufferArrayStride);
    fprintf(stderr, "         maxInterStageShaderComponents   = %u\n",  l->maxInterStageShaderComponents);
    fprintf(stderr, "         maxInterStageShaderVariables    = %u\n",  l->maxInterStageShaderVariables);
    fprintf(stderr, "         maxColorAttachments             = %u\n",  l->maxColorAttachments);
    fprintf(stderr, "         maxComputeWorkgroupStorageSize  = %u\n",  l->maxComputeWorkgroupStorageSize);
    fprintf(stderr, "         maxComputeInvocationsPerGroup   = %u\n",  l->maxComputeInvocationsPerWorkgroup);
    fprintf(stderr, "         maxComputeWorkgroupSizeX        = %u\n",  l->maxComputeWorkgroupSizeX);
    fprintf(stderr, "         maxComputeWorkgroupSizeY        = %u\n",  l->maxComputeWorkgroupSizeY);
    fprintf(stderr, "         maxComputeWorkgroupSizeZ        = %u\n",  l->maxComputeWorkgroupSizeZ);
    fprintf(stderr, "         maxComputeWorkgroupsPerDim      = %u\n",  l->maxComputeWorkgroupsPerDimension);
}

#endif /* !NDEBUG */

#endif /* CGFX_INTERNAL_H */
