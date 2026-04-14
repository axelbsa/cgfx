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


/* ── Internal error callbacks ─────────────────────────────────────── */

/**
 * Called by WebGPU when the device is lost (e.g., GPU disconnect, driver crash).
 * Prints the reason and message to stderr. This is registered once during init
 * and should not be called directly.
 */
static void cgfx__device_lost_callback(WGPUDeviceLostReason reason,
                                        char const *message,
                                        void *user_data) {
    (void)user_data;
    fprintf(stderr, "[cgfx] Device lost: reason %d", reason);
    if (message && strlen(message) > 0)
        fprintf(stderr, " (%s)", message);
    fprintf(stderr, "\n");
}

/**
 * Called by WebGPU for uncaptured device errors (validation errors, out-of-memory,
 * etc.). Prints the error type and message to stderr. Registered once during init.
 */
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


/* ── Public API ───────────────────────────────────────────────────── */

bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc) {
    /* Apply defaults for zero-initialized fields */
    const int32_t width  = desc->width  ? (int32_t)desc->width  : 1280;
    const int32_t height = desc->height ? (int32_t)desc->height : 720;
    const char *title = desc->title ? desc->title : "cgfx";
    const WGPUPresentMode present_mode = desc->present_mode ? desc->present_mode
                                                      : WGPUPresentMode_Fifo;

    /* ── Step 1: Initialize GLFW and create window ────────────────
     * GLFW_CLIENT_API = GLFW_NO_API because WebGPU provides its own
     * graphics context — we don't want GLFW to create an OpenGL one.
     */
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc->resizable ? GLFW_TRUE : GLFW_FALSE);
    ctx->window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!ctx->window) {
        fprintf(stderr, "[cgfx] Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    /* ── Step 2: Create WebGPU instance ───────────────────────────
     * The instance is the entry point to the WebGPU API. We create it
     * with default settings (empty descriptor).
     */
    WGPUInstanceDescriptor instance_desc = {};
    WGPUInstance instance = wgpuCreateInstance(&instance_desc);
    if (!instance) {
        fprintf(stderr, "[cgfx] Failed to create WebGPU instance\n");
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
        return false;
    }

    /* ── Step 3: Create surface from GLFW window ──────────────────
     * glfwGetWGPUSurface handles platform-specific surface creation:
     * Metal on macOS, X11/Wayland on Linux, HWND on Windows, Canvas on Web.
     */
    ctx->surface = glfwGetWGPUSurface(instance, ctx->window);

    /* ── Step 4: Request adapter ──────────────────────────────────
     * The adapter represents a physical GPU. We request one that is
     * compatible with our surface so it can render to the window.
     */
    fprintf(stderr, "[cgfx] Requesting adapter...\n");
    WGPURequestAdapterOptions adapter_opts = {};
    adapter_opts.nextInChain = nullptr;
    adapter_opts.compatibleSurface = ctx->surface;
    WGPUAdapter adapter = cgfx__request_adapter_sync(instance, &adapter_opts);
    fprintf(stderr, "[cgfx] Got adapter: %p\n", (void *)&adapter);

    /* Instance is no longer needed after adapter is obtained */
    wgpuInstanceRelease(instance);

    if (!adapter) {
        fprintf(stderr, "[cgfx] Failed to obtain WebGPU adapter\n");
        wgpuSurfaceRelease(ctx->surface);
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
        return false;
    }

#ifndef NDEBUG
    cgfx__inspect_adapter(adapter);

    WGPUSupportedLimits supported = {0};
    supported.nextInChain = nullptr;
    wgpuAdapterGetLimits(adapter, &supported);
    cgfx__inspect_limits("Adapter supported", &supported.limits);
    cgfx__inspect_limits("Requested", &desc->limits.limits);
#endif

    /* ── Step 5: Request device ───────────────────────────────────
     * The device is the logical GPU connection we use for all operations:
     * creating buffers, shaders, pipelines, and submitting commands.
     */
    fprintf(stderr, "[cgfx] Requesting device...\n");
    WGPUDeviceDescriptor device_desc = {};
    device_desc.nextInChain = nullptr;
    device_desc.label = "cgfx device";
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = &desc->limits;
    device_desc.defaultQueue.nextInChain = nullptr;
    device_desc.defaultQueue.label = "cgfx default queue";
    device_desc.deviceLostCallback = &cgfx__device_lost_callback;
    ctx->device = cgfx__request_device_sync(adapter, &device_desc);
    fprintf(stderr, "[cgfx] Got device: %p\n", (void *)ctx->device);

    if (!ctx->device) {
        fprintf(stderr, "[cgfx] Failed to obtain WebGPU device\n");
        wgpuAdapterRelease(adapter);
        wgpuSurfaceRelease(ctx->surface);
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
        return false;
    }

    /* ── Step 6: Register error callback ──────────────────────────
     * This catches validation errors, OOM, etc. during rendering.
     */
    wgpuDeviceSetUncapturedErrorCallback(ctx->device,
                                          &cgfx__device_error_callback,
                                          nullptr);

    /* ── Step 7: Get default queue ────────────────────────────────
     * The queue is used to submit command buffers and write buffer data.
     */
    ctx->queue = wgpuDeviceGetQueue(ctx->device);

    /* ── Step 8: Configure surface ────────────────────────────────
     * Query the preferred texture format for this adapter+surface combo,
     * then configure the surface swap chain with that format.
     */
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

    /* Adapter is no longer needed after surface configuration */
    wgpuAdapterRelease(adapter);

    ctx->width = width;
    ctx->height = height;

    return true;
}

bool cgfx_ctx_is_running(const CgfxCtx *ctx) {
    return !glfwWindowShouldClose(ctx->window);
}

void cgfx_ctx_destroy(CgfxCtx *ctx) {
    wgpuSurfaceUnconfigure(ctx->surface);
    wgpuQueueRelease(ctx->queue);
    wgpuSurfaceRelease(ctx->surface);
    wgpuDeviceRelease(ctx->device);
    glfwDestroyWindow(ctx->window);
    glfwTerminate();
}
