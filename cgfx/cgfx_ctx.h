/**
 * @file cgfx_ctx.h
 * @brief Context module — window, device, queue, and surface management.
 *
 * The context is the central object in cgfx. It encapsulates the entire
 * WebGPU initialization sequence (GLFW window, WebGPU instance, adapter,
 * device, queue, surface configuration) behind a single init call.
 *
 * All struct fields are public so you can access raw WebGPU handles
 * (e.g., ctx.device) for advanced usage beyond what cgfx wraps.
 */
#ifndef CGFX_CTX_H
#define CGFX_CTX_H

#include "cgfx_webgpu.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdint.h>
#include "cgfx_export.h"
#include "cgfx_texture.h"

#ifdef _WIN32
#ifndef nullptr
#define nullptr (void*)0
#endif
#endif

/**
 * Callback invoked when the WebGPU device is lost (GPU reset, driver TDR, etc.).
 * Signature matches WGPUDeviceLostCallback.
 */
typedef void (*CgfxDeviceLostCallback)(WGPUDeviceLostReason reason,
                                       const char *message,
                                       void *user_data);

/**
 * Callback invoked on uncaptured WebGPU validation/OOM/internal errors.
 * Signature matches WGPUErrorCallback.
 */
typedef void (*CgfxDeviceErrorCallback)(WGPUErrorType type,
                                        const char *message,
                                        void *user_data);

/**
 * Configuration for creating a cgfx context.
 *
 * Zero-initialize this struct to get sensible defaults:
 *   - 1280x720 window
 *   - Title "cgfx"
 *   - Resizable window
 *   - VSync (Fifo present mode)
 *
 * Example:
 *   CgfxCtxDesc desc = { .width = 1920, .height = 1080, .title = "My App" };
 */
typedef struct CgfxCtxDesc {
    uint32_t            width;         /**< Window width in pixels.  0 = 1280.           */
    uint32_t            height;        /**< Window height in pixels. 0 = 720.            */
    const char          *title;         /**< Window title string.     NULL = "cgfx".       */
    bool                resizable;     /**< Allow window resize.     Default: false.      */
    bool                depth_buffer;  /**< Create a depth buffer at surface dimensions.  */
    WGPUPresentMode     present_mode;  /**< Surface present mode.    0 = Fifo (VSync).    */
    WGPURequiredLimits  limits;        /**< User defined limits.                          */
    uint32_t                  feature_count;      /**< Required device features. 0 = none.    */
    const WGPUFeatureName    *features;           /**< Feature array. NULL = none.             */
    CgfxDeviceLostCallback    on_device_lost;     /**< Device-lost callback. NULL = stderr.    */
    CgfxDeviceErrorCallback   on_device_error;    /**< Error callback. NULL = stderr.          */
    void                     *callback_user_data; /**< Passed to both callbacks.               */
} CgfxCtxDesc;

/**
 * The central rendering context.
 *
 * Owns the GLFW window and all core WebGPU objects needed for rendering.
 * Created with cgfx_ctx_init(), destroyed with cgfx_ctx_destroy().
 */
typedef struct CgfxCtx {
    GLFWwindow        *window;             /**< The GLFW window handle.                    */
    WGPUInstance       instance;           /**< The WebGPU instance (kept for event pump). */
    WGPUDevice         device;             /**< The logical GPU device.                    */
    WGPUQueue          queue;              /**< The default command queue.                 */
    WGPUSurface        surface;            /**< The window surface for presenting frames.  */
    WGPUTextureFormat  surface_format;     /**< The preferred surface texture format.      */
    CgfxTexture        depth_texture;      /**< Depth buffer (zero if disabled).            */
    WGPUPresentMode    present_mode;       /**< Active present mode (stored for resize).   */
    uint32_t           width;              /**< Current window width in pixels.            */
    uint32_t           height;             /**< Current window height in pixels.           */
} CgfxCtx;

/**
 * Initialize a cgfx context.
 *
 * Performs the complete WebGPU initialization sequence:
 *   1. Initialize GLFW and create a window (GLFW_NO_API, no OpenGL context)
 *   2. Create a WebGPU instance
 *   3. Create a platform-specific surface via glfwGetWGPUSurface
 *   4. Request a GPU adapter (synchronous wrapper around async callback)
 *   5. Request a logical device from the adapter
 *   6. Register error and device-lost callbacks (print to stderr)
 *   7. Get the default queue from the device
 *   8. Query the preferred surface format and configure the surface
 *
 * After this call succeeds, the context is ready for creating shaders,
 * pipelines, and rendering frames.
 *
 * @param ctx   Pointer to a caller-allocated CgfxCtx (typically on the stack).
 * @param desc  Configuration. Pass a zero-initialized struct for defaults.
 * @return      true on success, false on failure (errors printed to stderr).
 */
