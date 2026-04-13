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

#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdint.h>

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
    uint32_t         width;         /**< Window width in pixels.  0 = 1280.           */
    uint32_t         height;        /**< Window height in pixels. 0 = 720.            */
    const char      *title;         /**< Window title string.     NULL = "cgfx".       */
    bool             resizable;     /**< Allow window resize.     Default: true.       */
    WGPUPresentMode  present_mode;  /**< Surface present mode.    0 = Fifo (VSync).    */
} CgfxCtxDesc;

/**
 * The central rendering context.
 *
 * Owns the GLFW window and all core WebGPU objects needed for rendering.
 * Created with cgfx_ctx_init(), destroyed with cgfx_ctx_destroy().
 */
typedef struct CgfxCtx {
    GLFWwindow        *window;         /**< The GLFW window handle.                    */
    WGPUDevice         device;         /**< The logical GPU device.                    */
    WGPUQueue          queue;          /**< The default command queue.                 */
    WGPUSurface        surface;        /**< The window surface for presenting frames.  */
    WGPUTextureFormat  surface_format; /**< The preferred surface texture format.      */
    uint32_t           width;          /**< Current window width in pixels.            */
    uint32_t           height;         /**< Current window height in pixels.           */
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
bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc);

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
bool cgfx_ctx_is_running(const CgfxCtx *ctx);

/**
 * Destroy the context and release all resources.
 *
 * Releases resources in reverse creation order:
 *   1. Release the command queue
 *   2. Unconfigure and release the surface
 *   3. Release the device
 *   4. Destroy the GLFW window
 *   5. Terminate GLFW
 *
 * After this call, the context must not be used.
 *
 * @param ctx  Context to destroy.
 */
void cgfx_ctx_destroy(CgfxCtx *ctx);

#endif /* CGFX_CTX_H */
