/**
 * @file cgfx_ctx.c
 * @brief Implementation of the cgfx context module.
 */
#include "cgfx_ctx.h"
#include "cgfx_internal.h"

#include <stdio.h>
#include <string.h>

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef _WIN32
#include <hwnd3webgpu.h>
#endif


/* ── Internal error callbacks ─────────────────────────────────────── */

static void cgfx__device_lost_callback(WGPUDeviceLostReason reason,
                                        char const *message,
                                        void *user_data) {
    (void)user_data;
    fprintf(stderr, "[cgfx] Device lost: reason %d", reason);
    if (message && strlen(message) > 0)
        fprintf(stderr, " (%s)", message);
    fprintf(stderr, "\n");
}

static void cgfx__device_error_callback(WGPUErrorType type,
                                         char const *message,
                                         void *user_data) {
    (void)user_data;
    fprintf(stderr, "[cgfx] Uncaptured device error: type %d", type);
    if (message)
        fprintf(stderr, " (%s)", message);
    fprintf(stderr, "\n");
}

WGPURequiredLimits cgfx_default_limits(void) {
    WGPURequiredLimits limits = {0};
    limits.nextInChain = nullptr;
    memset(&limits.limits, 0xFF, sizeof(limits.limits));
    return limits;
}


/* ── Shared WebGPU initialization ─────────────────────────────────── */

/**
 * Complete WebGPU initialization after the surface has been created.
 *
 * ctx->surface must already be set. Requests an adapter and device,
 * registers callbacks, gets the queue, configures the surface, and
 * optionally creates a depth buffer.
 */
static bool cgfx__init_from_surface(CgfxCtx *ctx,
                                    uint32_t width, uint32_t height,
                                    WGPUPresentMode present_mode,
                                    const WGPURequiredLimits *limits,
                                    bool depth_buffer,
                                    WGPUInstance instance) {
    /* ── Request adapter ─────────────────────────────────────────── */
    fprintf(stderr, "[cgfx] Requesting adapter...\n");
    WGPURequestAdapterOptions adapter_opts = {};
    adapter_opts.nextInChain = nullptr;
    adapter_opts.compatibleSurface = ctx->surface;
    WGPUAdapter adapter = cgfx__request_adapter_sync(instance, &adapter_opts);
    fprintf(stderr, "[cgfx] Got adapter: %p\n", (void *)&adapter);

    wgpuInstanceRelease(instance);

    if (!adapter) {
        fprintf(stderr, "[cgfx] Failed to obtain WebGPU adapter\n");
        wgpuSurfaceRelease(ctx->surface);
        return false;
    }

#ifndef NDEBUG
    cgfx__inspect_adapter(adapter);

    WGPUSupportedLimits supported = {0};
    supported.nextInChain = nullptr;
    wgpuAdapterGetLimits(adapter, &supported);
    cgfx__inspect_limits("Adapter supported", &supported.limits);
    cgfx__inspect_limits("Requested", &limits->limits);
#endif

    /* ── Request device ──────────────────────────────────────────── */
    fprintf(stderr, "[cgfx] Requesting device...\n");
    WGPUDeviceDescriptor device_desc = {};
    device_desc.nextInChain = nullptr;
    device_desc.label = "cgfx device";
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = limits;
    device_desc.defaultQueue.nextInChain = nullptr;
    device_desc.defaultQueue.label = "cgfx default queue";
    device_desc.deviceLostCallback = &cgfx__device_lost_callback;
    ctx->device = cgfx__request_device_sync(adapter, &device_desc);
    fprintf(stderr, "[cgfx] Got device: %p\n", (void *)ctx->device);

    if (!ctx->device) {
        fprintf(stderr, "[cgfx] Failed to obtain WebGPU device\n");
        wgpuAdapterRelease(adapter);
        wgpuSurfaceRelease(ctx->surface);
        return false;
    }

    /* ── Error callback ──────────────────────────────────────────── */
    wgpuDeviceSetUncapturedErrorCallback(ctx->device,
                                          &cgfx__device_error_callback,
                                          nullptr);

    /* ── Get default queue ───────────────────────────────────────── */
    ctx->queue = wgpuDeviceGetQueue(ctx->device);

    /* ── Configure surface ───────────────────────────────────────── */
    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = width;
    config.height = height;
    config.usage = WGPUTextureUsage_RenderAttachment;
    ctx->surface_format = wgpuSurfaceGetPreferredFormat(ctx->surface, adapter);
    config.format = ctx->surface_format;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = ctx->device;
    config.presentMode = present_mode;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(ctx->surface, &config);

    wgpuAdapterRelease(adapter);

    ctx->width = width;
    ctx->height = height;

    /* ── Optional depth buffer ───────────────────────────────────── */
    if (depth_buffer) {
        ctx->depth_format = WGPUTextureFormat_Depth24Plus;

        WGPUTextureDescriptor depth_desc = {};
        depth_desc.dimension = WGPUTextureDimension_2D;
        depth_desc.format = ctx->depth_format;
        depth_desc.mipLevelCount = 1;
        depth_desc.sampleCount = 1;
        depth_desc.size = (WGPUExtent3D){ctx->width, ctx->height, 1};
        depth_desc.usage = WGPUTextureUsage_RenderAttachment;
        depth_desc.viewFormatCount = 1;
        depth_desc.viewFormats = &ctx->depth_format;
        ctx->depth_texture = wgpuDeviceCreateTexture(ctx->device, &depth_desc);

        WGPUTextureViewDescriptor depth_view_desc = {};
        depth_view_desc.aspect = WGPUTextureAspect_DepthOnly;
        depth_view_desc.baseArrayLayer = 0;
        depth_view_desc.arrayLayerCount = 1;
        depth_view_desc.baseMipLevel = 0;
        depth_view_desc.mipLevelCount = 1;
        depth_view_desc.dimension = WGPUTextureViewDimension_2D;
        depth_view_desc.format = ctx->depth_format;
        ctx->depth_texture_view = wgpuTextureCreateView(ctx->depth_texture, &depth_view_desc);
    }

    return true;
}


