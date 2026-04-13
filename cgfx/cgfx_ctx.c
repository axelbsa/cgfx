/**
 * @file cgfx_ctx.c
 * @brief Implementation of the cgfx context module.
 */
#include "cgfx_ctx.h"
#include "cgfx_internal.h"

#include <stdio.h>
#include <string.h>

#include <webgpu/webgpu.h>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


/* -- Internal error callbacks --------------------------------------------- */

/**
 * Called by WebGPU when the device is lost (e.g., GPU disconnect, driver crash).
 * Prints the reason and message to stderr. This is registered once during init
 * and should not be called directly.
 */
static void cgfx__device_lost_callback(WGPUDevice const *device,
                                        WGPUDeviceLostReason reason,
                                        WGPUStringView message,
                                        void *user_data1,
                                        void *user_data2) {
    (void)device;
    (void)user_data1;
    (void)user_data2;
    fprintf(stderr, "[cgfx] Device lost: reason %d", reason);
    if (message.data && message.length > 0)
        fprintf(stderr, " (%.*s)", (int)message.length, message.data);
    fprintf(stderr, "\n");
}

/**
 * Called by WebGPU for uncaptured device errors (validation errors, out-of-memory,
 * etc.). Prints the error type and message to stderr. Registered once during init.
 */
static void cgfx__device_error_callback(WGPUDevice const *device,
                                         WGPUErrorType type,
                                         WGPUStringView message,
                                         void *user_data1,
                                         void *user_data2) {
    (void)device;
    (void)user_data1;
    (void)user_data2;
    fprintf(stderr, "[cgfx] Uncaptured device error: type %d", type);
    if (message.data)
        fprintf(stderr, " (%.*s)", (int)message.length, message.data);
    fprintf(stderr, "\n");
}


/* -- Public API ----------------------------------------------------------- */

bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc) {
    /* Apply defaults for zero-initialized fields */
    uint32_t width  = desc->width  ? desc->width  : 1280;
    uint32_t height = desc->height ? desc->height : 720;
    const char *title = desc->title ? desc->title : "cgfx";
    WGPUPresentMode present_mode = desc->present_mode ? desc->present_mode
                                                      : WGPUPresentMode_Fifo;

    /* -- Step 1: Initialize GLFW and create window --------------------
     * GLFW_CLIENT_API = GLFW_NO_API because WebGPU provides its own
     * graphics context -- we don't want GLFW to create an OpenGL one.
     */
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc->resizable ? GLFW_TRUE : GLFW_TRUE);
    ctx->window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!ctx->window) {
        fprintf(stderr, "[cgfx] Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    /* -- Step 2: Create WebGPU instance -------------------------------
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

    /* -- Step 3: Create surface from GLFW window ----------------------
     * glfwGetWGPUSurface handles platform-specific surface creation:
     * Metal on macOS, X11/Wayland on Linux, HWND on Windows, Canvas on Web.
     */
    ctx->surface = glfwGetWGPUSurface(instance, ctx->window);

    /* -- Step 4: Request adapter --------------------------------------
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

    /* Log adapter info for diagnostics */
    {
        WGPUAdapterInfo info = {};
        wgpuAdapterGetInfo(adapter, &info);
        const char *backend_names[] = {
            [WGPUBackendType_Undefined] = "Undefined",
            [WGPUBackendType_Null]      = "Null",
            [WGPUBackendType_WebGPU]    = "WebGPU",
            [WGPUBackendType_D3D11]     = "D3D11",
            [WGPUBackendType_D3D12]     = "D3D12",
            [WGPUBackendType_Metal]     = "Metal",
            [WGPUBackendType_Vulkan]    = "Vulkan",
            [WGPUBackendType_OpenGL]    = "OpenGL",
            [WGPUBackendType_OpenGLES]  = "OpenGLES",
        };
        const char *backend = (info.backendType <= WGPUBackendType_OpenGLES)
                              ? backend_names[info.backendType] : "Unknown";
        fprintf(stderr, "[cgfx] Adapter: %.*s (%s backend)\n",
                (int)info.description.length, info.description.data, backend);
        wgpuAdapterInfoFreeMembers(info);
    }

    /* -- Step 5: Request device ---------------------------------------
     * The device is the logical GPU connection we use for all operations:
     * creating buffers, shaders, pipelines, and submitting commands.
     *
     * Device lost and uncaptured error callbacks are now set via the
     * device descriptor (v29 API), not via separate setter functions.
     */
    fprintf(stderr, "[cgfx] Requesting device...\n");

    WGPUDeviceDescriptor device_desc = {};
    device_desc.nextInChain = nullptr;
    device_desc.label = (WGPUStringView){ .data = "cgfx device", .length = WGPU_STRLEN };
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredFeatures = nullptr;
    device_desc.requiredLimits = nullptr;
    device_desc.defaultQueue.nextInChain = nullptr;
    device_desc.defaultQueue.label = (WGPUStringView){ .data = "cgfx default queue", .length = WGPU_STRLEN };

    device_desc.deviceLostCallbackInfo = (WGPUDeviceLostCallbackInfo){
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = &cgfx__device_lost_callback,
        .userdata1 = nullptr,
        .userdata2 = nullptr,
    };

    device_desc.uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo){
        .nextInChain = nullptr,
        .callback = &cgfx__device_error_callback,
        .userdata1 = nullptr,
        .userdata2 = nullptr,
    };

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

    /* -- Step 6: Get default queue ------------------------------------
     * The queue is used to submit command buffers and write buffer data.
     */
    ctx->queue = wgpuDeviceGetQueue(ctx->device);

    /* -- Step 7: Configure surface ------------------------------------
     * Query surface capabilities and pick the preferred texture format,
     * then configure the surface swap chain with that format.
     *
     * wgpuSurfaceGetPreferredFormat was removed in the v29 API;
     * use wgpuSurfaceGetCapabilities and pick formats[0] instead.
     */
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(ctx->surface, adapter, &caps);
    ctx->surface_format = caps.formats[0];

    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = width;
    config.height = height;
    config.usage = WGPUTextureUsage_RenderAttachment;
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
