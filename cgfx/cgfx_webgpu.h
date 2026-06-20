/**
 * @file cgfx_webgpu.h
 * @brief WebGPU header selection + cross-generation compatibility helpers.
 *
 * cgfx targets two generations of the WebGPU C API from one source tree:
 *
 *   - Legacy: wgpu-native v0.19.x (Elie Michel's WebGPU-distribution). Labels
 *     are `const char *`, async requests take a callback + userdata, the device
 *     is drained with wgpuDevicePoll(), and the WGSL source struct is
 *     WGPUShaderModuleWGSLDescriptor.
 *   - Modern: Google Dawn / recent wgpu-native. Labels are WGPUStringView,
 *     async requests return WGPUFuture and take a *CallbackInfo, the device is
 *     drained with wgpuInstanceProcessEvents()/wgpuDeviceTick(), and the WGSL
 *     source struct is WGPUShaderSourceWGSL.
 *
 * The two are distinguished at compile time by WGPU_STRLEN, a macro only the
 * modern webgpu.h defines. All cgfx sources include this header instead of
 * <webgpu/webgpu.h> directly, and gate generation-specific code on
 * CGFX_WEBGPU_MODERN. See TRANSLATION.md for the full API mapping.
 */
#ifndef CGFX_WEBGPU_H
#define CGFX_WEBGPU_H

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif

#include <stdbool.h>

/* Modern header generation (Dawn / recent wgpu-native): StringView + Futures. */
#ifdef WGPU_STRLEN
#  define CGFX_WEBGPU_MODERN 1
#else
#  define CGFX_WEBGPU_MODERN 0
#endif

#if CGFX_WEBGPU_MODERN
/*
 * The modern API dropped the bitmask *Flags typedefs (the enum type is used
 * directly) and the WGPURequiredLimits / WGPUSupportedLimits wrapper structs
 * (a bare WGPULimits is passed instead). Provide compatibility aliases so the
 * existing cgfx type signatures keep compiling; call sites that hand these to
 * WebGPU unwrap the inner WGPULimits under CGFX_WEBGPU_MODERN.
 */
typedef WGPUTextureUsage WGPUTextureUsageFlags;
typedef WGPUShaderStage  WGPUShaderStageFlags;
typedef WGPUBufferUsage  WGPUBufferUsageFlags;

typedef struct { const void *nextInChain; WGPULimits limits; } WGPURequiredLimits;
typedef struct { const void *nextInChain; WGPULimits limits; } WGPUSupportedLimits;

/* Texture-copy structs were renamed (identical fields). */
typedef WGPUTexelCopyTextureInfo  WGPUImageCopyTexture;
typedef WGPUTexelCopyBufferLayout WGPUTextureDataLayout;

/* The compute pipeline stage struct was renamed (module/entryPoint preserved). */
typedef WGPUComputeState WGPUProgrammableStageDescriptor;
#endif

/**
 * Wrap a string literal as a label. WGPUStringView on the modern API, a plain
 * `const char *` on the legacy API. Use everywhere a `.label` is assigned.
 */
#if CGFX_WEBGPU_MODERN
#  define CGFX_STR(s) ((WGPUStringView){ .data = (s), .length = WGPU_STRLEN })
#else
#  define CGFX_STR(s) (s)
#endif

/**
 * Drain pending GPU work so async callbacks fire and submitted work advances.
 *
 * @param instance  Owning instance (used by the modern process-events path).
 * @param device    Device to poll/tick.
 * @param wait      Legacy wgpu-native only: block until work completes.
 */
static inline void cgfx__device_sync(WGPUInstance instance,
                                     WGPUDevice device, bool wait) {
#if defined(__EMSCRIPTEN__)
    (void)instance; (void)device; (void)wait; /* browser drives the event loop */
#elif CGFX_WEBGPU_MODERN
    (void)device; (void)wait;
    wgpuInstanceProcessEvents(instance);
#elif defined(WEBGPU_BACKEND_DAWN)
    (void)instance; (void)wait;
    wgpuDeviceTick(device);
#else
    (void)instance;
    wgpuDevicePoll(device, wait, nullptr);
#endif
}

/**
 * True when a surface texture was acquired successfully. The modern API split
 * the single Success status into SuccessOptimal / SuccessSuboptimal.
 */
static inline bool
cgfx__surface_texture_ok(WGPUSurfaceGetCurrentTextureStatus status) {
#if CGFX_WEBGPU_MODERN
    return status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
        || status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
#else
    return status == WGPUSurfaceGetCurrentTextureStatus_Success;
#endif
}

/**
 * Query the preferred surface texture format across header generations.
 *
 * Legacy exposes wgpuSurfaceGetPreferredFormat(); the modern API replaced it
 * with wgpuSurfaceGetCapabilities() whose first format is the preferred one.
 */
static inline WGPUTextureFormat
cgfx__surface_preferred_format(WGPUSurface surface, WGPUAdapter adapter) {
#if CGFX_WEBGPU_MODERN
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface, adapter, &caps);
    WGPUTextureFormat fmt = caps.formatCount
        ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return fmt;
#else
    return wgpuSurfaceGetPreferredFormat(surface, adapter);
#endif
}

#endif /* CGFX_WEBGPU_H */