CGFX_API bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc);

/**
 * Return a WGPURequiredLimits with every limit set to "undefined" (no preference).
 *
 * Use this as a starting point, then override only the fields you need:
 *
 *   WGPURequiredLimits limits = cgfx_default_limits();
 *   limits.limits.maxVertexAttributes = 2;
 */
CGFX_API WGPURequiredLimits cgfx_default_limits(void);

/**
 * Configuration for creating a context from an externally-owned window.
 *
 * Used with cgfx_ctx_init_external() when the window is not created by GLFW
 * (e.g., a WinForms Panel, a Win32 HWND from an editor host).
 */
typedef struct CgfxCtxExternalDesc {
    void               *native_handle;  /**< Platform window handle (HWND on Windows). */
    uint32_t            width;          /**< Render surface width in pixels.            */
    uint32_t            height;         /**< Render surface height in pixels.           */
    bool                depth_buffer;   /**< Create a depth buffer at surface dimensions. */
    WGPUPresentMode     present_mode;   /**< Surface present mode. 0 = Fifo (VSync).   */
    WGPURequiredLimits  limits;         /**< User defined limits.                       */
    uint32_t                  feature_count;      /**< Required device features. 0 = none.    */
    const WGPUFeatureName    *features;           /**< Feature array. NULL = none.             */
    CgfxDeviceLostCallback    on_device_lost;     /**< Device-lost callback. NULL = stderr.    */
    CgfxDeviceErrorCallback   on_device_error;    /**< Error callback. NULL = stderr.          */
    void                     *callback_user_data; /**< Passed to both callbacks.               */
} CgfxCtxExternalDesc;

/**
 * Initialize a cgfx context from an externally-owned window handle.
 *
 * Skips GLFW entirely — the caller owns the window and its event loop.
 * Currently supports Win32 HWND; on other platforms this returns false.
 *
 * ctx->window will be NULL after this call. cgfx_ctx_is_running() always
 * returns true for external contexts (the caller decides lifetime).
 * cgfx_ctx_destroy() will skip GLFW teardown.
 *
 * @param ctx   Pointer to a caller-allocated CgfxCtx.
 * @param desc  Configuration with the native window handle.
 * @return      true on success, false on failure.
 */
CGFX_API bool cgfx_ctx_init_external(CgfxCtx *ctx, const CgfxCtxExternalDesc *desc);

/**
 * Check if the context window is still open.
 *
 * Returns true as long as the user has not requested to close the window
 * (e.g., clicking the X button, pressing Alt+F4). Use this as the
 * condition for your main render loop:
 *
 *   while (cgfx_ctx_is_running(&ctx)) { ... }
 *
 * @param ctx  Initialized context.
 * @return     true if the window is open and rendering should continue.
 */
CGFX_API bool cgfx_ctx_is_running(const CgfxCtx *ctx);

/**
 * Resize the rendering surface.
 *
 * Reconfigures the WebGPU surface and recreates the depth buffer (if one
 * exists) at the new dimensions. Updates ctx->width and ctx->height.
 *
 * Call this when the window is resized. For GLFW windows, call from a
 * glfwSetFramebufferSizeCallback handler. For external windows, call
 * from the platform resize handler (e.g., WM_SIZE on Windows).
 *
 * Returns false and does nothing if width or height is zero (minimized
 * window). The camera projection is NOT updated automatically — call
 * cgfx_camera_perspective() after resizing if you have a camera.
 *
 * @param ctx     Initialized context.
 * @param width   New framebuffer width in pixels.
 * @param height  New framebuffer height in pixels.
 * @return        true on success, false if dimensions are zero.
 */
CGFX_API bool cgfx_ctx_resize(CgfxCtx *ctx, uint32_t width, uint32_t height);

/**
 * Destroy the context and release all resources.
 *
 * Releases resources in reverse creation order:
 *   1. Destroy the depth texture (if enabled)
 *   2. Release the command queue
 *   3. Unconfigure and release the surface
 *   4. Release the device
 *   5. Destroy the GLFW window
 *   6. Terminate GLFW
 *
 * After this call, the context must not be used.
 *
 * @param ctx  Context to destroy.
 */
CGFX_API void cgfx_ctx_destroy(CgfxCtx *ctx);

#endif /* CGFX_CTX_H */