/* ── Public API ───────────────────────────────────────────────────── */

bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc) {
    *ctx = (CgfxCtx){};

    const int32_t width  = desc->width  ? (int32_t)desc->width  : 1280;
    const int32_t height = desc->height ? (int32_t)desc->height : 720;
    const char *title = desc->title ? desc->title : "cgfx";
    const WGPUPresentMode present_mode = desc->present_mode ? desc->present_mode
                                                      : WGPUPresentMode_Fifo;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc->resizable ? GLFW_TRUE : GLFW_FALSE);
    ctx->window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!ctx->window) {
        fprintf(stderr, "[cgfx] Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    WGPUInstanceDescriptor instance_desc = {};
    WGPUInstance instance = wgpuCreateInstance(&instance_desc);
    if (!instance) {
        fprintf(stderr, "[cgfx] Failed to create WebGPU instance\n");
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
        return false;
    }

    ctx->surface = glfwGetWGPUSurface(instance, ctx->window);

    if (!cgfx__init_from_surface(ctx, width, height, present_mode,
                                 &desc->limits, desc->depth_buffer, instance)) {
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
        return false;
    }

    return true;
}


bool cgfx_ctx_init_external(CgfxCtx *ctx, const CgfxCtxExternalDesc *desc) {
    *ctx = (CgfxCtx){};

    if (!desc->native_handle) {
        fprintf(stderr, "[cgfx] native_handle is NULL\n");
        return false;
    }

    if (!desc->width || !desc->height) {
        fprintf(stderr, "[cgfx] width and height must be non-zero for external context\n");
        return false;
    }

    const WGPUPresentMode present_mode = desc->present_mode ? desc->present_mode
                                                      : WGPUPresentMode_Fifo;

#ifdef _WIN32
    WGPUInstanceDescriptor instance_desc = {};
    WGPUInstance instance = wgpuCreateInstance(&instance_desc);
    if (!instance) {
        fprintf(stderr, "[cgfx] Failed to create WebGPU instance\n");
        return false;
    }

    ctx->surface = hwndGetWGPUSurface(instance, desc->native_handle);
    if (!ctx->surface) {
        fprintf(stderr, "[cgfx] Failed to create surface from HWND\n");
        wgpuInstanceRelease(instance);
        return false;
    }

    return cgfx__init_from_surface(ctx, desc->width, desc->height,
                                   present_mode, &desc->limits,
                                   desc->depth_buffer, instance);
#else
    (void)present_mode;
    fprintf(stderr, "[cgfx] cgfx_ctx_init_external() is only supported on Windows\n");
    return false;
#endif
}


bool cgfx_ctx_is_running(const CgfxCtx *ctx) {
    if (!ctx->window) return true;
    return !glfwWindowShouldClose(ctx->window);
}

void cgfx_ctx_destroy(CgfxCtx *ctx) {
    if (ctx->depth_texture_view) {
        wgpuTextureViewRelease(ctx->depth_texture_view);
        wgpuTextureRelease(ctx->depth_texture);
    }
    wgpuSurfaceUnconfigure(ctx->surface);
    wgpuQueueRelease(ctx->queue);
    wgpuSurfaceRelease(ctx->surface);
    wgpuDeviceRelease(ctx->device);
    if (ctx->window) {
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
    }
}
